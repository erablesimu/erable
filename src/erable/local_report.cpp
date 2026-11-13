// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/local_report.h"

#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "common/fs/path_util.h"

namespace LocalReport {
namespace {

// Mail programs drop or refuse mailto links much longer than this; the full report stays in the
// saved file, whose path the message names.
constexpr qsizetype MaxEmailBody = 1800;

QString ProgramId(quint64 program_id) {
    return QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
}

QString Line(const QString& label, const QString& value) {
    const QString text = value.trimmed();
    return QStringLiteral("%1 : %2\n").arg(label, text.isEmpty() ? QStringLiteral("inconnu") : text);
}

} // Anonymous namespace

QString ReportsDirectory() {
    const auto root =
        QString::fromStdString(Common::FS::GetEdenPathString(Common::FS::EdenPath::EdenDir));
    return QDir(root).filePath(QStringLiteral("reports"));
}

Dialog::Dialog(const CompatibilityReporting::Session& session_, QWidget* parent)
    : QDialog(parent), session(session_) {
    setWindowTitle(QStringLiteral("Rapport de compatibilité"));
    auto* layout = new QVBoxLayout(this);

    auto* introduction = new QLabel(
        QStringLiteral("Le rapport est enregistré sur ce PC, dans le dossier « reports » d'Erable. "
                       "Rien n'est envoyé sur Internet, sauf si vous choisissez de "
                       "l'envoyer par e-mail."),
        this);
    introduction->setWordWrap(true);
    layout->addWidget(introduction);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Jeu"),
                 new QLabel(QStringLiteral("%1 (%2)").arg(session.title,
                                                          ProgramId(session.program_id)),
                            this));
    outcome = new QComboBox(this);
    outcome->addItems({QStringLiteral("Parfait : aucun problème"),
                       QStringLiteral("Bien : petits défauts graphiques ou sonores"),
                       QStringLiteral("Jouable : ralentissements ou bugs gênants"),
                       QStringLiteral("Difficile : plantages ou bugs bloquants"),
                       QStringLiteral("Ne démarre pas")});
    form->addRow(QStringLiteral("Résultat"), outcome);
    layout->addLayout(form);

    notes = new QPlainTextEdit(this);
    notes->setPlaceholderText(QStringLiteral(
        "Décrivez ce qui s'est passé : où dans le jeu, ce que vous faisiez, saccades, textures ou "
        "objets manquants, plantage..."));
    layout->addWidget(notes, 1);

    status = new QLabel(this);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status);

    auto* buttons = new QDialogButtonBox(this);
    auto* save = buttons->addButton(QStringLiteral("Enregistrer"), QDialogButtonBox::ActionRole);
    auto* email =
        buttons->addButton(QStringLiteral("Envoyer par e-mail"), QDialogButtonBox::ActionRole);
    auto* folder = buttons->addButton(QStringLiteral("Ouvrir le dossier des rapports"),
                                      QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    connect(save, &QPushButton::clicked, this, [this] { Save(); });
    connect(email, &QPushButton::clicked, this, [this] { SendByEmail(); });
    connect(folder, &QPushButton::clicked, this, [this] { OpenFolder(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    resize(600, 480);
}

QString Dialog::ReportText() const {
    QString text = QStringLiteral("Rapport de compatibilité Erable\n");
    text += Line(QStringLiteral("Date"),
                 QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")));
    text += Line(QStringLiteral("Jeu"), session.title);
    text += Line(QStringLiteral("Identifiant"), ProgramId(session.program_id));
    text += Line(QStringLiteral("Version du jeu"), session.game_version);
    text += Line(QStringLiteral("Résultat"), outcome->currentText());
    text += QStringLiteral("\nCommentaires :\n%1\n\n").arg(notes->toPlainText().trimmed());
    text += Line(QStringLiteral("Version d'Erable"), session.build);
    text += Line(QStringLiteral("Processeur"), session.cpu);
    text += Line(QStringLiteral("Carte graphique"), session.gpu);
    text += Line(QStringLiteral("Pilote"), session.driver);
    text += Line(QStringLiteral("Système"), session.os);
    text += Line(QStringLiteral("Mémoire"), session.ram);
    text += Line(QStringLiteral("API graphique"), session.graphics_api);
    text += Line(QStringLiteral("Mods"), session.mods);
    text += Line(QStringLiteral("Mises à jour du jeu"), session.updates);
    text += QStringLiteral("\nRéglages :\n%1\n").arg(session.settings.trimmed());
    return text;
}

bool Dialog::Save() {
    const QString directory = ReportsDirectory();
    if (!QDir().mkpath(directory)) {
        status->setText(QStringLiteral("Impossible de créer le dossier %1.").arg(directory));
        return false;
    }
    const QString name = QStringLiteral("%1_%2.txt").arg(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")),
        ProgramId(session.program_id));
    const QString path = QDir(directory).filePath(name);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) ||
        file.write(ReportText().toUtf8()) < 0 || !file.commit()) {
        status->setText(QStringLiteral("Impossible d'enregistrer le rapport dans %1.").arg(path));
        return false;
    }
    saved_path = path;
    status->setText(QStringLiteral("Rapport enregistré : %1").arg(QDir::toNativeSeparators(path)));
    return true;
}

void Dialog::SendByEmail() {
    if (!Save()) {
        return;
    }
    QString body = ReportText();
    if (body.size() > MaxEmailBody) {
        body.truncate(MaxEmailBody);
        body += QStringLiteral("\n[...]\nRapport complet : %1")
                    .arg(QDir::toNativeSeparators(saved_path));
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("subject"),
                       QStringLiteral("Rapport Erable : %1").arg(session.title));
    query.addQueryItem(QStringLiteral("body"), body);
    QUrl url(QStringLiteral("mailto:"));
    url.setQuery(query);
    if (!QDesktopServices::openUrl(url)) {
        status->setText(QStringLiteral(
            "Aucun logiciel de messagerie n'a pu s'ouvrir. Le rapport est enregistré ici, vous "
            "pouvez le joindre à un e-mail : %1")
                            .arg(QDir::toNativeSeparators(saved_path)));
    }
}

void Dialog::OpenFolder() {
    const QString directory = ReportsDirectory();
    QDir().mkpath(directory);
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

} // namespace LocalReport
