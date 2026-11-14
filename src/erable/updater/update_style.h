// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QFont>
#include <QString>

#include "common/common_types.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QTextDocument;

namespace UpdateStyle {

/// Erable's headline and body typefaces (Fredoka and Nunito), loaded once.
[[nodiscard]] QString DisplayFamily();
[[nodiscard]] QString BodyFamily();
[[nodiscard]] QFont Font(const QString& family, int pixels, int weight = QFont::Bold);

/// The style sheet of the updater's windows, in the theme's colours. `root` is the selector of
/// the window it is set on. Buttons named "primary" are filled with the accent.
[[nodiscard]] QString StyleSheet(const QString& root);

/// Gives release notes (Markdown already set on `document`) the theme's look: the bold lines that
/// head each group in the accent and the headline face, and the bullets less indented.
void StyleNotes(QTextDocument* document);

/// The app's maple leaf at `size` pixels, for the windows' headers.
[[nodiscard]] QLabel* MakeIcon(QWidget* parent, int size);

} // namespace UpdateStyle

/// The download and unpacking progress, in the updater's look: a title, a bar with the percent,
/// how much is done (with the speed and the time left for a download), and a Cancel button.
/// The caller keeps the event loop turning (QGuiApplication::processEvents), as with a
/// QProgressDialog.
class UpdateProgress : public QDialog {
    Q_OBJECT

public:
    explicit UpdateProgress(const QString& title, QWidget* parent = nullptr);

    void SetTitle(const QString& title);
    /// Percent done, 0..100, with no byte counts (unpacking).
    void SetPercent(int percent);
    /// Bytes received out of `total` (0 when unknown); also shows the speed and the time left.
    void SetBytes(u64 done, u64 total);
    /// Starts the speed over (a new download attempt).
    void Restart();

    [[nodiscard]] bool WasCanceled() const {
        return canceled;
    }

protected:
    void reject() override;

private:
    QLabel* title_label = nullptr;
    QLabel* percent_label = nullptr;
    QLabel* detail_label = nullptr;
    QProgressBar* bar = nullptr;
    QPushButton* cancel_button = nullptr;
    QElapsedTimer clock;
    bool canceled = false;
};
