// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>

// The home screen's background music: three original chiptune loops (tools/music), played on
// repeat. Windows only (PlaySound); elsewhere it stays silent.
class HomeMusic {
public:
    static constexpr int SongCount = 3;

    ~HomeMusic();

    /// Plays song `index` (0-based) on repeat, or keeps playing it.
    void Play(int index);
    void Stop();
    [[nodiscard]] bool IsPlaying() const {
        return playing >= 0;
    }

private:
    QByteArray data; ///< The WAV PlaySound reads while it loops
    int playing = -1;
};
