// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/account_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "erable/online/online_account.h"

namespace Online {

namespace {

QLineEdit* PasswordField(QWidget* parent) {
    auto* field = new QLineEdit(parent);
    field->setEchoMode(QLineEdit::Password);
    field->setMaxLength(128);
    return field;
}

QLineEdit* CodeField(QWidget* parent) {
    auto* field = new QLineEdit(parent);
    field->setEchoMode(QLineEdit::Password);
    field->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]{0,12}")), field));
    field->setPlaceholderText(QObject::tr("6 to 12 digits"));
    return field;
}

QLabel* MessageLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->hide();
    return label;
}

QPushButton* LinkButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // Anonymous namespace

AccountDialog::AccountDialog(Account& account_, QWidget* parent)
    : QDialog{parent}, account{account_}, pages{new QStackedWidget(this)} {
    setWindowTitle(tr("Erable account"));
    setMinimumWidth(540);
    sign_in_page = MakeSignIn();
    sign_up_page = MakeSignUp();
    recovery_page = MakeRecovery();
    signed_in_page = MakeSignedIn();
    for (QWidget* page : {sign_in_page, sign_up_page, recovery_page, signed_in_page}) {
        pages->addWidget(page);
    }
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(pages);
    // The window takes the size of the page shown, not of the largest one
    connect(pages, &QStackedWidget::currentChanged, this, [this](int current) {
        for (int i = 0; i < pages->count(); ++i) {
            const auto policy = i == current ? QSizePolicy::Preferred : QSizePolicy::Ignored;
            pages->widget(i)->setSizePolicy(policy, policy);
        }
        adjustSize();
    });
    connect(&account, &Account::SessionChanged, this, &AccountDialog::ShowCurrent);
    ShowCurrent();
}

AccountDialog::~AccountDialog() = default;

QWidget* AccountDialog::MakeSignIn() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* title = new QLabel(tr("<h3>Sign in to your Erable account</h3>"), page);
    sign_in_email = new QLineEdit(page);
    sign_in_email->setPlaceholderText(tr("you@example.com"));
    sign_in_password = PasswordField(page);
    auto* form = new QFormLayout;
    form->addRow(tr("E-mail"), sign_in_email);
    form->addRow(tr("Password"), sign_in_password);
    sign_in_message = MessageLabel(page);

    auto* submit = new QPushButton(tr("Sign in"), page);
    submit->setDefault(true);
    connect(submit, &QPushButton::clicked, this, &AccountDialog::SignIn);
    connect(sign_in_password, &QLineEdit::returnPressed, this, &AccountDialog::SignIn);
    auto* create = LinkButton(tr("Create an account"), page);
    connect(create, &QPushButton::clicked, this, [this] {
        sign_up_email->setText(sign_in_email->text());
        pages->setCurrentWidget(sign_up_page);
    });
    auto* forgot = LinkButton(tr("Forgot your password?"), page);
    connect(forgot, &QPushButton::clicked, this, [this] {
        recovery_email->setText(sign_in_email->text());
        pages->setCurrentWidget(recovery_page);
    });
    action_buttons << submit;

    auto* links = new QHBoxLayout;
    links->addWidget(create);
    links->addStretch();
    links->addWidget(forgot);
    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(sign_in_message);
    layout->addWidget(submit);
    layout->addLayout(links);
    layout->addStretch();
    return page;
}

QWidget* AccountDialog::MakeSignUp() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* title = new QLabel(tr("<h3>Create your Erable account</h3>"), page);
    sign_up_nickname = new QLineEdit(page);
    sign_up_nickname->setMaxLength(20);
    sign_up_nickname->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[ A-Za-z0-9._-]{0,20}")), sign_up_nickname));
    sign_up_nickname->setPlaceholderText(tr("4 to 20 characters, no accents"));
    sign_up_email = new QLineEdit(page);
    sign_up_password = PasswordField(page);
    sign_up_password->setPlaceholderText(tr("At least 8 characters"));
    sign_up_password_again = PasswordField(page);
    sign_up_question = new QComboBox(page);
    for (const QString& key : Account::QuestionKeys()) {
        sign_up_question->addItem(Account::QuestionText(key), key);
    }
    sign_up_answer = new QLineEdit(page);
    sign_up_answer->setMaxLength(128);
    sign_up_code = CodeField(page);
    sign_up_code_again = CodeField(page);

    auto* form = new QFormLayout;
    form->addRow(tr("Nickname"), sign_up_nickname);
    form->addRow(tr("E-mail"), sign_up_email);
    form->addRow(tr("Password"), sign_up_password);
    form->addRow(tr("Password again"), sign_up_password_again);
    form->addRow(tr("Security question"), sign_up_question);
    form->addRow(tr("Answer"), sign_up_answer);
    form->addRow(tr("Security code"), sign_up_code);
    form->addRow(tr("Security code again"), sign_up_code_again);
    auto* hint = new QLabel(tr("The answer and the security code let you choose a new password "
                               "if you forget yours: keep them somewhere safe."),
                            page);
    hint->setWordWrap(true);
    sign_up_privacy = new QCheckBox(tr("I have read the privacy notice"), page);
    auto* privacy = LinkButton(tr("Read the privacy notice"), page);
    connect(privacy, &QPushButton::clicked, this, &AccountDialog::ShowPrivacyNotice);
    auto* privacy_row = new QHBoxLayout;
    privacy_row->addWidget(sign_up_privacy);
    privacy_row->addStretch();
    privacy_row->addWidget(privacy);
    sign_up_message = MessageLabel(page);

    auto* submit = new QPushButton(tr("Create the account"), page);
    connect(submit, &QPushButton::clicked, this, &AccountDialog::SignUp);
    auto* back = LinkButton(tr("I already have an account"), page);
    connect(back, &QPushButton::clicked, this, [this] { pages->setCurrentWidget(sign_in_page); });
    action_buttons << submit;

    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addLayout(privacy_row);
    layout->addWidget(sign_up_message);
    layout->addWidget(submit);
    layout->addWidget(back, 0, Qt::AlignLeft);
    layout->addStretch();
    return page;
}

QWidget* AccountDialog::MakeRecovery() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    auto* title = new QLabel(tr("<h3>Choose a new password</h3>"), page);
    auto* intro = new QLabel(tr("Give your account's e-mail, then the answer to your security "
                                "question and your security code."),
                             page);
    intro->setWordWrap(true);
    recovery_email = new QLineEdit(page);
    auto* fetch = new QPushButton(tr("Show my question"), page);
    connect(fetch, &QPushButton::clicked, this, &AccountDialog::FetchQuestion);
    auto* email_row = new QHBoxLayout;
    email_row->addWidget(recovery_email);
    email_row->addWidget(fetch);
    recovery_question = new QLabel(page);
    recovery_question->setWordWrap(true);
    recovery_answer = new QLineEdit(page);
    recovery_code = CodeField(page);
    recovery_password = PasswordField(page);
    recovery_password->setPlaceholderText(tr("At least 8 characters"));
    recovery_password_again = PasswordField(page);

    auto* form = new QFormLayout;
    form->addRow(tr("E-mail"), email_row);
    form->addRow(tr("Question"), recovery_question);
    form->addRow(tr("Answer"), recovery_answer);
    form->addRow(tr("Security code"), recovery_code);
    form->addRow(tr("New password"), recovery_password);
    form->addRow(tr("New password again"), recovery_password_again);
    recovery_message = MessageLabel(page);

    recovery_submit = new QPushButton(tr("Change the password"), page);
    connect(recovery_submit, &QPushButton::clicked, this, &AccountDialog::Recover);
    auto* back = LinkButton(tr("Back to sign in"), page);
    connect(back, &QPushButton::clicked, this, [this] { pages->setCurrentWidget(sign_in_page); });
    action_buttons << fetch << recovery_submit;

    layout->addWidget(title);
    layout->addWidget(intro);
    layout->addLayout(form);
    layout->addWidget(recovery_message);
    layout->addWidget(recovery_submit);
    layout->addWidget(back, 0, Qt::AlignLeft);
    layout->addStretch();
    return page;
}

QWidget* AccountDialog::MakeSignedIn() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    signed_in_nickname = new QLabel(page);
    signed_in_email = new QLabel(page);
    signed_in_code = new QLabel(page);
    signed_in_code->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* copy = new QPushButton(tr("Copy my friend code"), page);
    connect(copy, &QPushButton::clicked, this, [this] {
        if (const auto& session = account.CurrentSession()) {
            QApplication::clipboard()->setText(session->friend_code);
            ShowMessage(signed_in_message, tr("Friend code copied."), false);
        }
    });
    signed_in_message = MessageLabel(page);
    auto* sign_out = new QPushButton(tr("Sign out"), page);
    connect(sign_out, &QPushButton::clicked, this, &AccountDialog::SignOut);
    auto* delete_account = LinkButton(tr("Delete my account"), page);
    connect(delete_account, &QPushButton::clicked, this, &AccountDialog::DeleteAccount);
    auto* privacy = LinkButton(tr("Privacy notice"), page);
    connect(privacy, &QPushButton::clicked, this, &AccountDialog::ShowPrivacyNotice);
    auto* close = new QPushButton(tr("Close"), page);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    action_buttons << sign_out;

    auto* form = new QFormLayout;
    form->addRow(tr("E-mail"), signed_in_email);
    form->addRow(tr("Friend code"), signed_in_code);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(copy);
    buttons->addStretch();
    buttons->addWidget(sign_out);
    buttons->addWidget(close);
    layout->addWidget(signed_in_nickname);
    layout->addLayout(form);
    layout->addWidget(signed_in_message);
    layout->addLayout(buttons);
    auto* links = new QHBoxLayout;
    links->addWidget(privacy);
    links->addStretch();
    links->addWidget(delete_account);
    layout->addLayout(links);
    layout->addStretch();
    return page;
}

void AccountDialog::SignIn() {
    if (sign_in_email->text().trimmed().isEmpty() || sign_in_password->text().isEmpty()) {
        ShowMessage(sign_in_message, tr("Give your e-mail and your password."), true);
        return;
    }
    SetBusy(true);
    QPointer<AccountDialog> self{this};
    account.Login(
        sign_in_email->text(), sign_in_password->text(), [self](const Account::Result& result) {
            if (!self) {
                return;
            }
            self->SetBusy(false);
            self->sign_in_password->clear();
            if (!result.ok) {
                self->ShowMessage(self->sign_in_message, Account::ErrorText(result.error), true);
            }
        });
}

void AccountDialog::SignUp() {
    // What the server would refuse anyway, said before sending
    if (sign_up_nickname->text().trimmed().size() < 4) {
        ShowMessage(sign_up_message, Account::ErrorText(QStringLiteral("bad_nickname")), true);
        return;
    }
    if (!sign_up_email->text().contains(QLatin1Char('@'))) {
        ShowMessage(sign_up_message, Account::ErrorText(QStringLiteral("bad_email")), true);
        return;
    }
    if (sign_up_password->text().size() < 8) {
        ShowMessage(sign_up_message, Account::ErrorText(QStringLiteral("weak_password")), true);
        return;
    }
    if (sign_up_password->text() != sign_up_password_again->text()) {
        ShowMessage(sign_up_message, tr("The two passwords differ."), true);
        return;
    }
    if (sign_up_answer->text().trimmed().size() < 2) {
        ShowMessage(sign_up_message, Account::ErrorText(QStringLiteral("bad_answer")), true);
        return;
    }
    if (sign_up_code->text().size() < 6) {
        ShowMessage(sign_up_message, Account::ErrorText(QStringLiteral("bad_code")), true);
        return;
    }
    if (sign_up_code->text() != sign_up_code_again->text()) {
        ShowMessage(sign_up_message, tr("The two security codes differ."), true);
        return;
    }
    if (!sign_up_privacy->isChecked()) {
        ShowMessage(sign_up_message, tr("Read and accept the privacy notice first."), true);
        return;
    }
    SetBusy(true);
    QPointer<AccountDialog> self{this};
    account.Register(
        sign_up_nickname->text(), sign_up_email->text(), sign_up_password->text(),
        sign_up_question->currentData().toString(), sign_up_answer->text(), sign_up_code->text(),
        [self](const Account::Result& result) {
            if (!self) {
                return;
            }
            self->SetBusy(false);
            if (!result.ok) {
                self->ShowMessage(self->sign_up_message, Account::ErrorText(result.error), true);
                return;
            }
            for (QLineEdit* field :
                 {self->sign_up_password, self->sign_up_password_again, self->sign_up_answer,
                  self->sign_up_code, self->sign_up_code_again}) {
                field->clear();
            }
        });
}

void AccountDialog::FetchQuestion() {
    if (recovery_email->text().trimmed().isEmpty()) {
        ShowMessage(recovery_message, tr("Give your account's e-mail."), true);
        return;
    }
    SetBusy(true);
    QPointer<AccountDialog> self{this};
    account.RecoveryQuestion(recovery_email->text(), [self](const Account::Result& result) {
        if (!self) {
            return;
        }
        self->SetBusy(false);
        if (!result.ok) {
            self->ShowMessage(self->recovery_message, Account::ErrorText(result.error), true);
            return;
        }
        self->recovery_message->hide();
        self->recovery_question->setText(
            Account::QuestionText(result.data.value(QStringLiteral("question")).toString()));
        self->recovery_answer->setFocus();
    });
}

void AccountDialog::Recover() {
    if (recovery_question->text().isEmpty()) {
        ShowMessage(recovery_message, tr("Show your question first."), true);
        return;
    }
    if (recovery_password->text().size() < 8) {
        ShowMessage(recovery_message, Account::ErrorText(QStringLiteral("weak_password")), true);
        return;
    }
    if (recovery_password->text() != recovery_password_again->text()) {
        ShowMessage(recovery_message, tr("The two passwords differ."), true);
        return;
    }
    SetBusy(true);
    QPointer<AccountDialog> self{this};
    account.Recover(
        recovery_email->text(), recovery_answer->text(), recovery_code->text(),
        recovery_password->text(), [self](const Account::Result& result) {
            if (!self) {
                return;
            }
            self->SetBusy(false);
            for (QLineEdit* field : {self->recovery_answer, self->recovery_code,
                                     self->recovery_password, self->recovery_password_again}) {
                field->clear();
            }
            if (!result.ok) {
                self->ShowMessage(self->recovery_message, Account::ErrorText(result.error), true);
                return;
            }
            self->ShowMessage(self->signed_in_message,
                              tr("Password changed. Your other devices were "
                                 "signed out."),
                              false);
        });
}

void AccountDialog::SignOut() {
    SetBusy(true);
    QPointer<AccountDialog> self{this};
    account.Logout([self](const Account::Result&) {
        if (self) {
            self->SetBusy(false);
        }
    });
}

void AccountDialog::DeleteAccount() {
    bool ok = false;
    const QString password = QInputDialog::getText(
        this, tr("Delete my account"),
        tr("This deletes your Erable account, its friend code and everything the server keeps "
           "about it. It can't be undone.\n\nEnter your password to confirm:"),
        QLineEdit::Password, {}, &ok);
    if (!ok || password.isEmpty()) {
        return;
    }
    SetBusy(true);
    QPointer<AccountDialog> self{this};
    account.DeleteAccount(password, [self](const Account::Result& result) {
        if (!self) {
            return;
        }
        self->SetBusy(false);
        if (!result.ok) {
            self->ShowMessage(self->signed_in_message, Account::ErrorText(result.error), true);
            return;
        }
        self->ShowMessage(self->sign_in_message, tr("Your account was deleted."), false);
    });
}

void AccountDialog::ShowPrivacyNotice() {
    QMessageBox box{this};
    box.setWindowTitle(tr("Privacy notice"));
    box.setTextFormat(Qt::RichText);
    box.setText(tr(
        "<h3>Your Erable account</h3>"
        "<p><b>What the Erable server keeps:</b> your nickname, your e-mail, your friend code, "
        "your password, the answer to your security question and your security code. The "
        "password, the answer and the code are stored scrambled (hashed): nobody can read them, "
        "not even us.</p>"
        "<p><b>While you use it:</b> your sessions (one per device, 180 days at most) and, for 15 "
        "minutes, the network address of failed sign-ins, to stop password guessing.</p>"
        "<p><b>Why:</b> to sign you in, to let you choose a new password, and to let you play "
        "online in the Erable rooms. Nothing is sold, shared or used for advertising. No e-mail "
        "is sent to you.</p>"
        "<p><b>Where:</b> on Erable's server, hosted by OVHcloud. Online rooms relay the game's "
        "traffic "
        "between the players; they keep nothing.</p>"
        "<p><b>How long:</b> until you delete your account: \"Delete my account\", once signed "
        "in, removes everything at once.</p>"));
    box.exec();
}

void AccountDialog::ShowCurrent() {
    if (const auto& session = account.CurrentSession()) {
        signed_in_nickname->setText(
            tr("<h3>Signed in as %1</h3>").arg(session->nickname.toHtmlEscaped()));
        signed_in_email->setText(session->email.toHtmlEscaped());
        signed_in_code->setText(QStringLiteral("<b>%1</b>").arg(session->friend_code));
        pages->setCurrentWidget(signed_in_page);
    } else {
        signed_in_message->hide();
        pages->setCurrentWidget(sign_in_page);
    }
}

void AccountDialog::SetBusy(bool busy) {
    for (QPushButton* button : action_buttons) {
        button->setEnabled(!busy);
    }
    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
        QApplication::restoreOverrideCursor();
    }
}

void AccountDialog::ShowMessage(QLabel* label, const QString& text, bool error) {
    label->setStyleSheet(error ? QStringLiteral("color: #e5534b;") : QString{});
    label->setText(text);
    label->show();
}

} // namespace Online
