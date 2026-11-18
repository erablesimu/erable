// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/friend_notifier.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTimer>

#include "erable/online/online_account.h"

namespace Online {

FriendNotifier::FriendNotifier(Account& account_, QWidget* window_)
    : QObject{window_}, account{account_}, window{window_}, banner{new QFrame(window_)} {
    banner->setObjectName(QStringLiteral("friend_banner"));
    banner->setStyleSheet(QStringLiteral(
        "QFrame#friend_banner { background: palette(window); border: 1px solid palette(highlight);"
        " border-radius: 14px; }"
        "QFrame#friend_banner QPushButton { padding: 5px 14px; }"));
    banner_text = new QLabel(banner);
    banner_text->setWordWrap(true);
    auto* accept = new QPushButton(tr("Accept"), banner);
    auto* refuse = new QPushButton(tr("Refuse"), banner);
    auto* later = new QPushButton(tr("Later"), banner);
    auto* layout = new QHBoxLayout(banner);
    layout->setContentsMargins(18, 10, 12, 10);
    layout->addWidget(banner_text, 1);
    layout->addWidget(accept);
    layout->addWidget(refuse);
    layout->addWidget(later);
    banner->hide();
    connect(accept, &QPushButton::clicked, this, [this] { Answer(true); });
    connect(refuse, &QPushButton::clicked, this, [this] { Answer(false); });
    connect(later, &QPushButton::clicked, this, [this] {
        // Still in the friends window: only this banner goes
        shown_code.clear();
        banner->hide();
        ShowNext();
    });
    window->installEventFilter(this);
    connect(&account, &Account::FriendRequestReceived, this, &FriendNotifier::Received);
    connect(&account, &Account::FriendsChanged, this, [this] {
        // Answered elsewhere (the friends window, another PC): its banner goes
        if (shown_code.isEmpty()) {
            return;
        }
        for (const FriendRequest& request : account.IncomingRequests()) {
            if (request.friend_code == shown_code) {
                return;
            }
        }
        shown_code.clear();
        banner->hide();
        ShowNext();
    });
}

FriendNotifier::~FriendNotifier() = default;

void FriendNotifier::SetInGame(bool in_game_) {
    in_game = in_game_;
    if (in_game) {
        if (!shown_code.isEmpty()) {
            // Back in the queue, shown again after the game
            for (const FriendRequest& request : account.IncomingRequests()) {
                if (request.friend_code == shown_code) {
                    waiting.prepend({request.nickname, request.friend_code});
                }
            }
            shown_code.clear();
        }
        banner->hide();
    } else {
        ShowNext();
    }
}

void FriendNotifier::Received(const QString& nickname, const QString& friend_code) {
    waiting.append({nickname, friend_code});
    // In the background: the taskbar flashes and the system shows a notification
    if (!window->isActiveWindow() || window->isMinimized()) {
        QApplication::alert(window);
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            if (!tray) {
                tray = new QSystemTrayIcon(window->windowIcon(), this);
                connect(tray, &QSystemTrayIcon::messageClicked, this, [this] {
                    window->showNormal();
                    window->activateWindow();
                    emit OpenFriendsRequested();
                });
            }
            tray->show();
            tray->showMessage(tr("Friend request"),
                              tr("%1 wants to be your friend on Erable.").arg(nickname),
                              window->windowIcon(), 8000);
            QTimer::singleShot(10000, tray, [this] {
                if (tray) {
                    tray->hide();
                }
            });
        }
    }
    ShowNext();
}

void FriendNotifier::ShowNext() {
    if (in_game || !shown_code.isEmpty() || waiting.isEmpty()) {
        return;
    }
    const auto [nickname, friend_code] = waiting.takeFirst();
    shown_code = friend_code;
    banner_text->setText(
        tr("<b>%1</b> wants to be your friend on Erable.").arg(nickname.toHtmlEscaped()));
    Place();
    banner->show();
    banner->raise();
}

void FriendNotifier::Place() {
    const int width = std::min(620, window->width() - 40);
    banner->setFixedWidth(width);
    banner->adjustSize();
    banner->move((window->width() - width) / 2, window->height() - banner->height() - 24);
}

bool FriendNotifier::eventFilter(QObject* watched, QEvent* event) {
    if (watched == window && event->type() == QEvent::Resize && banner->isVisible()) {
        Place();
    }
    return QObject::eventFilter(watched, event);
}

void FriendNotifier::Answer(bool accept) {
    const QString code = shown_code;
    shown_code.clear();
    banner->hide();
    account.RespondFriendRequest(code, accept, [](const Account::Result&) {});
    ShowNext();
}

} // namespace Online
