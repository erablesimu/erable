// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/saves_dialog.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <system_error>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <quazip.h>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "erable/updater/update_style.h"
#include "erable/util/util.h"
#include "qt_common/util/compress.h"

namespace fs = std::filesystem;

namespace {

/// A profile folder's name: the user id as 32 hex digits.
bool IsProfileName(const QString& name) {
    static const QRegularExpression profile{QStringLiteral("^[0-9A-Fa-f]{32}$")};
    return profile.match(name).hasMatch();
}

/// A game folder's name: the title id as 16 hex digits.
bool IsTitleName(const QString& name) {
    static const QRegularExpression title{QStringLiteral("^[0-9A-Fa-f]{16}$")};
    return title.match(name).hasMatch();
}

QString Name(const fs::path& path) {
    return QString::fromStdString(Common::FS::PathToUTF8String(path.filename()));
}

QString Native(const fs::path& path) {
    return QDir::toNativeSeparators(QString::fromStdString(Common::FS::PathToUTF8String(path)));
}

std::vector<fs::path> SubFolders(const fs::path& dir, bool (*matches)(const QString&)) {
    std::vector<fs::path> folders;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory(ec) && matches(Name(entry.path()))) {
            folders.push_back(entry.path());
        }
    }
    return folders;
}

bool HasFiles(const fs::path& dir) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            return true;
        }
    }
    return false;
}

u64 SizeOf(const fs::path& dir) {
    u64 size = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            size += it->file_size(ec);
        }
    }
    return size;
}

/// The other emulator's profile folders holding saves, from whatever folder the user picked: the
/// emulator's own folder, its nand, nand/user/save, the 0000000000000000 folder or one profile.
std::vector<fs::path> FindProfiles(const fs::path& picked) {
    const fs::path candidates[] = {
        picked / "nand" / "user" / "save" / "0000000000000000",
        picked / "user" / "save" / "0000000000000000",
        picked / "save" / "0000000000000000",
        picked / "0000000000000000",
        picked,
    };
    for (const fs::path& candidate : candidates) {
        std::vector<fs::path> profiles;
        for (const fs::path& profile : SubFolders(candidate, IsProfileName)) {
            if (!SubFolders(profile, IsTitleName).empty()) {
                profiles.push_back(profile);
            }
        }
        if (!profiles.empty()) {
            return profiles;
        }
    }
    // One profile's folder, its games straight inside
    if (IsProfileName(Name(picked)) && !SubFolders(picked, IsTitleName).empty()) {
        return {picked};
    }
    return {};
}

/// What a zip holds: every profile's saves (our backups), one profile's (the Data Manager's
/// export), or neither.
enum class ZipKind { AllProfiles, OneProfile, Unknown };

ZipKind KindOf(const QString& zip_path) {
    QuaZip zip{zip_path};
    if (!zip.open(QuaZip::mdUnzip)) {
        return ZipKind::Unknown;
    }
    bool profiles = false;
    bool titles = false;
    for (const QString& entry : zip.getFileNameList()) {
        const QString top = entry.section(QLatin1Char('/'), 0, 0);
        if (IsProfileName(top)) {
            profiles = true;
        } else if (IsTitleName(top)) {
            titles = true;
        } else {
            return ZipKind::Unknown;
        }
    }
    zip.close();
    if (profiles != titles) {
        return profiles ? ZipKind::AllProfiles : ZipKind::OneProfile;
    }
    return ZipKind::Unknown;
}

QPushButton* ActionButton(QWidget* parent, const QString& text, bool primary = false) {
    auto* button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    if (primary) {
        button->setObjectName(QStringLiteral("primary"));
    }
    return button;
}

/// A row: what the action does on the left, its button on the right.
void AddRow(QVBoxLayout* layout, QWidget* parent, const QString& title, const QString& text,
            QPushButton* button) {
    auto* row = new QHBoxLayout;
    row->setSpacing(16);
    auto* labels = new QVBoxLayout;
    labels->setSpacing(2);
    auto* title_label = new QLabel(title, parent);
    title_label->setObjectName(QStringLiteral("section_title"));
    title_label->setStyleSheet(QStringLiteral("font-size: 15px;"));
    auto* text_label = new QLabel(text, parent);
    text_label->setObjectName(QStringLiteral("subtitle"));
    text_label->setWordWrap(true);
    labels->addWidget(title_label);
    labels->addWidget(text_label);
    row->addLayout(labels, 1);
    button->setMinimumWidth(150);
    row->addWidget(button, 0, Qt::AlignVCenter);
    layout->addLayout(row);
}

} // Anonymous namespace

namespace SavesFolder {

fs::path Root() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::SaveDir) / "user" / "save" /
           "0000000000000000";
}

fs::path BackupsDir() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir) / "save-backups";
}

} // namespace SavesFolder

SavesDialog::SavesDialog(QWidget* parent) : QDialog{parent} {
    setWindowTitle(tr("Saves"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(560);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 22, 26, 20);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout;
    header->setSpacing(14);
    header->addWidget(UpdateStyle::MakeIcon(this, 44));
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* title = new QLabel(tr("Your saves"), this);
    title->setObjectName(QStringLiteral("title"));
    summary_label = new QLabel(this);
    summary_label->setObjectName(QStringLiteral("subtitle"));
    titles->addWidget(title);
    titles->addWidget(summary_label);
    header->addLayout(titles, 1);
    layout->addLayout(header);

    auto* backup = ActionButton(this, tr("Back up..."), true);
    AddRow(layout, this, tr("Make a backup"),
           tr("Every game and every profile in one zip. Keep it on a USB stick or another drive."),
           backup);
    auto* restore = ActionButton(this, tr("Restore..."));
    AddRow(layout, this, tr("Put a backup back"),
           tr("On this PC or another. A copy of your current saves is made first."), restore);
    auto* take = ActionButton(this, tr("Choose its folder..."));
    AddRow(layout, this, tr("Take them from another emulator"),
           tr("Eden, Lichii, Ryujinx... Copies its games' saves into one of your profiles; the "
              "other emulator's are left as they are."),
           take);

    auto* footer = new QHBoxLayout;
    auto* open = ActionButton(this, tr("Open the saves folder"));
    open->setObjectName(QStringLiteral("flat"));
    auto* open_backups = ActionButton(this, tr("Automatic backups"));
    open_backups->setObjectName(QStringLiteral("flat"));
    auto* close = ActionButton(this, tr("Close"));
    footer->addWidget(open);
    footer->addWidget(open_backups);
    footer->addStretch(1);
    footer->addWidget(close);
    layout->addSpacing(4);
    layout->addLayout(footer);

    connect(backup, &QPushButton::clicked, this, &SavesDialog::Backup);
    connect(restore, &QPushButton::clicked, this, &SavesDialog::Restore);
    connect(take, &QPushButton::clicked, this, &SavesDialog::TakeFromEmulator);
    connect(open, &QPushButton::clicked, this, &SavesDialog::OpenFolder);
    connect(open_backups, &QPushButton::clicked, this, [] {
        std::error_code ec;
        fs::create_directories(SavesFolder::BackupsDir(), ec);
        QDesktopServices::openUrl(QUrl::fromLocalFile(Native(SavesFolder::BackupsDir())));
    });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    setStyleSheet(UpdateStyle::StyleSheet(QStringLiteral("SavesDialog")) +
                  QStringLiteral("SavesDialog QLabel#section_title { font-family: \"%1\"; }")
                      .arg(UpdateStyle::DisplayFamily()));
    Refresh();
}

void SavesDialog::Refresh() {
    int profiles = 0;
    int games = 0;
    for (const fs::path& profile : SubFolders(SavesFolder::Root(), IsProfileName)) {
        const int count = static_cast<int>(SubFolders(profile, IsTitleName).size());
        if (count > 0) {
            ++profiles;
            games += count;
        }
    }
    if (games == 0) {
        summary_label->setText(tr("No saves yet."));
        return;
    }
    const QString size = QLocale{}.formattedDataSize(static_cast<qint64>(SizeOf(SavesFolder::Root())));
    summary_label->setText(tr("%n game save(s)", nullptr, games) + QStringLiteral("  ·  ") +
                           tr("%n profile(s)", nullptr, profiles) + QStringLiteral("  ·  ") +
                           size);
}

void SavesDialog::OpenFolder() {
    std::error_code ec;
    fs::create_directories(SavesFolder::Root(), ec);
    QDesktopServices::openUrl(QUrl::fromLocalFile(Native(SavesFolder::Root())));
}

bool SavesDialog::RunWithProgress(
    const QString& title,
    const std::function<bool(const std::function<bool(size_t, size_t)>&)>& work) {
    UpdateProgress progress{title, this};
    progress.show();
    std::atomic<size_t> done{0};
    std::atomic<size_t> total{0};
    std::atomic<bool> canceled{false};
    const auto report = [&](size_t total_size, size_t processed) {
        total = total_size;
        done = processed;
        return !canceled.load();
    };
    QFuture<bool> future = QtConcurrent::run([&] { return work(report); });
    while (!future.isFinished()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        if (progress.WasCanceled()) {
            canceled = true;
        }
        if (const size_t all = total.load(); all != 0) {
            progress.SetPercent(static_cast<int>((done.load() * 100) / all));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{15});
    }
    progress.close();
    return future.result() && !canceled.load();
}

bool SavesDialog::AutomaticBackup(const QString& reason, QString& zip) {
    zip.clear();
    const fs::path root = SavesFolder::Root();
    if (!HasFiles(root)) {
        return true;
    }
    std::error_code ec;
    fs::create_directories(SavesFolder::BackupsDir(), ec);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    const QString path = Native(SavesFolder::BackupsDir() /
                                QStringLiteral("saves_%1_%2.zip").arg(stamp, reason).toStdString());
    const QString source = Native(root);
    const bool ok = RunWithProgress(tr("Backing up your current saves..."),
                                    [&](const std::function<bool(size_t, size_t)>& report) {
                                        return QtCommon::Compress::compressDir(
                                            path, source, QtCommon::Compress::Options(), report);
                                    });
    if (!ok) {
        QFile::remove(path);
        QMessageBox::warning(this, tr("Saves"),
                             tr("Your current saves could not be backed up, so nothing was "
                                "changed."));
        return false;
    }
    LOG_INFO(Frontend, "Saves: backed up to {}", path.toStdString());
    zip = path;

    // The ten newest automatic backups stay
    std::vector<fs::path> backups;
    for (const auto& entry : fs::directory_iterator(SavesFolder::BackupsDir(), ec)) {
        if (entry.is_regular_file(ec) && Name(entry.path()).startsWith(QStringLiteral("saves_")) &&
            entry.path().extension() == ".zip") {
            backups.push_back(entry.path());
        }
    }
    std::ranges::sort(backups, std::greater{});
    for (size_t i = 10; i < backups.size(); ++i) {
        fs::remove(backups[i], ec);
    }
    return true;
}

void SavesDialog::Backup() {
    if (!HasFiles(SavesFolder::Root())) {
        QMessageBox::information(this, tr("Saves"), tr("There are no saves to back up yet."));
        return;
    }
    const QString suggested =
        QDir{QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)}.filePath(
            QStringLiteral("erable-saves-%1.zip")
                .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the backup as"), suggested,
                                                      tr("Zip archives (*.zip)"));
    if (path.isEmpty()) {
        return;
    }
    const QString source = Native(SavesFolder::Root());
    const bool ok = RunWithProgress(tr("Backing up your saves..."),
                                    [&](const std::function<bool(size_t, size_t)>& report) {
                                        return QtCommon::Compress::compressDir(
                                            path, source, QtCommon::Compress::Options(), report);
                                    });
    if (!ok) {
        QFile::remove(path);
        QMessageBox::warning(this, tr("Saves"),
                             tr("The backup could not be written. Check that you can write to "
                                "that folder and try again."));
        return;
    }
    LOG_INFO(Frontend, "Saves: exported to {}", path.toStdString());
    QMessageBox::information(this, tr("Saves"),
                             tr("Your saves are backed up in:\n%1").arg(QDir::toNativeSeparators(path)));
}

void SavesDialog::Restore() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a saves backup"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Zip archives (*.zip)"));
    if (path.isEmpty()) {
        return;
    }
    fs::path target = SavesFolder::Root();
    QString question;
    switch (KindOf(path)) {
    case ZipKind::AllProfiles:
        question = tr("The saves of every profile will be replaced by the ones in this backup.");
        break;
    case ZipKind::OneProfile: {
        // The Data Manager's export: one profile's games, put back into the profile chosen now
        const std::string user_id = GetProfileIDString();
        if (user_id.empty()) {
            return;
        }
        target /= user_id;
        question = tr("The saves of this profile will be replaced by the ones in this backup.");
        break;
    }
    case ZipKind::Unknown:
        QMessageBox::warning(this, tr("Saves"),
                             tr("This file doesn't hold saves from Erable or another emulator."));
        return;
    }
    if (QMessageBox::question(this, tr("Put a backup back"),
                              question + QStringLiteral("\n\n") +
                                  tr("Close the game first. Your current saves are backed up "
                                     "before anything changes. Continue?")) != QMessageBox::Yes) {
        return;
    }
    QString before;
    if (!AutomaticBackup(QStringLiteral("before-restore"), before)) {
        return;
    }
    const QString destination = Native(target);
    const bool ok = RunWithProgress(tr("Putting the backup back..."),
                                    [&](const std::function<bool(size_t, size_t)>& report) {
                                        std::error_code ec;
                                        fs::remove_all(target, ec);
                                        fs::create_directories(target, ec);
                                        return !QtCommon::Compress::extractDir(path, destination,
                                                                               report)
                                                    .isEmpty();
                                    });
    Refresh();
    const QString kept = before.isEmpty()
                             ? QString{}
                             : QStringLiteral("\n\n") +
                                   tr("Your previous saves are kept in:\n%1").arg(before);
    if (!ok) {
        QMessageBox::warning(this, tr("Saves"),
                             tr("The backup could not be put back completely.") + kept);
        return;
    }
    LOG_INFO(Frontend, "Saves: restored {} into {}", path.toStdString(), destination.toStdString());
    QMessageBox::information(this, tr("Saves"), tr("Your saves are back.") + kept);
}

void SavesDialog::TakeFromEmulator() {
    const QString start = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString picked = QFileDialog::getExistingDirectory(
        this, tr("Choose the other emulator's folder (or its nand folder)"), start);
    if (picked.isEmpty()) {
        return;
    }
    const fs::path picked_path{picked.toStdU16String()};
    std::vector<fs::path> profiles = FindProfiles(picked_path);
    std::error_code ec;
    std::erase_if(profiles, [&](const fs::path& profile) {
        return fs::equivalent(profile.parent_path(), SavesFolder::Root(), ec);
    });
    if (profiles.empty()) {
        QMessageBox::warning(this, tr("Saves"),
                             tr("No saves were found in this folder. Choose the other emulator's "
                                "own folder (the one holding nand), or its nand folder."));
        return;
    }

    // The other emulator's profile, when it has several
    fs::path source = profiles.front();
    if (profiles.size() > 1) {
        QStringList choices;
        for (const fs::path& profile : profiles) {
            choices << tr("Profile %1 (%n game(s))", nullptr,
                          static_cast<int>(SubFolders(profile, IsTitleName).size()))
                           .arg(Name(profile).left(8));
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(
            this, tr("Take them from another emulator"),
            tr("That emulator has several profiles. Which one's saves?"), choices, 0, false, &ok);
        if (!ok) {
            return;
        }
        source = profiles[static_cast<size_t>(choices.indexOf(choice))];
    }

    // Into which of ours
    const std::string user_id = GetProfileIDString();
    if (user_id.empty()) {
        return;
    }
    const fs::path target = SavesFolder::Root() / user_id;
    const std::vector<fs::path> titles = SubFolders(source, IsTitleName);
    int replaced = 0;
    for (const fs::path& title : titles) {
        if (HasFiles(target / title.filename())) {
            ++replaced;
        }
    }
    QString question = tr("%n game save(s) will be copied into this profile.", nullptr,
                          static_cast<int>(titles.size()));
    if (replaced > 0) {
        question += QStringLiteral(" ") + tr("%n of them replace(s) a save you already have here.",
                                             nullptr, replaced);
    }
    if (QMessageBox::question(this, tr("Take them from another emulator"),
                              question + QStringLiteral("\n\n") +
                                  tr("Close both emulators' games first. Your current saves are "
                                     "backed up before anything changes. Continue?")) !=
        QMessageBox::Yes) {
        return;
    }
    QString before;
    if (!AutomaticBackup(QStringLiteral("before-copy"), before)) {
        return;
    }
    const bool ok = RunWithProgress(
        tr("Copying the saves..."), [&](const std::function<bool(size_t, size_t)>& report) {
            size_t done = 0;
            for (const fs::path& title : titles) {
                if (!report(titles.size(), done)) {
                    return false;
                }
                std::error_code copy_ec;
                const fs::path destination = target / title.filename();
                fs::remove_all(destination, copy_ec);
                fs::create_directories(destination, copy_ec);
                fs::copy(title, destination,
                         fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                         copy_ec);
                if (copy_ec) {
                    LOG_ERROR(Frontend, "Saves: copying {} failed: {}",
                              Common::FS::PathToUTF8String(title), copy_ec.message());
                    return false;
                }
                report(titles.size(), ++done);
            }
            return true;
        });
    Refresh();
    const QString kept = before.isEmpty()
                             ? QString{}
                             : QStringLiteral("\n\n") +
                                   tr("Your previous saves are kept in:\n%1").arg(before);
    if (!ok) {
        QMessageBox::warning(this, tr("Saves"), tr("Not every save could be copied.") + kept);
        return;
    }
    LOG_INFO(Frontend, "Saves: copied {} game saves from {}", titles.size(),
             Common::FS::PathToUTF8String(source));
    QMessageBox::information(this, tr("Saves"),
                             tr("%n game save(s) copied. Start the game in Erable to continue "
                                "where you left off.",
                                nullptr, static_cast<int>(titles.size())) +
                                 kept);
}
