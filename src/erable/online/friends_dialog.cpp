// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/friends_dialog.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "erable/online/online_account.h"

namespace Online {

namespace {

QLabel* SectionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(QStringLiteral("<b>%1</b>").arg(text.toHtmlEscaped()), parent);
    label->setContentsMargins(0, 10, 0, 2);
    return label;
}

/// Deletes every row of a section
void Clear(QVBoxLayout* rows) {
    while (QLayoutItem* item = rows->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

} // Anonymous namespace

FriendsDialog::FriendsDialog(Account& account_, QWidget* parent)
    : QDialog{parent}, account{account_} {
    setWindowTitle(tr("Friends"));
    setMinimumSize(660, 460);
    auto* layout = new QVBoxLayout(this);

    // This account's code, and adding a friend by theirs
    own_code = new QLabel(this);
    own_code->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* copy = new QPushButton(tr("Copy"), this);
    connect(copy, &QPushButton::clicked, this, [this] {
        if (const auto& session = account.CurrentSession()) {
            QApplication::clipboard()->setText(session->friend_code);
            ShowMessage(tr("Friend code copied."), false);
        }
    });
    auto* own_row = new QHBoxLayout;
    own_row->addWidget(own_code, 1);
    own_row->addWidget(copy);

    add_code = new QLineEdit(this);
    add_code->setPlaceholderText(tr("A friend's code: ER-0000-0000-0000"));
    add_code->setMaxLength(32);
    auto* send = new QPushButton(tr("Send a request"), this);
    connect(send, &QPushButton::clicked, this, &FriendsDialog::SendRequest);
    connect(add_code, &QLineEdit::returnPressed, this, &FriendsDialog::SendRequest);
    auto* add_row = new QHBoxLayout;
    add_row->addWidget(add_code, 1);
    add_row->addWidget(send);

    message = new QLabel(this);
    message->setWordWrap(true);
    message->hide();

    // The lists, in a scroll area
    auto* lists = new QWidget(this);
    auto* lists_layout = new QVBoxLayout(lists);
    lists_layout->setContentsMargins(0, 0, 0, 0);
    incoming_title = SectionTitle(tr("Friend requests"), lists);
    incoming_rows = new QVBoxLayout;
    auto* friends_title = SectionTitle(tr("Friends"), lists);
    friend_rows = new QVBoxLayout;
    no_friends = new QLabel(tr("No friends yet: share your friend code, or add theirs."), lists);
    no_friends->setWordWrap(true);
    outgoing_title = SectionTitle(tr("Requests sent"), lists);
    outgoing_rows = new QVBoxLayout;
    lists_layout->addWidget(incoming_title);
    lists_layout->addLayout(incoming_rows);
    lists_layout->addWidget(friends_title);
    lists_layout->addWidget(no_friends);
    lists_layout->addLayout(friend_rows);
    lists_layout->addWidget(outgoing_title);
    lists_layout->addLayout(outgoing_rows);
    lists_layout->addStretch();
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(lists);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    layout->addLayout(own_row);
    layout->addLayout(add_row);
    layout->addWidget(message);
    layout->addWidget(scroll, 1);
    layout->addWidget(close, 0, Qt::AlignRight);

    connect(&account, &Account::FriendsChanged, this, &FriendsDialog::Rebuild);
    Rebuild();
    account.Sync();
}

FriendsDialog::~FriendsDialog() = default;

void FriendsDialog::SendRequest() {
    const QString code = add_code->text().trimmed();
    if (code.isEmpty()) {
        return;
    }
    QPointer<FriendsDialog> self{this};
    account.SendFriendRequest(code, [self](const Account::Result& result) {
        if (!self) {
            return;
        }
        if (!result.ok) {
            self->Done(false, result.error);
            return;
        }
        self->add_code->clear();
        self->ShowMessage(result.data.value(QStringLiteral("status")).toString() ==
                                  QStringLiteral("friends")
                              ? tr("You are now friends.")
                              : tr("Request sent."),
                          false);
    });
}

void FriendsDialog::Done(bool ok, const QString& error) {
    if (!ok) {
        ShowMessage(Account::ErrorText(error), true);
    }
}

void FriendsDialog::Rebuild() {
    const auto& session = account.CurrentSession();
    own_code->setText(session ? tr("Your friend code: <b>%1</b>").arg(session->friend_code)
                              : tr("Sign in to your Erable account to add friends."));

    const auto done = [self = QPointer<FriendsDialog>{this}](const Account::Result& result) {
        if (self) {
            self->Done(result.ok, result.error);
        }
    };
    const auto row = [this](QVBoxLayout* rows, const QString& text,
                            std::initializer_list<QPushButton*> buttons) {
        auto* widget = new QWidget(this);
        auto* line = new QHBoxLayout(widget);
        line->setContentsMargins(0, 2, 0, 2);
        auto* label = new QLabel(text, widget);
        label->setWordWrap(true);
        line->addWidget(label, 1);
        for (QPushButton* button : buttons) {
            button->setParent(widget);
            line->addWidget(button);
        }
        rows->addWidget(widget);
    };

    Clear(incoming_rows);
    for (const FriendRequest& request : account.IncomingRequests()) {
        auto* accept = new QPushButton(tr("Accept"));
        auto* refuse = new QPushButton(tr("Refuse"));
        auto* block = new QPushButton(tr("Block"));
        const QString code = request.friend_code;
        connect(accept, &QPushButton::clicked, this,
                [this, code, done] { account.RespondFriendRequest(code, true, done); });
        connect(refuse, &QPushButton::clicked, this,
                [this, code, done] { account.RespondFriendRequest(code, false, done); });
        connect(block, &QPushButton::clicked, this, [this, request, done] {
            if (QMessageBox::question(
                    this, tr("Block"),
                    tr("Block %1? You won't get their requests anymore.").arg(request.nickname)) ==
                QMessageBox::Yes) {
                account.BlockPlayer(request.friend_code, done);
            }
        });
        row(incoming_rows,
            tr("<b>%1</b> wants to be your friend").arg(request.nickname.toHtmlEscaped()),
            {accept, refuse, block});
    }
    incoming_title->setVisible(!account.IncomingRequests().isEmpty());

    Clear(friend_rows);
    for (const Friend& player : account.Friends()) {
        QString status;
        if (!player.online) {
            status = tr("offline");
        } else if (!player.game.isEmpty() && player.room_port != 0) {
            const QString room = account.RoomName(player.room_port);
            status = tr("plays %1 in %2")
                         .arg(player.game.toHtmlEscaped(),
                              room.isEmpty() ? QString::number(player.room_port) : room);
        } else if (!player.game.isEmpty()) {
            status = tr("plays %1").arg(player.game.toHtmlEscaped());
        } else if (player.room_port != 0) {
            const QString room = account.RoomName(player.room_port);
            status = tr("in %1").arg(room.isEmpty() ? QString::number(player.room_port) : room);
        } else {
            status = tr("online");
        }
        const QString dot = player.online ? QStringLiteral("<span style=\"color:#57c76b\">●</span>")
                                          : QStringLiteral("<span style=\"color:#888\">○</span>");
        QList<QPushButton*> buttons;
        if (player.online && player.room_port != 0) {
            auto* join = new QPushButton(tr("Join"));
            const unsigned port = player.room_port;
            connect(join, &QPushButton::clicked, this, [this, port] {
                emit JoinRoomRequested(port);
                ShowMessage(tr("Joining the room..."), false);
            });
            buttons << join;
        }
        auto* remove = new QPushButton(tr("Remove"));
        connect(remove, &QPushButton::clicked, this, [this, player, done] {
            if (QMessageBox::question(this, tr("Remove"),
                                      tr("Remove %1 from your friends?").arg(player.nickname)) ==
                QMessageBox::Yes) {
                account.RemoveFriend(player.friend_code, done);
            }
        });
        auto* block = new QPushButton(tr("Block"));
        connect(block, &QPushButton::clicked, this, [this, player, done] {
            if (QMessageBox::question(
                    this, tr("Block"),
                    tr("Block %1? You won't get their requests anymore.").arg(player.nickname)) ==
                QMessageBox::Yes) {
                account.BlockPlayer(player.friend_code, done);
            }
        });
        buttons << remove << block;
        auto* widget = new QWidget(this);
        auto* line = new QHBoxLayout(widget);
        line->setContentsMargins(0, 2, 0, 2);
        auto* label = new QLabel(
            QStringLiteral("%1 <b>%2</b> — %3").arg(dot, player.nickname.toHtmlEscaped(), status),
            widget);
        label->setWordWrap(true);
        line->addWidget(label, 1);
        for (QPushButton* button : buttons) {
            button->setParent(widget);
            line->addWidget(button);
        }
        friend_rows->addWidget(widget);
    }
    no_friends->setVisible(account.Friends().isEmpty());

    Clear(outgoing_rows);
    for (const FriendRequest& request : account.OutgoingRequests()) {
        auto* cancel = new QPushButton(tr("Cancel"));
        const QString code = request.friend_code;
        connect(cancel, &QPushButton::clicked, this,
                [this, code, done] { account.RemoveFriend(code, done); });
        row(outgoing_rows, tr("Waiting for %1").arg(request.nickname.toHtmlEscaped()), {cancel});
    }
    outgoing_title->setVisible(!account.OutgoingRequests().isEmpty());
}

void FriendsDialog::ShowMessage(const QString& text, bool error) {
    message->setStyleSheet(error ? QStringLiteral("color: #e5534b;") : QString{});
    message->setText(text);
    message->show();
}

} // namespace Online
