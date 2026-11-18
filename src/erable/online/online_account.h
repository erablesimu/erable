// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <optional>

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace Online {

/// The signed-in Erable account, as kept on this PC
struct Session {
    QString token;
    QString nickname;
    QString email;
    QString friend_code;
};

/// Erable accounts on the Erable server: sign-up, sign-in, recovery with the security answer and
/// code, sign-out. The server is reached by its address over HTTPS; its certificate is
/// self-signed and pinned by its SHA-256, so no domain name is needed.
class Account : public QObject {
    Q_OBJECT

public:
    /// What a request gives: the server's JSON, or the error code it sent ("wrong_credentials",
    /// "email_taken"...), "network" when the server could not be reached and "untrusted" when
    /// its certificate is not the pinned one
    struct Result {
        bool ok{};
        QString error;
        QJsonObject data;
    };
    using Callback = std::function<void(const Result&)>;

    explicit Account(QObject* parent = nullptr);
    ~Account() override;

    /// Whether the online features are enabled for this run (EDEN_NX2_ONLINE=1)
    static bool Enabled();

    /// The security questions the server accepts, by key
    static QStringList QuestionKeys();
    /// A security question's text, translated
    static QString QuestionText(const QString& key);
    /// An error code from the server, as a sentence for the player, translated
    static QString ErrorText(const QString& code);

    /// The session saved on this PC, if any
    const std::optional<Session>& CurrentSession() const {
        return session;
    }

    void Register(const QString& nickname, const QString& email, const QString& password,
                  const QString& question, const QString& answer, const QString& code,
                  Callback done);
    void Login(const QString& email, const QString& password, Callback done);
    /// The security question of the account of `email` (in `data["question"]`)
    void RecoveryQuestion(const QString& email, Callback done);
    /// A new password, given the security answer and code; signs every other device out
    void Recover(const QString& email, const QString& answer, const QString& code,
                 const QString& new_password, Callback done);
    /// Checks the saved session with the server; drops it when the server no longer knows it
    void Refresh(Callback done);
    /// Ends the session on the server and on this PC
    void Logout(Callback done);

signals:
    /// The session was opened, refreshed or closed
    void SessionChanged();

private:
    void Send(const QString& method, const QString& path, const QJsonObject& body, bool authorized,
              Callback done);
    /// Keeps the session a sign-in, sign-up or recovery returned
    void TakeSession(const Result& result);
    void LoadSession();
    void SaveSession();
    void ClearSession();

    QNetworkAccessManager* network;
    QUrl server;
    QByteArray pinned_sha256;
    std::optional<Session> session;
};

} // namespace Online
