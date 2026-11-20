// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

#include "common/common_types.h"

namespace Core {
class System;
}

/// Mods and cheats are not allowed in the rooms of the Erable server. A game's mods can be paused
/// for the online play in one click; they come back once the player goes offline, leaves the room
/// or starts Erable again (which starts offline).
namespace Online::Mods {

/// The player's own mods and cheats enabled for the title, Erable's built-in patches aside
std::vector<std::string> Active(Core::System& system, u64 program_id);

/// Turns `names` off for the title until Restore (remembered across restarts)
void Pause(u64 program_id, const std::vector<std::string>& names);

/// Turns back on every mod Pause turned off; how many
int Restore();

} // namespace Online::Mods
