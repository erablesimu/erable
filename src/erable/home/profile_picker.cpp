// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/home/profile_picker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include <fmt/format.h>

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "erable/util/controller_navigation.h"

namespace {

constexpr double TileSize = 124;   ///< The avatar's diameter
constexpr double TileGap = 34;
constexpr double TileTop = 104;    ///< Below the heading

QString FamilyOf(const QString& file) {
    const int id = QFontDatabase::addApplicationFont(file);
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    return families.isEmpty() ? QStringLiteral("Segoe UI") : families.first();
}

QFont MakeFont(const QString& family, int pixels, int weight = QFont::Bold) {
    QFont font{family};
    font.setPixelSize(pixels);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

std::filesystem::path PicturePath(const Common::UUID& uuid) {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::NANDDir) /
           fmt::format("system/save/8000000000000010/su/avators/{}.jpg", uuid.FormattedString());
}

QString NameOf(const Service::Account::ProfileManager& manager, const Common::UUID& uuid) {
    Service::Account::ProfileBase base{};
    if (!manager.GetProfileBase(uuid, base)) {
        return {};
    }
    return QString::fromStdString(Common::StringFromFixedZeroTerminatedBuffer(
        reinterpret_cast<const char*>(base.username.data()), base.username.size()));
}

/// The text field for a profile's name: what games show for the player, at most 32 bytes.
QString AskName(QWidget* parent, const QString& title, const QString& current) {
    bool ok = false;
    QString name;
    for (;;) {
        name = QInputDialog::getText(parent, title, QObject::tr("Name"), QLineEdit::Normal,
                                     name.isEmpty() ? current : name, &ok)
                   .trimmed();
        if (!ok) {
            return {};
        }
        if (!name.isEmpty() &&
            name.toStdString().size() <= Service::Account::profile_username_size) {
            return name;
        }
    }
}

} // namespace

QPixmap ProfilePicker::ProfilePicture(const Common::UUID& uuid) {
    return QPixmap{QString::fromStdString(Common::FS::PathToUTF8String(PicturePath(uuid)))};
}

QColor ProfilePicker::ProfileColour(const Common::UUID& uuid) {
    static constexpr std::array<QColor, 8> Colours{{
        {0xe8, 0x62, 0x2a}, {0x3f, 0x9a, 0xd8}, {0x5e, 0xb4, 0x32}, {0xc0, 0x4f, 0x9b},
        {0xe0, 0xa8, 0x2e}, {0x2f, 0xb3, 0xa3}, {0x8a, 0x6c, 0xe0}, {0xd9, 0x4b, 0x4b},
    }};
    return Colours[uuid.Hash() % Colours.size()];
}

ProfilePicker::ProfilePicker(Core::System& system_, QWidget* parent)
    : QDialog{parent}, system{system_} {
    setWindowTitle(tr("Who's playing?"));
    setModal(true);
    display_family = FamilyOf(QStringLiteral(":/fonts/Fredoka-SemiBold.ttf"));
    body_family = FamilyOf(QStringLiteral(":/fonts/Nunito-Bold.ttf"));
    setMinimumSize(760, 400);
    setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 0, 28, 22);
    layout->addStretch(1);
    auto* hint = new QLabel(tr("Each profile keeps its own saves. Switching never touches them."),
                            this);
    hint->setAlignment(Qt::AlignHCenter);
    hint->setObjectName(QStringLiteral("hint"));
    layout->addWidget(hint);
    layout->addSpacing(14);
    auto* actions = new QHBoxLayout;
    actions->addStretch();
    rename_button = new QPushButton(tr("Rename"), this);
    picture_button = new QPushButton(tr("Change picture"), this);
    delete_button = new QPushButton(tr("Delete"), this);
    auto* close_button = new QPushButton(tr("Close"), this);
    for (QPushButton* button : {rename_button, picture_button, delete_button, close_button}) {
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        actions->addWidget(button);
    }
    actions->addStretch();
    layout->addLayout(actions);
    connect(rename_button, &QPushButton::clicked, this, &ProfilePicker::RenameProfile);
    connect(picture_button, &QPushButton::clicked, this, &ProfilePicker::ChangePicture);
    connect(delete_button, &QPushButton::clicked, this, &ProfilePicker::DeleteProfile);
    connect(close_button, &QPushButton::clicked, this, &QDialog::reject);

    const QPalette colours = qApp->palette();
    setStyleSheet(QStringLiteral(
                      "ProfilePicker QLabel#hint { color: %1; font-family: \"%3\"; font-size: 13px; }"
                      "ProfilePicker QPushButton { font-family: \"%3\"; font-size: 14px; color: %2;"
                      " background: rgba(255,255,255,14); border: 1px solid rgba(255,255,255,40);"
                      " border-radius: 17px; padding: 7px 20px; }"
                      "ProfilePicker QPushButton:hover { border-color: %4; }"
                      "ProfilePicker QPushButton:disabled { color: rgba(255,255,255,70); }")
                      .arg(colours.color(QPalette::PlaceholderText).name(),
                           colours.color(QPalette::WindowText).name(), body_family,
                           colours.color(QPalette::Highlight).name()));

    // A controller picks too: the D-pad moves, A plays as the profile, B closes, Y renames
    controller = new ControllerNavigation(system.HIDCore(), this);
    connect(controller, &ControllerNavigation::TriggerKeyboardEvent, this, [this](Qt::Key key) {
        if (!isActiveWindow()) {
            return;
        }
        QKeyEvent event{QEvent::KeyPress, key, Qt::NoModifier};
        keyPressEvent(&event);
    });

    Reload();
    focused = current;
    UpdateButtons();
}

ProfilePicker::~ProfilePicker() {
    controller->UnloadController();
}

void ProfilePicker::Reload() {
    auto& manager = system.GetProfileManager();
    profiles.clear();
    current = 0;
    const auto current_uuid =
        manager.GetUser(static_cast<std::size_t>(Settings::values.current_user.GetValue()));
    for (std::size_t i = 0; i < manager.GetUserCount(); ++i) {
        const auto uuid = manager.GetUser(i);
        if (!uuid) {
            continue;
        }
        if (current_uuid && *uuid == *current_uuid) {
            current = static_cast<int>(profiles.size());
        }
        profiles.push_back({*uuid, NameOf(manager, *uuid), ProfilePicture(*uuid)});
    }
    focused = std::clamp(focused, 0, Count() - 1);
    UpdateButtons();
    update();
}

int ProfilePicker::Count() const {
    const bool room = profiles.size() < Service::Account::MAX_USERS;
    return static_cast<int>(profiles.size()) + (room ? 1 : 0);
}

void ProfilePicker::UpdateButtons() {
    const bool on_profile = focused < static_cast<int>(profiles.size());
    rename_button->setEnabled(on_profile);
    picture_button->setEnabled(on_profile);
    // The last profile stays: games need one
    delete_button->setEnabled(on_profile && profiles.size() > 1);
}

QRectF ProfilePicker::TileRect(int index) const {
    const double total = Count() * TileSize + (Count() - 1) * TileGap;
    const double left = (width() - total) / 2;
    return {left + index * (TileSize + TileGap), TileTop, TileSize, TileSize};
}

int ProfilePicker::TileAt(QPointF point) const {
    for (int i = 0; i < Count(); ++i) {
        // The name under the avatar counts too
        if (TileRect(i).adjusted(-8, -8, 8, 44).contains(point)) {
            return i;
        }
    }
    return -1;
}

void ProfilePicker::paintEvent(QPaintEvent*) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QPalette colours = qApp->palette();
    const QColor window = colours.color(QPalette::Window);
    const QColor accent = colours.color(QPalette::Highlight);
    const QColor text = colours.color(QPalette::WindowText);
    painter.fillRect(rect(), window.darker(118));
    QRadialGradient glow{QPointF(width() * 0.5, TileTop + TileSize / 2), width() * 0.55};
    glow.setColorAt(0, QColor(accent.red(), accent.green(), accent.blue(), 34));
    glow.setColorAt(1, QColor(accent.red(), accent.green(), accent.blue(), 0));
    painter.fillRect(rect(), glow);

    painter.setPen(text);
    painter.setFont(MakeFont(display_family, 30));
    painter.drawText(QRectF(0, 26, width(), 50), Qt::AlignHCenter | Qt::AlignVCenter,
                     tr("Who's playing?"));

    for (int i = 0; i < Count(); ++i) {
        const bool is_add = i >= static_cast<int>(profiles.size());
        const bool is_focused = i == focused;
        QRectF tile = TileRect(i);
        if (is_focused) {
            tile.adjust(-6, -6, 6, 6);
        }
        QPainterPath circle;
        circle.addEllipse(tile);
        if (is_add) {
            painter.setPen(QPen{QColor(255, 255, 255, is_focused ? 160 : 70), 2.5, Qt::DashLine});
            painter.setBrush(QColor(255, 255, 255, 10));
            painter.drawEllipse(tile);
            painter.setPen(QColor(255, 255, 255, is_focused ? 230 : 140));
            painter.setFont(MakeFont(display_family, 48));
            painter.drawText(tile, Qt::AlignCenter, QStringLiteral("+"));
        } else {
            const Profile& profile = profiles[i];
            painter.save();
            painter.setClipPath(circle);
            if (!profile.picture.isNull()) {
                painter.drawPixmap(tile.toRect(), profile.picture);
            } else {
                const QColor colour = ProfileColour(profile.uuid);
                QLinearGradient fill{tile.topLeft(), tile.bottomRight()};
                fill.setColorAt(0, colour.lighter(125));
                fill.setColorAt(1, colour.darker(130));
                painter.fillPath(circle, fill);
                painter.setPen(Qt::white);
                painter.setFont(MakeFont(display_family, static_cast<int>(tile.height() * 0.42)));
                painter.drawText(tile, Qt::AlignCenter, profile.name.left(1).toUpper());
            }
            painter.restore();
            if (i == current) {
                // The profile playing now: a check in the accent
                const QRectF badge{tile.right() - 34, tile.bottom() - 34, 32, 32};
                painter.setPen(QPen{window.darker(118), 3});
                painter.setBrush(accent);
                painter.drawEllipse(badge);
                painter.setPen(QPen{Qt::white, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
                const QPointF c = badge.center();
                painter.drawPolyline(QPolygonF{{c + QPointF(-7, 0), c + QPointF(-2, 5),
                                                c + QPointF(8, -6)}});
            }
        }
        if (is_focused) {
            painter.setPen(QPen{accent, 4});
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(tile.adjusted(-7, -7, 7, 7));
        }
        painter.setPen(is_focused ? text : QColor(text.red(), text.green(), text.blue(), 190));
        painter.setFont(MakeFont(body_family, 15, QFont::ExtraBold));
        const QString label = is_add ? tr("Add") : profiles[i].name;
        painter.drawText(QRectF(tile.center().x() - 90, TileRect(i).bottom() + 14, 180, 24),
                         Qt::AlignHCenter | Qt::AlignTop,
                         painter.fontMetrics().elidedText(label, Qt::ElideRight, 176));
    }
}

void ProfilePicker::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left:
        focused = std::max(0, focused - 1);
        break;
    case Qt::Key_Right:
        focused = std::min(Count() - 1, focused + 1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        Choose();
        return;
    case Qt::Key_Y:
    case Qt::Key_F2:
        RenameProfile();
        return;
    case Qt::Key_Delete:
    case Qt::Key_Minus:
        DeleteProfile();
        return;
    case Qt::Key_Escape:
        reject();
        return;
    default:
        QDialog::keyPressEvent(event);
        return;
    }
    UpdateButtons();
    update();
    event->accept();
}

void ProfilePicker::mousePressEvent(QMouseEvent* event) {
    const int tile = TileAt(event->position());
    if (tile < 0) {
        QDialog::mousePressEvent(event);
        return;
    }
    if (tile == focused) {
        Choose(); // a second click plays as it, as a double click does
        return;
    }
    focused = tile;
    UpdateButtons();
    update();
}

void ProfilePicker::mouseDoubleClickEvent(QMouseEvent* event) {
    if (TileAt(event->position()) >= 0) {
        Choose();
    }
}

void ProfilePicker::Choose() {
    if (focused >= static_cast<int>(profiles.size())) {
        AddProfile();
        return;
    }
    auto& manager = system.GetProfileManager();
    if (const auto index = manager.GetUserIndex(profiles[focused].uuid)) {
        Settings::values.current_user = static_cast<s32>(*index);
    }
    accept();
}

void ProfilePicker::AddProfile() {
    if (profiles.size() >= Service::Account::MAX_USERS) {
        return;
    }
    const QString name = AskName(this, tr("New profile"), {});
    if (name.isEmpty()) {
        return;
    }
    auto& manager = system.GetProfileManager();
    const Common::UUID uuid = Common::UUID::MakeRandom();
    if (manager.CreateNewUser(uuid, name.toStdString()).IsError()) {
        return;
    }
    manager.WriteUserSaveFile();
    Reload();
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (profiles[i].uuid == uuid) {
            focused = static_cast<int>(i);
        }
    }
    UpdateButtons();
    update();
}

void ProfilePicker::RenameProfile() {
    if (focused >= static_cast<int>(profiles.size())) {
        return;
    }
    Profile& profile = profiles[focused];
    const QString name = AskName(this, tr("Rename profile"), profile.name);
    if (name.isEmpty() || name == profile.name) {
        return;
    }
    auto& manager = system.GetProfileManager();
    Service::Account::ProfileBase base{};
    if (!manager.GetProfileBase(profile.uuid, base)) {
        return;
    }
    const std::string text = name.toStdString();
    std::fill(base.username.begin(), base.username.end(), u8{0});
    std::copy(text.begin(), text.end(), base.username.begin());
    manager.SetProfileBase(profile.uuid, base);
    manager.WriteUserSaveFile();
    Reload();
}

void ProfilePicker::ChangePicture() {
    if (focused >= static_cast<int>(profiles.size())) {
        return;
    }
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Choose a picture"), QDir::homePath(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (file.isEmpty()) {
        return;
    }
    QImage image{file};
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Change picture"), tr("This file is not a picture Erable can read."));
        return;
    }
    // Square, centred, 256 px: what the system applets show
    const int side = std::min(image.width(), image.height());
    image = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side)
                .scaled(256, 256, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const auto path = PicturePath(profiles[focused].uuid);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (!image.save(QString::fromStdString(Common::FS::PathToUTF8String(path)), "JPEG", 95)) {
        QMessageBox::warning(this, tr("Change picture"), tr("The picture could not be saved."));
        return;
    }
    Reload();
}

void ProfilePicker::DeleteProfile() {
    if (focused >= static_cast<int>(profiles.size()) || profiles.size() <= 1) {
        return;
    }
    const Profile profile = profiles[focused];
    const auto answer = QMessageBox::question(
        this, tr("Delete profile"),
        tr("Delete the profile \"%1\"?\n\nIts saves stay on the disk but no profile of Erable "
           "will use them any more. Copy them first if you want to keep playing them.")
            .arg(profile.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    auto& manager = system.GetProfileManager();
    const auto current_uuid =
        manager.GetUser(static_cast<std::size_t>(Settings::values.current_user.GetValue()));
    if (!manager.RemoveUser(profile.uuid)) {
        return;
    }
    manager.WriteUserSaveFile();
    // The profile playing now keeps its place; if it was the one deleted, the first one plays
    const auto index = current_uuid ? manager.GetUserIndex(*current_uuid) : std::nullopt;
    Settings::values.current_user = index ? static_cast<s32>(*index) : 0;
    Reload();
}
