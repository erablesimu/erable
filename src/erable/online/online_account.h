// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <optional>

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QTimer;
class QNetworkReply;

namespace Online {

/// The signed-in Erable account, as kept on this PC
struct Session {
    QString token;
    QString nickname;
    QString email;
    QString friend_code;
};

/// A room of the Erable server that signed-in players may join
struct Room {
    QString name;
    unsigned port{};
    QString password;
    int players{}; ///< The other players online in it (0 when the server is older)
    QString game;  ///< The game its players run: a room holds one game at a time
};

/// A friend, as the last sync saw them
struct Friend {
    QString nickname;
    QString friend_code;
    bool online{};
    QString game;         ///< The game they play, empty when none or offline
    unsigned room_port{}; ///< The Erable room they are in, 0 when none
};

/// A friend request, sent to this account or by it
struct FriendRequest {
    QString nickname;
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

    /// Whether the online features are enabled (EDEN_NX2_ONLINE=0 turns them off)
    static bool Enabled();
    /// The account shared by the windows, made on first use; null when not Enabled()
    static Account* Shared();
    /// The Erable server's address, without scheme or port (for its rooms)
    QString ServerHost() const {
        return server.host();
    }

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
    /// Deletes the account and everything the server keeps about it, given its password
    void DeleteAccount(const QString& password, Callback done);
    /// Fetches the rooms signed-in players may join, with their passwords
    void FetchRooms(Callback done);
    /// The rooms last fetched (empty until then, or when signed out)
    const QList<Room>& Rooms() const {
        return rooms;
    }
    /// The password of the room on `port`, if it was fetched
    QString RoomPassword(unsigned port) const;
    /// The name of the room on `port` ("Salon 3"), else empty
    QString RoomName(unsigned port) const;

    // Friends. While signed in, Erable syncs with the server every 30 s: it says this player is
    // online (with the game and room set by SetPresence) and gets the friends and the requests.
    void SendFriendRequest(const QString& friend_code, Callback done);
    void RespondFriendRequest(const QString& friend_code, bool accept, Callback done);
    void RemoveFriend(const QString& friend_code, Callback done);
    /// Removes them from the friends, drops requests either way, and ignores theirs from now on
    void BlockPlayer(const QString& friend_code, Callback done);
    /// Syncs now rather than at the next 30 s tick
    void Sync();
    /// What friends see this player doing: the game (empty for none) and the room (0 for none)
    void SetPresence(const QString& game, unsigned room_port);
    void SetPresenceGame(const QString& game) {
        SetPresence(game, presence_room);
    }
    void SetPresenceRoom(unsigned room_port) {
        SetPresence(presence_game, room_port);
    }
    const QList<Friend>& Friends() const {
        return friends;
    }
    const QList<FriendRequest>& IncomingRequests() const {
        return incoming;
    }
    const QList<FriendRequest>& OutgoingRequests() const {
        return outgoing;
    }

signals:
    /// The session was opened, refreshed or closed
    void SessionChanged();
    /// A sync brought new friends, requests or presence
    void FriendsChanged();
    /// A friend request arrived that this PC had not shown yet
    void FriendRequestReceived(const QString& nickname, const QString& friend_code);

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
    /// Calls a friends route, then syncs
    void FriendAction(const QString& path, const QJsonObject& body, Callback done);
    void StartSync();

    std::optional<Session> session;
    QString session_path;
    QList<Room> rooms;
    QList<Friend> friends;
    QList<FriendRequest> incoming;
    QList<FriendRequest> outgoing;
    QStringList notified_requests; ///< Friend codes whose request this PC has shown
    QTimer* sync_timer;
    bool syncing{};
    bool sync_again{};
    QString presence_game;
    unsigned presence_room{};
};

} // namespace Online
