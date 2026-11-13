// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <charconv>
#include <string>
#include <vector>
#include "common/logging.h"
#include "common/scm_rev.h"
#include "frontend_common/update_checker.h"

namespace UpdateChecker {

namespace {

std::optional<u64> ParseBuildNumber(std::string_view tag) {
    constexpr std::string_view prefix = "build-";
    if (!tag.starts_with(prefix)) {
        return std::nullopt;
    }
    tag.remove_prefix(prefix.size());
    u64 number{};
    const auto [end, error] = std::from_chars(tag.data(), tag.data() + tag.size(), number);
    if (error != std::errc{} || end != tag.data() + tag.size() || number == 0) {
        return std::nullopt;
    }
    return number;
}

/// A release's notes: its text above "# Packages".
std::string_view NotesOf(const Common::Net::Release& release) {
    std::string_view body{release.body};
    if (const auto pos = body.find("# Packages"); pos != std::string_view::npos) {
        body = body.substr(0, pos);
    }
    while (!body.empty() && (body.back() == '\n' || body.back() == '\r' || body.back() == ' ')) {
        body.remove_suffix(1);
    }
    return body;
}

} // namespace

std::string CombineNotes(std::span<const Common::Net::Release> releases,
                         std::string_view build_tag, std::string_view update_tag) {
    const auto installed = ParseBuildNumber(build_tag);
    const auto update = ParseBuildNumber(update_tag);
    if (!installed || !update) {
        return {};
    }
    std::vector<std::pair<u64, const Common::Net::Release*>> newer;
    for (const auto& release : releases) {
        const auto number = ParseBuildNumber(release.tag);
        if (number && *number > *installed && *number <= *update) {
            newer.emplace_back(*number, &release);
        }
    }
    std::ranges::sort(newer, [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });
    std::string notes;
    for (const auto& [number, release] : newer) {
        const auto text = NotesOf(*release);
        if (text.find_first_not_of(" \r\n") == std::string_view::npos) {
            continue;
        }
        if (newer.size() > 1) {
            notes += "## Build " + std::to_string(number) + "\n\n";
        }
        notes += text;
        notes += "\n\n";
    }
    return notes;
}

Failure ClassifyFailure(int status) {
    if (status == 403 || status == 429) {
        return Failure::RateLimited;
    }
    return status >= 400 ? Failure::Http : Failure::Network;
}

std::optional<Common::Net::Release> SelectUpdate(std::span<const Common::Net::Release> releases,
                                                 std::string_view build_tag,
                                                 std::string_view platform,
                                                 Common::Net::LinuxPackage package) {
    auto newest = ParseBuildNumber(build_tag);
    if (!newest) {
        return std::nullopt;
    }

    const Common::Net::Release* selected = nullptr;
    for (const auto& release : releases) {
        const auto number = ParseBuildNumber(release.tag);
        if (number && *number > *newest && !release.GetPlatformAssets(platform, package).empty()) {
            newest = number;
            selected = &release;
        }
    }
    return selected ? std::make_optional(*selected) : std::nullopt;
}

CheckResult Check() {
    if (!ParseBuildNumber(Common::g_build_release_tag) ||
        std::string_view{Common::g_build_release_platform}.empty()) {
        LOG_INFO(Frontend, "Update check skipped: build tag '{}', platform '{}'",
                 Common::g_build_release_tag, Common::g_build_release_platform);
        return {};
    }

    const auto list = Common::Net::GetReleases();
    if (!list.ok) {
        const auto failure = ClassifyFailure(list.status);
        LOG_WARNING(Frontend, "Update check failed with status {}", list.status);
        return {.failure = failure, .status = list.status};
    }

    auto update = SelectUpdate(list.releases, Common::g_build_release_tag,
                               Common::g_build_release_platform,
                               Common::Net::CurrentLinuxPackage());
    if (update) {
        LOG_INFO(Frontend, "Received update {}", update->title);
        // Every build since the installed one, newest first, rather than the latest one's notes
        // alone (each build's notes hold only what it changed).
        if (auto notes = CombineNotes(list.releases, Common::g_build_release_tag, update->tag);
            !notes.empty()) {
            update->body = std::move(notes) + "# Packages\n";
        }
    } else {
        LOG_INFO(Frontend, "No update newer than {} for {} among {} releases",
                 Common::g_build_release_tag, Common::g_build_release_platform,
                 list.releases.size());
    }
    return {.update = std::move(update)};
}

std::optional<Common::Net::Release> GetUpdate() {
    return Check().update;
}

} // namespace UpdateChecker
