// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/online_account.h"

#include <algorithm>

#include <memory>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QTimer>

#include "common/fs/path_util.h"
#include "common/logging.h"

namespace Online {

namespace {

/// The Erable server, reached by its address (no domain name)
constexpr char DefaultServer[] = "https://91.134.241.94:24870";
/// SHA-256 of the server's self-signed certificate, in hex (EDEN_NX2_ONLINE_PIN overrides it)
constexpr char DefaultPin[] = "3dfc8baf6a32e2a2df8b7bf6ac08cf91ddd1b637c9c631e3fe8b4e4409d54dee";

constexpr int TimeoutMs = 15000;
constexpr int SyncIntervalMs = 30000;

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

Account::Account(QObject* parent)
    : QObject{parent}, network{new QNetworkAccessManager(this)}, session_path{SessionPath()},
      sync_timer{new QTimer(this)} {
    const QByteArray server_env = qgetenv("EDEN_NX2_ONLINE_SERVER");
    server = QUrl(server_env.isEmpty() ? QString::fromLatin1(DefaultServer)
                                       : QString::fromUtf8(server_env));
    const QByteArray pin_env = qgetenv("EDEN_NX2_ONLINE_PIN");
    pinned_sha256 = ParsePin(pin_env.isEmpty() ? QByteArray{DefaultPin} : pin_env);
    sync_timer->setInterval(SyncIntervalMs);
    connect(sync_timer, &QTimer::timeout, this, &Account::Sync);
    LoadSession();
    StartSync();
}

Account::~Account() = default;

bool Account::Enabled() {
    return qgetenv("EDEN_NX2_ONLINE") != "0";
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
    if (code == QStringLiteral("rooms_available")) {
        return tr("A room for this game still has room: join it.");
    }
    if (code == QStringLiteral("no_room_left")) {
        return tr("The server has no room left to open: try again in a few minutes.");
    }
    if (code == QStringLiteral("no_room_opening") || code == QStringLiteral("unknown_room_game")) {
        return tr("This server opens no more rooms.");
    }
    if (code == QStringLiteral("nickname_must_change")) {
        return tr("Your nickname holds an insult or a swear word: change it to play online.");
    }
    if (code == QStringLiteral("not_friends")) {
        return tr("You are no longer friends.");
    }
    if (code == QStringLiteral("too_many_messages")) {
        return tr("Too many messages at once: wait a minute.");
    }
    if (code == QStringLiteral("offensive_nickname")) {
        return tr("This nickname holds an insult or a swear word: choose another one.");
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
    if (code == QStringLiteral("no_access")) {
        return tr("Your Erable account doesn't have access to the online rooms.");
    }
    if (code == QStringLiteral("unknown_friend_code")) {
        return tr("No player has this friend code.");
    }
    if (code == QStringLiteral("own_friend_code")) {
        return tr("This is your own friend code.");
    }
    if (code == QStringLiteral("already_friends")) {
        return tr("You are already friends.");
    }
    if (code == QStringLiteral("already_requested")) {
        return tr("You already sent this player a request.");
    }
    if (code == QStringLiteral("too_many_requests")) {
        return tr("You sent too many friend requests today. Try again tomorrow.");
    }
    if (code == QStringLiteral("too_many_friends")) {
        return tr("You have reached the maximum number of friends.");
    }
    if (code == QStringLiteral("no_request")) {
        return tr("This request no longer exists.");
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
                 session->nickname_must_change =
                     result.data.value(QStringLiteral("nickname_must_change")).toBool();
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

void Account::DeleteAccount(const QString& password, Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/account/delete"),
         {{QStringLiteral("password"), password}}, true,
         [this, done = std::move(done)](const Result& result) {
             if (result.ok) {
                 ClearSession();
             }
             done(result);
         });
}

void Account::FetchRooms(Callback done) {
    if (!session) {
        rooms.clear();
        done({.ok = false, .error = QStringLiteral("no_session"), .data = {}});
        return;
    }
    Send(QStringLiteral("GET"), QStringLiteral("/v1/rooms"), {}, true,
         [this, done = std::move(done)](const Result& result) {
             if (result.ok) {
                 rooms.clear();
                 for (const auto& value : result.data.value(QStringLiteral("rooms")).toArray()) {
                     const QJsonObject room = value.toObject();
                     rooms.append(
                         {.name = room.value(QStringLiteral("name")).toString(),
                          .port = static_cast<unsigned>(room.value(QStringLiteral("port")).toInt()),
                          .password = room.value(QStringLiteral("password")).toString(),
                          .players = room.value(QStringLiteral("players")).toInt(),
                          .game = room.value(QStringLiteral("game")).toString(),
                          .reserved = room.value(QStringLiteral("reserved")).toString(),
                          .title_ids = room.value(QStringLiteral("title_ids"))
                                           .toVariant()
                                           .toStringList(),
                          .keywords = room.value(QStringLiteral("keywords"))
                                          .toVariant()
                                          .toStringList(),
                          .max_players = room.value(QStringLiteral("max_players")).toInt(8),
                          .opened = room.value(QStringLiteral("dynamic")).toBool(),
                          .open_anytime =
                              room.value(QStringLiteral("open_anytime")).toBool()});
                 }
             } else if (result.error == QStringLiteral("no_session")) {
                 ClearSession();
             } else if (result.error == QStringLiteral("nickname_must_change") && session) {
                 session->nickname_must_change = true;
                 emit SessionChanged();
             }
             done(result);
         });
}

bool Room::Takes(u64 program_id, const QString& title) const {
    if (reserved.isEmpty() || (title_ids.isEmpty() && keywords.isEmpty())) {
        return true;
    }
    const QString id = QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
    if (title_ids.contains(id, Qt::CaseInsensitive)) {
        return true;
    }
    return std::any_of(keywords.begin(), keywords.end(), [&](const QString& word) {
        return title.contains(word, Qt::CaseInsensitive);
    });
}

void Account::OpenRoom(const QString& reserved, Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/rooms/open"),
         {{QStringLiteral("reserved"), reserved}}, true,
         [this, done = std::move(done)](const Result& result) {
             if (!result.ok) {
                 done(result);
                 return;
             }
             FetchRooms([done, result](const Result&) { done(result); });
         });
}

QString Account::RoomPassword(unsigned port) const {
    for (const Room& room : rooms) {
        if (room.port == port) {
            return room.password;
        }
    }
    return {};
}

QString Account::RoomName(unsigned port) const {
    for (const Room& room : rooms) {
        if (room.port == port) {
            return room.name;
        }
    }
    return {};
}

void Account::StartSync() {
    if (!session) {
        return;
    }
    sync_timer->start();
    Sync();
}

void Account::SetPresence(const QString& game, unsigned room_port) {
    if (game == presence_game && room_port == presence_room) {
        return;
    }
    presence_game = game;
    presence_room = room_port;
    Sync();
}

void Account::Sync() {
    if (!session) {
        return;
    }
    if (syncing) {
        sync_again = true; // A change made meanwhile is sent once this sync ends
        return;
    }
    syncing = true;
    Send(QStringLiteral("POST"), QStringLiteral("/v1/sync"),
         {{QStringLiteral("game"), presence_game},
          {QStringLiteral("room_port"), static_cast<int>(presence_room)}},
         true, [this](const Result& result) {
             syncing = false;
             if (sync_again) {
                 sync_again = false;
                 QTimer::singleShot(0, this, &Account::Sync);
             }
             if (!result.ok) {
                 if (result.error == QStringLiteral("no_session")) {
                     ClearSession();
                 }
                 return;
             }
             const auto requests = [](const QJsonArray& array) {
                 QList<FriendRequest> list;
                 for (const auto& value : array) {
                     const QJsonObject item = value.toObject();
                     list.append(
                         {.nickname = item.value(QStringLiteral("nickname")).toString(),
                          .friend_code = item.value(QStringLiteral("friend_code")).toString()});
                 }
                 return list;
             };
             friends.clear();
             for (const auto& value : result.data.value(QStringLiteral("friends")).toArray()) {
                 const QJsonObject item = value.toObject();
                 friends.append({
                     .nickname = item.value(QStringLiteral("nickname")).toString(),
                     .friend_code = item.value(QStringLiteral("friend_code")).toString(),
                     .online = item.value(QStringLiteral("online")).toBool(),
                     .game = item.value(QStringLiteral("game")).toString(),
                     .room_port =
                         static_cast<unsigned>(item.value(QStringLiteral("room_port")).toInt()),
                     .unread = item.value(QStringLiteral("unread")).toInt(),
                 });
             }
             incoming = requests(result.data.value(QStringLiteral("incoming")).toArray());
             outgoing = requests(result.data.value(QStringLiteral("outgoing")).toArray());
             emit FriendsChanged();
             for (const FriendRequest& request : incoming) {
                 if (!notified_requests.contains(request.friend_code)) {
                     notified_requests.append(request.friend_code);
                     emit FriendRequestReceived(request.nickname, request.friend_code);
                 }
             }
         });
}

void Account::ChangeNickname(const QString& nickname, Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/account/nickname"),
         {{QStringLiteral("nickname"), nickname.trimmed()}}, true,
         [this, done = std::move(done)](const Result& result) {
             if (result.ok && session) {
                 session->nickname = result.data.value(QStringLiteral("nickname")).toString();
                 session->nickname_must_change =
                     result.data.value(QStringLiteral("nickname_must_change")).toBool();
                 SaveSession();
                 emit SessionChanged();
                 Sync();
             }
             done(result);
         });
}

void Account::SendMessage(const QString& friend_code, const QString& text, Callback done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/messages/send"),
         {{QStringLiteral("friend_code"), friend_code}, {QStringLiteral("text"), text}}, true,
         std::move(done));
}

void Account::FetchMessages(const QString& friend_code, qint64 after_id,
                            std::function<void(const Result&, const QList<Message>&)> done) {
    Send(QStringLiteral("POST"), QStringLiteral("/v1/messages"),
         {{QStringLiteral("friend_code"), friend_code},
          {QStringLiteral("after"), static_cast<qint64>(after_id)}},
         true, [this, friend_code, done = std::move(done)](const Result& result) {
             QList<Message> messages;
             for (const auto& value : result.data.value(QStringLiteral("messages")).toArray()) {
                 const QJsonObject item = value.toObject();
                 messages.append({
                     .id = item.value(QStringLiteral("id")).toInteger(),
                     .from_me = item.value(QStringLiteral("from_me")).toBool(),
                     .sent_at = item.value(QStringLiteral("sent_at")).toInteger(),
                     .text = item.value(QStringLiteral("text")).toString(),
                 });
             }
             // Read now: the friend's count goes, without waiting for the next sync
             if (result.ok) {
                 for (Friend& player : friends) {
                     if (player.friend_code == friend_code && player.unread != 0) {
                         player.unread = 0;
                         emit FriendsChanged();
                     }
                 }
             }
             done(result, messages);
         });
}

void Account::FriendAction(const QString& path, const QJsonObject& body, Callback done) {
    Send(QStringLiteral("POST"), path, body, true,
         [this, done = std::move(done)](const Result& result) {
             done(result);
             Sync();
         });
}

void Account::SendFriendRequest(const QString& friend_code, Callback done) {
    FriendAction(QStringLiteral("/v1/friends/request"),
                 {{QStringLiteral("friend_code"), friend_code.trimmed().toUpper()}},
                 std::move(done));
}

void Account::RespondFriendRequest(const QString& friend_code, bool accept, Callback done) {
    FriendAction(QStringLiteral("/v1/friends/respond"),
                 {{QStringLiteral("friend_code"), friend_code}, {QStringLiteral("accept"), accept}},
                 std::move(done));
}

void Account::RemoveFriend(const QString& friend_code, Callback done) {
    FriendAction(QStringLiteral("/v1/friends/remove"),
                 {{QStringLiteral("friend_code"), friend_code}}, std::move(done));
}

void Account::BlockPlayer(const QString& friend_code, Callback done) {
    FriendAction(QStringLiteral("/v1/friends/block"),
                 {{QStringLiteral("friend_code"), friend_code}}, std::move(done));
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
        .nickname_must_change =
            result.data.value(QStringLiteral("nickname_must_change")).toBool(),
    };
    SaveSession();
    emit SessionChanged();
    StartSync();
}

void Account::LoadSession() {
    QFile file{session_path};
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
    QSaveFile file{session_path};
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
    rooms.clear();
    friends.clear();
    incoming.clear();
    outgoing.clear();
    notified_requests.clear();
    sync_timer->stop();
    QFile::remove(session_path);
    emit SessionChanged();
    emit FriendsChanged();
}

} // namespace Online
