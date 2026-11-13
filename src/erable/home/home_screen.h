// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "common/common_types.h"
#include "erable/home/home_music.h"

class ControllerNavigation;
class GameList;
class GameTile;
class HintBar;
class PillButton;
class QHBoxLayout;
class QLabel;
class QMenu;
class QScrollArea;

namespace Core {
class System;
}
namespace PlayTime {
class PlayTimeManager;
}

/// Erable's console-style home: the player's profile, one row of large game tiles, the selected
/// game's actions, a dock of round buttons and the home music. It reads the games GameList found.
class HomeScreen : public QWidget {
    Q_OBJECT

public:
    explicit HomeScreen(GameList* game_list, PlayTime::PlayTimeManager& play_time,
                        Core::System& system, QWidget* parent = nullptr);
    ~HomeScreen() override;

    /// The Debug mode menu the dock's button opens.
    void SetDebugMenu(QMenu* menu);

    /// Shows or hides the home, with its music.
    void SetActive(bool active);
    /// Stops the music while a game runs.
    void StopMusic();

    /// The folder Erable reads the player's games from first (Documents\Erable\Jeux).
    [[nodiscard]] static QString GamesFolder();

signals:
    void LaunchGame(const QString& path, u64 program_id);
    void OpenGameSettings(const QString& path);
    void ShowLibrary();
    void OpenSettings();
    void OpenWhatsNew();
    void OpenCaptures();
    void ReportProblem();
    /// The music was muted, unmuted or changed: saved at once, so a crash or a forced close keeps it.
    void MusicSettingsChanged();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    struct Game {
        QString title;
        QString path;
        u64 program_id = 0;
        QPixmap cover;
        bool favorite = false;
    };

    void CollectGames();
    void RebuildTiles();
    void Select(int index);
    void UpdateSelection();
    void UpdateProfile();
    /// Asks for the player's name while the current profile has none of its own (or always,
    /// when requested from the profile button).
    void EnsureProfile(bool requested = false);
    void UpdateMusic();
    void UpdateStatus();
    void Launch();
    void ToggleFavorite();
    void OpenGameSettingsOfSelected();
    void OpenGamesFolder();

    GameList* game_list;
    PlayTime::PlayTimeManager& play_time;
    Core::System& system;
    QMenu* debug_menu = nullptr;
    ControllerNavigation* controller = nullptr;

    std::vector<Game> games;
    std::vector<GameTile*> tiles;
    int selected = 0;
    bool active = false;
    bool profile_checked = false;

    PillButton* profile_button = nullptr;
    PillButton* music_button = nullptr;
    PillButton* song_button = nullptr;
    QLabel* controller_label = nullptr;
    QLabel* clock = nullptr;
    QLabel* title = nullptr;
    QLabel* meta = nullptr;
    QScrollArea* scroll = nullptr;
    QHBoxLayout* row = nullptr;
    PillButton* play_button = nullptr;
    PillButton* settings_button = nullptr;
    PillButton* favorite_button = nullptr;
    PillButton* report_button = nullptr;
    HintBar* hints = nullptr;
    QTimer status_timer;
    QTimer refresh_timer;
    HomeMusic music;
};
