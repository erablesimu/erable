// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QDialog>
#include <QString>

class QLabel;
class QListWidget;
class QPushButton;
class QTextBrowser;

/// A tutorial in chapters: what Erable needs (the player's own games, keys and firmware), adding
/// games, playing, the settings for the PC, the controllers, playing online, updates and help.
class TutorialDialog : public QDialog {
    Q_OBJECT

public:
    explicit TutorialDialog(QWidget* parent);
    ~TutorialDialog() override;

private:
    struct Page {
        QString title;
        QString body; ///< Rich text
    };

    void Show(int index);

    std::vector<Page> pages;
    QListWidget* chapters{};
    QTextBrowser* text{};
    QLabel* position{};
    QPushButton* previous{};
    QPushButton* next{};
};
