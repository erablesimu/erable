// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

/// A small game for the shader loading screen: move the sap bucket (mouse, or the left and right
/// arrows) to catch the maple leaves that fall. A golden leaf is worth five. Nothing is lost by
/// missing one, so it can be left alone at any time.
class LeafCatcher : public QWidget {
    Q_OBJECT

public:
    explicit LeafCatcher(QWidget* parent = nullptr);

    /// Starts or pauses the game (paused while hidden).
    void SetRunning(bool running);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    struct Leaf {
        double x;      ///< 0..1 across the field
        double y;      ///< 0..1 down the field
        double speed;  ///< Field heights per second
        double angle;  ///< Degrees
        double spin;   ///< Degrees per second
        bool golden;
    };

    void Step();
    void Drop();

    QTimer frame_timer;
    QElapsedTimer clock;
    std::vector<Leaf> leaves;
    double bucket = 0.5;     ///< Centre of the bucket, 0..1 across
    int direction = 0;       ///< -1, 0 or 1 while an arrow is held
    double next_drop = 0;    ///< Seconds until the next leaf
    double elapsed = 0;      ///< Seconds played, for the pace
    int score = 0;
    int combo = 0;
    static inline int best = 0;
};
