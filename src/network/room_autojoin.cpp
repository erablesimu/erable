// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/room_autojoin.h"

#include <charconv>
#include <cstdlib>
#include <random>

#include <fmt/format.h>

#include "common/logging.h"
#include "network/network.h"
#include "network/room.h"
#include "network/room_member.h"

namespace Network {

namespace {

std::string Environment(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string{value} : std::string{};
}

} // Anonymous namespace

std::string RoomNickname(std::string_view name) {
    // Rooms take 4 to 20 of ASCII letters, digits, space, '.', '_' and '-'
    std::string nickname;
    for (const char c : name) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == '_' || c == '-';
        if (allowed) {
            nickname.push_back(c);
        } else if (static_cast<unsigned char>(c) < 0x80 ||
                   (static_cast<unsigned char>(c) & 0xC0) == 0xC0) {
            // One '_' per character: UTF-8 continuation bytes add none
            nickname.push_back('_');
        }
        if (nickname.size() == 20) {
            break;
        }
    }
    while (nickname.size() < 4) {
        nickname.push_back('_');
    }
    return nickname;
}

std::optional<RoomTarget> ParseRoomAddress(std::string_view address) {
    while (!address.empty() && (address.front() == ' ' || address.front() == '\t')) {
        address.remove_prefix(1);
    }
    while (!address.empty() && (address.back() == ' ' || address.back() == '\t')) {
        address.remove_suffix(1);
    }
    if (address.empty()) {
        return std::nullopt;
    }
    RoomTarget target;
    target.port = DefaultRoomPort;
    std::string_view port;
    if (address.front() == '[') {
        const auto close = address.find(']');
        if (close == std::string_view::npos || close == 1) {
            return std::nullopt;
        }
        target.host = std::string{address.substr(1, close - 1)};
        const auto rest = address.substr(close + 1);
        if (!rest.empty()) {
            if (rest.front() != ':') {
                return std::nullopt;
            }
            port = rest.substr(1);
        }
    } else {
        const auto colon = address.find(':');
        // More than one colon without brackets: a bare IPv6 address, on the default port
        if (colon != std::string_view::npos &&
            address.find(':', colon + 1) == std::string_view::npos) {
            target.host = std::string{address.substr(0, colon)};
            port = address.substr(colon + 1);
        } else {
            target.host = std::string{address};
        }
    }
    if (target.host.empty()) {
        return std::nullopt;
    }
    if (!port.empty() || (address.back() == ':')) {
        unsigned value = 0;
        const auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), value);
        if (error != std::errc{} || end != port.data() + port.size() || value == 0 ||
            value > 65535) {
            return std::nullopt;
        }
        target.port = static_cast<u16>(value);
    }
    return target;
}

std::optional<RoomTarget> RoomFromEnvironment() {
    const std::string address = Environment("EDEN_NX2_ROOM");
    if (address.empty()) {
        return std::nullopt;
    }
    auto target = ParseRoomAddress(address);
    if (!target) {
        LOG_ERROR(Network, "EDEN_NX2_ROOM \"{}\" is not host, host:port or [ipv6]:port", address);
        return std::nullopt;
    }
    const std::string nickname = Environment("EDEN_NX2_ROOM_NICK");
    if (!nickname.empty()) {
        target->nickname = RoomNickname(nickname);
    } else {
        // Rooms refuse two members with the same nickname
        std::random_device device;
        target->nickname = fmt::format("Erable-{:04}", device() % 10000);
    }
    target->password = Environment("EDEN_NX2_ROOM_PASSWORD");
    return target;
}

void JoinRoomFromEnvironment() {
    const auto target = RoomFromEnvironment();
    if (!target) {
        return;
    }
    const auto member = GetRoomMember().lock();
    if (!member) {
        LOG_ERROR(Network, "Room: the network is not initialized");
        return;
    }
    member->BindOnStateChanged([](const RoomMember::State& state) {
        LOG_INFO(Network, "Room: state {}", static_cast<int>(state));
    });
    member->BindOnError([](const RoomMember::Error& error) {
        LOG_WARNING(Network, "Room: error {}", static_cast<int>(error));
    });
    LOG_INFO(Network, "Room: joining {}:{} as {}", target->host, target->port, target->nickname);
    member->Join(target->nickname, target->host.c_str(), target->port, 0, NoPreferredIP,
                 target->password);
}

} // namespace Network
