// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <QTimer>
#include <QEventLoop>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QSaveFile>
#include <QStandardPaths>
#include <qdesktopservices.h>
#include "common/logging.h"
#include "common/scm_rev.h"
#include "frontend_common/update_installer.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/abstract/progress.h"
#ifdef _WIN32
#include "qt_common/util/compress.h"
#endif
#include "erable/updater/update_style.h"
#include "update_dialog.h"

#include "common/httplib.h"

#ifdef ERABLE_BUNDLED_OPENSSL
#include <openssl/cert.h>
#endif

#include <QDesktopServices>

#undef GetSaveFileName

namespace {

/// "build-31" as "build 31".
QString ReadableVersion(QString version) {
    return version.trimmed().replace(QLatin1Char('-'), QLatin1Char(' '));
}

} // Anonymous namespace

UpdateDialog::UpdateDialog(const Common::Net::Release& release, QWidget* parent)
    : QDialog(parent) {
    setObjectName(QStringLiteral("UpdateDialog"));
    setWindowTitle(tr("Update Available"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(680, 600);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 22);
    layout->setSpacing(14);

    // The leaf, the new version and the one installed, and the release page
    auto* header = new QHBoxLayout;
    header->setSpacing(16);
    header->addWidget(UpdateStyle::MakeIcon(this, 56), 0, Qt::AlignTop);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* title = new QLabel(QString::fromStdString(release.title), this);
    title->setObjectName(QStringLiteral("title"));
    titles->addWidget(title);
    m_subtitle = new QLabel(tr("A new version of Erable is available. You have %1.")
                                .arg(ReadableVersion(QString::fromLatin1(Common::g_build_version))),
                            this);
    m_subtitle->setObjectName(QStringLiteral("subtitle"));
    m_subtitle->setWordWrap(true);
    titles->addWidget(m_subtitle);
    header->addLayout(titles, 1);
    auto* url = new QLabel(tr("<a href=\"%1\">View on GitHub</a>")
                               .arg(QString::fromStdString(release.html_url)),
                           this);
    url->setOpenExternalLinks(true);
    url->setObjectName(QStringLiteral("subtitle"));
    header->addWidget(url, 0, Qt::AlignTop);
    layout->addLayout(header);

    // What changed, build by build
    auto* section = new QLabel(tr("WHAT'S NEW"), this);
    section->setObjectName(QStringLiteral("section"));
    layout->addWidget(section);
    m_body = new QTextBrowser(this);
    m_body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_body->setOpenExternalLinks(true);
    m_body->setFrameShape(QFrame::NoFrame);
    std::string text{release.body};
    if (auto pos = text.find("# Packages"); pos != std::string::npos) {
        text = text.substr(0, pos);
    }
    m_body->setMarkdown(QString::fromStdString(text));
    m_body->document()->setDocumentMargin(10);
    UpdateStyle::StyleNotes(m_body->document());
    layout->addWidget(m_body, 1);

    m_versions = new QGroupBox(tr("Available Versions"), this);
    m_radio_buttons = new QVBoxLayout(m_versions);
    layout->addWidget(m_versions);

    // Skip on the left, later and update on the right
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    auto* skip = new QPushButton(tr("Skip This Version"), this);
    skip->setObjectName(QStringLiteral("flat"));
    auto* later = new QPushButton(tr("Remind Me Later"), this);
    auto* update = new QPushButton(tr("Update"), this);
    update->setObjectName(QStringLiteral("primary"));
    update->setDefault(true);
    for (QPushButton* button : {skip, later, update}) {
        button->setCursor(Qt::PointingHandCursor);
    }
    buttons->addWidget(skip);
    buttons->addStretch(1);
    buttons->addWidget(later);
    buttons->addWidget(update);
    layout->addLayout(buttons);
    connect(skip, &QPushButton::clicked, this, [this] { done(SkipVersion); });
    connect(later, &QPushButton::clicked, this, &QDialog::reject);
    connect(update, &QPushButton::clicked, this, &QDialog::accept);

    setStyleSheet(UpdateStyle::StyleSheet(QStringLiteral("QDialog#UpdateDialog")));
    update->setFocus();

    // TODO(crueter): Find a way to set default
    const auto assets = release.GetPlatformAssets();

    if (assets.empty()) {
        m_versions->setHidden(true);
        update->setText(tr("Open the Download Page"));
        connect(this, &QDialog::accepted, this, [release]() {
            QDesktopServices::openUrl(QUrl{QString::fromStdString(release.html_url)});
        });
    } else if (assets.size() == 1) {
        m_versions->setHidden(true);
        m_asset = assets[0];

        if (m_asset.name == "Standard" &&
            std::string_view{Common::g_build_release_platform}.ends_with("-pgo")) {
            m_subtitle->setText(
                m_subtitle->text() + QLatin1Char(' ') +
                tr("This release has no PGO build, so the standard build will be downloaded."));
        }

        connect(this, &QDialog::accepted, this, &UpdateDialog::Download);
    } else {
        u32 i = 0;
        for (const Common::Net::Asset& a : assets) {
            QRadioButton* r = new QRadioButton(tr(a.name.c_str()), this);
            connect(r, &QRadioButton::toggled, this, [a, this](bool checked) {
                if (checked)
                    m_asset = a;
            });

            if (i == 0)
                r->setChecked(true);
            ++i;

            m_radio_buttons->addWidget(r);
        }

        connect(this, &QDialog::accepted, this, &UpdateDialog::Download);
    }
}

UpdateDialog::~UpdateDialog() = default;

bool UpdateDialog::Confirm(QWidget* parent, const QString& title, const QString& text,
                           const QString& accept, const QString& reject) {
    QDialog dialog{parent};
    dialog.setObjectName(QStringLiteral("UpdateConfirm"));
    dialog.setWindowTitle(QStringLiteral("Erable"));
    dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog.setMinimumWidth(500);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(28, 24, 28, 22);
    layout->setSpacing(18);
    auto* header = new QHBoxLayout;
    header->setSpacing(16);
    header->addWidget(UpdateStyle::MakeIcon(&dialog, 48), 0, Qt::AlignTop);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(6);
    auto* title_label = new QLabel(title, &dialog);
    title_label->setObjectName(QStringLiteral("title"));
    title_label->setStyleSheet(QStringLiteral("font-size: 21px;"));
    texts->addWidget(title_label);
    auto* text_label = new QLabel(text, &dialog);
    text_label->setObjectName(QStringLiteral("subtitle"));
    text_label->setStyleSheet(QStringLiteral("font-size: 14px;"));
    text_label->setWordWrap(true);
    texts->addWidget(text_label);
    header->addLayout(texts, 1);
    layout->addLayout(header);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch(1);
    auto* no = new QPushButton(reject, &dialog);
    auto* yes = new QPushButton(accept, &dialog);
    yes->setObjectName(QStringLiteral("primary"));
    yes->setDefault(true);
    for (QPushButton* button : {no, yes}) {
        button->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);
    QObject::connect(no, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(yes, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.setStyleSheet(UpdateStyle::StyleSheet(QStringLiteral("QDialog#UpdateConfirm")));
    yes->setFocus();
    return dialog.exec() == QDialog::Accepted;
}

void UpdateDialog::Restart(QWidget* window, const QString& target) {
    auto environment = QProcessEnvironment::systemEnvironment();
    const auto app_dir = environment.value(QStringLiteral("APPDIR"));
    if (!app_dir.isEmpty()) {
        for (const auto& key : environment.keys()) {
            const auto value = environment.value(key);
            if (!value.contains(app_dir)) {
                continue;
            }
            const auto stripped = UpdateInstaller::StripPathListEntries(value.toStdString(),
                                                                        app_dir.toStdString());
            if (stripped.empty()) {
                environment.remove(key);
            } else {
                environment.insert(key, QString::fromStdString(stripped));
            }
        }
    }
    for (const auto& key : {"APPDIR", "APPIMAGE", "ARGV0", "OWD"}) {
        environment.remove(QString::fromLatin1(key));
    }

    // Wait for this process to exit, so the new one reads the settings this one writes on close.
    QProcess process;
    process.setProgram(QStringLiteral("/bin/sh"));
    process.setArguments(
        {QStringLiteral("-c"),
         QStringLiteral("i=0; while kill -0 \"$1\" 2>/dev/null && [ \"$i\" -lt 1500 ]; do "
                        "sleep 0.2; i=$((i+1)); done; kill -0 \"$1\" 2>/dev/null && exit 0; "
                        "exec \"$2\""),
         QStringLiteral("sh"), QString::number(QCoreApplication::applicationPid()), target});
    process.setProcessEnvironment(environment);
    if (!process.startDetached()) {
        LOG_ERROR(Frontend, "Could not start the restart helper for {}", target.toStdString());
        QtCommon::Frontend::Critical(tr("Failed to restart"),
                                     tr("Please start %1 again yourself.").arg(target));
        return;
    }
    if (window) {
        QMetaObject::invokeMethod(window, &QWidget::close, Qt::QueuedConnection);
    }
}

#ifdef _WIN32
bool UpdateDialog::LaunchInstaller(QWidget* window, const QString& staging) {
    // The helper runs from a copy, because the installed one is replaced along with everything else.
    const QString helper =
        QCoreApplication::applicationDirPath() + QStringLiteral("/erable-updater.exe");
    const QString copy = QDir::temp().filePath(
        QStringLiteral("erable-updater-%1.exe").arg(QCoreApplication::applicationPid()));
    QFile::remove(copy);
    if (!QFile::copy(helper, copy)) {
        LOG_ERROR(Frontend, "Could not copy the installer helper {}", helper.toStdString());
        QtCommon::Frontend::Critical(tr("Failed to install the update"),
                                     tr("Could not start the update installer."));
        return false;
    }

    const auto install_dir = QCoreApplication::applicationDirPath();
    const auto native = [](const QString& path) { return QDir::toNativeSeparators(path); };
    if (!QProcess::startDetached(
            copy, {QString::number(QCoreApplication::applicationPid()), native(staging),
                   native(install_dir), native(QCoreApplication::applicationFilePath())})) {
        LOG_ERROR(Frontend, "Could not start the installer helper {}", copy.toStdString());
        QtCommon::Frontend::Critical(tr("Failed to install the update"),
                                     tr("Could not start the update installer."));
        return false;
    }
    if (window) {
        QMetaObject::invokeMethod(window, &QWidget::close, Qt::QueuedConnection);
    }
    return true;
}
#endif

void UpdateDialog::Download() {
#ifdef _WIN32
    if (m_asset.filename.ends_with(".zip")) {
        const std::filesystem::path exe{QCoreApplication::applicationFilePath().toStdWString()};
        if (UpdateInstaller::CanReplace(exe) &&
            QFile::exists(QCoreApplication::applicationDirPath() +
                          QStringLiteral("/erable-updater.exe"))) {
            InstallOnRestart(exe);
            return;
        }
    }
#endif
#ifdef __linux__
    if (Common::Net::CurrentLinuxPackage() == Common::Net::LinuxPackage::AppImage &&
        m_asset.filename.ends_with(".AppImage")) {
        const std::filesystem::path target{qEnvironmentVariable("APPIMAGE").toStdString()};
        if (UpdateInstaller::CanReplace(target)) {
            InstallInPlace(target);
            return;
        }
    }
#endif

    // The application folder is read-only inside an AppImage and is the app bundle on macOS.
    auto default_dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (default_dir.isEmpty()) {
        default_dir = QDir::homePath();
    }

    const auto filename = QtCommon::Frontend::GetSaveFileName(
        tr("New Version Location"),
        default_dir % QStringLiteral("/") % QString::fromStdString(m_asset.filename),
        tr("All Files (*.*)"));

    if (filename.isEmpty() || !DownloadTo(filename))
        return;

    // Download is complete. User may choose to open in the file manager.
    if (Confirm(parentWidget(), tr("Download Complete"),
                tr("Successfully downloaded %1. Would you like to open it?")
                    .arg(QString::fromStdString(m_asset.filename)),
                tr("Open"), tr("Close"))) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
    }
}

void UpdateDialog::InstallInPlace(const std::filesystem::path& target) {
    const UpdateInstaller::Lock lock{UpdateInstaller::LockPath(target)};
    if (!lock.Held()) {
        QtCommon::Frontend::Critical(tr("Failed to install the update"),
                                     tr("Another Erable window is already updating this "
                                        "installation."));
        return;
    }

    const auto staged = UpdateInstaller::StagedPath(target);
    if (!DownloadTo(QString::fromStdString(staged.string()))) {
        return;
    }

    std::error_code ec;
    if (!UpdateInstaller::Swap(staged, target, ec)) {
        LOG_ERROR(Frontend, "Could not replace {}: {}", target.string(), ec.message());
        std::filesystem::remove(staged, ec);
        QtCommon::Frontend::Critical(
            tr("Failed to install the update"),
            tr("Could not replace %1. Your installation was not changed.")
                .arg(QString::fromStdString(target.string())));
        return;
    }

    LOG_INFO(Frontend, "Installed {} over {}", m_asset.filename, target.string());
    if (Confirm(parentWidget(), tr("Update Installed"),
                tr("The new version is installed. The previous version is kept as %1.")
                    .arg(QString::fromStdString(UpdateInstaller::PreviousPath(target).string())),
                tr("Restart Now"), tr("Later"))) {
        Restart(parentWidget(), QString::fromStdString(target.string()));
    }
}

#ifdef _WIN32
void UpdateDialog::InstallOnRestart(const std::filesystem::path& exe) {
    const auto install_dir = exe.parent_path();
    const auto staging = install_dir / ".update-staging";
    const auto archive = install_dir / ".update-download.zip";
    const auto cleanup = [&] {
        std::error_code ec;
        std::filesystem::remove_all(staging, ec);
        std::filesystem::remove(archive, ec);
    };
    const auto fail = [&](const QString& text) {
        cleanup();
        QtCommon::Frontend::Critical(tr("Failed to install the update"), text);
    };

    cleanup();
    std::error_code ec;
    std::filesystem::create_directories(staging, ec);
    if (ec) {
        fail(tr("Could not create a folder for the update."));
        return;
    }
    if (!DownloadTo(QString::fromStdWString(archive.wstring()))) {
        cleanup();
        return;
    }

    UpdateProgress progress{tr("Preparing the update"), parentWidget()};
    progress.show();
    const auto unpacked = QtCommon::Compress::extractDir(
        QString::fromStdWString(archive.wstring()), QString::fromStdWString(staging.wstring()),
        [&](std::size_t total, std::size_t done) {
            QGuiApplication::processEvents();
            if (total != 0) {
                progress.SetPercent(static_cast<int>((done * 100) / total));
            }
            return !progress.WasCanceled();
        });
    progress.hide();
    if (unpacked.isEmpty() || !std::filesystem::exists(staging / exe.filename(), ec)) {
        fail(tr("The downloaded update could not be unpacked."));
        return;
    }
    std::filesystem::remove(archive, ec);

    const bool restart = Confirm(
        parentWidget(), tr("Update Ready"),
        tr("Erable will close, install the update and start again. The previous version is "
           "kept, and your settings and data are not changed."),
        tr("Restart Now"), tr("Later"));
    if (!restart ||
        !LaunchInstaller(parentWidget(), QString::fromStdWString(staging.wstring()))) {
        cleanup();
    }
}
#endif

// TODO: migrate to a net.cpp wrapper
bool UpdateDialog::DownloadTo(const QString& filename) {
    QSaveFile file(filename);
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        LOG_WARNING(Frontend, "Could not open file {}", filename.toStdString());
        QtCommon::Frontend::Critical(tr("Failed to save file"),
                                     tr("Could not open file %1 for writing.").arg(filename));
        return false;
    }

    // TODO(crueter): Move to net.cpp
    constexpr std::size_t timeout_seconds = 15;

    std::unique_ptr<httplib::Client> client = std::make_unique<httplib::Client>(m_asset.url);
    client->set_connection_timeout(timeout_seconds);
    client->set_read_timeout(timeout_seconds);
    client->set_write_timeout(timeout_seconds);
    client->set_follow_location(true);

#ifdef ERABLE_BUNDLED_OPENSSL
    client->load_ca_cert_store(kCert, sizeof(kCert));
#endif

    if (client == nullptr) {
        LOG_ERROR(Frontend, "Invalid URL {}{}", m_asset.url, m_asset.path);
        return false;
    }

    UpdateProgress progress{tr("Downloading the update"), parentWidget()};
    progress.show();

    QGuiApplication::processEvents();

    // Progress dialog.
    auto progress_callback = [&](size_t processed_size, size_t total_size) {
        QGuiApplication::processEvents();
        progress.SetBytes(processed_size, total_size != 0 ? total_size : m_asset.size);
        return !progress.WasCanceled();
    };

    QCryptographicHash hash(QCryptographicHash::Sha256);
    u64 received = 0;

    // Write file in chunks.
    auto content_receiver = [&file, &hash, &received, filename](const char* t_data,
                                                                size_t data_length) -> bool {
        hash.addData(QByteArrayView(t_data, static_cast<qsizetype>(data_length)));
        received += data_length;
        if (file.write(t_data, data_length) == -1) {
            LOG_WARNING(Frontend, "Could not write {} bytes to file {}", data_length,
                        filename.toStdString());
            QtCommon::Frontend::Critical(tr("Failed to save file"),
                                         tr("Could not write to file %1.").arg(filename));
            return false;
        }

        return true;
    };

    // Now send off request. GitHub's file server sometimes answers 502-504 or drops the
    // connection for a moment (v5.2's release hour): try up to three times, a few seconds apart,
    // starting the file over each time. The size and SHA-256 checks below still decide.
    constexpr int attempts = 3;
    httplib::Result result;
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        if (attempt > 1) {
            file.seek(0);
            file.resize(0);
            hash.reset();
            received = 0;
            progress.Restart();
            progress.SetTitle(tr("Downloading the update (try %1 of %2)").arg(attempt).arg(attempts));
            QEventLoop wait;
            QTimer::singleShot(2000 * (attempt - 1), &wait, &QEventLoop::quit);
            wait.exec();
            if (progress.WasCanceled()) {
                break;
            }
        }
        result = client->Get(m_asset.path, content_receiver, progress_callback);
        const bool canceled = !result && result.error() == httplib::Error::Canceled;
        const bool transient = (!result && !canceled) ||
                               (result && (result->status >= 500 || result->status == 429));
        if (!transient || attempt == attempts) {
            break;
        }
        LOG_WARNING(Frontend, "GET to {}{} failed ({}), trying again", m_asset.url, m_asset.path,
                    result ? std::to_string(result->status) : httplib::to_string(result.error()));
    }
    progress.hide();

    if (!result) {
        LOG_ERROR(Frontend, "GET to {}{} returned null", m_asset.url, m_asset.path);
        return false;
    }

    const auto& response = result.value();
    if (response.status >= 400) {
        LOG_ERROR(Frontend, "GET to {}{} returned error status code: {}", m_asset.url, m_asset.path,
                  response.status);
        QtCommon::Frontend::Critical(tr("Failed to download file"),
                                     tr("Could not download from %1%2\nError code: %3")
                                         .arg(QString::fromStdString(m_asset.url),
                                              QString::fromStdString(m_asset.path),
                                              QString::number(response.status)));
        return false;
    }
    if (!response.has_header("content-type")) {
        LOG_ERROR(Frontend, "GET to {}{} returned no content", m_asset.url, m_asset.path);
        return false;
    }

    const bool wrong_size = m_asset.size != 0 && received != m_asset.size;
    const bool wrong_hash =
        !m_asset.sha256.empty() &&
        hash.result().toHex() != QByteArray::fromStdString(m_asset.sha256);
    if (wrong_size || wrong_hash) {
        LOG_ERROR(Frontend, "Downloaded {} does not match the published size or SHA-256",
                  m_asset.filename);
        file.cancelWriting();
        QtCommon::Frontend::Critical(
            tr("Download failed verification"),
            tr("The downloaded file does not match the checksum GitHub published for it, so it "
               "was discarded. Try again later."));
        return false;
    }

    if (!file.commit()) {
        LOG_WARNING(Frontend, "Could not commit to file {}", filename.toStdString());
        QtCommon::Frontend::Critical(tr("Failed to save file"),
                                     tr("Could not commit to file %1.").arg(filename));
        return false;
    }

    return true;
}
