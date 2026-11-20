// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/tutorial_dialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

TutorialDialog::TutorialDialog(QWidget* parent) : QDialog{parent} {
    setWindowTitle(tr("Erable tutorial"));
    resize(860, 560);

    pages = {
        {tr("Welcome to Erable"),
         tr("<p>Erable plays Nintendo Switch and Nintendo Switch 2 games on your PC.</p><p>It needs "
            "three things that come from <b>your own console</b>: your games, your keys and your "
            "firmware. Erable gives none of them: dump them from a console you own.</p><p>This "
            "tutorial walks through adding your games, playing, the settings for your PC, the "
            "controllers, playing online with friends, and getting help. You can open it again from "
            "<b>Help › Tutorial</b> or the <b>Tutorial</b> button of the home.</p>")},
        {tr("Your games"),
         tr("<p>Put your game files in your games folder: the <b>My games</b> button of the home "
            "opens it. They show up on the home on their own.</p><ul><li>Updates and DLC go in the "
            "same folder: Erable joins them to their game.</li><li>Extract archives (.zip, .7z, "
            ".rar) first.</li><li>A file OneDrive keeps online only must be downloaded first "
            "(right-click it, <i>Always keep on this device</i>).</li></ul><p>When a file cannot be "
            "shown, the home says why under its list.</p>")},
        {tr("Keys and firmware"),
         tr("<p>Games are encrypted: Erable reads them with the keys (<b>prod.keys</b>) and the "
            "firmware dumped from your console.</p><ul><li><b>Tools › Install Decryption Keys</b> "
            "takes your prod.keys.</li><li><b>Tools › Install Firmware</b> takes your firmware, from "
            "a folder or a .zip.</li></ul><p>A recent game may need keys dumped from a recent "
            "firmware: if a game does not start, dump your keys and firmware again after updating "
            "your console.</p>")},
        {tr("Playing"),
         tr("<p>On the home, pick a game with the mouse, the arrows or a controller, "
            "then:</p><ul><li><b>Play</b> (A) starts it;</li><li><b>Game settings</b> (+) sets this "
            "game apart from the others;</li><li><b>Options</b> (X) holds favourites and "
            "more;</li><li><b>Report a problem</b> (−) sends what we need to fix "
            "it.</li></ul><p><b>F11</b> switches between full screen and a window. The first time, a "
            "game prepares its shaders and can stutter a little: it goes away the next times.</p>")},
        {tr("Settings for your PC"),
         tr("<p><b>Help › Apply Recommended Settings</b> looks at your graphics card, processor and "
            "memory, and picks the resolution, the console mode, the upscaling and the filtering "
            "that suit them. <i>Restore my previous settings</i> puts yours back.</p><p>If Erable "
            "stutters on a capable PC, make Windows' page file larger (automatic, or at least 16 GB) "
            "and close the programs that use a lot of memory.</p>")},
        {tr("Controllers"),
         tr("<p>Plug in or pair your controller: Erable finds it on its own, and the top of the home "
            "says when one is connected.</p><p>To change the buttons, or to set up a controller for "
            "each player, open <b>Settings</b> on the home, then <b>Controls</b>.</p>")},
        {tr("Playing online"),
         tr("<p>The button at the top of the home shows your online state: <b>red</b> offline, "
            "<b>green</b> online. It opens the <b>Online</b> panel.</p><ul><li><b>Go online</b> with "
            "your Erable account. Your nickname must hold no insult.</li><li><b>Rooms</b>: the rooms "
            "of the Erable server, sorted by game, with their places. Join the room of your game, "
            "then use the game's local wireless mode: the other players of the room show up as if "
            "they were next to you. When the rooms of a game are full, open one "
            "more.</li><li><b>Friends</b>: add a friend with their friend code, join their room, and "
            "write to them (messages are kept 15 days).</li><li><b>Alerts</b>: the friend "
            "requests.</li></ul><p>Mods and cheats are not allowed in the rooms: Erable offers to "
            "pause them in one click. The games' official online modes need Nintendo's servers, "
            "which Erable does not reach.</p>")},
        {tr("Updates and help"),
         tr("<ul><li>When a new version of Erable is out, the <b>Update</b> button of the home gets "
            "a red dot. <b>Help › Check for Updates</b> looks for one at any time.</li><li><b>What's "
            "new</b> on the home tells what each version changed.</li><li>A game misbehaves? "
            "<b>Report a problem</b> (−) sends its description with the logs we need. <b>Debug mode "
            "› Record my session</b> keeps a fuller log to join to a report.</li></ul><p>Have "
            "fun!</p>")},
    };

    auto* layout = new QVBoxLayout(this);
    auto* body = new QHBoxLayout;
    chapters = new QListWidget(this);
    chapters->setFixedWidth(230);
    for (const Page& page : pages) {
        chapters->addItem(page.title);
    }
    text = new QTextBrowser(this);
    text->setOpenLinks(false);
    body->addWidget(chapters);
    body->addWidget(text, 1);
    layout->addLayout(body, 1);

    auto* bottom = new QHBoxLayout;
    position = new QLabel(this);
    previous = new QPushButton(tr("Previous"), this);
    next = new QPushButton(this);
    auto* close = new QPushButton(tr("Close"), this);
    bottom->addWidget(position, 1);
    bottom->addWidget(previous);
    bottom->addWidget(next);
    bottom->addWidget(close);
    layout->addLayout(bottom);

    connect(chapters, &QListWidget::currentRowChanged, this, &TutorialDialog::Show);
    connect(previous, &QPushButton::clicked, this,
            [this] { chapters->setCurrentRow(chapters->currentRow() - 1); });
    connect(next, &QPushButton::clicked, this, [this] {
        if (chapters->currentRow() + 1 < static_cast<int>(pages.size())) {
            chapters->setCurrentRow(chapters->currentRow() + 1);
        } else {
            accept();
        }
    });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    chapters->setCurrentRow(0);
    next->setFocus();
}

TutorialDialog::~TutorialDialog() = default;

void TutorialDialog::Show(int index) {
    if (index < 0 || index >= static_cast<int>(pages.size())) {
        return;
    }
    const Page& page = pages[static_cast<std::size_t>(index)];
    text->setHtml(QStringLiteral("<h2>%1</h2>%2").arg(page.title.toHtmlEscaped(), page.body));
    position->setText(tr("%1 of %2").arg(index + 1).arg(pages.size()));
    previous->setEnabled(index > 0);
    next->setText(index + 1 < static_cast<int>(pages.size()) ? tr("Next") : tr("Finish"));
}
