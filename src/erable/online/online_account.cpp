// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/online_account.h"

#include <memory>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>

#include "common/fs/path_util.h"
#include "common/logging.h"

namespace Online {

namespace {

/// The Erable server, reached by its address (no domain name)
constexpr char DefaultServer[] = "https://91.134.241.94:24870";
/// SHA-256 of the server's self-signed certificate, in hex (EDEN_NX2_ONLINE_PIN overrides it)
constexpr char DefaultPin[] = "3dfc8baf6a32e2a2df8b7bf6ac08cf91ddd1b637c9c631e3fe8b4e4409d54dee";

constexpr int TimeoutMs = 15000;

QString SessionPath() {
    return QString::fromStdString(Common::FS::PathToUTF8String(
        Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "online_session.json"));
}

QByteArray ParsePin(QByteArray text) {
    text = text.trimmed().toLower().replace(":", "");
    const QByteArray pin = QByteArray::fromHex(text);
    return pin.size() == 32 ? pin : QByteArray{};
}

} // Anonymous namespace

Account::Account(QObject* parent) : QObject{parent}, network{new QNetworkAccessManager(this)} {
    const QByteArray server_env = qgetenv("EDEN_NX2_ONLINE_SERVER");
    server = QUrl(server_env.isEmpty() ? QString::fromLatin1(DefaultServer)
                                       : QString::fromUtf8(server_env));
    const QByteArray pin_env = qgetenv("EDEN_NX2_ONLINE_PIN");
    pinned_sha256 = ParsePin(pin_env.isEmpty() ? QByteArray{DefaultPin} : pin_env);
    LoadSession();
}

Account::~Account() = default;

bool Account::Enabled() {
    return qgetenv("EDEN_NX2_ONLINE") == "1";
}

Account* Account::Shared() {
    if (!Enabled()) {
        return nullptr;
    }
    static QPointer<Account> shared;
    if (!shared) {
        shared = new Account(qApp);
    }
    return shared;
}

QStringList Account::QuestionKeys() {
    return {QStringLiteral("first_pet"),      QStringLiteral("birth_city"),
            QStringLiteral("first_school"),   QStringLiteral("childhood_friend"),
            QStringLiteral("favourite_game"), QStringLiteral("mother_maiden_name")};
}

QString Account::QuestionText(const QString& key) {
    if (key == QStringLiteral("first_pet")) {
        return tr("What was the name of your first pet?");
    }
    if (key == QStringLiteral("birth_city")) {
        return tr("In which city were you born?");
    }
    if (key == QStringLiteral("first_school")) {
        return tr("What was the name of your first school?");
    }
    if (key == QStringLiteral("childhood_friend")) {
        return tr("What is the first name of your childhood best friend?");
    }
    if (key == QStringLiteral("favourite_game")) {
        return tr("What was the first video game you loved?");
    }
    if (key == QStringLiteral("mother_maiden_name")) {
        return tr("What is your mother's maiden name?");
    }
    return key;
}

QString Account::ErrorText(const QString& code) {
    if (code == QStringLiteral("network")) {
        return tr("The Erable server can't be reached. Check your connection and try again.");
    }
    if (code == QStringLiteral("untrusted")) {
        return tr("The server's identity could not be verified, so nothing was sent.");
    }
    if (code == QStringLiteral("wrong_credentials")) {
        return tr("Wrong e-mail or password.");
    }
    if (code == QStringLiteral("wrong_recovery")) {
        return tr("The answer or the security code is wrong.");
    }
    if (code == QStringLiteral("too_many_attempts")) {
        return tr("Too many attempts. Wait 15 minutes and try again.");
    }
    if (code == QStringLiteral("email_taken")) {
        return tr("An account already uses this e-mail.");
    }
    if (code == QStringLiteral("nickname_taken")) {
        return tr("This nickname is taken.");
    }
    if (code == QStringLiteral("unknown_email")) {
        return tr("No account uses this e-mail.");
    }
    if (code == QStringLiteral("bad_nickname")) {
        return tr("The nickname needs 4 to 20 letters (without accents), digits, spaces, '.', "
                  "'_' or '-'.");
    }
    if (code == QStringLiteral("bad_email")) {
        return tr("This e-mail address is not valid.");
    }
    if (code == QStringLiteral("weak_password")) {
        return tr("The password needs at least 8 characters.");
    }
    if (code == QStringLiteral("bad_code")) {
        return tr("The security code needs 6 to 12 digits.");
    }
    if (code == QStringLiteral("bad_answer")) {
        return tr("The answer is too short.");
    }
    if (code == QStringLiteral("no_session")) {
        return tr("Your session has ended. Sign in again.");
    }
    return tr("The server refused the request (%1).").arg(code);
}

void Account::Register(const QString& nickname, const QString& email, const QString& password,
                       const QString& question, const QString& answer, const QString& code,
                       Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/register"),
         {{QStringLiteral("nickname"), nickname.trimmed()},
          {QStringLiteral("email"), email.trimmed()},
          {QStringLiteral("password"), password},
          {QStringLiteral("question"), question},
          {QStringLiteral("answer"), answer},
          {QStringLiteral("code"), code.trimmed()}},
         false, [this, done = std::move(done)](const Result& result) {
             TakeSession(result);
             done(result);
         });
}

void Account::Login(const QString& email, const QString& password, Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/login"),
         {{QStringLiteral("email"), email.trimmed()}, {QStringLiteral("password"), password}},
         false, [this, done = std::move(done)](const Result& result) {
             TakeSession(result);
             done(result);
         });
}

void Account::RecoveryQuestion(const QString& email, Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/recovery/question"),
         {{QStringLiteral("email"), email.trimmed()}}, false, std::move(done));
}

void Account::Recover(const QString& email, const QString& answer, const QString& code,
                      const QString& new_password, Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/recovery"),
         {{QStringLiteral("email"), email.trimmed()},
          {QStringLiteral("answer"), answer},
          {QStringLiteral("code"), code.trimmed()},
          {QStringLiteral("new_password"), new_password}},
         false, [this, done = std::move(done)](const Result& result) {
             TakeSession(result);
             done(result);
         });
}

void Account::Refresh(Callback done) {
    if (!session) {
        done({.ok = false, .error = QStringLiteral("no_session"), .data = {}});
        return;
    }
    Send(QStringLiteral("GET"), QStringLiteral("/v1/me"), {}, true,
         [this, done = std::move(done)](const Result& result) {
             if (result.ok && session) {
                 session->nickname = result.data.value(QStringLiteral("nickname")).toString();
                 session->email = result.data.value(QStringLiteral("email")).toString();
                 session->friend_code = result.data.value(QStringLiteral("friend_code")).toString();
                 SaveSession();
                 emit SessionChanged();
             } else if (result.error == QStringLiteral("no_session")) {
                 ClearSession();
             }
             done(result);
         });
}

void Account::Logout(Callback done) {
    if (!session) {
        done({.ok = true, .error = {}, .data = {}});
        return;
    }
    // Signed out on this PC whatever the server answers
    Send(QStringLiteral("POST"), QStringLiteral("/v1/logout"), {}, true,
         [this, done = std::move(done)](const Result& result) {
             ClearSession();
             done(result);
         });
}

void Account::Send(const QString& method, const QString& path, const QJsonObject& body,
                   bool authorized, Callback done) {
    if (pinned_sha256.isEmpty()) {
        LOG_ERROR(Frontend, "Online: no pinned certificate for {}",
                  server.toString().toStdString());
        done({.ok = false, .error = QStringLiteral("untrusted"), .data = {}});
        return;
    }
    QUrl url = server;
    url.setPath(path);
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(TimeoutMs);
    if (authorized && session) {
        request.setRawHeader("Authorization", "Bearer " + session->token.toUtf8());
    }

    QNetworkReply* reply =
        method == QStringLiteral("GET")
            ? network->get(request)
            : network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    auto pin_failed = std::make_shared<bool>(false);
    const auto matches_pin = [this, reply] {
        const QSslCertificate certificate = reply->sslConfiguration().peerCertificate();
        return !certificate.isNull() &&
               certificate.digest(QCryptographicHash::Sha256) == pinned_sha256;
    };
    // The certificate is self-signed for an address: accepted only when it is the pinned one
    connect(reply, &QNetworkReply::sslErrors, this,
            [reply, matches_pin, pin_failed](const QList<QSslError>& errors) {
                if (matches_pin()) {
                    reply->ignoreSslErrors(errors);
                } else {
                    *pin_failed = true;
                }
            });
    // Even a certificate with no error (one a public authority signed) must be the pinned one
    connect(reply, &QNetworkReply::encrypted, this, [reply, matches_pin, pin_failed] {
        if (!matches_pin()) {
            *pin_failed = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [reply, pin_failed, done = std::move(done)] {
        reply->deleteLater();
        Result result;
        if (*pin_failed) {
            LOG_ERROR(Frontend, "Online: the server's certificate is not the pinned one");
            result.error = QStringLiteral("untrusted");
            done(result);
            return;
        }
        const QByteArray payload = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject json = QJsonDocument::fromJson(payload).object();
        if (status == 0) {
            LOG_WARNING(Frontend, "Online: request failed: {}", reply->errorString().toStdString());
            result.error = QStringLiteral("network");
        } else if (status >= 200 && status < 300) {
            result.ok = true;
            result.data = json;
        } else {
            result.error = json.value(QStringLiteral("error")).toString();
            if (result.error.isEmpty()) {
                result.error = QStringLiteral("http_%1").arg(status);
            }
        }
        done(result);
    });
}

void Account::TakeSession(const Result& result) {
    const QString token = result.data.value(QStringLiteral("token")).toString();
    if (!result.ok || token.isEmpty()) {
        return;
    }
    session = Session{
        .token = token,
        .nickname = result.data.value(QStringLiteral("nickname")).toString(),
        .email = result.data.value(QStringLiteral("email")).toString(),
        .friend_code = result.data.value(QStringLiteral("friend_code")).toString(),
    };
    SaveSession();
    emit SessionChanged();
}

void Account::LoadSession() {
    QFile file{SessionPath()};
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    const QString token = json.value(QStringLiteral("token")).toString();
    if (token.isEmpty()) {
        return;
    }
    session = Session{
        .token = token,
        .nickname = json.value(QStringLiteral("nickname")).toString(),
        .email = json.value(QStringLiteral("email")).toString(),
        .friend_code = json.value(QStringLiteral("friend_code")).toString(),
    };
}

void Account::SaveSession() {
    if (!session) {
        return;
    }
    QSaveFile file{SessionPath()};
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(Frontend, "Online: the session could not be saved");
        return;
    }
    // Only this user may read the token
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QJsonObject json{{QStringLiteral("token"), session->token},
                           {QStringLiteral("nickname"), session->nickname},
                           {QStringLiteral("email"), session->email},
                           {QStringLiteral("friend_code"), session->friend_code}};
    file.write(QJsonDocument(json).toJson());
    file.commit();
}

void Account::ClearSession() {
    session.reset();
    QFile::remove(SessionPath());
    emit SessionChanged();
}

} // namespace Online
