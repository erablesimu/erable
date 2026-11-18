// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class QFrame;
class QLabel;
class QSystemTrayIcon;
class QWidget;

namespace Online {

class Account;

/// Tells the player a friend request arrived: a banner at the bottom of the window with Accept
/// and Refuse, and, while Erable is in the background, the taskbar flashes and a system
/// notification shows. During a game nothing shows: the requests wait for the game to end.
class FriendNotifier : public QObject {
    Q_OBJECT

public:
    FriendNotifier(Account& account, QWidget* window);
    ~FriendNotifier() override;

    /// Holds the banners back while a game runs; shows the waiting ones when it ends
    void SetInGame(bool in_game);

signals:
    /// The player clicked the notification of the system or "Friends" on the banner
    void OpenFriendsRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void Received(const QString& nickname, const QString& friend_code);
    void ShowNext();
    void Place();
    void Answer(bool accept);

    Account& account;
    QWidget* window;
    QFrame* banner;
    QLabel* banner_text;
    QSystemTrayIcon* tray = nullptr;
    QList<QPair<QString, QString>> waiting; ///< Nickname and friend code, oldest first
    QString shown_code;
    bool in_game = false;
};

} // namespace Online
