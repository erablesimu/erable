// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/debug_session.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include <QString>

#include "common/fs/fs_paths.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "common/scm_rev.h"
#include "qt_common/util/compress.h"

namespace fs = std::filesystem;

namespace DebugSession {
namespace {

// What the trace launcher (erable-trace.bat) set: frame hitches and their causes, the sound a game
// sends, the game's file reads and the texture cache.
constexpr std::array<std::pair<const char*, const char*>, 4> Switches{{
    {"EDEN_NX2_STUTTER_TRACE", "1"},
    {"EDEN_NX2_AUDIO_LEVEL", "1"},
    {"EDEN_NX2_FS_READ_TRACE", "1"},
    {"EDEN_NX2_TC_LOG", "1"},
}};

constexpr std::string_view Pending = "pending";
constexpr std::string_view Recording = "recording";

bool recording = false;
bool game_started = false;
bool restart_requested = false;
std::time_t started_at = 0;
std::optional<fs::path> interrupted_folder;

fs::path DataDir() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir);
}

fs::path MarkerPath() {
    return DataDir() / "debug-session.txt";
}

fs::path LogDir() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::LogDir);
}

void WriteMarker(std::string_view state, std::time_t since) {
    std::error_code ec;
    fs::create_directories(DataDir(), ec);
    std::ofstream{MarkerPath(), std::ios::trunc} << state << '\n' << since << '\n';
}

void RemoveMarker() {
    std::error_code ec;
    fs::remove(MarkerPath(), ec);
}

void SetSwitches() {
    for (const auto& [name, value] : Switches) {
        if (std::getenv(name) != nullptr) {
            continue;
        }
#ifdef _WIN32
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }
}

std::string Stamp(std::time_t time) {
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d_%H-%M-%S", &local);
    return text;
}

bool ModifiedSince(const fs::path& path, std::time_t since) {
    std::error_code ec;
    const auto written = fs::last_write_time(path, ec);
    if (ec) {
        return false;
    }
    // file_clock to system_clock without clock_cast, which not every standard library has yet
    const auto system = std::chrono::system_clock::now() +
                        std::chrono::duration_cast<std::chrono::system_clock::duration>(
                            written - fs::file_time_type::clock::now());
    return std::chrono::system_clock::to_time_t(system) >= since;
}

/// Copies a session's files into a new folder of SessionsDirectory().
std::optional<fs::path> Collect(const fs::path& log, std::time_t since, std::string_view ending) {
    const auto folder = SessionsDirectory() / ("erable-session-" + Stamp(since));
    std::error_code ec;
    fs::remove_all(folder, ec);
    fs::create_directories(folder, ec);
    if (ec) {
        return std::nullopt;
    }
    const auto copy = [&](const fs::path& from) {
        std::error_code copy_ec;
        if (fs::is_regular_file(from, copy_ec)) {
            fs::copy_file(from, folder / from.filename(), fs::copy_options::overwrite_existing,
                          copy_ec);
        }
    };
    copy(log);
    for (const char* name : {"erable_gpu.log", "erable_crash.dmp"}) {
        if (ModifiedSince(LogDir() / name, since)) {
            copy(LogDir() / name);
        }
    }
    std::ofstream info{folder / "session.txt"};
    info << "Erable " << Common::g_build_fullname << " (" << Common::g_build_version << ")\n"
         << "Recorded from " << Stamp(since) << " to " << Stamp(std::time(nullptr)) << '\n'
         << "Ended: " << ending << '\n'
         << "Switches:";
    for (const auto& [name, value] : Switches) {
        info << ' ' << name << '=' << value;
    }
    info << '\n';
    return folder;
}

std::optional<fs::path> Zip(const fs::path& folder) {
    auto zip = folder;
    zip += ".zip";
    std::error_code ec;
    fs::remove(zip, ec);
    const bool written = QtCommon::Compress::compressDir(
        QString::fromStdWString(zip.wstring()), QString::fromStdWString(folder.wstring()),
        QtCommon::Compress::Options(),
        [](std::size_t, std::size_t) { return true; });
    if (!written) {
        return std::nullopt;
    }
    fs::remove_all(folder, ec);
    return zip;
}

/// The log the last run wrote, read before this run's log moves it aside.
fs::path LastLog() {
    fs::path newest;
    fs::file_time_type newest_time{};
    for (const auto* name : {LOG_FILE, "erable_log_2.txt"}) {
        const auto path = LogDir() / name;
        std::error_code ec;
        const auto time = fs::last_write_time(path, ec);
        if (!ec && (newest.empty() || time > newest_time)) {
            newest = path;
            newest_time = time;
        }
    }
    return newest;
}

} // namespace

void Prepare() {
    std::ifstream marker{MarkerPath()};
    if (!marker) {
        return;
    }
    std::string state;
    std::time_t since = 0;
    marker >> state >> since;
    marker.close();
    if (state == Recording) {
        // The last run recorded and never finished: it crashed or was killed.
        interrupted_folder = Collect(LastLog(), since, "Erable stopped before the recording was saved");
        RemoveMarker();
        return;
    }
    if (state == Pending) {
        StartNow();
    }
}

bool IsRecording() {
    return recording;
}

void NoteGameStarted() {
    game_started = true;
}

bool CanStartNow() {
    return !game_started;
}

void StartNow() {
    SetSwitches();
    recording = true;
    started_at = std::time(nullptr);
    WriteMarker(Recording, started_at);
}

void StartAtNextLaunch() {
    WriteMarker(Pending, std::time(nullptr));
    restart_requested = true;
}

bool RestartRequested() {
    return restart_requested;
}

std::optional<fs::path> Finish() {
    if (!recording) {
        return std::nullopt;
    }
    LOG_INFO(Frontend, "Debug session: recording ends");
    Common::Log::Flush();
    recording = false;
    RemoveMarker();
    const auto folder = Collect(Common::Log::GetFilePath(), started_at, "Recording stopped");
    return folder ? Zip(*folder) : std::nullopt;
}

std::optional<fs::path> TakeInterruptedSession() {
    if (!interrupted_folder) {
        return std::nullopt;
    }
    const auto folder = *std::exchange(interrupted_folder, std::nullopt);
    return Zip(folder);
}

fs::path SessionsDirectory() {
    return DataDir() / "sessions";
}

} // namespace DebugSession
