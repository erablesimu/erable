// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/home/home_screen.h"

#include <algorithm>
#include <array>
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

#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "erable/game/game_list.h"
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
class GameTile : public QAbstractButton {
public:
    GameTile(const QString& title, const QPixmap& cover, bool favorite, bool add, QWidget* parent)
        : QAbstractButton{parent}, cover_{cover}, favorite_{favorite}, add_{add} {
        setText(title);
        setAccessibleName(title);
        setFixedSize(Box, Box);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

    void SetSelected(bool selected) {
        selected_ = selected;
        update();
    }

    static constexpr int TileSize = 196;
    static constexpr int Box = 236; ///< Room for the selected tile's growth and ring

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const double side = selected_ && !add_ ? TileSize * 1.08 : TileSize;
        const QRectF tile{(Box - side) / 2, (Box - side) / 2, side, side};
        const double radius = 22;
        if (selected_) {
            painter.setPen(QPen{Bg, 4});
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(tile.adjusted(-4, -4, 4, 4), radius + 4, radius + 4);
            painter.setPen(QPen{Maple, 4});
            painter.drawRoundedRect(tile.adjusted(-8, -8, 8, 8), radius + 8, radius + 8);
        }
        QPainterPath shape;
        shape.addRoundedRect(tile, radius, radius);
        if (add_) {
            painter.setPen(QPen{Line, 3, Qt::DashLine});
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(shape);
            painter.setPen(Muted);
            painter.setFont(MakeFont(HomeFonts().display, 46));
            painter.drawText(tile.adjusted(0, 0, 0, -40), Qt::AlignCenter, QStringLiteral("+"));
            painter.setFont(MakeFont(HomeFonts().display, 16));
            painter.drawText(tile.adjusted(14, 76, -14, 0), Qt::AlignCenter | Qt::TextWordWrap,
                             text());
            return;
        }
        // Soft shadow under the tile
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{0, 0, 0, 70});
        painter.drawRoundedRect(tile.translated(0, 8), radius, radius);
        painter.save();
        painter.setClipPath(shape);
        if (cover_.isNull()) {
            QLinearGradient fill{tile.topLeft(), tile.bottomRight()};
            fill.setColorAt(0, MapleHi);
            fill.setColorAt(1, Maple.darker(260));
            painter.fillRect(tile, fill);
            painter.fillPath(MapleLeaf(tile.center() - QPointF{0, 14}, side * 0.36),
                             QColor{255, 255, 255, 40});
        } else {
            painter.drawPixmap(tile.toRect(), cover_);
        }
        // The title on a shade that keeps it readable over any picture
        QLinearGradient shade{QPointF{0, tile.bottom() - 84}, QPointF{0, tile.bottom()}};
        shade.setColorAt(0, QColor{0, 0, 0, 0});
        shade.setColorAt(1, QColor{0, 0, 0, 200});
        painter.fillRect(QRectF(tile.left(), tile.bottom() - 84, tile.width(), 84), shade);
        painter.setPen(Qt::white);
        painter.setFont(MakeFont(HomeFonts().display, 15));
        painter.drawText(tile.adjusted(12, 0, -12, -10),
                         Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, text());
        painter.restore();
        if (favorite_) {
            const QRectF badge{tile.right() - 36, tile.top() + 10, 26, 26};
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor{0, 0, 0, 110});
            painter.drawEllipse(badge);
            painter.setPen(QColor{0xff, 0xd3, 0x6b});
            painter.setFont(MakeFont(HomeFonts().body, 14));
            painter.drawText(badge, Qt::AlignCenter, QStringLiteral("★"));
        }
    }

private:
    QPixmap cover_;
    bool favorite_;
    bool add_;
    bool selected_ = false;
};

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

/// The controller hints along the bottom: a badge and what it does, under a thin rule.
class HintBar : public QWidget {
public:
    explicit HintBar(QWidget* parent) : QWidget{parent} {
        setFixedHeight(40);
    }

    void SetHints(std::vector<std::pair<QString, QString>> hints, const QString& right) {
        hints_ = std::move(hints);
        right_ = right;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen{Line});
        painter.drawLine(QPointF{0, 0.5}, QPointF{static_cast<double>(width()), 0.5});
        const QFont label = MakeFont(HomeFonts().body, 13);
        const QFont key = MakeFont(HomeFonts().body, 11, QFont::ExtraBold);
        double x = 0;
        const double y = 12;
        for (const auto& [badge, text] : hints_) {
            painter.setFont(key);
            const double badge_width =
                std::max(22.0, QFontMetricsF{key}.horizontalAdvance(badge) + 12);
            const QRectF pill{x, y, badge_width, 22};
            painter.setPen(QPen{Line});
            painter.setBrush(Surface);
            painter.drawRoundedRect(pill, 11, 11);
            painter.setPen(Text);
            painter.drawText(pill, Qt::AlignCenter, badge);
            painter.setFont(label);
            painter.setPen(Muted);
            const double text_width = QFontMetricsF{label}.horizontalAdvance(text);
            painter.drawText(QRectF{x + badge_width + 7, y, text_width + 2, 22},
                             Qt::AlignVCenter | Qt::AlignLeft, text);
            x += badge_width + 7 + text_width + 22;
        }
        painter.setFont(label);
        painter.setPen(Muted);
        painter.drawText(QRectF{x, y, width() - x, 22}, Qt::AlignVCenter | Qt::AlignRight, right_);
    }

private:
    std::vector<std::pair<QString, QString>> hints_;
    QString right_;
};

/// The maple leaf and the name, centred in the top bar.
class BrandMark : public QWidget {
public:
    explicit BrandMark(QWidget* parent) : QWidget{parent} {
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
    setStyleSheet(QStringLiteral("#HomeScreen QScrollArea, #HomeScreen QScrollArea > QWidget > QWidget"
                                 " { background: transparent; border: none; }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 18, 32, 10);
    layout->setSpacing(12);

    // Top bar: profile, the Erable mark, music, controller and clock
    auto* top = new QHBoxLayout;
    profile_button = new PillButton({}, {}, false, this);
    profile_button->SetAvatar(true);
    profile_button->setToolTip(tr("Your profile"));
    connect(profile_button, &QAbstractButton::clicked, this, [this] {
        profile_checked = false;
        EnsureProfile(true);
    });
    top->addWidget(profile_button);
    top->addStretch();
    top->addWidget(new BrandMark(this));
    top->addStretch();
    music_button = new PillButton({}, QStringLiteral("♪"), false, this);
    music_button->setCheckable(true);
    music_button->setToolTip(tr("Home music"));
    connect(music_button, &QAbstractButton::toggled, this, [this](bool on) {
        UISettings::values.home_music = on;
        UpdateMusic();
    });
    top->addWidget(music_button);
    song_button = new PillButton({}, {}, false, this);
    song_button->setToolTip(tr("Next tune"));
    connect(song_button, &QAbstractButton::clicked, this, [this] {
        UISettings::values.home_song =
            (UISettings::values.home_song.GetValue() + 1) % HomeMusic::SongCount;
        UISettings::values.home_music = true;
        music.Stop();
        music_button->setChecked(true);
        UpdateMusic();
    });
    top->addWidget(song_button);
    top->addSpacing(10);
    controller_label = new QLabel(this);
    controller_label->setFont(MakeFont(fonts.body, 14));
    top->addWidget(controller_label);
    top->addSpacing(10);
    clock = new QLabel(this);
    clock->setFont(MakeFont(fonts.display, 22));
    top->addWidget(clock);
    layout->addLayout(top);

    // The selected game
    layout->addSpacing(10);
    title = new QLabel(this);
    title->setFont(MakeFont(fonts.display, 36));
    title->setWordWrap(true);
    meta = new QLabel(this);
    meta->setFont(MakeFont(fonts.body, 15, QFont::DemiBold));
    meta->setTextFormat(Qt::RichText);
    for (QLabel* label : {controller_label, clock, title, meta}) {
        QPalette palette = label->palette();
        palette.setColor(QPalette::WindowText, label == meta || label == controller_label ? Muted : Text);
        label->setPalette(palette);
    }
    layout->addWidget(title);
    layout->addWidget(meta);

    // The row of tiles; the selected one is kept in view, without a scroll bar
    scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedHeight(GameTile::Box + 8);
    scroll->setFocusPolicy(Qt::NoFocus);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* row_widget = new QWidget(scroll);
    row_widget->setAutoFillBackground(false);
    row = new QHBoxLayout(row_widget);
    row->setContentsMargins(0, 4, 0, 4);
    row->setSpacing(0);
    scroll->setWidget(row_widget);
    scroll->viewport()->setAutoFillBackground(false);
    layout->addWidget(scroll);

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
    for (PillButton* button : {play_button, settings_button, favorite_button, report_button}) {
        actions->addWidget(button);
    }
    actions->addStretch();
    layout->addLayout(actions);
    layout->addStretch();

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

    hints = new HintBar(this);
    hints->SetHints({{QStringLiteral("◀▶"), tr("Choose")},
                     {QStringLiteral("A"), tr("Play")},
                     {QStringLiteral("+"), tr("Game settings")},
                     {QStringLiteral("Y"), tr("Favourite")}},
                    tr("Ctrl + L  Library"));
    layout->addWidget(hints);

    controller = new ControllerNavigation(system.HIDCore(), this);
    connect(controller, &ControllerNavigation::TriggerKeyboardEvent, this, [this](Qt::Key key) {
        if (!isVisible() || !isActiveWindow() || system.IsPoweredOn()) {
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
}

void HomeScreen::paintEvent(QPaintEvent*) {
    QPainter painter{this};
    painter.fillRect(rect(), Bg);
    QRadialGradient glow{QPointF(width() * 0.5, -height() * 0.1), width() * 0.75};
    glow.setColorAt(0, BgGlow);
    glow.setColorAt(1, Bg);
    painter.fillRect(rect(), glow);
}

void HomeScreen::UpdateMusic() {
    const bool on = UISettings::values.home_music.GetValue();
    const int song = static_cast<int>(UISettings::values.home_song.GetValue());
    music_button->setText(on ? tr("Music") : tr("Muted"));
    music_button->updateGeometry();
    song_button->setText(QStringLiteral("%1/%2").arg(song + 1).arg(HomeMusic::SongCount));
    song_button->updateGeometry();
    if (on && active && isVisible() && !system.IsPoweredOn()) {
        music.Play(song);
    } else {
        music.Stop();
    }
}

void HomeScreen::UpdateStatus() {
    clock->setText(QTime::currentTime().toString(QStringLiteral("HH:mm")));
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
        scroll->ensureWidgetVisible(tiles[selected], GameTile::Box, 0);
    }
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
    QTimer::singleShot(0, this, [this] { EnsureProfile(); });
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
