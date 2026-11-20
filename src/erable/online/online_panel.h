// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFrame>
#include <QString>
#include <QTimer>

class ChatRoom;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace Online {

class Account;

/// The "Online" panel of the home screen, slid in on its right: going online, choosing a room of
/// the Erable server, then the friends, the room's chat and the alerts. The room chosen is kept
/// until Erable closes; the next start is offline again. A room holds one game at a time.
class OnlinePanel : public QFrame {
    Q_OBJECT

public:
    OnlinePanel(const QString& display_font, const QString& body_font, QWidget* parent);
    ~OnlinePanel() override;

    /// Slides the panel in or out
    void Toggle();
    void Open();

    /// What the home's status button says ("Offline", "Online · Salon 2 · 2 players")
    QString StatusText() const;
    /// Whether the status button shows the online colour
    bool IsOnline() const;
    /// The friend requests waiting, for the status button's count
    int AlertCount() const;

    /// Whether `title` may start: in a room another game holds, it may not, and the player is told
    /// so and offered the room list. Called before the home launches a game.
    bool MayLaunch(const QString& title);

    /// Follows the theme's palette
    void RefreshStyle();

signals:
    /// The status text, the online state or the alert count changed
    void StatusChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Page { Offline, Rooms, Room };
    enum class Tab { Friends, Chat, Alerts };

    void BuildOfflinePage();
    void BuildRoomsPage();
    void BuildRoomPage();
    void Place();
    void Update();
    void ShowTab(Tab tab);
    void FetchRooms();
    void RebuildRooms();
    void RebuildFriends();
    void RebuildAlerts();
    void GoOnline();
    void Choose(unsigned port);
    void LeaveRoom();
    void GoOffline();
    void ToggleWiFi();
    /// The room joined, 0 when none
    unsigned JoinedPort() const;
    QString RoomLabel(unsigned port) const;

    Account* account;
    QString display;
    QString body;
    bool browsing = false; ///< The room list is wanted (going online, or changing rooms)
    Page page = Page::Offline;
    Tab tab = Tab::Friends;
    QString member_signature; ///< The room's members last given to the chat

    QLabel* title = nullptr;
    QPushButton* refresh_button = nullptr;
    QStackedWidget* pages = nullptr;

    QLabel* offline_account = nullptr;
    QPushButton* wifi_button = nullptr;

    QLabel* rooms_status = nullptr;
    QVBoxLayout* rooms_list = nullptr;

    QLabel* room_name = nullptr;
    QLabel* room_game = nullptr;
    QPushButton* tab_buttons[3]{};
    QStackedWidget* tabs = nullptr;
    QVBoxLayout* friends_list = nullptr;
    QLineEdit* friend_code = nullptr;
    QLabel* friend_message = nullptr;
    QVBoxLayout* alerts_list = nullptr;
    ChatRoom* chat = nullptr;

    QTimer state_timer; ///< The room's state, every second
    QTimer rooms_timer; ///< The rooms' players and games, every 30 s while online
    bool fetching_rooms = false;
};

} // namespace Online
