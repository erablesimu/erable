// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QColor>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QWidget>

/// A small game for the shader loading screen: move the wicker basket (mouse, or the left and
/// right arrows) to catch the maple leaves that fall in a little autumn valley. A golden leaf is
/// worth five, catches in a row multiply the points, a drop of maple syrup widens the basket for a
/// while, and gusts of wind push the leaves aside. Nothing is lost by missing one, so it can be
/// left alone at any time. Drawn in the theme's colours.
class LeafCatcher : public QWidget {
    Q_OBJECT

public:
    explicit LeafCatcher(QWidget* parent = nullptr);

    /// Starts or pauses the game (paused while hidden).
    void SetRunning(bool running);

    /// Advances the game by `seconds` (what the frame timer does; also used to render previews).
    void Advance(double seconds);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    enum class Kind { Leaf, Golden, Syrup };

    struct Leaf {
        double x;     ///< 0..1 across the field
        double y;     ///< 0..1 down the field
        double speed; ///< Field heights per second
        double angle; ///< Degrees
        double spin;  ///< Degrees per second
        double sway;  ///< Phase of its side-to-side drift
        double shade; ///< -1..1: how much lighter or darker than the accent
        Kind kind;
    };
    struct Spark {
        double x, y, vx, vy, life; ///< life: 1 to 0
        QColor color;
    };
    struct Popup {
        double x, y, life;
        QString text;
        QColor color;
    };
    struct Fallen {
        double x, angle, shade, life;
    };

    void Step();
    void Drop();
    void Catch(const Leaf& leaf);
    [[nodiscard]] double BasketWidth() const;

    void PaintScenery(QPainter& painter, const QRectF& field) const;
    void PaintLeaf(QPainter& painter, const Leaf& leaf, const QRectF& field, double unit) const;
    void PaintBasket(QPainter& painter, const QRectF& field) const;
    void PaintHud(QPainter& painter, const QRectF& field) const;

    QTimer frame_timer;
    QElapsedTimer clock;
    std::vector<Leaf> leaves;
    std::vector<Spark> sparks;
    std::vector<Popup> popups;
    std::vector<Fallen> fallen;
    double basket = 0.5;       ///< Centre of the basket, 0..1 across
    double target = 0.5;       ///< Where the mouse or the arrows lead it
    double velocity = 0;       ///< Of the basket, for its tilt
    double squash = 0;         ///< 1 just after a catch, back to 0
    int direction = 0;         ///< -1, 0 or 1 while an arrow is held
    double next_drop = 0;      ///< Seconds until the next leaf
    double elapsed = 0;        ///< Seconds played, for the pace and the scenery
    double wide = 0;           ///< Seconds the syrup still widens the basket
    double wind = 0;           ///< Field widths per second the gust pushes leaves
    double gust = 0;           ///< Seconds left in the current gust
    double next_gust = 20;     ///< Seconds until the next gust
    double banner = 0;         ///< Seconds the event banner stays
    QString banner_text;
    int score = 0;
    int combo = 0;
    static inline int best = 0;
};
