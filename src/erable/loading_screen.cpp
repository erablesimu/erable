// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QBuffer>
#include <QByteArray>
#include <QGraphicsOpacityEffect>
#include <QIODevice>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStyle>
#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <QConicalGradient>
#include <QRadialGradient>
#include <array>
#include <QVBoxLayout>
#include <algorithm>
#include <initializer_list>
#include <cmath>
#include <iterator>
#include <string>
#include "erable/leaf_catcher.h"
#include "common/container/unordered_map.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/loader/loader.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/shader_warm/shader_warm_builder.h"
#include "erable/loading_screen.h"

// Mingw seems to not have QMovie at all. If QMovie is missing then use a single frame instead of an
// showing the full animation
#if !ERABLE_QT_MOVIE_MISSING
#include <QMovie>
#endif

namespace {

/// A maple leaf `size` wide centred on the origin (the outline of Erable's icon).
QPainterPath MapleLeaf(double size) {
    static constexpr QPointF Outline[] = {
        {12, 2},      {13.6, 5.8},  {16.7, 4.4}, {16.1, 8.0}, {20.0, 7.6}, {18.0, 10.7}, {21.0, 12.3},
        {17.4, 13.7}, {18.7, 16.9}, {15.1, 16.1}, {12, 22},   {9.9, 16.1}, {6.3, 16.9},  {7.6, 13.7},
        {4, 12.3},    {7, 10.7},    {5, 7.6},     {8.9, 8.0}, {8.3, 4.4},  {11.4, 5.8},
    };
    QPainterPath path;
    const double scale = size / 20.0;
    for (std::size_t i = 0; i < std::size(Outline); ++i) {
        const QPointF point = (Outline[i] - QPointF{12, 12}) * scale;
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}

/// The home's display font (Fredoka, SIL Open Font License), loaded once.
QString DisplayFamily() {
    static const QString family = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Fredoka-SemiBold.ttf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QStringLiteral("Segoe UI") : families.first();
    }();
    return family;
}

QString BodyFamily() {
    static const QString family = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Nunito-Bold.ttf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QStringLiteral("Segoe UI") : families.first();
    }();
    return family;
}

/// `picture` scaled to `side` pixels with rounded corners.
QPixmap Rounded(const QPixmap& picture, int side, qreal ratio) {
    QPixmap out(QSize(side, side) * ratio);
    out.setDevicePixelRatio(ratio);
    out.fill(Qt::transparent);
    QPainter painter{&out};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(0, 0, side, side), side * 0.18, side * 0.18);
    painter.setClipPath(shape);
    painter.drawPixmap(QRect(0, 0, side, side), picture);
    return out;
}

constexpr int BarWidth = 560;
constexpr int IconSide = 132;

} // namespace

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);
    setAttribute(Qt::WA_StyledBackground, false);
    // The theme's sheet below styles them; their own (black, Arial) would win over it.
    for (QWidget* widget : std::initializer_list<QWidget*>{ui->stage, ui->value, ui->banner,
                                                           ui->progress_bar, ui->logo}) {
        widget->setStyleSheet({});
    }

    // The game's contents fade out over the theme's room, which stays: the game appears from it.
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(450);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::InOutCubic);

    // After the fade completes, hide the widget and reset the opacity
    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);

    // The middle column: the game's icon and name, the stage and its percentage over a slim bar,
    // the estimate, Skip while precaching, and a small game while shaders build.
    game_icon = new QLabel(this);
    game_icon->setObjectName(QStringLiteral("game_icon"));
    game_icon->setFixedSize(IconSide, IconSide);
    game_title = new QLabel(this);
    game_title->setObjectName(QStringLiteral("game_title"));
    game_title->setAlignment(Qt::AlignHCenter);
    tip = new QLabel(this);
    tip->setObjectName(QStringLiteral("tip"));
    tip->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    tip->setWordWrap(true);
    tip->setFixedWidth(BarWidth + 80);
    connect(&tip_timer, &QTimer::timeout, this, &LoadingScreen::NextTip);
    tip_timer.setInterval(6500);
    percent = new QLabel(this);
    percent->setObjectName(QStringLiteral("percent"));
    percent->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    ui->stage->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    ui->value->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    ui->progress_bar->setFixedSize(BarWidth, 10);
    ui->progress_bar->setTextVisible(false);

    skip_button = new QPushButton(tr("Skip"), this);
    skip_button->setObjectName(QStringLiteral("skip"));
    skip_button->setCursor(Qt::PointingHandCursor);
    skip_button->setToolTip(tr("Start the game now. The shaders not precached yet are precached "
                               "on a later launch."));
    skip_button->hide();
    leaf_catcher = new LeafCatcher(this);
    leaf_catcher->hide();

    QVBoxLayout* column = ui->verticalLayout;
    while (QLayoutItem* item = column->takeAt(0)) {
        delete item; // the widgets stay; only their places go
    }
    column->setSpacing(0);
    auto* stage_row = new QHBoxLayout;
    stage_row->setContentsMargins(0, 0, 0, 0);
    stage_row->addWidget(ui->stage, 1);
    stage_row->addWidget(percent);
    auto* stage_box = new QWidget(this);
    stage_box->setFixedWidth(BarWidth);
    stage_box->setLayout(stage_row);
    column->addStretch(1);
    column->addWidget(game_icon, 0, Qt::AlignHCenter);
    column->addSpacing(30); // room for the ring around the icon
    column->addWidget(game_title, 0, Qt::AlignHCenter);
    column->addSpacing(30);
    column->addWidget(stage_box, 0, Qt::AlignHCenter);
    column->addSpacing(10);
    column->addWidget(ui->progress_bar, 0, Qt::AlignHCenter);
    column->addSpacing(8);
    ui->value->setFixedWidth(BarWidth);
    column->addWidget(ui->value, 0, Qt::AlignHCenter);
    column->addSpacing(14);
    column->addWidget(tip, 0, Qt::AlignHCenter);
    column->addSpacing(18);
    column->addWidget(skip_button, 0, Qt::AlignHCenter);
    column->addSpacing(18);
    column->addWidget(leaf_catcher, 0, Qt::AlignHCenter);
    column->addStretch(1);

    connect(skip_button, &QPushButton::clicked, this, [this] {
        VideoCommon::ShaderWarm::RequestSkipWarm();
        skip_button->setEnabled(false);
    });
    qRegisterMetaType<VideoCore::LoadCallbackStage>();

    stage_translations = {
        {VideoCore::LoadCallbackStage::Prepare, tr("Loading...")},
        {VideoCore::LoadCallbackStage::Build, tr("Loading Shaders %1 / %2")},
        {VideoCore::LoadCallbackStage::Complete, tr("Launching...")},
        {VideoCore::LoadCallbackStage::Warm, tr("Precaching shaders %1 / %2")},
    };

    ambient_timer.setInterval(33);
    connect(&ambient_timer, &QTimer::timeout, this, [this] { update(); });
    ApplyTheme();
}

void LoadingScreen::ApplyTheme() {
    const QPalette colours = qApp->palette();
    const QColor accent = colours.color(QPalette::Highlight);
    const QColor text = colours.color(QPalette::WindowText);
    const QColor muted = colours.color(QPalette::PlaceholderText);
    const QString display = DisplayFamily();
    const QString body = BodyFamily();
    // The app's stylesheet paints every QWidget: here only this widget paints its background.
    setStyleSheet(QStringLiteral(R"(
#LoadingScreen QWidget { background: transparent; border: none; }
#LoadingScreen QLabel { color: %1; }
#LoadingScreen QLabel#game_title { font-family: "%4"; font-size: 34px; }
#LoadingScreen QLabel#stage { font-family: "%5"; font-size: 15px; color: %2; }
#LoadingScreen QLabel#stage[centred="true"] { font-size: 18px; color: %1; }
#LoadingScreen QLabel#percent { font-family: "%4"; font-size: 22px; color: %3; }
#LoadingScreen QLabel#value { font-family: "%5"; font-size: 13px; color: %2; }
#LoadingScreen QLabel#tip { font-family: "%5"; font-size: 14px; color: %2; }
#LoadingScreen QProgressBar { background: rgba(255, 255, 255, 18); border: none; border-radius: 5px; }
#LoadingScreen QProgressBar::chunk { border-radius: 5px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %6, stop:1 %7); }
#LoadingScreen QPushButton#skip { font-family: "%5"; font-size: 14px; color: %1; background: rgba(255, 255, 255, 14); border: 1px solid %3; border-radius: 17px; padding: 7px 26px; }
#LoadingScreen QPushButton#skip:hover { background: %3; color: #ffffff; }
#LoadingScreen QPushButton#skip:disabled { color: %2; border-color: %2; background: transparent; }
)")
                      .arg(text.name(), muted.name(), accent.name(), display, body,
                           accent.darker(135).name(), accent.lighter(125).name()));
}

void LoadingScreen::NextTip() {
    static const std::array tips{
        QT_TR_NOOP("Tip: Ctrl + H brings you back to the home screen."),
        QT_TR_NOOP("Tip: L + Plus + Minus on the controller closes the game."),
        QT_TR_NOOP("Tip: F11 switches to full screen."),
        QT_TR_NOOP("Tip: put your games in Documents\\Erable\\Jeux and they show up on the home "
                   "screen."),
        QT_TR_NOOP("Tip: Spring Maple and Winter Maple are in Configure > General > Interface."),
        QT_TR_NOOP("Tip: Debug mode > Record my session saves a log to attach to a report."),
    };
    tip->setText(tr(tips[tip_index % tips.size()]));
    ++tip_index;
}

void LoadingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    ApplyTheme(); // the theme may have changed since the last game
    if (!ambient_clock.isValid()) {
        ambient_clock.start();
    }
    ambient_timer.start();
}

void LoadingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    ambient_timer.stop();
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    std::vector<u8> buffer;
    if (loader.ReadBanner(buffer) == Loader::ResultStatus::Success) {
#ifdef ERABLE_QT_MOVIE_MISSING
        QPixmap map;
        map.loadFromData(buffer.data(), buffer.size());
        ui->banner->setPixmap(map);
#else
        backing_mem = std::make_unique<QByteArray>(reinterpret_cast<char*>(buffer.data()),
                                                   static_cast<int>(buffer.size()));
        backing_buf = std::make_unique<QBuffer>(backing_mem.get());
        backing_buf->open(QIODevice::ReadOnly);
        animation = std::make_unique<QMovie>(backing_buf.get(), QByteArray());
        animation->start();
        ui->banner->setMovie(animation.get());
#endif
        buffer.clear();
    }
    if (loader.ReadLogo(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        map.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
        ui->logo->setPixmap(map);
    }
    buffer.clear();
    QPixmap icon;
    if (loader.ReadIcon(buffer) == Loader::ResultStatus::Success &&
        icon.loadFromData(buffer.data(), static_cast<uint>(buffer.size()))) {
        game_icon->setPixmap(Rounded(icon, IconSide, devicePixelRatioF()));
        game_icon->show();
    } else {
        game_icon->hide();
    }
    std::string title;
    if (loader.ReadTitle(title) == Loader::ResultStatus::Success && !title.empty()) {
        game_title->setText(QString::fromStdString(title));
        game_title->show();
    } else {
        game_title->hide();
    }

    slow_shader_compile_start = false;
    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::OnLoadComplete() {
    leaf_catcher->SetRunning(false);
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    // reset the timer if the stage changes
    if (stage != previous_stage) {
        // The bar keeps its place while hidden, so the column does not jump.
        QSizePolicy policy = ui->progress_bar->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        ui->progress_bar->setSizePolicy(policy);
        ui->progress_bar->setVisible(stage != VideoCore::LoadCallbackStage::Prepare &&
                                     stage != VideoCore::LoadCallbackStage::Complete);
        skip_button->setVisible(stage == VideoCore::LoadCallbackStage::Warm);
        // Loading the game or launching it: a centred line, a ring and a moving light, and tips
        indeterminate = stage == VideoCore::LoadCallbackStage::Prepare ||
                        stage == VideoCore::LoadCallbackStage::Complete;
        ui->stage->setAlignment((indeterminate ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignBottom);
        ui->stage->setProperty("centred", indeterminate);
        ui->stage->style()->unpolish(ui->stage);
        ui->stage->style()->polish(ui->stage);
        percent->setVisible(!indeterminate);
        tip->setVisible(stage == VideoCore::LoadCallbackStage::Prepare);
        if (stage == VideoCore::LoadCallbackStage::Prepare) {
            NextTip();
            tip_timer.start();
        } else {
            tip_timer.stop();
        }
        skip_button->setEnabled(true);
        const bool building = stage == VideoCore::LoadCallbackStage::Build ||
                              stage == VideoCore::LoadCallbackStage::Warm;
        leaf_catcher->setVisible(building);
        leaf_catcher->SetRunning(building);
        previous_stage = stage;
        // reset back to fast shader compiling since the stage changed
        slow_shader_compile_start = false;
    }
    // update the max of the progress bar if the number of shaders change
    if (total != previous_total) {
        ui->progress_bar->setMaximum(static_cast<int>(total));
        previous_total = total;
    }
    // Launching: the bar stays full
    if (stage == VideoCore::LoadCallbackStage::Complete) {
        ui->progress_bar->setRange(0, 1);
        value = 1;
        total = 1;
        previous_total = 1;
    }

    QString estimate;
    // If there's a drastic slowdown in the rate, then display an estimate
    if (now - previous_time > milliseconds{50} || slow_shader_compile_start) {
        if (!slow_shader_compile_start) {
            slow_shader_start = steady_clock::now();
            slow_shader_compile_start = true;
            slow_shader_first_value = value;
        }
        // only calculate an estimate time after a second has passed since stage change
        const auto diff = duration_cast<milliseconds>(now - slow_shader_start);
        if (diff > seconds{1} && value > slow_shader_first_value) {
            const auto eta_mseconds =
                static_cast<long>(static_cast<double>(total - slow_shader_first_value) /
                                  (value - slow_shader_first_value) * diff.count());
            estimate =
                tr("Estimated Time %1")
                    .arg(QTime(0, 0, 0, 0)
                             .addMSecs(std::max<long>(eta_mseconds - diff.count() + 1000, 1000))
                             .toString(QStringLiteral("mm:ss")));
        }
    }

    // update labels and progress bar
    if (stage == VideoCore::LoadCallbackStage::Build ||
        stage == VideoCore::LoadCallbackStage::Warm) {
        ui->stage->setText(stage_translations[stage].arg(value).arg(total));
    } else {
        ui->stage->setText(stage_translations[stage]);
    }
    ui->value->setText(estimate);
    ui->progress_bar->setValue(static_cast<int>(value));
    const bool counted = total != 0 && stage != VideoCore::LoadCallbackStage::Prepare;
    percent->setText(counted ? QStringLiteral("%1 %").arg(value * 100 / total) : QString{});
    previous_time = now;
}

void LoadingScreen::paintEvent(QPaintEvent* event) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette colours = qApp->palette();
    const QColor window = colours.color(QPalette::Window);
    const QColor accent = colours.color(QPalette::Highlight);
    const double w = width();
    const double h = height();
    const double t = ambient_clock.isValid() ? ambient_clock.elapsed() / 1000.0 : 0.0;

    painter.fillRect(rect(), window.darker(135));
    // A warm (or green, or frosted) glow behind the middle column
    QRadialGradient glow{QPointF(w * 0.5, h * 0.42), std::max(w, h) * 0.6};
    glow.setColorAt(0, QColor(accent.red(), accent.green(), accent.blue(), 46));
    glow.setColorAt(0.45, QColor(window.red(), window.green(), window.blue(), 160));
    glow.setColorAt(1, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), glow);

    // Slow waves of light
    for (int wave = 0; wave < 4; ++wave) {
        QPainterPath path;
        const double base = h * (0.18 + wave * 0.2);
        const double amplitude = h * (0.03 + 0.012 * wave);
        for (int step = 0; step <= 48; ++step) {
            const double x = w * step / 48.0;
            const double y = base + amplitude * std::sin(x / w * 6.283 * (0.7 + 0.2 * wave) +
                                                         t * (0.1 + 0.04 * wave) + wave * 1.9);
            if (step == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        QLinearGradient stroke{QPointF(0, 0), QPointF(w, 0)};
        const int alpha = 34 - wave * 6;
        stroke.setColorAt(0, QColor(accent.red(), accent.green(), accent.blue(), 0));
        stroke.setColorAt(0.5, QColor(accent.red(), accent.green(), accent.blue(), alpha));
        stroke.setColorAt(1, QColor(accent.red(), accent.green(), accent.blue(), 0));
        painter.setPen(QPen{QBrush{stroke}, 2.0 + wave * 0.5});
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    // Maple leaves drifting down, each at its own pace, turning slowly
    painter.setPen(Qt::NoPen);
    for (int leaf = 0; leaf < 16; ++leaf) {
        const double seed = std::sin(leaf * 78.233) * 43758.5453;
        const double r1 = seed - std::floor(seed);
        const double r2 = std::fmod(r1 * 57.31, 1.0);
        const double fall = std::fmod(r2 + t * (0.012 + 0.018 * r1), 1.0);
        const double x = w * r1 + std::sin(t * 0.35 + leaf) * 26;
        const double y = (h + 80) * fall - 40;
        const double size = 14 + 22 * r2;
        const int alpha = static_cast<int>((26 + 34 * r1) * std::sin(fall * 3.1416));
        painter.save();
        painter.translate(x, y);
        painter.rotate(t * (20 + 40 * r2) * (leaf % 2 == 0 ? 1 : -1) + 360 * r1);
        painter.fillPath(MapleLeaf(size), QColor(accent.red(), accent.green(), accent.blue(), alpha));
        painter.restore();
    }

    if (indeterminate) {
        // Faded with the rest of the contents when the game appears
        const double shown = opacity_effect->opacity();
        // A ring around the game's icon, an arc of the accent turning on it
        if (game_icon->isVisible()) {
            const QRectF icon{QRectF(game_icon->mapTo(this, QPoint(0, 0)), game_icon->size())};
            const QRectF ring = icon.adjusted(-14, -14, 14, 14);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor(255, 255, 255, static_cast<int>(20 * shown)), 4});
            painter.drawRoundedRect(ring, ring.width() * 0.24, ring.width() * 0.24);
            QConicalGradient sweep{ring.center(), -t * 220.0};
            sweep.setColorAt(0, QColor(accent.red(), accent.green(), accent.blue(),
                                       static_cast<int>(255 * shown)));
            sweep.setColorAt(0.35, QColor(accent.red(), accent.green(), accent.blue(), 0));
            sweep.setColorAt(1, QColor(accent.red(), accent.green(), accent.blue(), 0));
            painter.setPen(QPen{QBrush{sweep}, 4, Qt::SolidLine, Qt::RoundCap});
            painter.drawRoundedRect(ring, ring.width() * 0.24, ring.width() * 0.24);
        }
        // Where the bar goes: a track and a light moving along it
        const QRectF bar{QRectF(ui->progress_bar->mapTo(this, QPoint(0, 0)),
                                ui->progress_bar->size())};
        QPainterPath track;
        track.addRoundedRect(bar, bar.height() / 2, bar.height() / 2);
        painter.setPen(Qt::NoPen);
        painter.fillPath(track, QColor(255, 255, 255, static_cast<int>(18 * shown)));
        const double run = std::fmod(t * 0.55, 1.0) * (bar.width() + bar.width() * 0.35) -
                           bar.width() * 0.35;
        QLinearGradient light{QPointF(bar.left() + run, 0),
                              QPointF(bar.left() + run + bar.width() * 0.35, 0)};
        light.setColorAt(0, QColor(accent.red(), accent.green(), accent.blue(), 0));
        light.setColorAt(0.5, QColor(accent.red(), accent.green(), accent.blue(),
                                     static_cast<int>(230 * shown)));
        light.setColorAt(1, QColor(accent.red(), accent.green(), accent.blue(), 0));
        painter.save();
        painter.setClipPath(track);
        painter.fillRect(bar, light);
        painter.restore();
    }

    // Darker edges, so the middle reads first
    QRadialGradient vignette{QPointF(w * 0.5, h * 0.5), std::max(w, h) * 0.75};
    vignette.setColorAt(0.6, QColor(0, 0, 0, 0));
    vignette.setColorAt(1, QColor(0, 0, 0, 110));
    painter.fillRect(rect(), vignette);
    QWidget::paintEvent(event);
}

void LoadingScreen::Clear() {
#ifndef ERABLE_QT_MOVIE_MISSING
    animation.reset();
    backing_buf.reset();
    backing_mem.reset();
#endif
}
