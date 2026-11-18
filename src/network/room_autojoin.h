// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "common/common_types.h"

namespace Network {

/// A room to join without the multiplayer dialogs
struct RoomTarget {
    std::string host;
    u16 port{};
    std::string nickname;
    std::string password;
};

/// `name` as a nickname rooms accept: 4 to 20 ASCII letters, digits, spaces, '.', '_' or '-'
/// (other characters become '_', and short names are padded with it)
std::string RoomNickname(std::string_view name);

/// Reads "host", "host:port" or "[ipv6]:port"; nothing when it is empty or the port is not a
/// number from 1 to 65535.
std::optional<RoomTarget> ParseRoomAddress(std::string_view address);

/// The room set in EDEN_NX2_ROOM (address), EDEN_NX2_ROOM_NICK and EDEN_NX2_ROOM_PASSWORD, if any
std::optional<RoomTarget> RoomFromEnvironment();

/// Joins the room set in the environment, once Network::Init() ran. Local wireless (LDN) and the
/// game's sockets then go through it, as with a room joined from the multiplayer dialogs.
void JoinRoomFromEnvironment();

} // namespace Network
