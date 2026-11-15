// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <functional>

#include <QDialog>
#include <QString>

class QLabel;

/// Tools > Saves: the saves of every profile in one place. Backs them up to a zip, puts a zip back
/// and takes a game's saves from another emulator's folder. Before it changes anything it zips the
/// saves as they are into the automatic backups folder, and says where.
class SavesDialog : public QDialog {
    Q_OBJECT

public:
    explicit SavesDialog(QWidget* parent = nullptr);

private:
    void Refresh();
    void OpenFolder();
    void Backup();
    void Restore();
    void TakeFromEmulator();

    /// Zips the saves as they are into the automatic backups folder before they change, and keeps
    /// the ten newest. `zip` gets the file, or stays empty when there was nothing to back up.
    /// False (after telling the user) if the backup failed or was cancelled.
    bool AutomaticBackup(const QString& reason, QString& zip);

    /// Runs `work` on a worker thread behind a progress window. `work` gets a callback to report
    /// progress (done, total) that returns false once the user cancels.
    bool RunWithProgress(const QString& title,
                         const std::function<bool(const std::function<bool(size_t, size_t)>&)>& work);

    QLabel* summary_label = nullptr;
};

namespace SavesFolder {

/// .../nand/user/save/0000000000000000: a folder per profile, then one per game.
[[nodiscard]] std::filesystem::path Root();

/// Where the automatic backups go.
[[nodiscard]] std::filesystem::path BackupsDir();

} // namespace SavesFolder
