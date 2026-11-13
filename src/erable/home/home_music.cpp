// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/home/home_music.h"

#include <QFile>
#include <QString>

#ifdef _WIN32
#include <windows.h>

#include <mmsystem.h>
#endif

HomeMusic::~HomeMusic() {
    Stop();
}

void HomeMusic::Play(int index) {
    if (index == playing) {
        return;
    }
    Stop();
#ifdef _WIN32
    QFile file{QStringLiteral(":/sounds/home_%1.wav").arg(index + 1)};
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    data = file.readAll();
    if (PlaySoundA(data.constData(), nullptr, SND_MEMORY | SND_ASYNC | SND_LOOP | SND_NODEFAULT)) {
        playing = index;
    }
#endif
}

void HomeMusic::Stop() {
    if (playing < 0) {
        return;
    }
#ifdef _WIN32
    // Stops the loop before the buffer it reads goes.
    PlaySoundA(nullptr, nullptr, 0);
#endif
    playing = -1;
    data.clear();
}
