// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>
#include <utility>
#include <vector>

namespace GameListDiscovery {

struct Result {
    std::vector<std::filesystem::path> files;
    std::vector<std::filesystem::path> directories;
    /// Compressed archives (.zip, .7z, .rar) found where games are searched: never read, they
    /// are reported so the player knows to extract them.
    std::vector<std::filesystem::path> archives;
    std::size_t skipped_romfs{};
    std::size_t errors{};
};

inline bool IsExeFS(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path / "main", ec) &&
           std::filesystem::is_regular_file(path / "main.npdm", ec);
}

inline bool IsArchive(const std::filesystem::path& path) {
    auto extension = path.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](char8_t c) {
        return static_cast<char8_t>(std::tolower(static_cast<unsigned char>(c)));
    });
    return extension == u8".zip" || extension == u8".7z" || extension == u8".rar";
}

/// `shortcut` gives the target of a Windows shortcut file (.lnk), an empty path for any other
/// file: a shortcut to a folder is walked like the folder, one to a game lists the game.
template <typename Supported, typename Stopped, typename Shortcut>
Result Discover(const std::filesystem::path& root, bool deep, Supported supported,
                Stopped stopped, Shortcut shortcut) {
    namespace fs = std::filesystem;
    Result result;
    std::unordered_set<fs::path> files;
    std::unordered_set<fs::path> directories;
    const auto add_file = [&](const fs::path& path) {
        if (files.insert(path).second) {
            result.files.push_back(path);
        }
    };
    const auto watch = [&](const fs::path& path) {
        if (directories.insert(path).second) {
            result.directories.push_back(path);
        }
    };
    const auto probe = [&](const fs::path& path) {
        if (IsExeFS(path)) {
            watch(path);
            add_file(path / "main");
        }
    };
    const auto probe_layouts = [&](const fs::path& path) {
        for (const auto& content : {path, path / "base"}) {
            probe(content / "exefs");
            const auto updates = content / "updates";
            std::error_code ec;
            if (!fs::is_directory(updates, ec)) {
                continue;
            }
            watch(updates);
            for (fs::directory_iterator it{updates, fs::directory_options::skip_permission_denied,
                                           ec};
                 !ec && it != fs::directory_iterator{} && !stopped(); it.increment(ec)) {
                probe(it->path() / "exefs");
            }
        }
    };

    // Folders already walked, by their real path: a linked folder (shortcut, junction or symlink)
    // is followed, and one that leads back into a walked folder is not walked again.
    std::unordered_set<fs::path> walked;
    const auto first_walk = [&](const fs::path& path) {
        std::error_code ec;
        const auto real = fs::canonical(path, ec);
        return walked.insert(ec ? path : real).second;
    };
    std::vector<std::pair<fs::path, unsigned>> pending{{root, 0}};
    while (!pending.empty() && !stopped()) {
        auto [directory, depth] = std::move(pending.back());
        pending.pop_back();
        if (!first_walk(directory)) {
            continue;
        }
        watch(directory);
        if (IsExeFS(directory)) {
            add_file(directory / "main");
            continue;
        }
        if (!deep) {
            probe_layouts(directory);
        }
        std::error_code ec;
        for (fs::directory_iterator it{directory, fs::directory_options::skip_permission_denied,
                                       ec};
             !ec && it != fs::directory_iterator{} && !stopped(); it.increment(ec)) {
            const auto& entry = *it;
            std::error_code entry_ec;
            if (entry.is_directory(entry_ec)) {
                auto name = entry.path().filename().u8string();
                std::transform(name.begin(), name.end(), name.begin(), [](char8_t c) {
                    return static_cast<char8_t>(std::tolower(static_cast<unsigned char>(c)));
                });
                if (name == u8"romfs" && IsExeFS(directory / "exefs")) {
                    ++result.skipped_romfs;
                    continue;
                }
                if (deep || depth == 0) {
                    pending.emplace_back(entry.path(), depth + 1);
                }
            } else if (entry.is_regular_file(entry_ec) && (deep || depth == 0)) {
                if (supported(entry.path())) {
                    add_file(entry.path());
                } else if (IsArchive(entry.path())) {
                    result.archives.push_back(entry.path());
                } else if (const fs::path target = shortcut(entry.path()); !target.empty()) {
                    std::error_code target_ec;
                    if (fs::is_directory(target, target_ec)) {
                        pending.emplace_back(target, depth + 1);
                    } else if (fs::is_regular_file(target, target_ec) && supported(target)) {
                        add_file(target);
                    }
                }
            }
        }
        result.errors += ec ? 1 : 0;
    }
    return result;
}

} // namespace GameListDiscovery
