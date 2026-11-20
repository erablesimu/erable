// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/online_panel.h"

#include <algorithm>

#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "common/profanity_filter.h"
#include "common/settings.h"
#include "erable/online/account_dialog.h"
#include "erable/online/friends_dialog.h"
#include "erable/online/online_account.h"
#include "erable/online/online_mode.h"
#include "erable/online/online_mods.h"
#include "network/network.h"
#include "network/room_member.h"

namespace Online {

namespace {

constexpr int PanelWidth = 400;
constexpr int MaxMessageLength = 500;

QLabel* MakeLabel(const QString& text, const char* name, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(name));
    label->setWordWrap(true);
    return label;
}

QPushButton* MakeButton(const QString& text, const char* name, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

/// Empties a list built of rows
void ClearLayout(QVBoxLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

/// A scrolling column the rows of a list go in, with the stretch under them
QScrollArea* MakeList(QVBoxLayout*& rows, QWidget* parent) {
    auto* area = new QScrollArea(parent);
    area->setObjectName(QStringLiteral("list"));
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inner = new QWidget(area);
    inner->setObjectName(QStringLiteral("listInner"));
    auto* column = new QVBoxLayout(inner);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(6);
    rows = new QVBoxLayout;
    rows->setSpacing(6);
    column->addLayout(rows);
    column->addStretch();
    area->setWidget(inner);
    return area;
}

/// A text as shown: insults and swear words as stars
QString Shown(const QString& text) {
    return QString::fromStdString(Common::Profanity::Mask(text.toStdString()));
}

} // Anonymous namespace

OnlinePanel::OnlinePanel(const QString& display_font, const QString& body_font, QWidget* parent)
    : QFrame{parent}, account{Account::Shared()}, display{display_font}, body{body_font} {
    setObjectName(QStringLiteral("onlinePanel"));
    setFixedWidth(PanelWidth);
    hide();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    auto* head = new QHBoxLayout;
    title = MakeLabel(tr("Online"), "panelTitle", this);
    title->setWordWrap(false);
    refresh_button = MakeButton(tr("Refresh"), "small", this);
    refresh_button->setToolTip(tr("Fetch the rooms' players again"));
    connect(refresh_button, &QPushButton::clicked, this, &OnlinePanel::FetchRooms);
    auto* minimize = MakeButton(QStringLiteral("–"), "minimize", this);
    minimize->setToolTip(tr("Minimize: the button at the top keeps the state and opens it again"));
    minimize->setAccessibleName(tr("Minimize the panel"));
    connect(minimize, &QPushButton::clicked, this, &QWidget::hide);
    head->addWidget(title, 1);
    head->addWidget(refresh_button);
    head->addWidget(minimize);
    layout->addLayout(head);

    pages = new QStackedWidget(this);
    layout->addWidget(pages, 1);
    BuildOfflinePage();
    BuildOnlinePage();

    if (parent) {
        parent->installEventFilter(this);
    }
    if (account) {
        connect(account, &Account::SessionChanged, this, &OnlinePanel::Update);
        connect(account, &Account::FriendsChanged, this, [this] {
            RebuildFriends();
            RebuildAlerts();
            Update();
        });
    }
    state_timer.setInterval(1000);
    connect(&state_timer, &QTimer::timeout, this, &OnlinePanel::Update);
    state_timer.start();
    rooms_timer.setInterval(30000);
    connect(&rooms_timer, &QTimer::timeout, this, &OnlinePanel::FetchRooms);
    rooms_timer.start();
    conversation_timer.setInterval(4000);
    connect(&conversation_timer, &QTimer::timeout, this, &OnlinePanel::FetchConversation);

    RefreshStyle();
    RebuildFriends();
    RebuildAlerts();
    Update();
    // Erable starts offline: mods a former session paused come back
    QTimer::singleShot(0, this, &OnlinePanel::RestoreMods);
}

OnlinePanel::~OnlinePanel() = default;

void OnlinePanel::BuildOfflinePage() {
    auto* page_widget = new QWidget(pages);
    auto* column = new QVBoxLayout(page_widget);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(18);

    auto* card = new QFrame(page_widget);
    card->setObjectName(QStringLiteral("card"));
    auto* card_column = new QVBoxLayout(card);
    card_column->setContentsMargins(22, 22, 22, 22);
    card_column->setSpacing(12);
    card_column->addWidget(MakeLabel(tr("Play with your friends"), "cardTitle", card));
    card_column->addWidget(MakeLabel(tr("Go online on the Erable server to see its rooms, find "
                                        "your friends and get their requests."),
                                     "muted", card));
    auto* connect_button = MakeButton(tr("Go online"), "primary", card);
    connect(connect_button, &QPushButton::clicked, this, &OnlinePanel::GoOnline);
    card_column->addWidget(connect_button);
    offline_account = MakeLabel({}, "hint", card);
    card_column->addWidget(offline_account);
    column->addWidget(card);

    column->addWidget(MakeLabel(tr("On the same network?"), "muted", page_widget));
    wifi_button = MakeButton({}, "row", page_widget);
    connect(wifi_button, &QPushButton::clicked, this, &OnlinePanel::ToggleWiFi);
    column->addWidget(wifi_button);
    mods_note = MakeLabel({}, "hint", page_widget);
    mods_note->hide();
    column->addWidget(mods_note);
    column->addStretch();
    column->addWidget(MakeLabel(tr("Friends and alerts show once you are online."), "hint",
                                page_widget));
    pages->addWidget(page_widget);
}

void OnlinePanel::BuildOnlinePage() {
    auto* page_widget = new QWidget(pages);
    auto* column = new QVBoxLayout(page_widget);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);

    // The nickname to change before anything online
    auto* notice = new QFrame(page_widget);
    notice->setObjectName(QStringLiteral("card"));
    auto* notice_column = new QVBoxLayout(notice);
    notice_column->setContentsMargins(16, 14, 16, 14);
    notice_column->addWidget(MakeLabel(tr("Your nickname holds an insult or a swear word. Choose "
                                          "another one to play online and write to your friends."),
                                       "", notice));
    auto* rename = MakeButton(tr("Change my nickname"), "accept", notice);
    connect(rename, &QPushButton::clicked, this, [this] { ChangeNickname(); });
    notice_column->addWidget(rename, 0, Qt::AlignLeft);
    nickname_notice = notice;
    column->addWidget(notice);

    // The room joined
    auto* card = new QFrame(page_widget);
    card->setObjectName(QStringLiteral("card"));
    auto* card_column = new QVBoxLayout(card);
    card_column->setContentsMargins(16, 14, 16, 14);
    card_column->setSpacing(8);
    auto* name_row = new QHBoxLayout;
    auto* names = new QVBoxLayout;
    names->setSpacing(2);
    room_name = MakeLabel({}, "roomName", card);
    room_game = MakeLabel({}, "muted", card);
    names->addWidget(room_name);
    names->addWidget(room_game);
    name_row->addLayout(names, 1);
    auto* leave = MakeButton(tr("Leave"), "small", card);
    leave->setToolTip(tr("Leave the room and see the others"));
    connect(leave, &QPushButton::clicked, this, &OnlinePanel::LeaveRoom);
    name_row->addWidget(leave, 0, Qt::AlignTop);
    card_column->addLayout(name_row);
    card_column->addWidget(MakeLabel(tr("Kept until Erable closes"), "hint", card));
    room_card = card;
    column->addWidget(card);

    auto* tab_row = new QHBoxLayout;
    tab_row->setSpacing(4);
    const QString tab_names[] = {tr("Rooms"), tr("Friends"), tr("Alerts")};
    for (int i = 0; i < 3; ++i) {
        tab_buttons[i] = MakeButton(tab_names[i], "tab", page_widget);
        tab_buttons[i]->setCheckable(true);
        connect(tab_buttons[i], &QPushButton::clicked, this,
                [this, i] { ShowTab(static_cast<Tab>(i)); });
        tab_row->addWidget(tab_buttons[i]);
    }
    tab_row->addStretch();
    column->addLayout(tab_row);

    tabs = new QStackedWidget(page_widget);
    tabs->addWidget(BuildRoomsTab());
    tabs->addWidget(BuildFriendsTab());
    tabs->addWidget(BuildAlertsTab());
    column->addWidget(tabs, 1);

    auto* offline = MakeButton(tr("Go offline"), "row", page_widget);
    connect(offline, &QPushButton::clicked, this, &OnlinePanel::GoOffline);
    column->addWidget(offline);
    pages->addWidget(page_widget);
    ShowTab(Tab::Rooms);
}

QWidget* OnlinePanel::BuildRoomsTab() {
    auto* page_widget = new QWidget(tabs);
    auto* column = new QVBoxLayout(page_widget);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);
    column->addWidget(MakeLabel(tr("A room holds one game at a time: the game its players run. "
                                   "It is free again once empty."),
                                "muted", page_widget));
    rooms_status = MakeLabel({}, "hint", page_widget);
    column->addWidget(rooms_status);
    column->addWidget(MakeList(rooms_list, page_widget), 1);
    column->addWidget(MakeLabel(tr("The room you choose stays yours until Erable closes. Erable "
                                   "starts offline the next time."),
                                "note", page_widget));
    return page_widget;
}

QWidget* OnlinePanel::BuildFriendsTab() {
    friends_pages = new QStackedWidget(tabs);

    // The friends: who is online and where, joining their room, writing to them
    auto* list_page = new QWidget(friends_pages);
    auto* list_column = new QVBoxLayout(list_page);
    list_column->setContentsMargins(0, 0, 0, 0);
    list_column->setSpacing(8);
    list_column->addWidget(MakeList(friends_list, list_page), 1);
    auto* add_row = new QHBoxLayout;
    friend_code = new QLineEdit(list_page);
    friend_code->setPlaceholderText(tr("Friend code: ER-0000-0000-0000"));
    friend_code->setAccessibleName(tr("Friend code"));
    auto* add = MakeButton(tr("Add"), "small", list_page);
    const auto send_request = [this] {
        const QString code = friend_code->text().trimmed().toUpper();
        if (!account || code.isEmpty()) {
            return;
        }
        QPointer<OnlinePanel> self{this};
        account->SendFriendRequest(code, [self](const Account::Result& result) {
            if (!self) {
                return;
            }
            if (result.ok) {
                self->friend_code->clear();
            }
            self->friend_message->setText(result.ok ? tr("Request sent.")
                                                    : Account::ErrorText(result.error));
        });
    };
    connect(add, &QPushButton::clicked, this, send_request);
    connect(friend_code, &QLineEdit::returnPressed, this, send_request);
    add_row->addWidget(friend_code, 1);
    add_row->addWidget(add);
    list_column->addLayout(add_row);
    friend_message = MakeLabel({}, "hint", list_page);
    list_column->addWidget(friend_message);
    auto* manage = MakeButton(tr("Manage friends..."), "link", list_page);
    connect(manage, &QPushButton::clicked, this, [this] {
        if (!account) {
            return;
        }
        FriendsDialog dialog(*account, window());
        connect(&dialog, &FriendsDialog::JoinRoomRequested, this,
                [this](unsigned port) { Choose(port); });
        dialog.exec();
    });
    list_column->addWidget(manage, 0, Qt::AlignLeft);
    friends_pages->addWidget(list_page);

    // A conversation with one of them
    auto* talk_page = new QWidget(friends_pages);
    auto* talk_column = new QVBoxLayout(talk_page);
    talk_column->setContentsMargins(0, 0, 0, 0);
    talk_column->setSpacing(8);
    auto* talk_head = new QHBoxLayout;
    auto* back = MakeButton(tr("Back"), "small", talk_page);
    connect(back, &QPushButton::clicked, this, &OnlinePanel::CloseConversation);
    talking_name = MakeLabel({}, "roomName", talk_page);
    talk_head->addWidget(back);
    talk_head->addWidget(talking_name, 1);
    talk_column->addLayout(talk_head);
    conversation = new QTextBrowser(talk_page);
    conversation->setObjectName(QStringLiteral("conversation"));
    conversation->setOpenLinks(false);
    talk_column->addWidget(conversation, 1);
    auto* write_row = new QHBoxLayout;
    message_edit = new QLineEdit(talk_page);
    message_edit->setMaxLength(MaxMessageLength);
    message_edit->setPlaceholderText(tr("Write a message"));
    message_edit->setAccessibleName(tr("Message"));
    auto* send = MakeButton(tr("Send"), "accept", talk_page);
    connect(send, &QPushButton::clicked, this, &OnlinePanel::SendToFriend);
    connect(message_edit, &QLineEdit::returnPressed, this, &OnlinePanel::SendToFriend);
    write_row->addWidget(message_edit, 1);
    write_row->addWidget(send);
    talk_column->addLayout(write_row);
    message_error = MakeLabel({}, "hint", talk_page);
    talk_column->addWidget(message_error);
    talk_column->addWidget(MakeLabel(tr("Messages are kept 15 days."), "hint", talk_page));
    friends_pages->addWidget(talk_page);
    return friends_pages;
}

QWidget* OnlinePanel::BuildAlertsTab() {
    auto* page_widget = new QWidget(tabs);
    auto* column = new QVBoxLayout(page_widget);
    column->setContentsMargins(0, 0, 0, 0);
    column->addWidget(MakeList(alerts_list, page_widget), 1);
    return page_widget;
}

void OnlinePanel::RefreshStyle() {
    const QPalette palette = qApp->palette();
    const QString surface = palette.color(QPalette::Button).name();
    const QString text = palette.color(QPalette::WindowText).name();
    const QString line = palette.color(QPalette::Light).name();
    const QString accent = palette.color(QPalette::Highlight).name();
    const QString ink = palette.color(QPalette::HighlightedText).name();
    const QString ground = palette.color(QPalette::Window).darker(125).name();
    QColor muted_colour = palette.color(QPalette::WindowText);
    muted_colour.setAlpha(175);
    const QString muted = muted_colour.name(QColor::HexArgb);
    setStyleSheet(
        QStringLiteral(
            "#onlinePanel { background: %1; border-left: 1px solid %4; }"
            "#onlinePanel QWidget { color: %3; font-family: \"%8\"; font-size: 14px; }"
            "#onlinePanel QScrollArea, #onlinePanel #listInner { background: transparent; }"
            "#onlinePanel #panelTitle { font-family: \"%9\"; font-size: 26px; }"
            "#onlinePanel #cardTitle { font-family: \"%9\"; font-size: 21px; }"
            "#onlinePanel #roomName { font-size: 16px; }"
            "#onlinePanel #muted { color: %7; }"
            "#onlinePanel #hint, #onlinePanel #note { color: %7; font-size: 12px; }"
            "#onlinePanel #note { background: %2; border-radius: 12px; padding: 10px; }"
            "#onlinePanel #card { background: %2; border-radius: 16px; }"
            "#onlinePanel QPushButton { background: %2; border: 1px solid %4; border-radius: 12px;"
            " padding: 9px 14px; }"
            "#onlinePanel QPushButton:hover { border-color: %5; }"
            "#onlinePanel QPushButton#primary { background: %5; color: %6; border: none;"
            " border-radius: 22px; padding: 13px; font-size: 16px; font-weight: 800; }"
            "#onlinePanel QPushButton#small { padding: 6px 12px; font-size: 13px; }"
            "#onlinePanel QPushButton#minimize { min-width: 38px; max-width: 38px;"
            " padding: 6px 0; font-size: 18px; }"
            "#onlinePanel QPushButton#row { text-align: left; }"
            "#onlinePanel QPushButton#room { text-align: left; padding: 12px 14px;"
            " border-radius: 14px; }"
            "#onlinePanel QPushButton#roomJoined { text-align: left; padding: 12px 14px;"
            " border-radius: 14px; border: 2px solid %5; }"
            "#onlinePanel QPushButton#tab { background: transparent; border: none;"
            " border-radius: 16px; padding: 7px 14px; color: %7; }"
            "#onlinePanel QPushButton#tab:checked { background: %2; color: %3; }"
            "#onlinePanel QPushButton#link { background: transparent; border: none; color: %5;"
            " padding: 4px 0; }"
            "#onlinePanel QPushButton#accept { background: %5; color: %6; border: none;"
            " padding: 6px 12px; font-size: 13px; }"
            "#onlinePanel QLineEdit, #onlinePanel QTextBrowser { background: %10;"
            " border: 1px solid %4; border-radius: 12px; padding: 9px 11px; }"
            "#onlinePanel #friendRow { background: transparent; border-radius: 12px; }")
            .arg(palette.color(QPalette::Window).name(), surface, text, line, accent, ink, muted,
                 body, display, ground));
}

bool OnlinePanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parent() && event->type() == QEvent::Resize) {
        Place();
    }
    return QFrame::eventFilter(watched, event);
}

void OnlinePanel::Place() {
    if (QWidget* const home = parentWidget()) {
        setGeometry(home->width() - PanelWidth, 0, PanelWidth, home->height());
    }
}

void OnlinePanel::Toggle() {
    if (isVisible()) {
        hide();
    } else {
        Open();
    }
}

void OnlinePanel::Open() {
    Place();
    show();
    raise();
    if (online) {
        FetchRooms();
        FetchConversation();
    }
    Update();
}

bool OnlinePanel::SignedIn() const {
    return account && account->CurrentSession().has_value();
}

unsigned OnlinePanel::JoinedPort() const {
    if (CurrentMode() != Mode::Server) {
        return 0;
    }
    return Settings::values.online_room_port.GetValue();
}

QString OnlinePanel::RoomLabel(unsigned port) const {
    const QString name = account ? account->RoomName(port) : QString{};
    if (!name.isEmpty()) {
        return name;
    }
    const int index = static_cast<int>(port) - static_cast<int>(FirstRoomPort);
    return index >= 0 && index < RoomCount ? tr("Room %1").arg(index + 1)
                                           : tr("Room %1").arg(port);
}

QString OnlinePanel::StatusText() const {
    if (CurrentMode() == Mode::WiFi) {
        return tr("Local Wi-Fi");
    }
    if (const unsigned port = JoinedPort()) {
        const auto member = Network::GetRoomMember().lock();
        if (!member || !member->IsConnected()) {
            return tr("Joining %1...").arg(RoomLabel(port));
        }
        const int players = static_cast<int>(member->GetMemberInformation().size());
        return tr("Online · %1 · %n player(s)", "", players).arg(RoomLabel(port));
    }
    if (online) {
        return tr("Online");
    }
    return tr("Offline");
}

bool OnlinePanel::IsOnline() const {
    return JoinedPort() != 0 || online || CurrentMode() == Mode::WiFi;
}

int OnlinePanel::AlertCount() const {
    if (!SignedIn()) {
        return 0;
    }
    int count = static_cast<int>(account->IncomingRequests().size());
    for (const Friend& player : account->Friends()) {
        count += player.unread;
    }
    return count;
}

void OnlinePanel::Update() {
    const bool signed_in = SignedIn();
    if (!signed_in && CurrentMode() != Mode::Server) {
        online = false;
    }
    if (JoinedPort() != 0) {
        online = true; // A room joined from elsewhere (a friend's "Join") is online too
    }
    pages->setCurrentIndex(online ? 1 : 0);
    refresh_button->setVisible(online && tab == Tab::Rooms);

    offline_account->setText(signed_in ? tr("As %1, with your Erable account")
                                             .arg(Shown(account->CurrentSession()->nickname))
                                       : tr("Needs an Erable account: you sign in first."));
    wifi_button->setText(CurrentMode() == Mode::WiFi ? tr("Local Wi-Fi is on · Turn it off")
                                                     : tr("Play on the local Wi-Fi (same network)"));

    nickname_notice->setVisible(signed_in && account->CurrentSession()->nickname_must_change);

    const unsigned port = JoinedPort();
    room_card->setVisible(port != 0);
    if (port != 0) {
        const auto member = Network::GetRoomMember().lock();
        const bool connected = member && member->IsConnected();
        room_name->setText(RoomLabel(port));
        QString game;
        if (account) {
            for (const Room& room : account->Rooms()) {
                if (room.port == port) {
                    game = room.game;
                }
            }
        }
        if (!connected) {
            room_game->setText(tr("Joining the room..."));
        } else if (!game.isEmpty()) {
            room_game->setText(tr("For %1").arg(game));
        } else {
            room_game->setText(tr("Free: the first game started here holds it"));
        }
    }

    int requests = 0;
    int unread = 0;
    int online_friends = 0;
    if (signed_in) {
        requests = static_cast<int>(account->IncomingRequests().size());
        for (const Friend& player : account->Friends()) {
            online_friends += player.online ? 1 : 0;
            unread += player.unread;
        }
    }
    tab_buttons[static_cast<int>(Tab::Alerts)]->setText(
        requests > 0 ? tr("Alerts · %1").arg(requests) : tr("Alerts"));
    tab_buttons[static_cast<int>(Tab::Friends)]->setText(
        unread > 0           ? tr("Friends · %1 new").arg(unread)
        : online_friends > 0 ? tr("Friends · %1").arg(online_friends)
                             : tr("Friends"));
    emit StatusChanged();
}

void OnlinePanel::ShowTab(Tab chosen) {
    tab = chosen;
    tabs->setCurrentIndex(static_cast<int>(tab));
    for (int i = 0; i < 3; ++i) {
        tab_buttons[i]->setChecked(i == static_cast<int>(tab));
    }
    if (tab == Tab::Rooms) {
        FetchRooms();
    } else if (tab == Tab::Friends) {
        FetchConversation();
    }
    refresh_button->setVisible(online && tab == Tab::Rooms);
}

bool OnlinePanel::ChangeNickname() {
    if (!SignedIn()) {
        return false;
    }
    QString error;
    while (true) {
        bool ok = false;
        QString label = tr("Your new nickname: 4 to 20 letters (without accents), digits, spaces, "
                           "'.', '_' or '-'.");
        if (!error.isEmpty()) {
            label = error + QStringLiteral("\n\n") + label;
        }
        const QString nickname = QInputDialog::getText(window(), tr("Change my nickname"), label,
                                                       QLineEdit::Normal, {}, &ok);
        if (!ok) {
            return false;
        }
        if (Common::Profanity::Contains(nickname.toStdString())) {
            error = Account::ErrorText(QStringLiteral("offensive_nickname"));
            continue;
        }
        // The server's answer before asking again
        bool answered = false;
        bool changed = false;
        QEventLoop loop;
        account->ChangeNickname(nickname, [&](const Account::Result& result) {
            answered = true;
            changed = result.ok;
            error = result.ok ? QString{} : Account::ErrorText(result.error);
            loop.quit();
        });
        if (!answered) {
            loop.exec();
        }
        if (changed) {
            Update();
            return true;
        }
    }
}

void OnlinePanel::GoOnline() {
    if (!account) {
        return;
    }
    if (!account->CurrentSession()) {
        AccountDialog dialog(*account, window());
        dialog.exec();
        if (!account->CurrentSession()) {
            return;
        }
    }
    if (account->CurrentSession()->nickname_must_change && !ChangeNickname()) {
        return;
    }
    if (CurrentMode() == Mode::WiFi) {
        SetMode(Mode::Off);
        ApplyMode(account, {});
    }
    online = true;
    Update();
    ShowTab(Tab::Rooms);
}

void OnlinePanel::FetchRooms() {
    if (!SignedIn() || fetching_rooms || !online || !isVisible()) {
        return;
    }
    fetching_rooms = true;
    rooms_status->setText(tr("Fetching the rooms..."));
    QPointer<OnlinePanel> self{this};
    account->FetchRooms([self](const Account::Result& result) {
        if (!self) {
            return;
        }
        self->fetching_rooms = false;
        self->rooms_status->setText(result.ok ? QString{} : Account::ErrorText(result.error));
        self->RebuildRooms();
        self->Update();
    });
}

void OnlinePanel::RebuildRooms() {
    ClearLayout(rooms_list);
    QList<Room> rooms = account ? account->Rooms() : QList<Room>{};
    if (rooms.isEmpty()) {
        // A server that gives no list: its rooms by number
        for (int i = 0; i < RoomCount; ++i) {
            const unsigned port = FirstRoomPort + static_cast<unsigned>(i);
            rooms.append({.name = tr("Room %1").arg(i + 1), .port = port});
        }
    }
    const unsigned joined = JoinedPort();
    for (const Room& room : rooms) {
        QString state;
        if (room.port == joined) {
            state = tr("Your room");
        } else if (room.players == 0) {
            state = tr("Free");
        } else if (room.game.isEmpty()) {
            state = tr("%n player(s), no game yet", "", room.players);
        } else {
            state = tr("%1 · %n player(s)", "", room.players).arg(room.game);
        }
        auto* button = MakeButton(QStringLiteral("%1\n%2").arg(room.name, state),
                                  room.port == joined ? "roomJoined" : "room", nullptr);
        const unsigned port = room.port;
        connect(button, &QPushButton::clicked, this, [this, port] { Choose(port); });
        rooms_list->addWidget(button);
    }
}

void OnlinePanel::Choose(unsigned port) {
    if (!account) {
        return;
    }
    if (SignedIn() && account->CurrentSession()->nickname_must_change && !ChangeNickname()) {
        return;
    }
    if (port != JoinedPort()) {
        JoinRoom(account, port);
    }
    online = true;
    RebuildRooms();
    Update();
}

void OnlinePanel::LeaveRoom() {
    SetMode(Mode::Off);
    ApplyMode(account, {});
    RestoreMods();
    online = true;
    ShowTab(Tab::Rooms);
    RebuildRooms();
    Update();
}

void OnlinePanel::GoOffline() {
    SetMode(Mode::Off);
    ApplyMode(account, {});
    RestoreMods();
    online = false;
    CloseConversation();
    Update();
}

void OnlinePanel::ToggleWiFi() {
    SetMode(CurrentMode() == Mode::WiFi ? Mode::Off : Mode::WiFi);
    ApplyMode(account, {});
    online = false;
    Update();
}

void OnlinePanel::RebuildFriends() {
    ClearLayout(friends_list);
    if (!SignedIn()) {
        return;
    }
    const auto& friends = account->Friends();
    if (friends.isEmpty()) {
        friends_list->addWidget(MakeLabel(
            tr("No friends yet: share your friend code (%1), or add theirs.")
                .arg(account->CurrentSession()->friend_code),
            "hint", nullptr));
        return;
    }
    const unsigned joined = JoinedPort();
    for (const Friend& player : friends) {
        auto* row = new QFrame;
        row->setObjectName(QStringLiteral("friendRow"));
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(4, 4, 4, 4);
        auto* names = new QVBoxLayout;
        names->setSpacing(0);
        names->addWidget(new QLabel(Shown(player.nickname).toHtmlEscaped(), row));
        QString status;
        if (!player.online) {
            status = tr("Offline");
        } else if (!player.game.isEmpty() && player.room_port != 0) {
            status = tr("Plays %1 in %2").arg(player.game, RoomLabel(player.room_port));
        } else if (!player.game.isEmpty()) {
            status = tr("Plays %1").arg(player.game);
        } else if (player.room_port != 0) {
            status = tr("In %1").arg(RoomLabel(player.room_port));
        } else {
            status = tr("Online");
        }
        if (player.unread > 0) {
            status = tr("%n new message(s)", "", player.unread) + QStringLiteral(" · ") + status;
        }
        names->addWidget(MakeLabel(status, "hint", row));
        line->addLayout(names, 1);
        if (player.online && player.room_port != 0 && player.room_port != joined) {
            auto* join = MakeButton(tr("Join"), "small", row);
            const unsigned port = player.room_port;
            connect(join, &QPushButton::clicked, this, [this, port] { Choose(port); });
            line->addWidget(join);
        }
        auto* write = MakeButton(tr("Write"), player.unread > 0 ? "accept" : "small", row);
        const QString code = player.friend_code;
        const QString nickname = player.nickname;
        connect(write, &QPushButton::clicked, this,
                [this, code, nickname] { OpenConversation(code, nickname); });
        line->addWidget(write);
        friends_list->addWidget(row);
    }
}

void OnlinePanel::RebuildAlerts() {
    ClearLayout(alerts_list);
    if (!SignedIn()) {
        return;
    }
    const auto& incoming = account->IncomingRequests();
    const auto& outgoing = account->OutgoingRequests();
    if (incoming.isEmpty() && outgoing.isEmpty()) {
        alerts_list->addWidget(MakeLabel(tr("Nothing new."), "hint", nullptr));
        return;
    }
    for (const FriendRequest& request : incoming) {
        auto* card = new QFrame;
        card->setObjectName(QStringLiteral("card"));
        auto* card_column = new QVBoxLayout(card);
        card_column->setContentsMargins(12, 10, 12, 10);
        card_column->addWidget(MakeLabel(
            tr("<b>%1</b> wants to be your friend").arg(Shown(request.nickname).toHtmlEscaped()),
            "", card));
        auto* answers = new QHBoxLayout;
        auto* accept = MakeButton(tr("Accept"), "accept", card);
        auto* refuse = MakeButton(tr("Refuse"), "small", card);
        const QString code = request.friend_code;
        const auto answer = [this, code](bool yes) {
            if (account) {
                account->RespondFriendRequest(code, yes, [](const Account::Result&) {});
            }
        };
        connect(accept, &QPushButton::clicked, this, [answer] { answer(true); });
        connect(refuse, &QPushButton::clicked, this, [answer] { answer(false); });
        answers->addWidget(accept);
        answers->addWidget(refuse);
        answers->addStretch();
        card_column->addLayout(answers);
        alerts_list->addWidget(card);
    }
    for (const FriendRequest& request : outgoing) {
        alerts_list->addWidget(MakeLabel(tr("Request sent to %1, waiting for their answer")
                                             .arg(Shown(request.nickname).toHtmlEscaped()),
                                         "hint", nullptr));
    }
}

void OnlinePanel::OpenConversation(const QString& code, const QString& nickname) {
    talking_code = code;
    talking_name->setText(Shown(nickname));
    conversation->clear();
    message_error->clear();
    last_message_id = 0;
    friends_pages->setCurrentIndex(1);
    FetchConversation();
    conversation_timer.start();
    message_edit->setFocus();
}

void OnlinePanel::CloseConversation() {
    conversation_timer.stop();
    talking_code.clear();
    friends_pages->setCurrentIndex(0);
}

void OnlinePanel::FetchConversation() {
    if (talking_code.isEmpty() || !SignedIn() || fetching_messages) {
        return;
    }
    if (!isVisible() || tab != Tab::Friends) {
        return; // Fetched again when shown: the friends' counts tell meanwhile
    }
    fetching_messages = true;
    QPointer<OnlinePanel> self{this};
    const QString code = talking_code;
    account->FetchMessages(
        code, last_message_id,
        [self, code](const Account::Result& result, const QList<Message>& messages) {
            if (!self) {
                return;
            }
            self->fetching_messages = false;
            if (code != self->talking_code) {
                return;
            }
            if (!result.ok) {
                self->message_error->setText(Account::ErrorText(result.error));
                return;
            }
            const QString friend_name = self->talking_name->text().toHtmlEscaped();
            for (const Message& message : messages) {
                const QDateTime when = QDateTime::fromSecsSinceEpoch(message.sent_at);
                const QString time = when.date() == QDate::currentDate()
                                         ? when.toString(QStringLiteral("HH:mm"))
                                         : when.toString(QStringLiteral("dd/MM HH:mm"));
                self->conversation->append(
                    QStringLiteral("<p style=\"margin:4px 0\"><b>%1</b> "
                                   "<span style=\"color:gray\">%2</span><br>%3</p>")
                        .arg(message.from_me ? tr("You") : friend_name, time,
                             Shown(message.text).toHtmlEscaped()));
                self->last_message_id = std::max(self->last_message_id, message.id);
            }
            if (!messages.isEmpty()) {
                QScrollBar* bar = self->conversation->verticalScrollBar();
                bar->setValue(bar->maximum());
            }
        });
}

void OnlinePanel::SendToFriend() {
    const QString text = message_edit->text().trimmed();
    if (text.isEmpty() || talking_code.isEmpty() || !SignedIn()) {
        return;
    }
    // Sent with its insults and swear words as stars
    QPointer<OnlinePanel> self{this};
    account->SendMessage(talking_code, Shown(text), [self](const Account::Result& result) {
        if (!self) {
            return;
        }
        if (result.ok) {
            self->message_edit->clear();
            self->message_error->clear();
            self->FetchConversation();
        } else {
            self->message_error->setText(Account::ErrorText(result.error));
        }
    });
}

void OnlinePanel::RestoreMods() {
    const int restored = Mods::Restore();
    if (restored == 0) {
        return;
    }
    mods_note->setText(tr("%n mod(s) paused for the online play turned back on.", "", restored));
    mods_note->show();
    emit ModsChanged();
}

bool OnlinePanel::MayLaunch(const QString& game_title, u64 program_id,
                            const std::vector<std::string>& mods) {
    const unsigned port = JoinedPort();
    if (port == 0 || !account) {
        return true;
    }
    if (!mods.empty()) {
        QStringList names;
        for (const std::string& name : mods) {
            names.append(QStringLiteral("• ") + QString::fromStdString(name));
        }
        QMessageBox box(QMessageBox::Information, tr("No mods in the rooms"),
                        tr("Mods and cheats are not allowed in the rooms of the Erable server. "
                           "%1 has these on:\n\n%2\n\nPause them to play online: they come back "
                           "when you go offline, leave the room or start Erable again.")
                            .arg(game_title, names.join(QLatin1Char('\n'))),
                        QMessageBox::NoButton, window());
        QPushButton* pause = box.addButton(tr("Pause them and play"), QMessageBox::AcceptRole);
        QPushButton* offline = box.addButton(tr("Play offline with them"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == pause) {
            Mods::Pause(program_id, mods);
            emit ModsChanged();
        } else if (box.clickedButton() == offline) {
            GoOffline();
            return true;
        } else {
            return false;
        }
    }
    if (game_title.isEmpty()) {
        return true;
    }
    for (const Room& room : account->Rooms()) {
        if (room.port != port || room.players == 0 || room.game.isEmpty() ||
            room.game.compare(game_title, Qt::CaseInsensitive) == 0) {
            continue;
        }
        QMessageBox box(QMessageBox::Information, tr("Another game holds this room"),
                        tr("%1 is played in %2: a room holds one game at a time. Choose another "
                           "room to play %3 with friends, or go offline.")
                            .arg(room.game, RoomLabel(port), game_title),
                        QMessageBox::NoButton, window());
        QPushButton* rooms_button = box.addButton(tr("See the rooms"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == rooms_button) {
            online = true;
            Open();
            ShowTab(Tab::Rooms);
        }
        return false;
    }
    return true;
}

} // namespace Online
