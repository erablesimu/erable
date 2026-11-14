// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/home/home_screen.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>

#include <fmt/format.h>

#include <QAbstractButton>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "erable/game/game_list.h"
#include "erable/home/case_shelf.h"
#include "erable/util/controller_navigation.h"
#include "frontend_common/play_time_manager.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/game_list/game_list_p.h"

namespace {

// The home's own palette: a warm dark room, whatever theme the rest of Erable uses
constexpr QColor Bg{0x17, 0x12, 0x0f};
constexpr QColor BgGlow{0x2a, 0x1a, 0x12};
constexpr QColor Surface{0x24, 0x1d, 0x19};
constexpr QColor SurfaceHi{0x2f, 0x26, 0x21};
constexpr QColor Text{0xf6, 0xee, 0xe8};
constexpr QColor Muted{0xb8, 0xa8, 0x9c};
constexpr QColor Line{0x3b, 0x30, 0x2a};
constexpr QColor Maple{0xf0, 0x70, 0x2f};
constexpr QColor MapleHi{0xff, 0xa2, 0x5a};
constexpr QColor Ok{0x57, 0xc7, 0x6b};
constexpr QColor Ink{0x1a, 0x12, 0x0d};

struct Fonts {
    QString display; ///< Fredoka SemiBold: titles, the brand, the clock
    QString body;    ///< Nunito Bold: buttons and labels
};

/// The home's two typefaces (SIL Open Font License, dist/fonts), loaded once.
const Fonts& HomeFonts() {
    static const Fonts fonts = [] {
        const auto load = [](const QString& file, const QString& fallback) {
            const int id = QFontDatabase::addApplicationFont(file);
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            return families.isEmpty() ? fallback : families.first();
        };
        return Fonts{
            .display = load(QStringLiteral(":/fonts/Fredoka-SemiBold.ttf"), QStringLiteral("Segoe UI")),
            .body = load(QStringLiteral(":/fonts/Nunito-Bold.ttf"), QStringLiteral("Segoe UI")),
        };
    }();
    return fonts;
}

QFont MakeFont(const QString& family, int pixels, int weight = QFont::Bold) {
    QFont font{family};
    font.setPixelSize(pixels);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

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

/// A maple leaf of `size` pixels centred on `centre` (the outline of Erable's icon).
QPainterPath MapleLeaf(QPointF centre, double size) {
    static constexpr QPointF Outline[] = {
        {12, 2},      {13.6, 5.8},  {16.7, 4.4}, {16.1, 8.0}, {20.0, 7.6}, {18.0, 10.7}, {21.0, 12.3},
        {17.4, 13.7}, {18.7, 16.9}, {15.1, 16.1}, {12, 22},   {9.9, 16.1}, {6.3, 16.9},  {7.6, 13.7},
        {4, 12.3},    {7, 10.7},    {5, 7.6},     {8.9, 8.0}, {8.3, 4.4},  {11.4, 5.8},
    };
    QPainterPath path;
    const double scale = size / 20.0;
    for (std::size_t i = 0; i < std::size(Outline); ++i) {
        const QPointF point = centre + (Outline[i] - QPointF{12, 12}) * scale;
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}

enum class Icon { Library, News, Folder, Captures, Debug, Settings };

/// The dock's line icons, drawn on a 24-unit grid in `box`.
void DrawIcon(QPainter& painter, Icon icon, const QRectF& box, const QColor& color) {
    painter.save();
    painter.translate(box.topLeft());
    painter.scale(box.width() / 24.0, box.height() / 24.0);
    painter.setPen(QPen{color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
    painter.setBrush(Qt::NoBrush);
    switch (icon) {
    case Icon::Library:
        for (const QPointF corner : {QPointF{3, 3}, QPointF{14, 3}, QPointF{3, 14}, QPointF{14, 14}}) {
            painter.drawRoundedRect(QRectF{corner, QSizeF{7, 7}}, 2, 2);
        }
        break;
    case Icon::News:
        painter.drawLine(QPointF{12, 3}, QPointF{12, 7});
        painter.drawLine(QPointF{12, 17}, QPointF{12, 21});
        painter.drawLine(QPointF{3, 12}, QPointF{7, 12});
        painter.drawLine(QPointF{17, 12}, QPointF{21, 12});
        painter.drawLine(QPointF{6, 6}, QPointF{8.5, 8.5});
        painter.drawLine(QPointF{15.5, 15.5}, QPointF{18, 18});
        painter.drawLine(QPointF{18, 6}, QPointF{15.5, 8.5});
        painter.drawLine(QPointF{8.5, 15.5}, QPointF{6, 18});
        break;
    case Icon::Folder: {
        QPainterPath folder;
        folder.moveTo(3, 7);
        folder.quadTo(3, 5, 5, 5);
        folder.lineTo(9, 5);
        folder.lineTo(11, 7);
        folder.lineTo(19, 7);
        folder.quadTo(21, 7, 21, 9);
        folder.lineTo(21, 17);
        folder.quadTo(21, 19, 19, 19);
        folder.lineTo(5, 19);
        folder.quadTo(3, 19, 3, 17);
        folder.closeSubpath();
        painter.drawPath(folder);
        break;
    }
    case Icon::Captures:
        painter.drawRoundedRect(QRectF{3, 5, 18, 14}, 3, 3);
        painter.drawEllipse(QPointF{12, 12}, 3.5, 3.5);
        break;
    case Icon::Debug:
        painter.drawRoundedRect(QRectF{7, 8, 10, 12}, 5, 5);
        painter.drawArc(QRectF{9, 4, 6, 6}, 0, 180 * 16);
        painter.drawLine(QPointF{12, 12}, QPointF{12, 20});
        for (const auto& [from, to] : std::array<std::pair<QPointF, QPointF>, 6>{{
                 {{3, 13}, {7, 13}}, {{17, 13}, {21, 13}}, {{4, 7}, {7, 9}},
                 {{20, 7}, {17, 9}}, {{4, 19}, {7, 17}}, {{20, 19}, {17, 17}}}}) {
            painter.drawLine(from, to);
        }
        break;
    case Icon::Settings:
        painter.drawEllipse(QPointF{12, 12}, 3, 3);
        for (const auto& [from, to] : std::array<std::pair<QPointF, QPointF>, 8>{{
                 {{12, 2}, {12, 5}}, {{12, 19}, {12, 22}}, {{4.9, 4.9}, {7, 7}}, {{17, 17}, {19.1, 19.1}},
                 {{2, 12}, {5, 12}}, {{19, 12}, {22, 12}}, {{4.9, 19.1}, {7, 17}}, {{17, 7}, {19.1, 4.9}}}}) {
            painter.drawLine(from, to);
        }
        break;
    }
    painter.restore();
}

} // namespace

/// A rounded button: an optional controller badge (A, +, Y, -) or avatar letter, then its text.
/// The primary one is filled in maple; a checked one is outlined in maple.
class PillButton : public QAbstractButton {
public:
    PillButton(const QString& text, const QString& badge, bool primary, QWidget* parent)
        : QAbstractButton{parent}, badge_{badge}, primary_{primary} {
        setText(text);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFont(MakeFont(HomeFonts().body, 15, QFont::ExtraBold));
    }

    void SetBadge(const QString& badge) {
        badge_ = badge;
        updateGeometry();
        update();
    }
    void SetAvatar(bool avatar) {
        avatar_ = avatar;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override {
        const int text_width = fontMetrics().horizontalAdvance(text());
        const int lead = badge_.isEmpty() ? 0 : (avatar_ ? 34 : 24) + 10;
        return {text_width + lead + (avatar_ ? 24 : 40), avatar_ ? 46 : 44};
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF pill = QRectF(rect()).adjusted(1, 1, -1, -1);
        const double radius = pill.height() / 2;
        const bool hovered = underMouse() && isEnabled();
        if (primary_) {
            QLinearGradient fill{pill.topLeft(), pill.bottomRight()};
            fill.setColorAt(0, hovered ? MapleHi.lighter(110) : MapleHi);
            fill.setColorAt(1, Maple);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
        } else {
            painter.setPen(QPen{isChecked() ? Maple : Line, isChecked() ? 1.6 : 1.0});
            painter.setBrush(hovered ? SurfaceHi : Surface);
        }
        painter.setOpacity(isEnabled() ? 1.0 : 0.45);
        painter.drawRoundedRect(pill, radius, radius);

        double x = pill.left() + (avatar_ ? 6 : 18);
        const QColor ink = primary_ ? Ink : (isChecked() ? Maple : Text);
        if (!badge_.isEmpty()) {
            const double size = avatar_ ? 34 : 24;
            const QRectF circle{x, pill.center().y() - size / 2, size, size};
            if (avatar_) {
                QLinearGradient fill{circle.topLeft(), circle.bottomRight()};
                fill.setColorAt(0, MapleHi);
                fill.setColorAt(1, Maple);
                painter.setPen(Qt::NoPen);
                painter.setBrush(fill);
            } else {
                painter.setPen(primary_ ? Qt::NoPen : QPen{Line});
                painter.setBrush(primary_ ? QColor{0, 0, 0, 60} : SurfaceHi);
            }
            painter.drawEllipse(circle);
            painter.setPen(avatar_ || primary_ ? Ink : Text);
            painter.setFont(MakeFont(avatar_ ? HomeFonts().display : HomeFonts().body,
                                     avatar_ ? 16 : 12, QFont::ExtraBold));
            painter.drawText(circle, Qt::AlignCenter, badge_);
            x += size + 10;
        }
        painter.setPen(ink);
        painter.setFont(font());
        painter.drawText(QRectF{x, pill.top(), pill.right() - x - 12, pill.height()},
                         Qt::AlignVCenter | Qt::AlignLeft, text());
    }

    void enterEvent(QEnterEvent* event) override {
        update();
        QAbstractButton::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        update();
        QAbstractButton::leaveEvent(event);
    }

private:
    QString badge_;
    bool primary_;
    bool avatar_ = false;
};

/// One game: its picture with rounded corners and its title, larger and ringed in maple when
/// selected.
/// A round dock button: a line icon in a circle, its name below.
class DockButton : public QAbstractButton {
public:
    DockButton(Icon icon, const QString& label, QWidget* parent)
        : QAbstractButton{parent}, icon_{icon} {
        setText(label);
        setToolTip(label);
        setFixedSize(88, 94);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        const bool hovered = underMouse();
        const QRectF circle{15, hovered ? 2.0 : 5.0, 58, 58};
        if (hovered) {
            painter.setPen(QPen{Maple, 3});
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(circle.adjusted(-3, -3, 3, 3));
        }
        painter.setPen(QPen{Line});
        painter.setBrush(Surface);
        painter.drawEllipse(circle);
        DrawIcon(painter, icon_, circle.adjusted(16, 16, -16, -16), Maple);
        painter.setPen(Muted);
        painter.setFont(MakeFont(HomeFonts().body, 12));
        painter.drawText(QRectF{0, 68, 88, 24}, Qt::AlignHCenter | Qt::AlignTop, text());
    }
    void enterEvent(QEnterEvent* event) override {
        update();
        QAbstractButton::enterEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        update();
        QAbstractButton::leaveEvent(event);
    }

private:
    Icon icon_;
};

/// The maple leaf and the name, centred in the top bar.
class BrandMark : public QWidget {
public:
    explicit BrandMark(QWidget* parent) : QWidget{parent} {
        setObjectName(QStringLiteral("brand"));
        setAttribute(Qt::WA_NoSystemBackground);
        setFixedSize(120, 40);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillPath(MapleLeaf(QPointF{14, 20}, 22), Maple);
        painter.setPen(Text);
        painter.setFont(MakeFont(HomeFonts().display, 20));
        painter.drawText(QRectF{32, 0, 88, 40}, Qt::AlignVCenter | Qt::AlignLeft,
                         QStringLiteral("Erable"));
    }
};

HomeScreen::HomeScreen(GameList* game_list_, PlayTime::PlayTimeManager& play_time_,
                       Core::System& system_, QWidget* parent)
    : QWidget{parent}, game_list{game_list_}, play_time{play_time_}, system{system_} {
    setObjectName(QStringLiteral("HomeScreen"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, false);
    const Fonts& fonts = HomeFonts();
    setStyleSheet(
        QStringLiteral("#HomeScreen QWidget#brand { background: transparent; }"
                       "#HomeScreen QLineEdit#search { background: %1; color: %2; border: 1px solid %3;"
                       " border-radius: 18px; padding: 7px 16px; selection-background-color: %4; }"
                       "#HomeScreen QLineEdit#search:focus { border-color: %4; }")
            .arg(Surface.name(), Text.name(), Line.name(), Maple.name()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 18, 32, 10);
    layout->setSpacing(12);

    // Top bar: the profile on the left; the Erable mark and the search in the middle; music,
    // controller and clock on the right. The two sides share the width, so the middle is centred.
    auto* top = new QHBoxLayout;
    auto* left = new QHBoxLayout;
    auto* right = new QHBoxLayout;
    auto* middle = new QHBoxLayout;
    middle->setSpacing(14);
    profile_button = new PillButton({}, {}, false, this);
    profile_button->SetAvatar(true);
    profile_button->setToolTip(tr("Your profile"));
    connect(profile_button, &QAbstractButton::clicked, this, [this] {
        profile_checked = false;
        EnsureProfile(true);
    });
    left->addWidget(profile_button);
    left->addStretch();
    middle->addWidget(new BrandMark(this));
    search = new QLineEdit(this);
    search->setObjectName(QStringLiteral("search"));
    search->setPlaceholderText(tr("Search a game"));
    search->setClearButtonEnabled(true);
    search->setFixedWidth(260);
    search->setFont(MakeFont(fonts.body, 14, QFont::DemiBold));
    search->installEventFilter(this);
    connect(search, &QLineEdit::textChanged, this, [this] {
        CollectGames();
        RebuildTiles();
    });
    connect(search, &QLineEdit::returnPressed, this, [this] { setFocus(); });
    middle->addWidget(search);
    right->addStretch();
    music_button = new PillButton({}, QStringLiteral("♪"), false, this);
    music_button->setCheckable(true);
    music_button->setToolTip(tr("Home music"));
    connect(music_button, &QAbstractButton::toggled, this, [this](bool on) {
        const bool changed = UISettings::values.home_music.GetValue() != on;
        UISettings::values.home_music = on;
        UpdateMusic();
        if (changed) {
            emit MusicSettingsChanged();
        }
    });
    right->addWidget(music_button);
    song_button = new PillButton({}, {}, false, this);
    song_button->setToolTip(tr("Next tune"));
    connect(song_button, &QAbstractButton::clicked, this, [this] {
        UISettings::values.home_song =
            (UISettings::values.home_song.GetValue() + 1) % HomeMusic::SongCount;
        UISettings::values.home_music = true;
        music.Stop();
        music_button->setChecked(true);
        UpdateMusic();
        emit MusicSettingsChanged();
    });
    right->addWidget(song_button);
    right->addSpacing(10);
    controller_label = new QLabel(this);
    controller_label->setFont(MakeFont(fonts.body, 14));
    right->addWidget(controller_label);
    right->addSpacing(10);
    clock = new QLabel(this);
    clock->setFont(MakeFont(fonts.display, 22));
    right->addWidget(clock);
    top->addLayout(left, 1);
    top->addLayout(middle, 0);
    top->addLayout(right, 1);
    layout->addLayout(top);

    // The games as a shelf of cases, the selected one's name under it
    shelf = new CaseShelf(fonts.display, fonts.body, this);
    connect(shelf, &CaseShelf::Clicked, this, [this](int index) {
        if (index == selected) {
            Launch();
        } else {
            Select(index);
        }
    });
    connect(shelf, &CaseShelf::SelectionRequested, this, &HomeScreen::Select);
    layout->addWidget(shelf, 1);
    title = new QLabel(this);
    title->setFont(MakeFont(fonts.display, 40));
    title->setAlignment(Qt::AlignHCenter);
    title->setWordWrap(true);
    meta = new QLabel(this);
    meta->setFont(MakeFont(fonts.body, 15, QFont::DemiBold));
    meta->setTextFormat(Qt::RichText);
    meta->setAlignment(Qt::AlignHCenter);
    for (QLabel* label : {controller_label, clock, title, meta}) {
        QPalette palette = label->palette();
        palette.setColor(QPalette::WindowText, label == meta || label == controller_label ? Muted : Text);
        label->setPalette(palette);
    }
    layout->addWidget(title);
    layout->addWidget(meta);

    // The selected game's actions, with their controller buttons
    auto* actions = new QHBoxLayout;
    actions->setSpacing(12);
    play_button = new PillButton(tr("Play"), QStringLiteral("A"), true, this);
    connect(play_button, &QAbstractButton::clicked, this, &HomeScreen::Launch);
    settings_button = new PillButton(tr("Game settings"), QStringLiteral("+"), false, this);
    connect(settings_button, &QAbstractButton::clicked, this, &HomeScreen::OpenGameSettingsOfSelected);
    favorite_button = new PillButton({}, QStringLiteral("Y"), false, this);
    connect(favorite_button, &QAbstractButton::clicked, this, &HomeScreen::ToggleFavorite);
    report_button = new PillButton(tr("Report a problem"), QStringLiteral("−"), false, this);
    connect(report_button, &QAbstractButton::clicked, this, &HomeScreen::ReportProblem);
    actions->addStretch();
    for (PillButton* button : {play_button, settings_button, favorite_button, report_button}) {
        actions->addWidget(button);
    }
    actions->addStretch();
    layout->addLayout(actions);
    layout->addSpacing(8);

    // Dock of round buttons
    auto* dock = new QHBoxLayout;
    dock->setSpacing(10);
    dock->addStretch();
    const auto add_dock = [this, dock](Icon icon, const QString& label) {
        auto* button = new DockButton(icon, label, this);
        dock->addWidget(button);
        return button;
    };
    connect(add_dock(Icon::Library, tr("Library")), &QAbstractButton::clicked, this,
            &HomeScreen::ShowLibrary);
    connect(add_dock(Icon::News, tr("What's new")), &QAbstractButton::clicked, this,
            &HomeScreen::OpenWhatsNew);
    connect(add_dock(Icon::Folder, tr("Games folder")), &QAbstractButton::clicked, this,
            &HomeScreen::OpenGamesFolder);
    connect(add_dock(Icon::Captures, tr("Captures")), &QAbstractButton::clicked, this,
            &HomeScreen::OpenCaptures);
    DockButton* const debug = add_dock(Icon::Debug, tr("Debug mode"));
    connect(debug, &QAbstractButton::clicked, this, [this, debug] {
        if (debug_menu) {
            debug_menu->popup(debug->mapToGlobal(QPoint(0, -debug_menu->sizeHint().height())));
        }
    });
    connect(add_dock(Icon::Settings, tr("Settings")), &QAbstractButton::clicked, this,
            &HomeScreen::OpenSettings);
    dock->addStretch();
    layout->addLayout(dock);


    controller = new ControllerNavigation(system.HIDCore(), this);
    connect(controller, &ControllerNavigation::TriggerKeyboardEvent, this, [this](Qt::Key key) {
        if (!isVisible() || !isActiveWindow() || system.IsPoweredOn() || search->hasFocus()) {
            return;
        }
        QKeyEvent event{QEvent::KeyPress, key, Qt::NoModifier};
        keyPressEvent(&event);
    });

    status_timer.setInterval(2000);
    connect(&status_timer, &QTimer::timeout, this, &HomeScreen::UpdateStatus);
    status_timer.start();
    UpdateStatus();

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

    // The background drifts: slow waves of light and a few floating specks.
    ambient_timer.setInterval(33);
    connect(&ambient_timer, &QTimer::timeout, this, [this] { update(); });
    ambient_clock.start();
    covers = new QNetworkAccessManager(this);

    music_button->setChecked(UISettings::values.home_music.GetValue());
    UpdateMusic();
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
    ambient_timer.stop();
}

void HomeScreen::paintEvent(QPaintEvent*) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Bg);
    QRadialGradient glow{QPointF(width() * 0.5, height() * 0.42), width() * 0.7};
    glow.setColorAt(0, QColor{0x4a, 0x24, 0x14});
    glow.setColorAt(0.55, BgGlow);
    glow.setColorAt(1, Bg);
    painter.fillRect(rect(), glow);

    // Slow waves of light across the room, and specks drifting up
    const double t = ambient_clock.isValid() ? ambient_clock.elapsed() / 1000.0 : 0.0;
    const double w = width();
    const double h = height();
    for (int wave = 0; wave < 5; ++wave) {
        QPainterPath path;
        const double base = h * (0.22 + wave * 0.13);
        const double amplitude = h * (0.035 + 0.012 * wave);
        const double speed = 0.12 + 0.05 * wave;
        for (int step = 0; step <= 48; ++step) {
            const double x = w * step / 48.0;
            const double y = base + amplitude * std::sin(x / w * 6.283 * (0.8 + 0.25 * wave) +
                                                         t * speed + wave * 1.7);
            if (step == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        QLinearGradient stroke{QPointF(0, 0), QPointF(w, 0)};
        const int alpha = 44 - wave * 5;
        stroke.setColorAt(0, QColor{MapleHi.red(), MapleHi.green(), MapleHi.blue(), 0});
        stroke.setColorAt(0.5, QColor{MapleHi.red(), MapleHi.green(), MapleHi.blue(), alpha});
        stroke.setColorAt(1, QColor{MapleHi.red(), MapleHi.green(), MapleHi.blue(), 0});
        painter.setPen(QPen{QBrush{stroke}, 2.0 + wave * 0.6});
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }
    painter.setPen(Qt::NoPen);
    for (int speck = 0; speck < 42; ++speck) {
        // Fixed pseudo-random places, each rising at its own pace and wrapping round
        const double seed = std::sin(speck * 12.9898) * 43758.5453;
        const double r1 = seed - std::floor(seed);
        const double r2 = std::fmod(r1 * 91.7, 1.0);
        const double rise = std::fmod(r2 + t * (0.006 + 0.01 * r1), 1.0);
        const double x = w * r1 + std::sin(t * 0.4 + speck) * 8;
        const double y = h * (1.0 - rise);
        const double radius = 1.0 + 1.6 * r2;
        const int alpha = static_cast<int>(40 + 60 * std::sin(rise * 3.1416));
        painter.setBrush(QColor{0xff, 0xd9, 0xb8, alpha});
        painter.drawEllipse(QPointF(x, y), radius, radius);
    }
}

void HomeScreen::FetchCovers() {
    // Covers the community made for Erable-family emulators, by title ID; kept on disk once
    // fetched. A game without one keeps the icon from its own files.
    const auto dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "covers";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (std::size_t i = 0; i < games.size(); ++i) {
        const u64 id = games[i].program_id;
        const QPixmap& own = games[i].cover;
        if (id == 0 || (!own.isNull() && own.height() > own.width() * 1.15)) {
            continue; // a portrait cover of its own (SteamGridDB, custom artwork) stays
        }
        const auto name = fmt::format("{:016X}", id);
        const auto file = dir / (name + ".png");
        if (std::filesystem::exists(file, ec)) {
            QPixmap art;
            if (art.load(QString::fromStdWString(file.wstring()))) {
                games[i].cover = art;
                shelf->SetArt(static_cast<int>(i), art);
            }
            continue;
        }
        // Asked once a session; a title without a cover is asked again a week later.
        const auto missing = dir / (name + ".none");
        const auto checked = std::filesystem::last_write_time(missing, ec);
        if ((!ec && std::filesystem::file_time_type::clock::now() - checked < std::chrono::days{7}) ||
            !covers_requested.insert(id).second) {
            ec.clear();
            continue;
        }
        QNetworkRequest request{QUrl{
            QStringLiteral("https://raw.githubusercontent.com/mushymeow/eglantine-covers/main/%1.png")
                .arg(QString::fromStdString(name))}};
        request.setTransferTimeout(8000);
        QNetworkReply* reply = covers->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, id, file, missing] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 404) {
                std::ofstream{missing};
                return;
            }
            QPixmap art;
            const QByteArray bytes = reply->readAll();
            if (reply->error() != QNetworkReply::NoError || !art.loadFromData(bytes, "PNG")) {
                return;
            }
            // Kept at a size the shelf can use, not the full download
            art = art.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            art.save(QString::fromStdWString(file.wstring()), "PNG");
            for (std::size_t index = 0; index < games.size(); ++index) {
                if (games[index].program_id == id) {
                    games[index].cover = art;
                    shelf->SetArt(static_cast<int>(index), art);
                }
            }
        });
    }
}

void HomeScreen::UpdateMusic() {
    const bool on = UISettings::values.home_music.GetValue();
    const int song = static_cast<int>(UISettings::values.home_song.GetValue());
    music_button->setText(on ? tr("Music") : tr("Muted"));
    music_button->updateGeometry();
    song_button->setText(QStringLiteral("%1/%2").arg(song + 1).arg(HomeMusic::SongCount));
    song_button->updateGeometry();
    const bool shown = active && isVisible() && !system.IsPoweredOn();
    if (on && shown) {
        music.Play(song);
    } else {
        music.Stop();
    }
    if (shown) {
        ambient_timer.start();
    } else {
        ambient_timer.stop();
    }
}

void HomeScreen::UpdateStatus() {
    clock->setText(QTime::currentTime().toString(QStringLiteral("HH:mm")));
    // The profiles may load after the home is built: the name is read again now and then.
    UpdateProfile();
    const auto& hid = system.HIDCore();
    const bool connected =
        hid.GetEmulatedController(Core::HID::NpadIdType::Player1)->IsConnected() ||
        hid.GetEmulatedController(Core::HID::NpadIdType::Handheld)->IsConnected();
    controller_label->setText(connected
                                  ? QStringLiteral("<span style=\"color:%1\">●</span> %2")
                                        .arg(Ok.name(), tr("Controller"))
                                  : QStringLiteral("○ %1").arg(tr("No controller")));
}

void HomeScreen::CollectGames() {
    games.clear();
    const QString filter = search->text().trimmed();
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
                // A dump's name may carry its title ID: not on a case
                static const QRegularExpression title_id{
                    QStringLiteral(R"(\s*\[[0-9A-Fa-f]{16}\]\s*$)")};
                game.title.remove(title_id);
                if (!filter.isEmpty() && !game.title.contains(filter, Qt::CaseInsensitive)) {
                    continue;
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
    selected = 0;
    for (std::size_t i = 0; i < games.size(); ++i) {
        if (games[i].path == selected_path) {
            selected = static_cast<int>(i);
        }
    }
    std::vector<CaseShelf::Entry> entries;
    entries.reserve(games.size() + 1);
    for (const Game& game : games) {
        entries.push_back({.title = game.title, .art = game.cover, .favorite = game.favorite});
    }
    entries.push_back({.title = tr("Add games"), .add = true});
    shelf->SetEntries(std::move(entries), selected);
    FetchCovers();
    UpdateSelection();
}

void HomeScreen::Select(int index) {
    const int count = static_cast<int>(games.size()) + 1;
    selected = std::clamp(index, 0, count - 1);
    UpdateSelection();
}

void HomeScreen::UpdateSelection() {
    shelf->SetSelected(selected);
    const bool is_game = selected < static_cast<int>(games.size());
    for (PillButton* button : {play_button, settings_button, favorite_button, report_button}) {
        button->setEnabled(is_game || button == report_button);
    }
    if (!is_game) {
        title->setText(games.empty() ? tr("Welcome to Erable") : tr("Add games"));
        meta->setText(tr("Put your games in %1 and they show up here.")
                          .arg(QDir::toNativeSeparators(GamesFolder()).toHtmlEscaped()));
        favorite_button->setText(tr("Add to favourites"));
        favorite_button->updateGeometry();
        return;
    }
    const Game& game = games[selected];
    title->setText(game.title);
    const u64 seconds = play_time.GetPlayTime(game.program_id);
    const QString played =
        seconds == 0 ? tr("Never played")
                     : tr("Played for %1")
                           .arg(QString::fromStdString(
                               PlayTime::PlayTimeManager::GetReadablePlayTime(seconds)));
    // The game's shader cache, once Erable has built one
    const auto cache = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir) /
                       fmt::format("{:016x}", game.program_id) / "erable-cache.v2.bin";
    std::error_code ec;
    const bool shaders_ready = std::filesystem::exists(cache, ec);
    meta->setText(QStringLiteral("%1 &nbsp;·&nbsp; %2")
                      .arg(played.toHtmlEscaped(),
                           shaders_ready
                               ? QStringLiteral("<span style=\"color:%1\">● %2</span>")
                                     .arg(Ok.name(), tr("Shaders ready"))
                               : QStringLiteral("○ %1").arg(tr("Shaders prepared at first launch"))));
    favorite_button->setText(game.favorite ? tr("Remove from favourites") : tr("Add to favourites"));
    favorite_button->updateGeometry();
}

void HomeScreen::Launch() {
    if (selected < static_cast<int>(games.size())) {
        music.Stop();
        emit LaunchGame(games[selected].path, games[selected].program_id);
    } else {
        OpenGamesFolder();
    }
}

void HomeScreen::OpenGameSettingsOfSelected() {
    if (selected < static_cast<int>(games.size())) {
        emit OpenGameSettings(games[selected].path);
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

bool HomeScreen::eventFilter(QObject* watched, QEvent* event) {
    // Escape or Down in the search goes back to the games (Escape also clears it)
    if (watched == search && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Escape || key == Qt::Key_Down) {
            if (key == Qt::Key_Escape) {
                search->clear();
            }
            setFocus();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void HomeScreen::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::Find)) {
        search->setFocus();
        search->selectAll();
        event->accept();
        return;
    }
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
    case Qt::Key_Plus:
        OpenGameSettingsOfSelected();
        break;
    case Qt::Key_Y:
        ToggleFavorite();
        break;
    case Qt::Key_Minus:
        emit ReportProblem();
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
    QTimer::singleShot(0, this, [this] {
        UpdateProfile();
        EnsureProfile();
    });
    UpdateMusic();
}

void HomeScreen::UpdateProfile() {
    auto& manager = system.GetProfileManager();
    const auto user =
        manager.GetUser(static_cast<std::size_t>(Settings::values.current_user.GetValue()));
    const QString name = user ? ProfileName(manager, *user) : QString{};
    const QString shown = name.isEmpty() ? tr("Profile") : name;
    profile_button->setText(shown);
    profile_button->SetBadge(shown.left(1).toUpper());
}

void HomeScreen::EnsureProfile(bool requested) {
    if (profile_checked) {
        return;
    }
    auto& manager = system.GetProfileManager();
    const auto user =
        manager.GetUser(static_cast<std::size_t>(Settings::values.current_user.GetValue()));
    const QString current = user ? ProfileName(manager, *user) : QString{};
    if (user && !IsDefaultProfileName(current) && !requested) {
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
    heading->setFont(MakeFont(HomeFonts().display, 24));
    layout->addWidget(heading);
    auto* intro = new QLabel(tr("Choose the name games show for you. Your saves stay with this "
                                "profile."),
                             &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* name = new QLineEdit(&dialog);
    name->setPlaceholderText(tr("Your name"));
    name->setMaxLength(static_cast<int>(Service::Account::profile_username_size));
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
    if (requested && user && !IsDefaultProfileName(current)) {
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
