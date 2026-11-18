// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/online_mode.h"

#include <string>

#include "common/logging.h"
#include "common/settings.h"
#include "erable/online/online_account.h"
#include "network/network.h"
#include "network/room.h"
#include "network/room_autojoin.h"
#include "network/room_member.h"

namespace Online {

namespace {

/// The room of the Erable server this PC joined, so another room joined from the multiplayer
/// menus is left alone
struct Joined {
    std::string host;
    unsigned port{};
};
Joined joined;

void LeaveServerRoom() {
    if (joined.host.empty()) {
        return;
    }
    if (const auto member = Network::GetRoomMember().lock(); member && member->IsConnected()) {
        member->Leave();
    }
    LOG_INFO(Network, "Online: left the room {}:{}", joined.host, joined.port);
    joined = {};
}

} // Anonymous namespace

Mode CurrentMode() {
    const std::string& mode = Settings::values.online_mode.GetValue();
    if (mode == "wifi") {
        return Mode::WiFi;
    }
    if (mode == "server") {
        return Mode::Server;
    }
    return Mode::Off;
}

void SetMode(Mode mode) {
    Settings::values.online_mode = std::string{mode == Mode::WiFi     ? "wifi"
                                               : mode == Mode::Server ? "server"
                                                                      : "off"};
}

void ApplyMode(Account* account, const QString& profile_name) {
    const Mode mode = CurrentMode();
    Settings::values.lan_play = mode == Mode::WiFi;
    if (mode != Mode::Server || account == nullptr) {
        LeaveServerRoom();
        if (account) {
            account->SetPresenceRoom(0);
        }
        return;
    }
    const std::string host = account->ServerHost().toStdString();
    const unsigned port = Settings::values.online_room_port.GetValue();
    const auto member = Network::GetRoomMember().lock();
    if (!member) {
        return;
    }
    if (joined.host == host && joined.port == port && member->IsConnected()) {
        return;
    }
    LeaveServerRoom();
    const auto& session = account->CurrentSession();
    const QString name = session ? session->nickname : profile_name;
    const std::string nickname = Network::RoomNickname(name.toStdString());
    LOG_INFO(Network, "Online: joining the room {}:{} as {}", host, port, nickname);
    // The server gives signed-in players the rooms' passwords
    const QString fetched = account->RoomPassword(port);
    const std::string password = fetched.isEmpty()
                                     ? Settings::values.online_room_password.GetValue()
                                     : fetched.toStdString();
    member->Join(nickname, host.c_str(), static_cast<u16>(port), 0, Network::NoPreferredIP,
                 password);
    joined = {host, port};
    // Friends see the room and can join it
    account->SetPresenceRoom(port);
}

void JoinRoom(Account* account, unsigned port) {
    SetMode(Mode::Server);
    Settings::values.online_room_port = port;
    ApplyMode(account, {});
}

} // namespace Online
