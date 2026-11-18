// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace Online {

class Account;

/// How local wireless games reach other players
enum class Mode {
    Off,    ///< Not at all
    WiFi,   ///< Directly, on the same network (LAN play)
    Server, ///< Through a room on the Erable server
};

/// The number of rooms on the Erable server, on consecutive ports from FirstRoomPort
constexpr int RoomCount = 16;
constexpr unsigned FirstRoomPort = 24872;

Mode CurrentMode();
void SetMode(Mode mode);

/// Puts the chosen mode in effect: LAN play for Wi-Fi, the chosen room of the Erable server for
/// Server (as the account's nickname, else `profile_name`), neither for Off
void ApplyMode(const Account* account, const QString& profile_name);

} // namespace Online
