// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QDialog>
#include <QPixmap>
#include <QString>

#include "common/uuid.h"

class ControllerNavigation;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace Core {
class System;
}

/// "Who's playing?": the profiles as large round avatars, as a console shows them. Pick one to
/// play as (mouse, arrows and Enter, or a controller), add one, rename it, give it a picture or
/// delete it. Each profile keeps its own saves; switching never touches them.
class ProfilePicker : public QDialog {
    Q_OBJECT

public:
    explicit ProfilePicker(Core::System& system, QWidget* parent = nullptr);
    ~ProfilePicker() override;

    /// The profile's picture (saved by Erable or the profile manager), or null.
    [[nodiscard]] static QPixmap ProfilePicture(const Common::UUID& uuid);
    /// A colour of its own for a profile without a picture.
    [[nodiscard]] static QColor ProfileColour(const Common::UUID& uuid);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    struct Profile {
        Common::UUID uuid;
        QString name;
        QPixmap picture;
    };

    void Reload();
    void Choose();
    void AddProfile();
    void RenameProfile();
    void ChangePicture();
    void DeleteProfile();
    void UpdateButtons();
    /// "Online play" above the profiles (only with the online features enabled)
    void MakeOnlineBar(QVBoxLayout* layout);
    void UpdateOnlineBar();
    void ChooseOnlineMode(bool wifi);
    /// Fills the room list with the rooms the server gives signed-in players
    void FetchRooms();
    [[nodiscard]] QRectF TileRect(int index) const;
    [[nodiscard]] int TileAt(QPointF point) const;
    [[nodiscard]] int Count() const; ///< Profiles, and the "Add" tile while there is room

    Core::System& system;
    ControllerNavigation* controller = nullptr;
    std::vector<Profile> profiles;
    int focused = 0;
    int current = 0;
    QPushButton* rename_button = nullptr;
    QPushButton* picture_button = nullptr;
    QPushButton* delete_button = nullptr;
    // Online play
    double top_offset = 0; ///< How far the heading and the profiles move down for it
    QPushButton* wifi_button = nullptr;
    QPushButton* server_button = nullptr;
    QComboBox* room_box = nullptr;
    QPushButton* account_button = nullptr;
    bool fetching_rooms = false;
    bool rooms_tried = false;
    QString display_family;
    QString body_family;
};
