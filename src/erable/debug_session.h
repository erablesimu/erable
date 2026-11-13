// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>

// "Record my session": turns on the diagnostic switches the trace launcher used to set
// (EDEN_NX2_STUTTER_TRACE and the like) and, when the session ends, packs the log, the GPU log and
// a crash dump into a zip in %APPDATA%\erable\sessions, ready to attach to a report.
//
// The switches are read the first time a game uses them, so recording starts at once only while no
// game has run in this process; otherwise it starts with the next launch. A marker file in the data
// folder keeps the state across launches, and a recording the last run never finished (a crash)
// is packed at the next start from the log it left.
namespace DebugSession {

/// Reads the marker and sets the switches. Call in main() before the log starts.
void Prepare();

/// Whether this process records.
[[nodiscard]] bool IsRecording();

/// Called when a game boots: after that, recording needs a new launch.
void NoteGameStarted();
[[nodiscard]] bool CanStartNow();

/// Starts recording in this process (CanStartNow() must hold).
void StartNow();

/// Records the next launch; RestartRequested() then tells main() to start it.
void StartAtNextLaunch();
[[nodiscard]] bool RestartRequested();

/// Packs this process's session and stops recording. Returns the zip, if one could be written.
std::optional<std::filesystem::path> Finish();

/// The zip packed from a recording the last run left unfinished, once.
std::optional<std::filesystem::path> TakeInterruptedSession();

/// The folder holding the recorded sessions.
[[nodiscard]] std::filesystem::path SessionsDirectory();

} // namespace DebugSession
