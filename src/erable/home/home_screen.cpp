// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/home/home_screen.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>

#include <QAbstractButton>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "erable/game/game_list.h"
#include "erable/util/controller_navigation.h"
#include "frontend_common/play_time_manager.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"

namespace {

constexpr QColor Maple{0xf0, 0x70, 0x2f};

/// Names Erable and its upstreams gave the profile they make on their own.
bool IsDefaultProfileName(const QString& name) {
    return name.isEmpty() || name == QStringLiteral("Eden") || name == QStringLiteral("Joueur") ||
           name == QStringLiteral("Player");
}

QString ProfileName(const Service::Account::ProfileManager& manager, Common::UUID uuid) {
    Service::Account::ProfileBase profile{};
    if (!manager.GetProfileBase(uuid, profile)) {
        return {};
    }
    return QString::fromStdString(Common::StringFromFixedZeroTerminatedBuffer(
        reinterpret_cast<const char*>(profile.username.data()), profile.username.size()));
}

} // namespace

/// One game: its picture with rounded corners and its title, ringed in maple when selected.
class GameTile : public QAbstractButton {
public:
    GameTile(const QString& title, const QPixmap& cover, bool favorite, bool add,
             QWidget* parent)
        : QAbstractButton{parent}, cover_{cover}, favorite_{favorite}, add_{add} {
        setText(title);
        setAccessibleName(title);
        setFixedSize(TileSize + 2 * Ring, TileSize + 2 * Ring);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

    void SetSelected(bool selected) {
        selected_ = selected;
        update();
    }

    static constexpr int TileSize = 196;
    static constexpr int Ring = 9;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF tile = QRectF(Ring, Ring, TileSize, TileSize);
        constexpr qreal Radius = 22;
        if (selected_) {
            painter.setPen(QPen{Maple, 4});
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(tile.adjusted(-6, -6, 6, 6), Radius + 6, Radius + 6);
        }
        QPainterPath shape;
        shape.addRoundedRect(tile, Radius, Radius);
        if (add_) {
            painter.setPen(QPen{palette().color(QPalette::Mid), 3, Qt::DashLine});
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(shape);
            painter.setPen(palette().color(QPalette::PlaceholderText));
            QFont font = painter.font();
            font.setPointSizeF(font.pointSizeF() * 3.2);
            painter.setFont(font);
            painter.drawText(tile.adjusted(0, 0, 0, -36), Qt::AlignCenter, QStringLiteral("+"));
            font.setPointSizeF(font.pointSizeF() / 3.2 * 1.05);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(tile.adjusted(12, 70, -12, 0), Qt::AlignCenter | Qt::TextWordWrap,
                             text());
            return;
        }
        painter.setClipPath(shape);
        if (cover_.isNull()) {
            QLinearGradient fill{tile.topLeft(), tile.bottomRight()};
            fill.setColorAt(0, Maple.lighter(120));
            fill.setColorAt(1, Maple.darker(220));
            painter.fillRect(tile, fill);
        } else {
            painter.drawPixmap(tile.toRect(), cover_);
        }
        // The title, on a shade that keeps it readable over any picture
        QLinearGradient shade{tile.bottomLeft() - QPointF(0, 76), tile.bottomLeft()};
        shade.setColorAt(0, QColor{0, 0, 0, 0});
        shade.setColorAt(1, QColor{0, 0, 0, 190});
        painter.fillRect(QRectF(tile.left(), tile.bottom() - 76, tile.width(), 76), shade);
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(tile.adjusted(12, 0, -12, -10), Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap,
                         text());
        if (favorite_) {
            const QRectF badge{tile.right() - 34, tile.top() + 8, 26, 26};
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor{0, 0, 0, 110});
            painter.drawEllipse(badge);
            painter.setPen(QColor{0xff, 0xd3, 0x6b});
            painter.drawText(badge, Qt::AlignCenter, QStringLiteral("★"));
        }
    }

private:
    QPixmap cover_;
    bool favorite_;
    bool add_;
    bool selected_ = false;
};

HomeScreen::HomeScreen(GameList* game_list_, PlayTime::PlayTimeManager& play_time_,
                       Core::System& system_, QWidget* parent)
    : QWidget{parent}, game_list{game_list_}, play_time{play_time_}, system{system_} {
    setObjectName(QStringLiteral("HomeScreen"));
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(QStringLiteral(
        "#HomeScreen QPushButton { border-radius: 18px; padding: 8px 18px; font-weight: 600; }"
        "#HomeScreen QPushButton#PlayButton { background: #f0702f; color: #1a120d; border: none; }"
        "#HomeScreen QPushButton#PlayButton:hover { background: #ff8a4a; }"
        "#HomeScreen QPushButton[dock=\"true\"] { border-radius: 29px; min-width: 58px; max-width: 58px;"
        " min-height: 58px; max-height: 58px; padding: 0; font-size: 22px; }"
        "#HomeScreen QLabel#HomeTitle { font-size: 26px; font-weight: 700; }"
        "#HomeScreen QLabel#HomeClock { font-size: 20px; font-weight: 700; }"
        "#HomeScreen QScrollArea { background: transparent; border: none; }"
        "#HomeScreen QLabel[hint=\"true\"] { color: palette(placeholder-text); }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 18, 28, 12);
    layout->setSpacing(14);

    // Top bar: profile, the Erable name, music, clock
    auto* top = new QHBoxLayout;
    profile_button = new QPushButton(this);
    profile_button->setToolTip(tr("Your profile"));
    connect(profile_button, &QPushButton::clicked, this, [this] {
        profile_checked = false;
        EnsureProfile(true);
    });
    top->addWidget(profile_button);
    top->addStretch();
    auto* brand = new QLabel(QStringLiteral("\U0001F341 Erable"), this);
    brand->setObjectName(QStringLiteral("HomeClock"));
    top->addWidget(brand);
    top->addStretch();
    music_button = new QPushButton(this);
    music_button->setCheckable(true);
    music_button->setToolTip(tr("Home music"));
    connect(music_button, &QPushButton::toggled, this, [this](bool on) {
        UISettings::values.home_music = on;
        UpdateMusic();
    });
    top->addWidget(music_button);
    song_button = new QPushButton(this);
    song_button->setToolTip(tr("Next tune"));
    connect(song_button, &QPushButton::clicked, this, [this] {
        UISettings::values.home_song =
            (UISettings::values.home_song.GetValue() + 1) % HomeMusic::SongCount;
        UISettings::values.home_music = true;
        music_button->setChecked(true);
        music.Stop();
        UpdateMusic();
    });
    top->addWidget(song_button);
    clock = new QLabel(this);
    clock->setObjectName(QStringLiteral("HomeClock"));
    top->addSpacing(12);
    top->addWidget(clock);
    layout->addLayout(top);

    // The selected game
    title = new QLabel(this);
    title->setObjectName(QStringLiteral("HomeTitle"));
    title->setWordWrap(true);
    meta = new QLabel(this);
    meta->setProperty("hint", true);
    layout->addSpacing(8);
    layout->addWidget(title);
    layout->addWidget(meta);

    // The row of tiles
    scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(GameTile::TileSize + 2 * GameTile::Ring + 24);
    scroll->setFocusPolicy(Qt::NoFocus);
    auto* row_widget = new QWidget(scroll);
    row = new QHBoxLayout(row_widget);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(14);
    scroll->setWidget(row_widget);
    layout->addWidget(scroll);

    // The selected game's actions
    auto* actions = new QHBoxLayout;
    play_button = new QPushButton(QStringLiteral("Ⓐ  ") + tr("Play"), this);
    play_button->setObjectName(QStringLiteral("PlayButton"));
    connect(play_button, &QPushButton::clicked, this, &HomeScreen::Launch);
    settings_button = new QPushButton(tr("Game settings"), this);
    connect(settings_button, &QPushButton::clicked, this, [this] {
        if (selected < static_cast<int>(games.size())) {
            emit OpenGameSettings(games[selected].path);
        }
    });
    favorite_button = new QPushButton(this);
    connect(favorite_button, &QPushButton::clicked, this, &HomeScreen::ToggleFavorite);
    report_button = new QPushButton(tr("Report a problem"), this);
    connect(report_button, &QPushButton::clicked, this, &HomeScreen::ReportProblem);
    for (auto* button : {play_button, settings_button, favorite_button, report_button}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        actions->addWidget(button);
    }
    actions->addStretch();
    layout->addLayout(actions);
    layout->addStretch();

    // Dock of round buttons
    auto* dock = new QHBoxLayout;
    dock->addStretch();
    const auto add_dock = [this, dock](const QString& glyph, const QString& label, auto&& action) {
        auto* column = new QVBoxLayout;
        auto* button = new QPushButton(glyph, this);
        button->setProperty("dock", true);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(label);
        connect(button, &QPushButton::clicked, this, action);
        auto* text = new QLabel(label, this);
        text->setAlignment(Qt::AlignCenter);
        text->setProperty("hint", true);
        column->addWidget(button, 0, Qt::AlignHCenter);
        column->addWidget(text);
        dock->addLayout(column);
        dock->addSpacing(14);
        return button;
    };
    add_dock(QStringLiteral("▦"), tr("Library"), [this] { emit ShowLibrary(); });
    add_dock(QStringLiteral("✨"), tr("What's new"), [this] { emit OpenWhatsNew(); });
    add_dock(QStringLiteral("\U0001F4C1"), tr("Games folder"), [this] { OpenGamesFolder(); });
    add_dock(QStringLiteral("\U0001F4F7"), tr("Captures"), [this] { emit OpenCaptures(); });
    QPushButton* const debug = add_dock(QStringLiteral("\U0001F41E"), tr("Debug mode"), [] {});
    connect(debug, &QPushButton::clicked, this, [this, debug] {
        if (debug_menu) {
            debug_menu->popup(debug->mapToGlobal(QPoint(0, -debug_menu->sizeHint().height())));
        }
    });
    add_dock(QStringLiteral("⚙"), tr("Settings"), [this] { emit OpenSettings(); });
    dock->addStretch();
    layout->addLayout(dock);

    auto* hints = new QLabel(
        QStringLiteral("◀ ▶  %1     Ⓐ  %2     %3").arg(tr("Choose"), tr("Play"), tr("Put your games in the Games folder")), this);
    hints->setProperty("hint", true);
    layout->addWidget(hints);

    controller = new ControllerNavigation(system.HIDCore(), this);
    connect(controller, &ControllerNavigation::TriggerKeyboardEvent, this, [this](Qt::Key key) {
        if (!isVisible() || !isActiveWindow() || system.IsPoweredOn()) {
            return;
        }
        QKeyEvent event{QEvent::KeyPress, key, Qt::NoModifier};
        keyPressEvent(&event);
    });

    clock_timer.setInterval(15000);
    connect(&clock_timer, &QTimer::timeout, this, [this] {
        clock->setText(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));
    });
    clock->setText(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));
    clock_timer.start();

    // The game list fills its model as it scans: rebuild once it settles.
    refresh_timer.setSingleShot(true);
    refresh_timer.setInterval(300);
    connect(&refresh_timer, &QTimer::timeout, this, [this] {
        CollectGames();
        RebuildTiles();
    });
    QStandardItemModel* model = game_list->GetModel();
    for (const auto signal : {&QStandardItemModel::rowsInserted, &QStandardItemModel::rowsRemoved}) {
        connect(model, signal, this, [this] { refresh_timer.start(); });
    }
    connect(model, &QStandardItemModel::modelReset, this, [this] { refresh_timer.start(); });
    connect(model, &QStandardItemModel::dataChanged, this, [this] { refresh_timer.start(); });

    music_button->setChecked(UISettings::values.home_music.GetValue());
    UpdateProfile();
    CollectGames();
    RebuildTiles();
}

HomeScreen::~HomeScreen() = default;

QString HomeScreen::GamesFolder() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QStringLiteral("Erable/Jeux"));
}

void HomeScreen::SetDebugMenu(QMenu* menu) {
    debug_menu = menu;
}

void HomeScreen::SetActive(bool active_) {
    active = active_;
    setVisible(active);
    if (active) {
        setFocus();
    }
    UpdateMusic();
}

void HomeScreen::StopMusic() {
    music.Stop();
}

void HomeScreen::UpdateMusic() {
    const bool on = UISettings::values.home_music.GetValue();
    const int song = static_cast<int>(UISettings::values.home_song.GetValue());
    music_button->setText(QStringLiteral("♪ ") + (on ? tr("Music") : tr("Muted")));
    song_button->setText(tr("%1/%2").arg(song + 1).arg(HomeMusic::SongCount));
    if (on && active && isVisible() && !system.IsPoweredOn()) {
        music.Play(song);
    } else {
        music.Stop();
    }
}

void HomeScreen::CollectGames() {
    games.clear();
    std::set<QString> seen;
    const QStandardItemModel* model = game_list->GetModel();
    const auto visit = [&](auto&& self, const QModelIndex& parent) -> void {
        for (int i = 0; i < model->rowCount(parent); ++i) {
            const QModelIndex index = model->index(i, 0, parent);
            const QStandardItem* item = model->itemFromIndex(index);
            if (!item) {
                continue;
            }
            if (item->data(GameListItem::TypeRole).value<GameListItemType>() ==
                GameListItemType::Game) {
                const QString path = item->data(GameListItemPath::FullPathRole).toString();
                if (path.isEmpty() || !seen.insert(path).second) {
                    continue;
                }
                Game game;
                game.path = path;
                game.title = item->data(GameListItemPath::TitleRole).toString();
                if (game.title.isEmpty()) {
                    game.title = QFileInfo(path).completeBaseName();
                }
                game.program_id = item->data(GameListItemPath::ProgramIdRole).toULongLong();
                game.cover = item->data(GameListItemPath::CoverRole).value<QPixmap>();
                if (game.cover.isNull()) {
                    game.cover = item->data(Qt::DecorationRole).value<QPixmap>();
                }
                game.favorite = UISettings::values.favorited_ids.contains(game.program_id);
                games.push_back(std::move(game));
            }
            if (model->hasChildren(index)) {
                self(self, index);
            }
        }
    };
    visit(visit, QModelIndex{});
    // Favourites first, then the games played most
    std::stable_sort(games.begin(), games.end(), [this](const Game& a, const Game& b) {
        if (a.favorite != b.favorite) {
            return a.favorite;
        }
        return play_time.GetPlayTime(a.program_id) > play_time.GetPlayTime(b.program_id);
    });
}

void HomeScreen::RebuildTiles() {
    QString selected_path;
    if (selected < static_cast<int>(games.size())) {
        selected_path = games[selected].path;
    }
    for (GameTile* tile : tiles) {
        tile->deleteLater();
    }
    tiles.clear();
    while (QLayoutItem* item = row->takeAt(0)) {
        delete item;
    }
    for (std::size_t i = 0; i < games.size(); ++i) {
        auto* tile = new GameTile(games[i].title, games[i].cover, games[i].favorite, false, this);
        connect(tile, &QAbstractButton::clicked, this, [this, i] {
            if (selected == static_cast<int>(i)) {
                Launch();
            } else {
                Select(static_cast<int>(i));
            }
        });
        row->addWidget(tile);
        tiles.push_back(tile);
    }
    auto* add = new GameTile(tr("Add games"), {}, false, true, this);
    connect(add, &QAbstractButton::clicked, this, &HomeScreen::OpenGamesFolder);
    row->addWidget(add);
    tiles.push_back(add);
    row->addStretch();

    selected = 0;
    for (std::size_t i = 0; i < games.size(); ++i) {
        if (games[i].path == selected_path) {
            selected = static_cast<int>(i);
        }
    }
    UpdateSelection();
}

void HomeScreen::Select(int index) {
    const int count = static_cast<int>(tiles.size());
    selected = std::clamp(index, 0, count - 1);
    UpdateSelection();
}

void HomeScreen::UpdateSelection() {
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        tiles[i]->SetSelected(static_cast<int>(i) == selected);
    }
    if (selected < static_cast<int>(tiles.size())) {
        scroll->ensureWidgetVisible(tiles[selected], 60, 0);
    }
    const bool is_game = selected < static_cast<int>(games.size());
    for (auto* button : {play_button, settings_button, favorite_button}) {
        button->setEnabled(is_game);
    }
    if (!is_game) {
        title->setText(games.empty() ? tr("Welcome to Erable") : tr("Add games"));
        meta->setText(tr("Put your games in %1 and they show up here.")
                          .arg(QDir::toNativeSeparators(GamesFolder())));
        favorite_button->setText(tr("Add to favourites"));
        return;
    }
    const Game& game = games[selected];
    title->setText(game.title);
    const u64 seconds = play_time.GetPlayTime(game.program_id);
    meta->setText(seconds == 0
                      ? tr("Never played")
                      : tr("Played for %1").arg(QString::fromStdString(
                            PlayTime::PlayTimeManager::GetReadablePlayTime(seconds))));
    favorite_button->setText(game.favorite ? QStringLiteral("★ ") + tr("Remove from favourites")
                                           : QStringLiteral("☆ ") + tr("Add to favourites"));
}

void HomeScreen::Launch() {
    if (selected < static_cast<int>(games.size())) {
        music.Stop();
        emit LaunchGame(games[selected].path, games[selected].program_id);
    } else {
        OpenGamesFolder();
    }
}

void HomeScreen::ToggleFavorite() {
    if (selected >= static_cast<int>(games.size())) {
        return;
    }
    Game& game = games[selected];
    if (game.favorite) {
        UISettings::values.favorited_ids.removeOne(game.program_id);
    } else if (!UISettings::values.favorited_ids.contains(game.program_id)) {
        UISettings::values.favorited_ids.append(game.program_id);
    }
    game.favorite = !game.favorite;
    const QString path = game.path;
    CollectGames();
    RebuildTiles();
    for (std::size_t i = 0; i < games.size(); ++i) {
        if (games[i].path == path) {
            Select(static_cast<int>(i));
        }
    }
}

void HomeScreen::OpenGamesFolder() {
    QDir{}.mkpath(GamesFolder());
    QDesktopServices::openUrl(QUrl::fromLocalFile(GamesFolder()));
}

void HomeScreen::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left:
        Select(selected - 1);
        break;
    case Qt::Key_Right:
        Select(selected + 1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        Launch();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void HomeScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // After the window is up, so the profile dialog opens over it
    QTimer::singleShot(0, this, [this] { EnsureProfile(); });
    UpdateMusic();
}

void HomeScreen::UpdateProfile() {
    auto& manager = system.GetProfileManager();
    const auto user = manager.GetUser(static_cast<std::size_t>(Settings::values.current_user.GetValue()));
    const QString name = user ? ProfileName(manager, *user) : QString{};
    profile_button->setText(QStringLiteral("●  %1").arg(name.isEmpty() ? tr("Profile") : name));
}

void HomeScreen::EnsureProfile(bool requested) {
    if (profile_checked) {
        return;
    }
    auto& manager = system.GetProfileManager();
    const auto user = manager.GetUser(static_cast<std::size_t>(Settings::values.current_user.GetValue()));
    const QString current = user ? ProfileName(manager, *user) : QString{};
    const bool clicked = requested;
    if (user && !IsDefaultProfileName(current) && !clicked) {
        profile_checked = true;
        return;
    }

    QDialog dialog{this};
    dialog.setWindowTitle(tr("Your profile"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* heading = new QLabel(user && !IsDefaultProfileName(current)
                                   ? tr("Change your profile's name")
                                   : tr("Create your profile"),
                               &dialog);
    QFont font = heading->font();
    font.setPointSizeF(font.pointSizeF() * 1.4);
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    auto* intro = new QLabel(tr("Choose the name games show for you. Your saves stay with this "
                                "profile."),
                             &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* name = new QLineEdit(&dialog);
    name->setPlaceholderText(tr("Your name"));
    name->setMaxLength(Service::Account::profile_username_size);
    if (!IsDefaultProfileName(current)) {
        name->setText(current);
    }
    layout->addWidget(name);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Save my profile"));
    const auto valid = [name] {
        const QString text = name->text().trimmed();
        return !text.isEmpty() && !IsDefaultProfileName(text) &&
               text.toStdString().size() <= Service::Account::profile_username_size;
    };
    buttons->button(QDialogButtonBox::Ok)->setEnabled(valid());
    connect(name, &QLineEdit::textChanged, &dialog,
            [buttons, valid] { buttons->button(QDialogButtonBox::Ok)->setEnabled(valid()); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    if (clicked && user && !IsDefaultProfileName(current)) {
        buttons->addButton(QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    } else {
        // A profile is required: closing the window does not skip it.
        dialog.setWindowFlag(Qt::WindowCloseButtonHint, false);
    }
    if (dialog.exec() != QDialog::Accepted || !valid()) {
        return;
    }
    const std::string text = name->text().trimmed().toStdString();
    if (user) {
        Service::Account::ProfileBase profile{};
        if (manager.GetProfileBase(*user, profile)) {
            std::fill(profile.username.begin(), profile.username.end(), '\0');
            std::copy(text.begin(), text.end(), profile.username.begin());
            manager.SetProfileBase(*user, profile);
        }
    } else {
        manager.CreateNewUser(Common::UUID::MakeRandom(), text);
    }
    manager.WriteUserSaveFile();
    profile_checked = true;
    UpdateProfile();
}
