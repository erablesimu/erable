// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QDialog>
#include <QPixmap>
#include <QString>

#include "common/uuid.h"

class ControllerNavigation;
class QLabel;
class QPushButton;

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
    QString display_family;
    QString body_family;
};
