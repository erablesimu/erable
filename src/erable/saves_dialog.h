// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <vector>

#include <QDialog>
#include <QString>

#include "common/common_types.h"

class QLabel;

/// Tools > Saves: the saves of every profile in one place. Backs them up to a zip, puts a zip back
/// (also dropped on the window) and takes the saves of another emulator found on this PC. Before
/// it changes anything it zips the saves as they are into the automatic backups folder, and says
/// where.
class SavesDialog : public QDialog {
    Q_OBJECT

public:
    explicit SavesDialog(QWidget* parent = nullptr);

    /// The game list's "Import Save Data...": one game's save, from another emulator on this PC,
    /// another folder or a zip.
    static void ImportGameSave(QWidget* parent, u64 program_id, const QString& game_name);

    /// Starts on taking another emulator's saves once the window shows (the home's Saves button).
    void StartWithImport();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void Refresh();
    void OpenFolder();
    void Backup();
    void Restore();
    void RestoreZip(const QString& path);
    void RecoverAutomatic();
    void TakeFromEmulator();

    QLabel* summary_label = nullptr;
};

namespace SavesFolder {

/// .../nand/user/save/0000000000000000: a folder per profile, then one per game.
[[nodiscard]] std::filesystem::path Root();

/// Where the automatic backups go.
[[nodiscard]] std::filesystem::path BackupsDir();

/// Another emulator of the same family found on this PC, with saves.
struct Emulator {
    QString name;
    std::filesystem::path root; ///< Its .../nand/user/save/0000000000000000
};

[[nodiscard]] std::vector<Emulator> FindEmulators();

} // namespace SavesFolder
