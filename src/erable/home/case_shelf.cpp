// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/home/case_shelf.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numeric>
#include <utility>

#include <QApplication>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QWheelEvent>

namespace {

// The case's maple parts in the theme's accent (Maple's orange, Spring's green, Winter's frost)
QColor Accent() {
    return qApp->palette().color(QPalette::Highlight);
}
constexpr QColor Text{0xf6, 0xee, 0xe8};
constexpr QColor Muted{0xb8, 0xa8, 0x9c};

constexpr double CaseAspect = 0.64;   ///< Width over height, about a game case's
constexpr double SideScale = 0.74;    ///< The neighbours' size
constexpr double SideTurn = 40;       ///< Degrees the neighbours turn towards the middle
constexpr double ReflectionShare = 0.32; ///< Of the case's height, shown under it

QFont MakeFont(const QString& family, double pixels, int weight = QFont::Bold) {
    QFont font{family};
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(pixels))));
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

/// A maple leaf `size` wide centred on `centre`.
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

/// `art` blurred: shrunk to a few pixels and smoothly scaled back up.
QImage Blurred(const QPixmap& art, QSize size) {
    const QImage tiny = art.toImage().scaled(12, 12, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const QImage middle = tiny.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return middle.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
}

} // namespace

CaseShelf::CaseShelf(QString display, QString body, QWidget* parent)
    : QWidget{parent}, display_family{std::move(display)}, body_family{std::move(body)} {
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(false);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    slide_timer.setInterval(15);
    connect(&slide_timer, &QTimer::timeout, this, &CaseShelf::Step);
}

QSize CaseShelf::sizeHint() const {
    return {900, 440};
}

QSize CaseShelf::minimumSizeHint() const {
    return {320, 240};
}

void CaseShelf::RefreshTheme() {
    // Pictures without a colour of note take the accent, which the theme just changed
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].add) {
            spine_colours[i] = SpineColour(entries[i].art);
        }
    }
    ClearCache();
    update();
}

void CaseShelf::SetStyle(Style style_) {
    style = style_;
    update();
}

QColor CaseShelf::SpineColour(const QPixmap& art) {
    if (art.isNull()) {
        return Accent();
    }
    // The picture's main colour: its pixels sorted into 24 hues, each weighed by how colourful
    // and bright it is (an average of all of them is a muddy brown); the heaviest hue wins, and
    // the colour is the mean of its pixels. A picture with no colour of note keeps the accent.
    const QImage small = art.toImage().convertToFormat(QImage::Format_RGB32).scaled(
        32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    constexpr int Hues = 24;
    std::array<double, Hues> weight{};
    std::array<std::array<double, 3>, Hues> sum{};
    for (int y = 0; y < small.height(); ++y) {
        for (int x = 0; x < small.width(); ++x) {
            const QColor colour = QColor::fromRgb(small.pixel(x, y));
            float h, s, v;
            colour.getHsvF(&h, &s, &v);
            if (h < 0 || s < 0.2f || v < 0.18f) {
                continue; // greys, near black
            }
            const int hue = std::min(Hues - 1, static_cast<int>(h * Hues));
            const double w = s * s * v;
            weight[hue] += w;
            sum[hue][0] += colour.redF() * w;
            sum[hue][1] += colour.greenF() * w;
            sum[hue][2] += colour.blueF() * w;
        }
    }
    const auto best = std::ranges::max_element(weight);
    if (*best < 1.0) {
        return Accent();
    }
    const auto& rgb = sum[static_cast<std::size_t>(best - weight.begin())];
    const QColor main = QColor::fromRgbF(static_cast<float>(rgb[0] / *best),
                                         static_cast<float>(rgb[1] / *best),
                                         static_cast<float>(rgb[2] / *best));
    float h, s, v;
    main.getHsvF(&h, &s, &v);
    // Deep enough for white text, colourful enough to tell the games apart
    return QColor::fromHsvF(h, std::clamp(s, 0.5f, 0.9f), std::clamp(v, 0.45f, 0.72f));
}

void CaseShelf::SetEntries(std::vector<Entry> entries_, int selected_) {
    entries = std::move(entries_);
    spine_colours.clear();
    for (const Entry& entry : entries) {
        spine_colours.push_back(entry.add ? QColor{0x3b, 0x30, 0x2a} : SpineColour(entry.art));
    }
    ClearCache();
    selected = entries.empty() ? 0 : std::clamp(selected_, 0, static_cast<int>(entries.size()) - 1);
    position = selected;
    slide_timer.stop();
    update();
}

void CaseShelf::SetSelected(int index) {
    if (entries.empty()) {
        return;
    }
    selected = std::clamp(index, 0, static_cast<int>(entries.size()) - 1);
    if (std::abs(position - selected) > 0.001 && !slide_timer.isActive()) {
        slide_clock.start();
        slide_timer.start();
    }
    update();
}

void CaseShelf::SetArt(int index, const QPixmap& art) {
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return;
    }
    entries[index].art = art;
    spine_colours[index] = SpineColour(art);
    case_images[index] = {};
    reflections[index] = {};
    spine_images[index] = {};
    update();
}

void CaseShelf::ClearCache() {
    case_images.assign(entries.size(), QImage{});
    reflections.assign(entries.size(), QImage{});
    spine_images.assign(entries.size(), QImage{});
}

void CaseShelf::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ClearCache();
}

void CaseShelf::Step() {
    // Eases towards the selection: quick at first, settling softly.
    const double dt = std::min(0.05, slide_clock.restart() / 1000.0);
    const double gap = selected - position;
    position += gap * (1.0 - std::exp(-dt * 11.0));
    if (std::abs(selected - position) < 0.002) {
        position = selected;
        slide_timer.stop();
    }
    update();
}

QSizeF CaseShelf::CaseSize() const {
    // Room for the reflection under the middle case, and for the neighbours on each side
    const double case_height =
        0.92 * std::min(height() / (1.0 + ReflectionShare * 0.6), width() / 2.4 / CaseAspect);
    return {case_height * CaseAspect, case_height};
}

QSizeF CaseShelf::SpineSize() const {
    const QSizeF size = CaseSize();
    return {size.width() * 0.16, size.height() * 0.92};
}

std::vector<CaseShelf::Placement> CaseShelf::CrateLayout() const {
    const QSizeF size = CaseSize();
    const QSizeF spine = SpineSize();
    const double centre = width() / 2.0;
    const double floor = size.height() + (height() - size.height() * (1.0 + ReflectionShare * 0.6)) / 2;
    const double gap = size.width() * 0.08 + spine.height() * 0.11; // room for a spine's lean
    const double first = size.width() / 2 + gap + spine.width() / 2; // the nearest spine's centre
    const double step = spine.width() + 4;
    std::vector<Placement> placements;
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const double offset = i - position;
        const double distance = std::abs(offset);
        const double sign = offset < 0 ? -1.0 : 1.0;
        if (distance > 0.001) {
            const double x = distance >= 1 ? first + (distance - 1) * step : distance * first;
            if (x - spine.width() > width() / 2.0 + spine.width()) {
                continue;
            }
            // The two spines next to the selected case lean on it, as if just pushed aside
            const double lean = -sign * 6.0 * std::min(distance, 1.0) * std::clamp(2.0 - distance, 0.0, 1.0);
            QTransform transform;
            transform.translate(centre + sign * x, floor);
            transform.rotate(lean);
            placements.push_back({.index = i, .offset = offset, .transform = transform,
                                  .opacity = std::min(1.0, distance * 1.5), .spine = true});
        }
        if (distance < 1.0) {
            // Coming out of the crate: it turns to face the player as it slides to the middle
            const double scale = 1.0 - 0.25 * distance;
            QTransform transform;
            transform.translate(centre + sign * distance * size.width() * 0.5, floor);
            transform.rotate(-sign * 70.0 * distance, Qt::YAxis);
            transform.scale(scale, scale);
            placements.push_back({.index = i, .offset = offset, .transform = transform,
                                  .opacity = 1.0 - distance});
        }
    }
    // Spines far to near, then the front cases, the selected one last
    std::ranges::stable_sort(placements, [](const Placement& a, const Placement& b) {
        if (a.spine != b.spine) {
            return a.spine;
        }
        return std::abs(a.offset) > std::abs(b.offset);
    });
    return placements;
}

std::vector<CaseShelf::Placement> CaseShelf::Layout() const {
    if (style == Style::Crate) {
        return CrateLayout();
    }
    const QSizeF size = CaseSize();
    const double centre = width() / 2.0;
    const double floor = size.height() + (height() - size.height() * (1.0 + ReflectionShare * 0.6)) / 2;
    std::vector<Placement> placements;
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const double offset = i - position;
        const double distance = std::abs(offset);
        if (distance > 4.6) {
            continue;
        }
        const double sign = offset < 0 ? -1.0 : 1.0;
        // Up to one case away it grows into the middle; further ones stack closer together.
        const double inner = std::min(distance, 1.0);
        const double outer = std::max(distance - 1.0, 0.0);
        const double scale = (1.0 - (1.0 - SideScale) * inner) * std::pow(0.92, outer);
        // Half stacked: each neighbour slides under the one nearer the middle.
        const double shift = inner * size.width() * 0.66 + outer * size.width() * 0.24;
        const double turn = -sign * SideTurn * std::min(distance, 1.0);
        QTransform transform;
        transform.translate(centre + sign * shift, floor - (1.0 - scale) * size.height() * 0.12);
        transform.rotate(turn, Qt::YAxis);
        transform.scale(scale, scale);
        placements.push_back({
            .index = i,
            .offset = offset,
            .transform = transform,
            .opacity = std::clamp(1.1 - 0.2 * distance, 0.0, 1.0),
        });
    }
    // Far ones first, so the near ones cover them
    std::ranges::sort(placements, [](const Placement& a, const Placement& b) {
        return std::abs(a.offset) > std::abs(b.offset);
    });
    return placements;
}

void CaseShelf::DrawCase(QPainter& painter, const Entry& entry, const QRectF& box,
                         const QColor& colour) const {
    const double w = box.width();
    const double h = box.height();
    const double radius = w * 0.035;
    QPainterPath shell;
    shell.addRoundedRect(box, radius, radius);

    if (entry.add) {
        painter.setPen(QPen{QColor{0x6b, 0x5a, 0x4e}, std::max(2.0, w * 0.012), Qt::DashLine});
        painter.setBrush(QColor{0x24, 0x1d, 0x19, 200});
        painter.drawPath(shell);
        painter.setPen(Muted);
        painter.setFont(MakeFont(display_family, w * 0.3));
        painter.drawText(box.adjusted(0, 0, 0, -h * 0.12), Qt::AlignCenter, QStringLiteral("+"));
        painter.setFont(MakeFont(display_family, w * 0.085));
        painter.drawText(box.adjusted(w * 0.1, h * 0.55, -w * 0.1, 0),
                         Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, entry.title);
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(qApp->palette().color(QPalette::Window).darker(115));
    painter.drawPath(shell);
    painter.save();
    painter.setClipPath(shell);

    // The spine: maple, with the title running down it
    const double spine = w * 0.085;
    const QRectF spine_box{box.left(), box.top(), spine, h};
    QLinearGradient spine_fill{spine_box.topLeft(), spine_box.topRight()};
    spine_fill.setColorAt(0, colour.darker(155));
    spine_fill.setColorAt(0.45, colour);
    spine_fill.setColorAt(1, colour.darker(155));
    painter.fillRect(spine_box, spine_fill);
    painter.fillPath(MapleLeaf({spine_box.center().x(), box.top() + spine * 0.9}, spine * 0.62),
                     QColor{255, 255, 255, 220});
    painter.save();
    painter.translate(spine_box.center().x(), box.top() + spine * 1.7);
    painter.rotate(90);
    painter.setPen(QColor{255, 255, 255, 235});
    painter.setFont(MakeFont(display_family, spine * 0.5));
    painter.drawText(QRectF(0, -spine / 2, h - spine * 2.2, spine),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(entry.title, Qt::ElideRight,
                                                      static_cast<int>(h - spine * 2.4)));
    painter.restore();

    // The front: a band with Erable's mark, then the game's picture
    const QRectF front{box.left() + spine, box.top(), w - spine, h};
    const double band = h * 0.075;
    const QRectF band_box{front.left(), front.top(), front.width(), band};
    painter.fillRect(band_box, colour);
    painter.fillPath(MapleLeaf({band_box.left() + band * 0.75, band_box.center().y()}, band * 0.62),
                     Qt::white);
    painter.setPen(Qt::white);
    painter.setFont(MakeFont(display_family, band * 0.46));
    painter.drawText(band_box.adjusted(band * 1.35, 0, -band * 0.5, 0),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("ERABLE"));

    const QRectF art_box{front.left(), band_box.bottom(), front.width(), front.bottom() - band_box.bottom()};
    const QPixmap& art = entry.art;
    const bool portrait = !art.isNull() && art.height() > art.width() * 1.15;
    if (portrait) {
        // A cover fills the front
        const QPixmap scaled = art.scaled(art_box.size().toSize() * painter.device()->devicePixelRatioF(),
                                          Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        painter.drawPixmap(art_box, scaled, QRectF(QPointF(0, 0), QSizeF(scaled.size())));
    } else {
        // A square icon: on a blur of its own colours, with the title under it
        if (art.isNull()) {
            QLinearGradient fill{art_box.topLeft(), art_box.bottomRight()};
            fill.setColorAt(0, Accent().lighter(130));
            fill.setColorAt(1, Accent().darker(280));
            painter.fillRect(art_box, fill);
        } else {
            const QImage backdrop = Blurred(art, art_box.size().toSize());
            painter.drawImage(art_box, backdrop,
                              QRectF((backdrop.width() - art_box.width()) / 2,
                                     (backdrop.height() - art_box.height()) / 2, art_box.width(),
                                     art_box.height()));
            painter.fillRect(art_box, QColor{0, 0, 0, 70});
        }
        const double side = art_box.width() * 0.78;
        const QRectF icon{art_box.center().x() - side / 2, art_box.top() + art_box.width() * 0.12,
                          side, side};
        QPainterPath icon_shape;
        icon_shape.addRoundedRect(icon, side * 0.08, side * 0.08);
        painter.setBrush(QColor{0, 0, 0, 90});
        painter.drawRoundedRect(icon.translated(0, side * 0.03), side * 0.08, side * 0.08);
        if (art.isNull()) {
            painter.fillPath(MapleLeaf(icon.center(), side * 0.7), QColor{255, 255, 255, 60});
        } else {
            painter.save();
            painter.setClipPath(icon_shape, Qt::IntersectClip);
            painter.drawPixmap(icon.toRect(), art);
            painter.restore();
        }
        painter.setPen(Text);
        painter.setFont(MakeFont(display_family, w * 0.075));
        const QRectF title_box{art_box.left() + w * 0.07, icon.bottom() + h * 0.035,
                               art_box.width() - w * 0.14, art_box.bottom() - icon.bottom() - h * 0.06};
        painter.drawText(title_box, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, entry.title);
    }

    // Plastic sheen and an edge highlight
    QLinearGradient sheen{box.topLeft(), QPointF(box.left() + w * 0.7, box.top() + h * 0.55)};
    sheen.setColorAt(0, QColor{255, 255, 255, 46});
    sheen.setColorAt(0.5, QColor{255, 255, 255, 8});
    sheen.setColorAt(1, QColor{255, 255, 255, 0});
    painter.fillRect(box, sheen);
    painter.restore();
    painter.setPen(QPen{QColor{255, 255, 255, 40}, std::max(1.0, w * 0.006)});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    if (entry.favorite) {
        const double badge = w * 0.13;
        const QRectF star{box.right() - badge * 1.25, band_box.bottom() + badge * 0.25, badge, badge};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{0, 0, 0, 120});
        painter.drawEllipse(star);
        painter.setPen(QColor{0xff, 0xd3, 0x6b});
        painter.setFont(MakeFont(body_family, badge * 0.62));
        painter.drawText(star, Qt::AlignCenter, QStringLiteral("★"));
    }
}

const QImage& CaseShelf::CaseImage(int index) {
    QImage& image = case_images[index];
    if (image.isNull()) {
        const qreal ratio = devicePixelRatioF();
        const QSizeF size = CaseSize();
        image = QImage((size * ratio).toSize(), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(ratio);
        image.fill(Qt::transparent);
        QPainter painter{&image};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setRenderHint(QPainter::TextAntialiasing);
        // The spine and band in the game's main colour, as its spine in the crate
        DrawCase(painter, entries[index], QRectF(QPointF(0, 0), size), spine_colours[index]);
    }
    return image;
}

void CaseShelf::DrawSpine(QPainter& painter, int index, const QRectF& box) const {
    const Entry& entry = entries[index];
    const QColor colour = spine_colours[index];
    const double w = box.width();
    QPainterPath shape;
    shape.addRoundedRect(box, w * 0.12, w * 0.12);
    QLinearGradient fill{box.topLeft(), box.topRight()};
    fill.setColorAt(0, colour.darker(150));
    fill.setColorAt(0.45, colour);
    fill.setColorAt(1, colour.darker(160));
    painter.fillPath(shape, fill);
    painter.fillPath(MapleLeaf({box.center().x(), box.top() + w * 0.62}, w * 0.56),
                     QColor{255, 255, 255, 225});
    painter.save();
    painter.translate(box.center().x(), box.top() + w * 1.2);
    painter.rotate(90);
    painter.setPen(QColor{255, 255, 255, entry.add ? 170 : 240});
    painter.setFont(MakeFont(display_family, w * 0.4));
    const double length = box.height() - w * 1.5;
    painter.drawText(QRectF(0, -w / 2, length, w), Qt::AlignLeft | Qt::AlignVCenter,
                     painter.fontMetrics().elidedText(entry.add ? QStringLiteral("+ ") + entry.title
                                                                : entry.title,
                                                      Qt::ElideRight, static_cast<int>(length)));
    painter.restore();
    painter.setPen(QPen{QColor{255, 255, 255, 40}, 1});
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
}

const QImage& CaseShelf::SpineImage(int index) {
    QImage& image = spine_images[index];
    if (image.isNull()) {
        const qreal ratio = devicePixelRatioF();
        const QSizeF size = SpineSize();
        image = QImage((size * ratio).toSize(), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(ratio);
        image.fill(Qt::transparent);
        QPainter painter{&image};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        DrawSpine(painter, index, QRectF(QPointF(0, 0), size));
    }
    return image;
}

const QImage& CaseShelf::Reflection(int index) {
    QImage& image = reflections[index];
    if (image.isNull()) {
        const QImage& source = CaseImage(index);
        const int rows = static_cast<int>(source.height() * ReflectionShare);
        image = source.mirrored(false, true).copy(0, 0, source.width(), rows);
        image.setDevicePixelRatio(source.devicePixelRatio());
        QPainter painter{&image};
        painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QLinearGradient fade{QPointF(0, 0), QPointF(0, rows / source.devicePixelRatio())};
        fade.setColorAt(0, QColor{0, 0, 0, 95});
        fade.setColorAt(1, QColor{0, 0, 0, 0});
        painter.fillRect(QRectF(0, 0, source.width(), rows), fade);
    }
    return image;
}

void CaseShelf::paintEvent(QPaintEvent*) {
    if (entries.empty()) {
        return;
    }
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSizeF size = CaseSize();
    const QRectF flat{-size.width() / 2, -size.height(), size.width(), size.height()};
    const auto placements = Layout();

    // A warm glow on the floor under the middle case
    if (!placements.empty()) {
        const QPointF foot = placements.back().transform.map(QPointF(0, 0));
        QRadialGradient glow{foot, size.width() * 0.9};
        glow.setColorAt(0, QColor{Accent().red(), Accent().green(), Accent().blue(), 70});
        glow.setColorAt(1, QColor{Accent().red(), Accent().green(), Accent().blue(), 0});
        painter.save();
        painter.translate(foot);
        painter.scale(1.0, 0.22);
        painter.translate(-foot);
        painter.fillRect(QRectF(foot.x() - size.width(), foot.y() - size.width(), size.width() * 2,
                                size.width() * 2),
                         glow);
        painter.restore();
    }

    if (style == Style::Crate) {
        // The shelf the cases stand on
        const QPointF foot = placements.back().transform.map(QPointF(0, 0));
        QLinearGradient wood{QPointF(0, foot.y()), QPointF(0, foot.y() + 22)};
        wood.setColorAt(0, QColor{0x5a, 0x3a, 0x26, 230});
        wood.setColorAt(1, QColor{0x2a, 0x1a, 0x10, 0});
        painter.fillRect(QRectF(0, foot.y(), width(), 22), wood);
    }
    const QSizeF spine_size = SpineSize();
    const QRectF spine_flat{-spine_size.width() / 2, -spine_size.height(), spine_size.width(),
                            spine_size.height()};
    for (const Placement& place : placements) {
        painter.setOpacity(place.opacity);
        painter.setTransform(place.transform);
        if (place.spine) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor{0, 0, 0, 70});
            painter.drawRoundedRect(spine_flat.translated(3, 2), 5, 5);
            painter.drawImage(spine_flat, SpineImage(place.index));
            continue;
        }
        // Reflection first: it lies under the case
        const QImage& reflection = Reflection(place.index);
        painter.drawImage(QRectF(flat.left(), 0, size.width(), size.height() * ReflectionShare),
                          reflection);
        // Soft shadow behind
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{0, 0, 0, 60});
        painter.drawRoundedRect(flat.translated(size.width() * 0.02, size.height() * 0.015),
                                size.width() * 0.04, size.width() * 0.04);
        painter.drawImage(flat, CaseImage(place.index));
        if (style == Style::Shelf && std::abs(place.offset) > 0.5) {
            // The neighbours a little darker, so the middle one stands out
            const QColor shade = qApp->palette().color(QPalette::Window).darker(125);
            painter.setBrush(QColor{shade.red(), shade.green(), shade.blue(),
                                    static_cast<int>(std::min(1.0, std::abs(place.offset) - 0.5) * 70)});
            painter.drawRoundedRect(flat, size.width() * 0.035, size.width() * 0.035);
        }
    }
}

void CaseShelf::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QSizeF size = CaseSize();
    const QRectF flat{-size.width() / 2, -size.height(), size.width(), size.height()};
    const QSizeF spine_size = SpineSize();
    const QRectF spine_flat{-spine_size.width() / 2, -spine_size.height(), spine_size.width(),
                            spine_size.height()};
    const auto placements = Layout();
    // The nearest case on top: the last drawn
    for (auto it = placements.rbegin(); it != placements.rend(); ++it) {
        const QRectF& shape = it->spine ? spine_flat : flat;
        if (it->opacity > 0.3 &&
            it->transform.map(QPolygonF(shape)).containsPoint(event->position(), Qt::OddEvenFill)) {
            emit Clicked(it->index);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void CaseShelf::wheelEvent(QWheelEvent* event) {
    wheel_delta += event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
    if (std::abs(wheel_delta) >= 120) {
        const int steps = wheel_delta / 120;
        wheel_delta -= steps * 120;
        emit SelectionRequested(selected - steps);
    }
    event->accept();
}
