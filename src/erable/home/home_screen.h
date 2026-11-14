// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <set>
#include <vector>

#include <QElapsedTimer>

#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "common/common_types.h"
#include "common/uuid.h"
#include "erable/home/home_music.h"

class ControllerNavigation;
class GameList;
class CaseShelf;
class PillButton;
class QNetworkAccessManager;
class QLabel;
class QLineEdit;
class QMenu;

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

    /// Takes the colours of the theme just applied (the palette MainWindow set).
    void RefreshTheme();

    /// Whether the window fills the screen: the full screen button says how to leave it.
    void SetFullscreen(bool fullscreen);

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
    /// The music or the shelf style changed: saved at once, so a crash or a forced close keeps it.
    void HomeSettingsChanged();
    /// The theme button picked the next theme (in UISettings): MainWindow applies it.
    void ThemeChangeRequested();
    /// The full screen button (or F11): MainWindow switches the window.
    void FullscreenRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
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
    /// Puts the downloaded covers on the cases, and fetches the missing ones.
    void FetchCovers();
    void Select(int index);
    void UpdateSelection();
    void UpdateProfile();
    /// "Who's playing?": pick, add, rename or delete a profile.
    void ChooseProfile();
    /// Asks for the player's name while the current profile has none of its own (or always,
    /// when requested from the profile button).
    void EnsureProfile(bool requested = false);
    void UpdateMusic();
    void UpdateStyle();
    void UpdateTheme();
    void ApplyStyleSheet();
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
    int selected = 0;
    bool active = false;
    bool profile_checked = false;
    Common::UUID shown_profile{}; ///< Whose picture the profile button shows

    PillButton* profile_button = nullptr;
    PillButton* music_button = nullptr;
    PillButton* song_button = nullptr;
    PillButton* style_button = nullptr;
    PillButton* theme_button = nullptr;
    PillButton* fullscreen_button = nullptr;
    QLabel* controller_label = nullptr;
    QLabel* clock = nullptr;
    QLabel* title = nullptr;
    QLabel* meta = nullptr;
    CaseShelf* shelf = nullptr;
    PillButton* play_button = nullptr;
    PillButton* settings_button = nullptr;
    PillButton* favorite_button = nullptr;
    PillButton* report_button = nullptr;
    QLineEdit* search = nullptr;
    QTimer status_timer;
    QTimer refresh_timer;
    QTimer ambient_timer;
    QElapsedTimer ambient_clock;
    QNetworkAccessManager* covers = nullptr;
    std::set<u64> covers_requested;
    HomeMusic music;
};
