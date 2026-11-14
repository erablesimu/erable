// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/updater/update_style.h"

#include <algorithm>

#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QVBoxLayout>

namespace UpdateStyle {
namespace {

QString FamilyOf(const QString& file) {
    const int id = QFontDatabase::addApplicationFont(file);
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    return families.isEmpty() ? QStringLiteral("Segoe UI") : families.first();
}

QString Rgba(QColor colour, int alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(colour.red())
        .arg(colour.green())
        .arg(colour.blue())
        .arg(alpha);
}

} // Anonymous namespace

QString DisplayFamily() {
    static const QString family = FamilyOf(QStringLiteral(":/fonts/Fredoka-SemiBold.ttf"));
    return family;
}

QString BodyFamily() {
    static const QString family = FamilyOf(QStringLiteral(":/fonts/Nunito-Bold.ttf"));
    return family;
}

QFont Font(const QString& family, int pixels, int weight) {
    QFont font{family};
    font.setPixelSize(pixels);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

QString StyleSheet(const QString& root) {
    // The application's palette: a widget's own one has the style sheet's translucent highlight.
    const QPalette colours = qApp->palette();
    const QColor window = colours.color(QPalette::Window);
    const QColor text = colours.color(QPalette::WindowText);
    const QColor muted = colours.color(QPalette::PlaceholderText);
    QColor accent = colours.color(QPalette::Highlight);
    accent.setAlpha(255);
    const bool dark = window.lightness() < 128;
    const QColor card = dark ? window.lighter(118) : window.darker(104);
    const QColor accent_text = accent.lightness() > 150 ? QColor(0x1d, 0x1a, 0x17) : Qt::white;
    return QStringLiteral(
               "%1 { background: %2; }"
               "%1 QLabel { color: %3; font-family: \"%6\"; font-size: 14px; }"
               "%1 QLabel#title { font-family: \"%7\"; font-size: 24px; }"
               "%1 QLabel#subtitle, %1 QLabel#detail { color: %13; font-size: 13px; }"
               "%1 QLabel#section { color: %4; font-size: 12px; letter-spacing: 1px; }"
               "%1 QLabel#percent { font-family: \"%7\"; font-size: 18px; color: %5; }"
               "%1 QLabel a { color: %5; }"
               "%1 QTextBrowser { background: %8; color: %3; border: 1px solid %9;"
               " border-radius: 14px; padding: 10px 6px; font-family: \"%6\"; font-size: 14px;"
               " selection-background-color: %10; }"
               "%1 QGroupBox { color: %3; font-family: \"%6\"; border: 1px solid %9;"
               " border-radius: 12px; margin-top: 18px; padding: 10px; }"
               "%1 QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
               "%1 QRadioButton { color: %3; font-family: \"%6\"; font-size: 14px; }"
               "%1 QPushButton { font-family: \"%6\"; font-size: 14px; color: %3;"
               " background: %8; border: 1px solid %9; border-radius: 18px;"
               " padding: 8px 22px; min-height: 20px; }"
               "%1 QPushButton:hover { border-color: %5; }"
               "%1 QPushButton:focus { border-color: %5; }"
               "%1 QPushButton#primary { background: %5; color: %11; border-color: %5; }"
               "%1 QPushButton#primary:hover { background: %12; border-color: %12; }"
               "%1 QPushButton#flat { background: transparent; border: none; color: %4;"
               " padding: 8px 6px; }"
               "%1 QPushButton#flat:hover { color: %3; text-decoration: underline; }"
               "%1 QProgressBar { background: %8; border: none; border-radius: 6px;"
               " min-height: 12px; max-height: 12px; }"
               "%1 QProgressBar::chunk { border-radius: 6px; background: qlineargradient("
               "x1:0, y1:0, x2:1, y2:0, stop:0 %12, stop:1 %5); }"
               "%1 QScrollBar:vertical { background: transparent; width: 10px; margin: 6px 2px; }"
               "%1 QScrollBar::handle:vertical { background: %9; border-radius: 3px;"
               " min-height: 30px; }"
               "%1 QScrollBar::add-line:vertical, %1 QScrollBar::sub-line:vertical"
               " { height: 0; }")
        .arg(root, window.name(), text.name(), muted.name(), accent.name(), BodyFamily(),
             DisplayFamily(), card.name(), Rgba(text, dark ? 38 : 46), Rgba(accent, 120),
             accent_text.name())
        .arg(accent.darker(dark ? 125 : 115).name(), Rgba(text, 175));
}

void StyleNotes(QTextDocument* document) {
    QColor accent = qApp->palette().color(QPalette::Highlight);
    accent.setAlpha(255);
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor{block};
        if (QTextList* list = block.textList()) {
            QTextListFormat format = list->format();
            if (format.indent() > 1) {
                format.setIndent(1);
                list->setFormat(format);
            }
            continue;
        }
        // A heading, or a paragraph that is all bold ("**Tous les jeux**")
        bool all_bold = block.blockFormat().headingLevel() > 0;
        if (!all_bold && !block.text().trimmed().isEmpty()) {
            all_bold = true;
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                if (fragment.isValid() && !fragment.text().trimmed().isEmpty() &&
                    fragment.charFormat().fontWeight() < QFont::DemiBold) {
                    all_bold = false;
                    break;
                }
            }
        }
        if (!all_bold) {
            continue;
        }
        cursor.select(QTextCursor::BlockUnderCursor);
        QTextCharFormat format;
        format.setForeground(accent);
        format.setFontFamilies({DisplayFamily()});
        format.setFontPointSize(0);
        format.setProperty(QTextFormat::FontPixelSize, 17);
        cursor.mergeCharFormat(format);
        QTextBlockFormat spacing = block.blockFormat();
        spacing.setTopMargin(block == document->begin() ? 0 : 10);
        spacing.setBottomMargin(4);
        cursor.mergeBlockFormat(spacing);
    }
}

QLabel* MakeIcon(QWidget* parent, int size) {
    auto* icon = new QLabel(parent);
    icon->setPixmap(qApp->windowIcon().pixmap(size, size));
    icon->setFixedSize(size, size);
    return icon;
}

} // namespace UpdateStyle

UpdateProgress::UpdateProgress(const QString& title, QWidget* parent) : QDialog{parent} {
    setWindowTitle(QStringLiteral("Erable"));
    setModal(true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(460);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(12);

    auto* header = new QHBoxLayout;
    header->setSpacing(14);
    header->addWidget(UpdateStyle::MakeIcon(this, 40));
    title_label = new QLabel(title, this);
    title_label->setObjectName(QStringLiteral("title"));
    title_label->setStyleSheet(QStringLiteral("font-size: 20px;"));
    header->addWidget(title_label, 1);
    percent_label = new QLabel(QStringLiteral("0 %"), this);
    percent_label->setObjectName(QStringLiteral("percent"));
    header->addWidget(percent_label);
    layout->addLayout(header);

    bar = new QProgressBar(this);
    bar->setRange(0, 1000);
    bar->setValue(0);
    bar->setTextVisible(false);
    layout->addWidget(bar);

    auto* footer = new QHBoxLayout;
    detail_label = new QLabel(tr("Starting..."), this);
    detail_label->setObjectName(QStringLiteral("detail"));
    footer->addWidget(detail_label, 1);
    cancel_button = new QPushButton(tr("Cancel"), this);
    cancel_button->setCursor(Qt::PointingHandCursor);
    footer->addWidget(cancel_button);
    layout->addLayout(footer);
    connect(cancel_button, &QPushButton::clicked, this, &UpdateProgress::reject);

    setStyleSheet(UpdateStyle::StyleSheet(QStringLiteral("UpdateProgress")));
    clock.start();
}

void UpdateProgress::SetTitle(const QString& title) {
    title_label->setText(title);
}

void UpdateProgress::SetPercent(int percent) {
    percent = std::clamp(percent, 0, 100);
    bar->setValue(percent * 10);
    percent_label->setText(QStringLiteral("%1 %").arg(percent));
    detail_label->setText(percent >= 100 ? tr("Almost done...") : tr("Please wait..."));
}

void UpdateProgress::SetBytes(u64 done, u64 total) {
    const QLocale locale;
    const auto megabytes = [&](u64 bytes) {
        return locale.toString(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1);
    };
    const double seconds = static_cast<double>(clock.elapsed()) / 1000.0;
    const double speed = seconds > 0.5 ? static_cast<double>(done) / seconds : 0.0;
    QString detail;
    if (total != 0) {
        const int permille = static_cast<int>((done * 1000) / total);
        bar->setValue(permille);
        percent_label->setText(QStringLiteral("%1 %").arg(permille / 10));
        detail = tr("%1 MB of %2 MB").arg(megabytes(done), megabytes(total));
    } else {
        bar->setRange(0, 0);
        detail = tr("%1 MB").arg(megabytes(done));
    }
    if (speed > 0) {
        detail += QStringLiteral("  ·  ") +
                  tr("%1 MB/s").arg(locale.toString(speed / (1024.0 * 1024.0), 'f', 1));
        if (total > done) {
            const int left = static_cast<int>(static_cast<double>(total - done) / speed) + 1;
            detail += QStringLiteral("  ·  ") +
                      (left >= 60 ? tr("%n min left", nullptr, (left + 59) / 60)
                                  : tr("%n s left", nullptr, left));
        }
    }
    detail_label->setText(detail);
}

void UpdateProgress::Restart() {
    clock.restart();
    bar->setRange(0, 1000);
    bar->setValue(0);
    percent_label->setText(QStringLiteral("0 %"));
}

void UpdateProgress::reject() {
    // Like QProgressDialog: the caller sees it and stops; the window goes when the caller is done.
    canceled = true;
    cancel_button->setEnabled(false);
    detail_label->setText(tr("Cancelling..."));
}
