// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/leaf_catcher.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr double BucketWidth = 0.16;  ///< Of the field's width
constexpr double BucketSpeed = 1.1;   ///< Field widths per second with an arrow held
constexpr double CatchLine = 0.86;    ///< Leaves reaching this height meet the bucket's rim

std::mt19937& Random() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

double Uniform(double low, double high) {
    return std::uniform_real_distribution<double>{low, high}(Random());
}

/// A maple leaf of `size` pixels, centred on the origin (the outline of Erable's site icon).
QPainterPath MapleLeaf(double size) {
    static constexpr QPointF Outline[] = {
        {12, 2},    {13.6, 5.8}, {16.7, 4.4}, {16.1, 8.0}, {20.0, 7.6}, {18.0, 10.7}, {21.0, 12.3},
        {17.4, 13.7}, {18.7, 16.9}, {15.1, 16.1}, {12, 22},  {9.9, 16.1}, {6.3, 16.9},  {7.6, 13.7},
        {4, 12.3},  {7, 10.7},   {5, 7.6},    {8.9, 8.0},  {8.3, 4.4},  {11.4, 5.8},
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

} // namespace

LeafCatcher::LeafCatcher(QWidget* parent) : QWidget{parent} {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::BlankCursor);
    setMinimumSize(320, 200);
    frame_timer.setInterval(16);
    connect(&frame_timer, &QTimer::timeout, this, &LeafCatcher::Step);
}

QSize LeafCatcher::sizeHint() const {
    return {560, 260};
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

void LeafCatcher::Drop() {
    // Leaves come faster and fall quicker the longer one plays, up to a gentle cap.
    const double pace = std::min(1.0, elapsed / 90.0);
    leaves.push_back(Leaf{
        .x = Uniform(0.05, 0.95),
        .y = -0.08,
        .speed = Uniform(0.22, 0.32) * (1.0 + pace),
        .angle = Uniform(0, 360),
        .spin = Uniform(-120, 120),
        .golden = Uniform(0, 1) < 0.08,
    });
    next_drop = Uniform(0.55, 1.1) * (1.0 - 0.5 * pace);
}

void LeafCatcher::Step() {
    const double dt = std::min(0.05, clock.restart() / 1000.0);
    elapsed += dt;
    bucket = std::clamp(bucket + direction * BucketSpeed * dt, BucketWidth / 2, 1 - BucketWidth / 2);
    next_drop -= dt;
    if (next_drop <= 0) {
        Drop();
    }
    for (Leaf& leaf : leaves) {
        const double before = leaf.y;
        leaf.y += leaf.speed * dt;
        leaf.angle += leaf.spin * dt;
        // A slow sway, so leaves drift rather than drop
        leaf.x += std::sin((leaf.y + leaf.angle / 360.0) * 6.0) * 0.06 * dt;
        if (before < CatchLine && leaf.y >= CatchLine &&
            std::abs(leaf.x - bucket) < BucketWidth / 2 + 0.02) {
            ++combo;
            score += (leaf.golden ? 5 : 1) * std::min(combo, 5);
            best = std::max(best, score);
            leaf.y = 2; // caught: removed below
        } else if (before < 1.0 && leaf.y >= 1.0) {
            combo = 0;
        }
    }
    std::erase_if(leaves, [](const Leaf& leaf) { return leaf.y > 1.1; });
    update();
}

void LeafCatcher::paintEvent(QPaintEvent*) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF field = rect();
    // In the theme's colours: Maple's warm greys, Spring's green, Winter's frost
    const QPalette colours = palette();
    const QColor accent = colours.color(QPalette::Highlight);
    const QColor window = colours.color(QPalette::Window);
    const QColor text = colours.color(QPalette::WindowText);
    const QColor muted = colours.color(QPalette::PlaceholderText);
    QLinearGradient sky{field.topLeft(), field.bottomLeft()};
    const QColor sky_top = window.lighter(125);
    sky.setColorAt(0, QColor{sky_top.red(), sky_top.green(), sky_top.blue(), 120});
    sky.setColorAt(1, QColor{window.red(), window.green(), window.blue(), 170});
    QPainterPath frame;
    frame.addRoundedRect(field.adjusted(1, 1, -1, -1), 18, 18);
    painter.fillPath(frame, sky);
    painter.setPen(QPen{QColor{255, 255, 255, 22}, 1});
    painter.drawPath(frame);
    painter.setPen(Qt::NoPen);
    painter.setClipPath(frame);

    const double unit = std::min(field.width(), field.height() * 2.2);
    for (const Leaf& leaf : leaves) {
        painter.save();
        painter.translate(leaf.x * field.width(), leaf.y * field.height());
        painter.rotate(leaf.angle);
        const QColor color = leaf.golden ? QColor{0xff, 0xc8, 0x3d} : accent;
        painter.fillPath(MapleLeaf(unit * (leaf.golden ? 0.075 : 0.065)), color);
        painter.restore();
    }

    // The sap bucket: a tapered pail with a band and a handle
    const double width = BucketWidth * field.width();
    const double height = field.height() * 0.13;
    const double left = bucket * field.width() - width / 2;
    const double top = field.height() * CatchLine;
    QPainterPath pail;
    pail.moveTo(left, top);
    pail.lineTo(left + width, top);
    pail.lineTo(left + width * 0.9, top + height);
    pail.lineTo(left + width * 0.1, top + height);
    pail.closeSubpath();
    painter.fillPath(pail, muted.lighter(150));
    painter.setPen(QPen{muted, 3});
    painter.drawLine(QPointF(left + width * 0.05, top + height * 0.35),
                     QPointF(left + width * 0.95, top + height * 0.35));
    painter.setPen(QPen{muted.lighter(150), 2});
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(QRectF(left + width * 0.15, top - height * 0.6, width * 0.7, height * 1.2), 0,
                    180 * 16);

    painter.setClipping(false);
    painter.setPen(text);
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    const QString line = tr("Leaves: %1   Best: %2").arg(score).arg(best) +
                         (combo >= 2 ? tr("   Streak x%1").arg(std::min(combo, 5)) : QString{});
    painter.drawText(field.adjusted(16, 10, -16, -10), Qt::AlignTop | Qt::AlignLeft, line);
    painter.setPen(muted);
    painter.drawText(field.adjusted(16, 10, -16, -10), Qt::AlignTop | Qt::AlignRight,
                     tr("Catch the leaves while the shaders load"));
}

void LeafCatcher::mouseMoveEvent(QMouseEvent* event) {
    bucket = std::clamp(event->position().x() / std::max(1, width()), BucketWidth / 2,
                        1 - BucketWidth / 2);
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
