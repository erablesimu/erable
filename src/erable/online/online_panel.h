// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include <QFrame>
#include <QString>
#include <QTimer>

#include "common/common_types.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QVBoxLayout;

namespace Online {

class Account;

/// The "Online" panel of the home screen, slid in on its right: going online, then the rooms of
/// the Erable server, the friends with their messages, and the alerts. The room chosen is kept
/// until Erable closes; the next start is offline again. A room holds one game at a time and has
/// no chat: players write to their friends, kept 15 days on the server.
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
    /// The friend requests and unread messages, for the status button's count
    int AlertCount() const;

    /// Whether a room is joined (its mods then checked before a game starts)
    bool InRoom() const {
        return JoinedPort() != 0;
    }

    /// Whether `title` may start: not in a room another game holds (the room list is offered),
    /// nor with mods or cheats in a room (`mods`, the game's enabled ones: they can be paused, or
    /// the room left). Called before the home launches a game.
    bool MayLaunch(const QString& title, u64 program_id, const std::vector<std::string>& mods);

    /// Follows the theme's palette
    void RefreshStyle();

signals:
    /// The status text, the online state or the alert count changed
    void StatusChanged();
    /// Mods were paused for the online play or turned back on: the configuration to save
    void ModsChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Tab { Rooms, Friends, Alerts };

    void BuildOfflinePage();
    void BuildOnlinePage();
    QWidget* BuildRoomsTab();
    QWidget* BuildFriendsTab();
    QWidget* BuildAlertsTab();
    void Place();
    void Update();
    void ShowTab(Tab tab);
    void FetchRooms();
    void RebuildRooms();
    void RebuildFriends();
    void RebuildAlerts();
    void GoOnline();
    /// Asks for a new nickname until one is taken or the player gives up; whether it was changed
    bool ChangeNickname();
    void Choose(unsigned port);
    void LeaveRoom();
    void GoOffline();
    void ToggleWiFi();
    /// Turns back on the mods paused for the online play, saying so
    void RestoreMods();
    void OpenConversation(const QString& friend_code, const QString& nickname);
    void CloseConversation();
    void FetchConversation();
    void SendToFriend();
    /// The room joined, 0 when none
    unsigned JoinedPort() const;
    bool SignedIn() const;
    QString RoomLabel(unsigned port) const;

    Account* account;
    QString display;
    QString body;
    bool online = false; ///< Gone online this session (the rooms, friends and alerts shown)
    Tab tab = Tab::Rooms;

    QLabel* title = nullptr;
    QPushButton* refresh_button = nullptr;
    QStackedWidget* pages = nullptr;

    QLabel* offline_account = nullptr;
    QLabel* mods_note = nullptr;
    QPushButton* wifi_button = nullptr;

    QWidget* room_card = nullptr;
    QLabel* room_name = nullptr;
    QLabel* room_game = nullptr;
    QWidget* nickname_notice = nullptr;
    QPushButton* tab_buttons[3]{};
    QStackedWidget* tabs = nullptr;

    QLabel* rooms_status = nullptr;
    QVBoxLayout* rooms_list = nullptr;

    QStackedWidget* friends_pages = nullptr;
    QVBoxLayout* friends_list = nullptr;
    QLineEdit* friend_code = nullptr;
    QLabel* friend_message = nullptr;

    // The conversation open with a friend
    QString talking_code;
    QLabel* talking_name = nullptr;
    QTextBrowser* conversation = nullptr;
    QLineEdit* message_edit = nullptr;
    QLabel* message_error = nullptr;
    qint64 last_message_id = 0;
    bool fetching_messages = false;

    QVBoxLayout* alerts_list = nullptr;

    QTimer state_timer;        ///< The room's state, every second
    QTimer rooms_timer;        ///< The rooms' players and games, every 30 s while online
    QTimer conversation_timer; ///< New messages, every 4 s while a conversation is open
    bool fetching_rooms = false;
};

} // namespace Online
