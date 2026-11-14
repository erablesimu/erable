// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/leaf_catcher.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numbers>
#include <random>

#include <QApplication>
#include <QKeyEvent>

#include "common/settings_input.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

namespace {

constexpr double BasketBase = 0.15;  ///< Basket width, of the field's width
constexpr double BasketSpeed = 1.3;  ///< Field widths per second with an arrow held
constexpr double CatchLine = 0.84;   ///< Leaves reaching this height meet the basket's rim
constexpr double Ground = 0.95;      ///< Where missed leaves come to rest
constexpr double WideSeconds = 8;    ///< How long a drop of syrup widens the basket
constexpr QColor Gold{0xff, 0xc8, 0x3d};
constexpr QColor Syrup{0xc9, 0x7a, 0x1e};

std::mt19937& Random() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

double Uniform(double low, double high) {
    return std::uniform_real_distribution<double>{low, high}(Random());
}

QColor WithAlpha(QColor color, int alpha) {
    color.setAlpha(std::clamp(alpha, 0, 255));
    return color;
}

/// The accent, a little lighter or darker: leaves of one tree are not all the same.
QColor Shade(const QColor& accent, double shade) {
    return shade >= 0 ? accent.lighter(100 + static_cast<int>(shade * 35))
                      : accent.darker(100 + static_cast<int>(-shade * 40));
}

/// A maple leaf `size` wide, centred on the origin (the outline of Erable's icon).
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

/// A leaf with its veins and stem, in `color`, centred on the origin.
void DrawLeaf(QPainter& painter, double size, const QColor& color) {
    const QPainterPath outline = MapleLeaf(size);
    QLinearGradient fill{QPointF(-size / 2, -size / 2), QPointF(size / 2, size / 2)};
    fill.setColorAt(0, color.lighter(125));
    fill.setColorAt(1, color.darker(125));
    painter.fillPath(outline, fill);
    const double s = size / 20.0;
    painter.setPen(QPen{WithAlpha(color.darker(160), 170), std::max(0.8, size * 0.04),
                        Qt::SolidLine, Qt::RoundCap});
    // Stem, then the veins to the three main lobes
    painter.drawLine(QPointF(0, 10 * s), QPointF(0, 6 * s));
    painter.drawLine(QPointF(0, 4 * s), QPointF(0, -8 * s));
    painter.drawLine(QPointF(0, 3 * s), QPointF(-6.5 * s, -3 * s));
    painter.drawLine(QPointF(0, 3 * s), QPointF(6.5 * s, -3 * s));
    painter.drawLine(QPointF(0, 5 * s), QPointF(-4.5 * s, 3.5 * s));
    painter.drawLine(QPointF(0, 5 * s), QPointF(4.5 * s, 3.5 * s));
    painter.setPen(Qt::NoPen);
}

/// A drop of maple syrup `size` high, centred on the origin.
QPainterPath SyrupDrop(double size) {
    QPainterPath path;
    const double r = size * 0.32;
    path.moveTo(0, -size / 2);
    path.cubicTo(QPointF(r * 0.4, -size * 0.15), QPointF(r, 0), QPointF(r, size * 0.18));
    path.arcTo(QRectF(-r, size * 0.18 - r, 2 * r, 2 * r), 0, -180);
    path.cubicTo(QPointF(-r, 0), QPointF(-r * 0.4, -size * 0.15), QPointF(0, -size / 2));
    return path;
}

} // namespace

LeafCatcher::LeafCatcher(QWidget* parent) : QWidget{parent} {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::BlankCursor);
    setMinimumSize(360, 220);
    frame_timer.setInterval(16);
    connect(&frame_timer, &QTimer::timeout, this, &LeafCatcher::Step);
}

QSize LeafCatcher::sizeHint() const {
    return {620, 290};
}

void LeafCatcher::SetRunning(bool running) {
    if (running == frame_timer.isActive()) {
        return;
    }
    if (running) {
        clock.start();
        frame_timer.start();
        setFocus();
    } else {
        frame_timer.stop();
    }
}

double LeafCatcher::BasketWidth() const {
    return BasketBase * (wide > 0 ? 1.6 : 1.0);
}

void LeafCatcher::Drop() {
    // Leaves come faster and fall quicker the longer one plays, up to a gentle cap.
    const double pace = std::min(1.0, elapsed / 90.0);
    const double roll = Uniform(0, 1);
    const Kind kind = roll < 0.03 && wide <= 0 ? Kind::Syrup : roll < 0.11 ? Kind::Golden : Kind::Leaf;
    leaves.push_back(Leaf{
        .x = Uniform(0.06, 0.94),
        .y = -0.08,
        .speed = Uniform(0.2, 0.3) * (1.0 + pace) * (kind == Kind::Syrup ? 1.25 : 1.0),
        .angle = Uniform(0, 360),
        .spin = kind == Kind::Syrup ? 0 : Uniform(-140, 140),
        .sway = Uniform(0, 2 * std::numbers::pi),
        .shade = Uniform(-1, 1),
        .kind = kind,
    });
    next_drop = Uniform(0.5, 1.0) * (1.0 - 0.5 * pace);
}

void LeafCatcher::Catch(const Leaf& leaf) {
    const QColor accent = qApp->palette().color(QPalette::Highlight);
    QColor color = Shade(accent, leaf.shade);
    QString text;
    if (leaf.kind == Kind::Syrup) {
        wide = WideSeconds;
        banner = 2.2;
        banner_text = tr("Maple syrup! Wider basket");
        color = Syrup;
        text = QStringLiteral("+");
    } else {
        ++combo;
        const int points = (leaf.kind == Kind::Golden ? 5 : 1) * std::min(combo, 5);
        score += points;
        best = std::max(best, score);
        color = leaf.kind == Kind::Golden ? Gold : color;
        text = QStringLiteral("+%1").arg(points);
    }
    squash = 1;
    popups.push_back({leaf.x, CatchLine - 0.06, 1.0, text, color});
    const int count = leaf.kind == Kind::Leaf ? 8 : 16;
    for (int i = 0; i < count; ++i) {
        const double angle = Uniform(-std::numbers::pi, 0);
        const double speed = Uniform(0.15, 0.45);
        sparks.push_back({leaf.x, CatchLine, std::cos(angle) * speed, std::sin(angle) * speed * 1.6,
                          1.0, leaf.kind == Kind::Leaf ? color.lighter(130) : color});
    }
}

double LeafCatcher::ControllerAxis() const {
    if (hid_core == nullptr) {
        return 0;
    }
    for (const auto id : {Core::HID::NpadIdType::Player1, Core::HID::NpadIdType::Handheld}) {
        const auto* controller = hid_core->GetEmulatedController(id);
        if (controller == nullptr || !controller->IsConnected()) {
            continue;
        }
        const auto buttons = controller->GetButtonsValues();
        const double pad = (buttons[Settings::NativeButton::DRight].value ? 1.0 : 0.0) -
                           (buttons[Settings::NativeButton::DLeft].value ? 1.0 : 0.0);
        if (pad != 0) {
            return pad;
        }
        // The left stick, with a small dead zone and a gentle start
        const double stick = controller->GetSticksValues()[Settings::NativeAnalog::LStick].x.value;
        if (std::abs(stick) > 0.18) {
            const double amount = (std::abs(stick) - 0.18) / 0.82;
            return std::copysign(std::min(1.0, amount * 1.25), stick);
        }
    }
    return 0;
}

void LeafCatcher::Advance(double dt) {
    elapsed += dt;
    // The basket follows the arrows or a controller, or eases towards the mouse
    const double axis = std::clamp(direction + ControllerAxis(), -1.0, 1.0);
    target = std::clamp(target + axis * BasketSpeed * dt, 0.0, 1.0);
    const double half = BasketWidth() / 2;
    const double before_x = basket;
    basket = std::clamp(basket + (target - basket) * (1.0 - std::exp(-dt * 16.0)), half, 1 - half);
    velocity = dt > 0 ? (basket - before_x) / dt : 0;
    squash = std::max(0.0, squash - dt * 4);
    wide = std::max(0.0, wide - dt);
    banner = std::max(0.0, banner - dt);

    // Now and then, a gust of wind pushes the leaves aside
    next_gust -= dt;
    if (next_gust <= 0 && gust <= 0) {
        gust = 4.0;
        wind = Uniform(0, 1) < 0.5 ? -0.22 : 0.22;
        banner = 2.0;
        banner_text = tr("Gust of wind!");
        next_gust = Uniform(18, 30);
    }
    if (gust > 0) {
        gust = std::max(0.0, gust - dt);
    }
    const double push = gust > 0 ? wind * std::sin(std::min(1.0, gust / 4.0) * std::numbers::pi) : 0;

    next_drop -= dt;
    if (next_drop <= 0) {
        Drop();
    }
    const double catch_half = half + 0.02;
    for (Leaf& leaf : leaves) {
        const double before = leaf.y;
        leaf.y += leaf.speed * dt;
        leaf.angle += leaf.spin * dt;
        if (leaf.kind != Kind::Syrup) {
            // A slow sway, so leaves drift rather than drop
            leaf.x += std::sin(leaf.y * 7.0 + leaf.sway) * 0.07 * dt;
        }
        leaf.x = std::clamp(leaf.x + push * dt, 0.02, 0.98);
        if (before < CatchLine && leaf.y >= CatchLine && std::abs(leaf.x - basket) < catch_half) {
            Catch(leaf);
            leaf.y = 2; // caught: removed below
        } else if (before < Ground && leaf.y >= Ground) {
            if (leaf.kind != Kind::Syrup) {
                combo = 0;
                fallen.push_back({leaf.x, leaf.angle, leaf.shade, 1.0});
            }
            leaf.y = 2;
        }
    }
    std::erase_if(leaves, [](const Leaf& leaf) { return leaf.y > 1.1; });

    for (Spark& spark : sparks) {
        spark.x += spark.vx * dt;
        spark.y += spark.vy * dt;
        spark.vy += 0.9 * dt;
        spark.life -= dt * 1.6;
    }
    std::erase_if(sparks, [](const Spark& spark) { return spark.life <= 0; });
    for (Popup& popup : popups) {
        popup.y -= 0.12 * dt;
        popup.life -= dt * 1.1;
    }
    std::erase_if(popups, [](const Popup& popup) { return popup.life <= 0; });
    for (Fallen& leaf : fallen) {
        leaf.life -= dt * 0.25;
    }
    std::erase_if(fallen, [](const Fallen& leaf) { return leaf.life <= 0; });
}

void LeafCatcher::Step() {
    Advance(std::min(0.05, clock.restart() / 1000.0));
    update();
}

void LeafCatcher::PaintScenery(QPainter& painter, const QRectF& field) const {
    // The application's palette: the stylesheet makes this widget's highlight translucent.
    const QPalette colours = qApp->palette();
    const QColor accent = colours.color(QPalette::Highlight);
    const QColor window = colours.color(QPalette::Window);
    const double w = field.width();
    const double h = field.height();

    // Dusk sky in the theme's colours, a low sun behind the hills
    QLinearGradient sky{field.topLeft(), field.bottomLeft()};
    sky.setColorAt(0, window.lighter(115));
    sky.setColorAt(0.75, window.lighter(150));
    sky.setColorAt(1, window.lighter(170));
    painter.fillRect(field, sky);
    QRadialGradient sun{QPointF(w * 0.72, h * 0.62), h * 0.55};
    sun.setColorAt(0, WithAlpha(accent.lighter(130), 90));
    sun.setColorAt(1, WithAlpha(accent, 0));
    painter.fillRect(field, sun);

    // Two rows of hills, the far one lighter
    const auto hills = [&](double base, double height, double phase, const QColor& color) {
        QPainterPath path;
        path.moveTo(0, h);
        for (int step = 0; step <= 40; ++step) {
            const double x = w * step / 40.0;
            const double y = h * base - h * height *
                                            (0.6 * std::sin(x / w * 5.0 + phase) +
                                             0.4 * std::sin(x / w * 11.0 + phase * 2.3));
            path.lineTo(x, y);
        }
        path.lineTo(w, h);
        path.closeSubpath();
        painter.fillPath(path, color);
    };
    hills(0.72, 0.06, 0.8, window.lighter(122));
    hills(0.82, 0.05, 2.1, window.lighter(108));

    // Maple trees on the near hills: a trunk and a round canopy in the accent's shades
    const auto tree = [&](double x, double scale, double shade) {
        const double base = h * 0.86;
        const double trunk = h * 0.2 * scale;
        painter.setPen(QPen{window.darker(130), std::max(2.0, w * 0.006 * scale), Qt::SolidLine,
                            Qt::RoundCap});
        painter.drawLine(QPointF(x, base), QPointF(x, base - trunk));
        painter.setPen(Qt::NoPen);
        const QColor canopy = WithAlpha(Shade(accent, shade), 120);
        const double r = h * 0.11 * scale;
        for (const QPointF& blob : {QPointF(0, -1.0), QPointF(-0.7, -0.55), QPointF(0.7, -0.55),
                                    QPointF(-0.35, -1.45), QPointF(0.4, -1.4)}) {
            painter.setBrush(canopy);
            painter.drawEllipse(QPointF(x + blob.x() * r, base - trunk + blob.y() * r * 0.6 + r * 0.3),
                                r * 0.75, r * 0.7);
        }
        painter.setBrush(Qt::NoBrush);
    };
    tree(w * 0.07, 1.0, -0.6);
    tree(w * 0.16, 0.7, 0.3);
    tree(w * 0.9, 1.1, -0.2);

    // The ground, with the leaves that were missed resting on it for a while
    painter.fillRect(QRectF(0, h * Ground, w, h * (1 - Ground)), window.darker(115));
    const double unit = std::min(w, h * 2.2);
    for (const Fallen& leaf : fallen) {
        painter.save();
        painter.translate(leaf.x * w, h * Ground + 2);
        painter.scale(1.0, 0.45);
        painter.rotate(leaf.angle);
        painter.setOpacity(leaf.life * 0.8);
        DrawLeaf(painter, unit * 0.05, Shade(accent, leaf.shade).darker(130));
        painter.restore();
    }
}

void LeafCatcher::PaintLeaf(QPainter& painter, const Leaf& leaf, const QRectF& field,
                            double unit) const {
    const QColor accent = qApp->palette().color(QPalette::Highlight);
    const QPointF centre{leaf.x * field.width(), leaf.y * field.height()};
    painter.save();
    painter.translate(centre);
    if (leaf.kind == Kind::Syrup) {
        const double size = unit * 0.075;
        QRadialGradient halo{QPointF(0, 0), size};
        halo.setColorAt(0, WithAlpha(Syrup.lighter(140), 120));
        halo.setColorAt(1, WithAlpha(Syrup, 0));
        painter.fillRect(QRectF(-size, -size, 2 * size, 2 * size), halo);
        QLinearGradient fill{QPointF(-size / 3, -size / 2), QPointF(size / 3, size / 2)};
        fill.setColorAt(0, Syrup.lighter(150));
        fill.setColorAt(1, Syrup.darker(120));
        painter.fillPath(SyrupDrop(size), fill);
        painter.setBrush(WithAlpha(Qt::white, 170));
        painter.drawEllipse(QPointF(-size * 0.1, size * 0.08), size * 0.07, size * 0.12);
        painter.restore();
        return;
    }
    const bool golden = leaf.kind == Kind::Golden;
    const double size = unit * (golden ? 0.078 : 0.066);
    // A soft shadow below the leaf
    painter.save();
    painter.translate(size * 0.12, size * 0.18);
    painter.rotate(leaf.angle);
    painter.fillPath(MapleLeaf(size), QColor(0, 0, 0, 50));
    painter.restore();
    if (golden) {
        const double pulse = 0.75 + 0.25 * std::sin(elapsed * 6 + leaf.sway);
        QRadialGradient glow{QPointF(0, 0), size * 1.1};
        glow.setColorAt(0, WithAlpha(Gold, static_cast<int>(150 * pulse)));
        glow.setColorAt(1, WithAlpha(Gold, 0));
        painter.fillRect(QRectF(-size * 1.1, -size * 1.1, size * 2.2, size * 2.2), glow);
    }
    painter.rotate(leaf.angle);
    DrawLeaf(painter, size, golden ? Gold : Shade(accent, leaf.shade));
    painter.restore();
    if (golden) {
        // A twinkle that circles the golden leaf
        const double turn = elapsed * 3 + leaf.sway;
        const QPointF star = centre + QPointF(std::cos(turn), std::sin(turn)) * size * 0.75;
        painter.setPen(QPen{WithAlpha(Qt::white, 220), 1.5, Qt::SolidLine, Qt::RoundCap});
        const double arm = size * 0.16;
        painter.drawLine(star - QPointF(arm, 0), star + QPointF(arm, 0));
        painter.drawLine(star - QPointF(0, arm), star + QPointF(0, arm));
        painter.setPen(Qt::NoPen);
    }
}

void LeafCatcher::PaintBasket(QPainter& painter, const QRectF& field) const {
    const QPalette colours = qApp->palette();
    const QColor accent = colours.color(QPalette::Highlight);
    const double width = BasketWidth() * field.width() * (1.0 + 0.08 * squash);
    const double height = field.height() * 0.12 * (1.0 - 0.12 * squash);
    const QPointF rim{basket * field.width(), field.height() * CatchLine};
    painter.save();
    painter.translate(rim);
    painter.rotate(std::clamp(velocity * 18.0, -14.0, 14.0)); // leans the way it moves

    // A woven wicker basket: a tapered body, its weave in bands, a rim and a handle
    const QColor wicker{0xc4, 0x8a, 0x52};
    QPainterPath body;
    body.moveTo(-width / 2, 0);
    body.lineTo(width / 2, 0);
    body.quadTo(width * 0.46, height * 0.9, width * 0.36, height);
    body.lineTo(-width * 0.36, height);
    body.quadTo(-width * 0.46, height * 0.9, -width / 2, 0);
    QLinearGradient shade{QPointF(-width / 2, 0), QPointF(width / 2, 0)};
    shade.setColorAt(0, wicker.darker(140));
    shade.setColorAt(0.4, wicker.lighter(110));
    shade.setColorAt(1, wicker.darker(150));
    painter.fillPath(body, shade);
    painter.save();
    painter.setClipPath(body);
    painter.setPen(QPen{wicker.darker(170), std::max(1.0, height * 0.05)});
    for (int band = 1; band < 4; ++band) {
        const double y = height * band / 4.0;
        painter.drawLine(QPointF(-width / 2, y), QPointF(width / 2, y));
    }
    for (int stake = -3; stake <= 3; ++stake) {
        const double x = width * stake / 8.0;
        painter.drawLine(QPointF(x, 0), QPointF(x * 0.75, height));
    }
    painter.restore();
    // When the syrup widens it, the rim glows amber; else in the accent
    const QColor rim_colour = wide > 0 ? Syrup.lighter(130) : accent;
    painter.setPen(QPen{rim_colour, std::max(3.0, height * 0.14), Qt::SolidLine, Qt::RoundCap});
    painter.drawLine(QPointF(-width / 2, 0), QPointF(width / 2, 0));
    painter.setPen(QPen{wicker.darker(120), std::max(2.0, height * 0.08), Qt::SolidLine, Qt::RoundCap});
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(QRectF(-width * 0.32, -height * 0.75, width * 0.64, height * 1.5), 0, 180 * 16);
    painter.restore();
}

void LeafCatcher::PaintHud(QPainter& painter, const QRectF& field) const {
    const QPalette colours = qApp->palette();
    const QColor accent = colours.color(QPalette::Highlight);
    const QColor text = colours.color(QPalette::WindowText);
    QFont font = painter.font();
    font.setBold(true);

    // The score in a pill, the streak multiplier next to it, the best on the right
    font.setPixelSize(16);
    painter.setFont(font);
    const QString score_text = QString::number(score);
    const QRectF pill{12, 10, painter.fontMetrics().horizontalAdvance(score_text) + 44.0, 28};
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 90));
    painter.drawRoundedRect(pill, 14, 14);
    painter.save();
    painter.translate(pill.left() + 15, pill.center().y());
    DrawLeaf(painter, 15, accent);
    painter.restore();
    painter.setPen(text);
    painter.drawText(pill.adjusted(28, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, score_text);
    if (combo >= 2) {
        const double pulse = 1.0 + 0.08 * std::sin(elapsed * 10);
        const QRectF chip{pill.right() + 8, pill.top() + 2, 44 * pulse, 24};
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(chip, 12, 12);
        painter.setPen(Qt::white);
        font.setPixelSize(13);
        painter.setFont(font);
        painter.drawText(chip, Qt::AlignCenter, QStringLiteral("x%1").arg(std::min(combo, 5)));
    }
    font.setPixelSize(13);
    painter.setFont(font);
    painter.setPen(WithAlpha(text, 170));
    painter.drawText(field.adjusted(16, 14, -16, 0), Qt::AlignTop | Qt::AlignRight,
                     tr("Best %1").arg(best));

    // Points rising from the basket
    font.setPixelSize(18);
    painter.setFont(font);
    for (const Popup& popup : popups) {
        painter.setPen(WithAlpha(popup.color.lighter(120), static_cast<int>(255 * popup.life)));
        const QPointF at{popup.x * field.width(), popup.y * field.height()};
        painter.drawText(QRectF(at.x() - 40, at.y() - 14, 80, 28), Qt::AlignCenter, popup.text);
    }

    // The event banner (a gust, the syrup), or the hint before the first catch
    if (banner > 0) {
        font.setPixelSize(20);
        painter.setFont(font);
        painter.setPen(WithAlpha(text, static_cast<int>(255 * std::min(1.0, banner * 2))));
        painter.drawText(field.adjusted(0, field.height() * 0.3, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                         banner_text);
    } else if (score == 0 && elapsed < 8) {
        font.setPixelSize(14);
        font.setBold(false);
        painter.setFont(font);
        painter.setPen(WithAlpha(text, 200));
        painter.drawText(field.adjusted(0, field.height() * 0.32, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                         tr("Catch the leaves while the shaders load"));
    }
}

void LeafCatcher::paintEvent(QPaintEvent*) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF field = rect();
    QPainterPath frame;
    frame.addRoundedRect(field.adjusted(1, 1, -1, -1), 18, 18);
    painter.setClipPath(frame);
    PaintScenery(painter, field);

    const double unit = std::min(field.width(), field.height() * 2.2);
    for (const Leaf& leaf : leaves) {
        PaintLeaf(painter, leaf, field, unit);
    }
    PaintBasket(painter, field);
    for (const Spark& spark : sparks) {
        painter.setBrush(WithAlpha(spark.color, static_cast<int>(255 * spark.life)));
        painter.drawEllipse(QPointF(spark.x * field.width(), spark.y * field.height()),
                            2.5 * spark.life + 0.8, 2.5 * spark.life + 0.8);
    }
    if (gust > 0) {
        // Streaks of wind across the field
        painter.setPen(QPen{QColor(255, 255, 255, 40), 1.5, Qt::SolidLine, Qt::RoundCap});
        for (int streak = 0; streak < 6; ++streak) {
            const double y = field.height() * (0.15 + 0.12 * streak);
            const double shift = std::fmod(elapsed * 0.9 + streak * 0.37, 1.0);
            const double x = wind > 0 ? shift * field.width() : (1 - shift) * field.width();
            painter.drawLine(QPointF(x, y), QPointF(x - (wind > 0 ? 40 : -40), y));
        }
        painter.setPen(Qt::NoPen);
    }
    PaintHud(painter, field);

    painter.setClipping(false);
    painter.setPen(QPen{QColor{255, 255, 255, 26}, 1});
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(frame);
}

void LeafCatcher::mouseMoveEvent(QMouseEvent* event) {
    target = event->position().x() / std::max(1, width());
}

void LeafCatcher::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Left) {
        direction = -1;
    } else if (event->key() == Qt::Key_Right) {
        direction = 1;
    } else {
        QWidget::keyPressEvent(event);
        return;
    }
    target = basket;
    event->accept();
}

void LeafCatcher::keyReleaseEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Left && direction < 0) ||
        (event->key() == Qt::Key_Right && direction > 0)) {
        direction = 0;
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}
