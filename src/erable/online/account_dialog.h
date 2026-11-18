// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QWidget;

namespace Online {

class Account;

/// The Erable account window: sign in, create an account, recover a forgotten password, and,
/// once signed in, the account with its friend code
class AccountDialog : public QDialog {
    Q_OBJECT

public:
    explicit AccountDialog(Account& account, QWidget* parent = nullptr);
    ~AccountDialog() override;

private:
    QWidget* MakeSignIn();
    QWidget* MakeSignUp();
    QWidget* MakeRecovery();
    QWidget* MakeSignedIn();

    void SignIn();
    void SignUp();
    void FetchQuestion();
    void Recover();
    void SignOut();

    /// Shows the account page when signed in, else the sign-in page
    void ShowCurrent();
    /// Disables the buttons while a request runs, and shows `message` (an error in red)
    void SetBusy(bool busy);
    void ShowMessage(QLabel* label, const QString& text, bool error);

    Account& account;
    QStackedWidget* pages;
    QList<QPushButton*> action_buttons;

    // Sign in
    QWidget* sign_in_page;
    QLineEdit* sign_in_email;
    QLineEdit* sign_in_password;
    QLabel* sign_in_message;

    // Sign up
    QWidget* sign_up_page;
    QLineEdit* sign_up_nickname;
    QLineEdit* sign_up_email;
    QLineEdit* sign_up_password;
    QLineEdit* sign_up_password_again;
    QComboBox* sign_up_question;
    QLineEdit* sign_up_answer;
    QLineEdit* sign_up_code;
    QLineEdit* sign_up_code_again;
    QLabel* sign_up_message;

    // Recovery
    QWidget* recovery_page;
    QLineEdit* recovery_email;
    QLabel* recovery_question;
    QLineEdit* recovery_answer;
    QLineEdit* recovery_code;
    QLineEdit* recovery_password;
    QLineEdit* recovery_password_again;
    QPushButton* recovery_submit;
    QLabel* recovery_message;

    // Signed in
    QWidget* signed_in_page;
    QLabel* signed_in_nickname;
    QLabel* signed_in_email;
    QLabel* signed_in_code;
    QLabel* signed_in_message;
};

} // namespace Online
