// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QVBoxLayout;

namespace Online {

class Account;

/// The friends of the Erable account: its friend code, adding a friend by code, the requests
/// received and sent, and each friend with what they play and in which room, to join them
class FriendsDialog : public QDialog {
    Q_OBJECT

public:
    explicit FriendsDialog(Account& account, QWidget* parent = nullptr);
    ~FriendsDialog() override;

signals:
    /// The player asked to join the room `port` a friend is in
    void JoinRoomRequested(unsigned port);

private:
    void SendRequest();
    void Rebuild();
    void ShowMessage(const QString& text, bool error);
    /// Shows the server's answer to an action, when it is an error
    void Done(bool ok, const QString& error);

    Account& account;
    QLabel* own_code;
    QLineEdit* add_code;
    QLabel* message;
    QVBoxLayout* incoming_rows;
    QVBoxLayout* friend_rows;
    QVBoxLayout* outgoing_rows;
    QLabel* incoming_title;
    QLabel* outgoing_title;
    QLabel* no_friends;
};

} // namespace Online
