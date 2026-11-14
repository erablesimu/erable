// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QTimer>
#include <QTransform>
#include <QWidget>

/// The home's games as a shelf of game cases: the selected one large in the middle, its
/// neighbours smaller and turned towards it, each over its reflection. Moving the selection
/// slides the shelf. Erable's own case: a maple spine with the title, the leaf on the front.
class CaseShelf : public QWidget {
    Q_OBJECT

public:
    struct Entry {
        QString title;
        QPixmap art; ///< The game's picture: a square icon or a portrait cover
        bool favorite = false;
        bool add = false; ///< The "Add games" case at the end
    };

    /// Shelf: cases half stacked on each side of the selected one. Crate: the others stand by
    /// their spine, tinted with their picture's colour, and the selected one stands out in front.
    enum class Style { Shelf, Crate };

    /// `display` and `body`: the home's two font families.
    CaseShelf(QString display, QString body, QWidget* parent = nullptr);

    /// Replaces the cases; `selected` is shown at once, without sliding.
    void SetEntries(std::vector<Entry> entries, int selected);
    /// Slides to `index`.
    void SetSelected(int index);
    /// A better picture for one case (a downloaded cover).
    void SetArt(int index, const QPixmap& art);
    void SetStyle(Style style_);
    [[nodiscard]] Style CurrentStyle() const {
        return style;
    }

    [[nodiscard]] int Selected() const {
        return selected;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// A case was clicked: the selected one to play it, another to select it.
    void Clicked(int index);
    /// The wheel asks for the case `index`.
    void SelectionRequested(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Placement {
        int index;
        double offset;    ///< From the middle, in cases
        QTransform transform; ///< Maps the case (bottom centre at 0,0) to the widget
        double opacity;
        bool spine = false; ///< Crate: the case seen by its spine
    };

    [[nodiscard]] QSizeF CaseSize() const;
    [[nodiscard]] QSizeF SpineSize() const;
    [[nodiscard]] std::vector<Placement> CrateLayout() const;
    void DrawSpine(QPainter& painter, int index, const QRectF& box) const;
    /// The colour of a case's spine in the crate: its picture's, made deep enough for white text.
    [[nodiscard]] static QColor SpineColour(const QPixmap& art);
    [[nodiscard]] std::vector<Placement> Layout() const;
    /// The case drawn flat, at the device's resolution; cached per entry.
    const QImage& CaseImage(int index);
    const QImage& Reflection(int index);
    const QImage& SpineImage(int index);
    void DrawCase(QPainter& painter, const Entry& entry, const QRectF& box) const;
    void Step();
    void ClearCache();

    QString display_family;
    QString body_family;
    std::vector<Entry> entries;
    std::vector<QImage> case_images;
    std::vector<QImage> reflections;
    std::vector<QImage> spine_images;
    std::vector<QColor> spine_colours;
    Style style = Style::Shelf;
    int selected = 0;
    double position = 0; ///< The case in the middle, sliding towards `selected`
    QTimer slide_timer;
    QElapsedTimer slide_clock;
    int wheel_delta = 0;
};
