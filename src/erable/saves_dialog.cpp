// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/saves_dialog.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
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

using Report = std::function<bool(size_t, size_t)>;

/// A profile folder's name: the user id as 32 hex digits (all zeros for device saves).
bool IsProfileName(const QString& name) {
    static const QRegularExpression profile{QStringLiteral("^[0-9A-Fa-f]{32}$")};
    return profile.match(name).hasMatch();
}

/// A game folder's name: the title id as 16 hex digits.
bool IsTitleName(const QString& name) {
    static const QRegularExpression title{QStringLiteral("^[0-9A-Fa-f]{16}$")};
    return title.match(name).hasMatch();
}

bool IsDeviceProfile(const fs::path& profile) {
    return profile.filename() == "00000000000000000000000000000000";
}

QString Name(const fs::path& path) {
    return QString::fromStdString(Common::FS::PathToUTF8String(path.filename()));
}

QString Native(const fs::path& path) {
    return QDir::toNativeSeparators(QString::fromStdString(Common::FS::PathToUTF8String(path)));
}

fs::path PathOf(const QString& path) {
    return fs::path{path.toStdU16String()};
}

QString TitleName(u64 program_id) {
    return QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
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

/// The game's folder in `profile`, whatever the case of its name, if it holds a save.
std::optional<fs::path> GameIn(const fs::path& profile, const QString& title) {
    for (const fs::path& folder : SubFolders(profile, IsTitleName)) {
        if (Name(folder).compare(title, Qt::CaseInsensitive) == 0 && HasFiles(folder)) {
            return folder;
        }
    }
    return std::nullopt;
}

/// The profile folders holding saves, from whatever folder the user picked: an emulator's own
/// folder, its nand, nand/user/save, the 0000000000000000 folder or one profile.
std::vector<fs::path> FindProfiles(const fs::path& picked) {
    const fs::path candidates[] = {
        picked / "nand" / "user" / "save" / "0000000000000000",
        picked / "user" / "save" / "0000000000000000",
        picked / "save" / "0000000000000000",
        picked / "0000000000000000",
        picked,
    };
    std::error_code ec;
    for (const fs::path& candidate : candidates) {
        if (fs::equivalent(candidate, SavesFolder::Root(), ec)) {
            continue;
        }
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
    if (IsProfileName(Name(picked)) && !SubFolders(picked, IsTitleName).empty() &&
        !fs::equivalent(picked.parent_path(), SavesFolder::Root(), ec)) {
        return {picked};
    }
    return {};
}

int GameCount(const std::vector<fs::path>& profiles) {
    int games = 0;
    for (const fs::path& profile : profiles) {
        games += static_cast<int>(SubFolders(profile, IsTitleName).size());
    }
    return games;
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

/// Runs `work` on a worker thread behind a progress window. `work` reports (done, total) through
/// its callback, which returns false once the user cancels.
bool RunWithProgress(QWidget* parent, const QString& title,
                     const std::function<bool(const Report&)>& work) {
    UpdateProgress progress{title, parent};
    progress.show();
    std::atomic<size_t> done{0};
    std::atomic<size_t> total{0};
    std::atomic<bool> canceled{false};
    const Report report = [&](size_t total_size, size_t processed) {
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

/// Zips the saves as they are into the automatic backups folder before they change, and keeps the
/// ten newest (and `keep`, a backup being put back). `zip` gets the file, or stays empty when there
/// was nothing to back up. False (after telling the user) if the backup failed or was cancelled.
bool AutomaticBackup(QWidget* parent, const QString& reason, QString& zip,
                     const QString& keep = {}) {
    zip.clear();
    const fs::path root = SavesFolder::Root();
    if (!HasFiles(root)) {
        return true;
    }
    std::error_code ec;
    fs::create_directories(SavesFolder::BackupsDir(), ec);
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    const QString path =
        Native(SavesFolder::BackupsDir() /
               PathOf(QStringLiteral("saves_%1_%2.zip").arg(stamp, reason)));
    const QString source = Native(root);
    const bool ok = RunWithProgress(
        parent, SavesDialog::tr("Backing up your current saves..."), [&](const Report& report) {
            return QtCommon::Compress::compressDir(path, source, QtCommon::Compress::Options(),
                                                   report);
        });
    if (!ok) {
        QFile::remove(path);
        QMessageBox::warning(parent, SavesDialog::tr("Saves"),
                             SavesDialog::tr("Your current saves could not be backed up, so "
                                             "nothing was changed."));
        return false;
    }
    LOG_INFO(Frontend, "Saves: backed up to {}", path.toStdString());
    zip = path;

    std::vector<fs::path> backups;
    for (const auto& entry : fs::directory_iterator(SavesFolder::BackupsDir(), ec)) {
        if (entry.is_regular_file(ec) && Name(entry.path()).startsWith(QStringLiteral("saves_")) &&
            entry.path().extension() == ".zip" &&
            !(!keep.isEmpty() && fs::equivalent(entry.path(), PathOf(keep), ec))) {
            backups.push_back(entry.path());
        }
    }
    std::ranges::sort(backups, std::greater{});
    for (size_t i = 10; i < backups.size(); ++i) {
        fs::remove(backups[i], ec);
    }
    return true;
}

QString KeptIn(const QString& backup) {
    return backup.isEmpty() ? QString{}
                            : QStringLiteral("\n\n") +
                                  SavesDialog::tr("Your previous saves are kept in:\n%1").arg(backup);
}

/// Copies game folders into `target` (one of our profiles), each replacing the save there.
bool CopyGames(QWidget* parent, const std::vector<std::pair<fs::path, QString>>& games,
               const fs::path& target) {
    return RunWithProgress(parent, SavesDialog::tr("Copying the saves..."),
                           [&](const Report& report) {
                               size_t done = 0;
                               for (const auto& [source, title] : games) {
                                   if (!report(games.size(), done)) {
                                       return false;
                                   }
                                   std::error_code ec;
                                   const fs::path destination = target / PathOf(title);
                                   fs::remove_all(destination, ec);
                                   fs::create_directories(destination, ec);
                                   fs::copy(source, destination,
                                            fs::copy_options::recursive |
                                                fs::copy_options::overwrite_existing,
                                            ec);
                                   if (ec) {
                                       LOG_ERROR(Frontend, "Saves: copying {} failed: {}",
                                                 Common::FS::PathToUTF8String(source),
                                                 ec.message());
                                       return false;
                                   }
                                   report(games.size(), ++done);
                               }
                               return true;
                           });
}

/// Which of our profiles gets the saves: device saves stay device saves, others go to the
/// profile the user picks (asked only when there are several).
std::optional<fs::path> TargetProfile(const fs::path& source_profile) {
    if (IsDeviceProfile(source_profile)) {
        return SavesFolder::Root() / source_profile.filename();
    }
    const std::string user_id = GetProfileIDString();
    if (user_id.empty()) {
        return std::nullopt;
    }
    return SavesFolder::Root() / user_id;
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

std::vector<Emulator> FindEmulators() {
    // Emulators that keep their saves as Erable does, by their data folder's name
    static constexpr std::pair<const char*, const char*> known[] = {
        {"eden", "Eden"},         {"lichii", "Lichii"}, {"eglantine", "Eglantine"},
        {"citron", "Citron"},     {"sudachi", "Sudachi"}, {"suyu", "suyu"},
        {"torzu", "torzu"},       {"yuzu", "yuzu"},
    };
    std::vector<fs::path> bases;
#ifdef _WIN32
    if (const QString appdata = qEnvironmentVariable("APPDATA"); !appdata.isEmpty()) {
        bases.push_back(PathOf(appdata));
    }
#elif defined(__APPLE__)
    bases.push_back(PathOf(QDir::homePath()) / "Library" / "Application Support");
#else
    const QString data_home = qEnvironmentVariable("XDG_DATA_HOME");
    bases.push_back(data_home.isEmpty() ? PathOf(QDir::homePath()) / ".local" / "share"
                                        : PathOf(data_home));
#endif
    std::vector<Emulator> found;
    std::error_code ec;
    for (const fs::path& base : bases) {
        for (const auto& [folder, name] : known) {
            const fs::path root = base / folder / "nand" / "user" / "save" / "0000000000000000";
            if (!fs::is_directory(root, ec) || fs::equivalent(root, Root(), ec)) {
                continue;
            }
            const bool seen = std::ranges::any_of(found, [&](const Emulator& emulator) {
                return fs::equivalent(emulator.root, root, ec);
            });
            if (!seen && !FindProfiles(root).empty()) {
                found.push_back({QString::fromLatin1(name), root});
            }
        }
    }
    return found;
}

} // namespace SavesFolder

SavesDialog::SavesDialog(QWidget* parent) : QDialog{parent} {
    setWindowTitle(tr("Saves"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(580);
    setAcceptDrops(true);

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

    auto* take = ActionButton(this, tr("Import..."), true);
    AddRow(layout, this, tr("Take them from another emulator"),
           tr("Eden, Lichii, yuzu... found on this PC in one click. Their saves are left as they "
              "are. For one game only: right-click it in the library > Import Save Data."),
           take);
    auto* backup = ActionButton(this, tr("Back up..."));
    AddRow(layout, this, tr("Make a backup"),
           tr("Every game and every profile in one zip. Keep it on a USB stick or another drive."),
           backup);
    auto* restore = ActionButton(this, tr("Restore..."));
    AddRow(layout, this, tr("Put a backup back"),
           tr("On this PC or another, or drop the zip on this window. A copy of your current "
              "saves is made first."),
           restore);

    auto* footer = new QHBoxLayout;
    auto* open = ActionButton(this, tr("Open the saves folder"));
    open->setObjectName(QStringLiteral("flat"));
    auto* recover = ActionButton(this, tr("Recover an automatic backup..."));
    recover->setObjectName(QStringLiteral("flat"));
    auto* close = ActionButton(this, tr("Close"));
    footer->addWidget(open);
    footer->addWidget(recover);
    footer->addStretch(1);
    footer->addWidget(close);
    layout->addSpacing(4);
    layout->addLayout(footer);

    connect(take, &QPushButton::clicked, this, &SavesDialog::TakeFromEmulator);
    connect(backup, &QPushButton::clicked, this, &SavesDialog::Backup);
    connect(restore, &QPushButton::clicked, this, &SavesDialog::Restore);
    connect(open, &QPushButton::clicked, this, &SavesDialog::OpenFolder);
    connect(recover, &QPushButton::clicked, this, &SavesDialog::RecoverAutomatic);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    setStyleSheet(UpdateStyle::StyleSheet(QStringLiteral("SavesDialog")) +
                  QStringLiteral("SavesDialog QLabel#section_title { font-family: \"%1\";"
                                 " font-size: 15px; }")
                      .arg(UpdateStyle::DisplayFamily()));
    Refresh();
}

void SavesDialog::StartWithImport() {
    QMetaObject::invokeMethod(this, &SavesDialog::TakeFromEmulator, Qt::QueuedConnection);
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
    const QString size =
        QLocale{}.formattedDataSize(static_cast<qint64>(SizeOf(SavesFolder::Root())));
    summary_label->setText(tr("%n game save(s)", nullptr, games) + QStringLiteral("  ·  ") +
                           tr("%n profile(s)", nullptr, profiles) + QStringLiteral("  ·  ") +
                           size);
}

void SavesDialog::OpenFolder() {
    std::error_code ec;
    fs::create_directories(SavesFolder::Root(), ec);
    QDesktopServices::openUrl(QUrl::fromLocalFile(Native(SavesFolder::Root())));
}

void SavesDialog::dragEnterEvent(QDragEnterEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1 && urls.front().isLocalFile() &&
        urls.front().toLocalFile().endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        event->acceptProposedAction();
    }
}

void SavesDialog::dropEvent(QDropEvent* event) {
    const QString path = event->mimeData()->urls().front().toLocalFile();
    event->acceptProposedAction();
    // After the drop returns, so the file manager isn't held by the dialogs that follow
    QMetaObject::invokeMethod(this, [this, path] { RestoreZip(path); }, Qt::QueuedConnection);
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
    const bool ok =
        RunWithProgress(this, tr("Backing up your saves..."), [&](const Report& report) {
            return QtCommon::Compress::compressDir(path, source, QtCommon::Compress::Options(),
                                                   report);
        });
    if (!ok) {
        QFile::remove(path);
        QMessageBox::warning(this, tr("Saves"),
                             tr("The backup could not be written. Check that you can write to "
                                "that folder and try again."));
        return;
    }
    LOG_INFO(Frontend, "Saves: exported to {}", path.toStdString());
    QMessageBox::information(
        this, tr("Saves"), tr("Your saves are backed up in:\n%1").arg(QDir::toNativeSeparators(path)));
}

void SavesDialog::Restore() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a saves backup"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Zip archives (*.zip)"));
    if (!path.isEmpty()) {
        RestoreZip(path);
    }
}

void SavesDialog::RestoreZip(const QString& path) {
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
                             tr("This file doesn't hold saves from Erable or another emulator. "
                                "For one game's save, right-click the game in the library > "
                                "Import Save Data."));
        return;
    }
    if (QMessageBox::question(this, tr("Put a backup back"),
                              question + QStringLiteral("\n\n") +
                                  tr("Close the game first. Your current saves are backed up "
                                     "before anything changes. Continue?")) != QMessageBox::Yes) {
        return;
    }
    QString before;
    if (!AutomaticBackup(this, QStringLiteral("before-restore"), before, path)) {
        return;
    }
    const QString destination = Native(target);
    const bool ok =
        RunWithProgress(this, tr("Putting the backup back..."), [&](const Report& report) {
            std::error_code ec;
            fs::remove_all(target, ec);
            fs::create_directories(target, ec);
            return !QtCommon::Compress::extractDir(path, destination, report).isEmpty();
        });
    Refresh();
    if (!ok) {
        QMessageBox::warning(this, tr("Saves"),
                             tr("The backup could not be put back completely.") + KeptIn(before));
        return;
    }
    LOG_INFO(Frontend, "Saves: restored {} into {}", path.toStdString(), destination.toStdString());
    QMessageBox::information(this, tr("Saves"), tr("Your saves are back.") + KeptIn(before));
}

void SavesDialog::RecoverAutomatic() {
    std::vector<fs::path> backups;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(SavesFolder::BackupsDir(), ec)) {
        if (entry.is_regular_file(ec) && Name(entry.path()).startsWith(QStringLiteral("saves_")) &&
            entry.path().extension() == ".zip") {
            backups.push_back(entry.path());
        }
    }
    if (backups.empty()) {
        QMessageBox::information(this, tr("Recover an automatic backup"),
                                 tr("No automatic backup yet. Erable makes one before each "
                                    "import or restore."));
        return;
    }
    std::ranges::sort(backups, std::greater{});

    // "saves_<date>_<time>_<why>.zip": shown as its date, why it was made and its size
    static const QRegularExpression pattern{
        QStringLiteral("^saves_(\\d{4}-\\d{2}-\\d{2}_\\d{2}-\\d{2}-\\d{2})_(.+)\\.zip$")};
    const QLocale locale;
    QStringList choices;
    for (const fs::path& backup : backups) {
        const QString file = Name(backup);
        const QRegularExpressionMatch match = pattern.match(file);
        if (!match.hasMatch()) {
            choices << file;
            continue;
        }
        const QDateTime when =
            QDateTime::fromString(match.captured(1), QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
        const QString why = match.captured(2);
        const QString reason = why == QStringLiteral("before-restore")
                                   ? tr("before a restore")
                                   : tr("before an import");
        choices << QStringLiteral("%1  ·  %2  ·  %3")
                       .arg(locale.toString(when, QLocale::ShortFormat), reason,
                            locale.formattedDataSize(
                                static_cast<qint64>(fs::file_size(backup, ec))));
    }
    const QString open_folder = tr("Open their folder");
    choices << open_folder;
    bool ok = false;
    const QString choice = QInputDialog::getItem(
        this, tr("Recover an automatic backup"),
        tr("Your saves as they were just before a change, newest first. Which one?"), choices, 0,
        false, &ok);
    if (!ok) {
        return;
    }
    if (choice == open_folder) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Native(SavesFolder::BackupsDir())));
        return;
    }
    RestoreZip(Native(backups[static_cast<size_t>(choices.indexOf(choice))]));
}

void SavesDialog::TakeFromEmulator() {
    // The emulators found on this PC first, then any folder
    const std::vector<SavesFolder::Emulator> emulators = SavesFolder::FindEmulators();
    std::vector<fs::path> profiles;
    if (!emulators.empty()) {
        QStringList choices;
        for (const SavesFolder::Emulator& emulator : emulators) {
            choices << tr("%1 (%n game(s))", nullptr, GameCount(FindProfiles(emulator.root)))
                           .arg(emulator.name);
        }
        const QString other = tr("Another folder...");
        choices << other;
        bool ok = false;
        const QString choice = QInputDialog::getItem(
            this, tr("Take them from another emulator"),
            tr("Emulators found on this PC. Whose saves do you want?"), choices, 0, false, &ok);
        if (!ok) {
            return;
        }
        if (choice != other) {
            profiles =
                FindProfiles(emulators[static_cast<size_t>(choices.indexOf(choice))].root);
        }
    }
    if (profiles.empty()) {
        const QString picked = QFileDialog::getExistingDirectory(
            this, tr("Choose the other emulator's folder (or its nand folder)"),
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
        if (picked.isEmpty()) {
            return;
        }
        profiles = FindProfiles(PathOf(picked));
    }
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
            const int games = static_cast<int>(SubFolders(profile, IsTitleName).size());
            choices << (IsDeviceProfile(profile)
                            ? tr("Saves kept on the console (%n game(s))", nullptr, games)
                            : tr("Profile %1 (%n game(s))", nullptr, games)
                                  .arg(Name(profile).left(8)));
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

    const std::optional<fs::path> target = TargetProfile(source);
    if (!target) {
        return;
    }
    std::vector<std::pair<fs::path, QString>> games;
    int replaced = 0;
    for (const fs::path& title : SubFolders(source, IsTitleName)) {
        if (!HasFiles(title)) {
            continue;
        }
        const QString name = Name(title).toUpper();
        if (GameIn(*target, name)) {
            ++replaced;
        }
        games.emplace_back(title, name);
    }
    if (games.empty()) {
        QMessageBox::warning(this, tr("Saves"), tr("That profile has no saves."));
        return;
    }
    QString question = tr("%n game save(s) will be copied into this profile.", nullptr,
                          static_cast<int>(games.size()));
    if (replaced > 0) {
        question += QStringLiteral(" ") +
                    tr("%n of them replace(s) a save you already have here.", nullptr, replaced);
    }
    if (QMessageBox::question(this, tr("Take them from another emulator"),
                              question + QStringLiteral("\n\n") +
                                  tr("Close both emulators' games first. Your current saves are "
                                     "backed up before anything changes. Continue?")) !=
        QMessageBox::Yes) {
        return;
    }
    QString before;
    if (!AutomaticBackup(this, QStringLiteral("before-import"), before)) {
        return;
    }
    const bool ok = CopyGames(this, games, *target);
    Refresh();
    if (!ok) {
        QMessageBox::warning(this, tr("Saves"),
                             tr("Not every save could be copied.") + KeptIn(before));
        return;
    }
    LOG_INFO(Frontend, "Saves: copied {} game saves from {}", games.size(),
             Common::FS::PathToUTF8String(source));
    QMessageBox::information(this, tr("Saves"),
                             tr("%n game save(s) copied. Start the game in Erable to continue "
                                "where you left off.",
                                nullptr, static_cast<int>(games.size())) +
                                 KeptIn(before));
}

void SavesDialog::ImportGameSave(QWidget* parent, u64 program_id, const QString& game_name) {
    const QString title = TitleName(program_id);
    const QString window_title = tr("Import the save of %1").arg(game_name);

    // Every save of this game another emulator on this PC has
    struct Source {
        QString label;
        fs::path folder;
        fs::path profile;
    };
    std::vector<Source> sources;
    for (const SavesFolder::Emulator& emulator : SavesFolder::FindEmulators()) {
        std::vector<std::pair<fs::path, fs::path>> found;
        for (const fs::path& profile : FindProfiles(emulator.root)) {
            if (const auto folder = GameIn(profile, title)) {
                found.emplace_back(*folder, profile);
            }
        }
        for (const auto& [folder, profile] : found) {
            QString label = emulator.name;
            if (found.size() > 1) {
                label += IsDeviceProfile(profile)
                             ? QStringLiteral(" · ") + tr("kept on the console")
                             : QStringLiteral(" · ") + tr("profile %1").arg(Name(profile).left(8));
            }
            const QDateTime when =
                QFileInfo{Native(folder)}.lastModified();
            label += QStringLiteral(" (%1)").arg(
                QLocale{}.toString(when, QLocale::ShortFormat));
            sources.push_back({label, folder, profile});
        }
    }

    QStringList choices;
    for (const Source& source : sources) {
        choices << source.label;
    }
    const QString other_folder = tr("Another folder...");
    const QString zip_file = tr("A zip file...");
    choices << other_folder << zip_file;
    bool ok = false;
    const QString choice = QInputDialog::getItem(
        parent, window_title,
        sources.empty() ? tr("No other emulator on this PC has a save of this game. Where is it?")
                        : tr("Saves of this game found on this PC. Which one?"),
        choices, 0, false, &ok);
    if (!ok) {
        return;
    }

    fs::path folder;
    fs::path source_profile;
    QTemporaryDir unpacked;
    if (choice == other_folder || choice == zip_file) {
        fs::path searched;
        if (choice == other_folder) {
            const QString picked = QFileDialog::getExistingDirectory(
                parent, tr("Choose the save's folder, or the other emulator's folder"),
                QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
            if (picked.isEmpty()) {
                return;
            }
            searched = PathOf(picked);
        } else {
            const QString zip = QFileDialog::getOpenFileName(
                parent, tr("Choose the zip holding the save"),
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                tr("Zip archives (*.zip)"));
            if (zip.isEmpty()) {
                return;
            }
            const QString into = unpacked.path();
            const bool extracted =
                RunWithProgress(parent, tr("Opening the zip..."), [&](const Report& report) {
                    return !QtCommon::Compress::extractDir(zip, into, report).isEmpty();
                });
            if (!extracted) {
                QMessageBox::warning(parent, window_title, tr("This zip could not be opened."));
                return;
            }
            searched = PathOf(into);
        }
        // The game's folder anywhere inside, else the picked folder holds the save itself
        std::error_code ec;
        if (Name(searched).compare(title, Qt::CaseInsensitive) == 0 && HasFiles(searched)) {
            folder = searched;
        }
        for (auto it = fs::recursive_directory_iterator(searched, ec);
             folder.empty() && !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (it->is_directory(ec) &&
                Name(it->path()).compare(title, Qt::CaseInsensitive) == 0 &&
                HasFiles(it->path())) {
                folder = it->path();
            }
        }
        if (folder.empty() && HasFiles(searched) && SubFolders(searched, IsTitleName).empty() &&
            SubFolders(searched, IsProfileName).empty()) {
            folder = searched;
        }
        if (folder.empty()) {
            QMessageBox::warning(parent, window_title,
                                 tr("No save of this game was found there. Its folder is named "
                                    "%1.")
                                     .arg(title));
            return;
        }
        source_profile = folder.parent_path();
    } else {
        const Source& source = sources[static_cast<size_t>(choices.indexOf(choice))];
        folder = source.folder;
        source_profile = source.profile;
    }

    const std::optional<fs::path> target = TargetProfile(source_profile);
    if (!target) {
        return;
    }
    const QString replace =
        GameIn(*target, title) ? tr("It replaces the save you have in Erable.") + QStringLiteral(" ")
                               : QString{};
    if (QMessageBox::question(parent, window_title,
                              replace + tr("Close the game first. Your current saves are backed "
                                           "up before anything changes. Continue?")) !=
        QMessageBox::Yes) {
        return;
    }
    QString before;
    if (!AutomaticBackup(parent, QStringLiteral("before-import"), before)) {
        return;
    }
    if (!CopyGames(parent, {{folder, title}}, *target)) {
        QMessageBox::warning(parent, window_title,
                             tr("The save could not be copied.") + KeptIn(before));
        return;
    }
    LOG_INFO(Frontend, "Saves: imported {} from {}", title.toStdString(),
             Common::FS::PathToUTF8String(folder));
    QMessageBox::information(parent, window_title,
                             tr("Save imported. Start the game to continue where you left off.") +
                                 KeptIn(before));
}
