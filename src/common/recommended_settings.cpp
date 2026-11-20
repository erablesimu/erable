// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <map>
#include <utility>
#include "common/recommended_settings.h"
#include "common/hardware_settings.h"

namespace Settings {

std::map<std::string, std::string> ParseRecommendedSettingsBackup(const std::string& backup) {
    std::map<std::string, std::string> saved;
    for (std::size_t begin = 0; begin < backup.size();) {
        const std::size_t end = (std::min)(backup.find(',', begin), backup.size());
        const std::size_t colon = backup.find(':', begin);
        if (colon < end && colon + 1 < end) {
            saved.emplace(backup.substr(begin, colon - begin),
                          backup.substr(colon + 1, end - colon - 1));
        }
        begin = end + 1;
    }
    return saved;
}

std::string FormatRecommendedSettingsBackup(const std::map<std::string, std::string>& entries) {
    std::string backup;
    for (const auto& [label, value] : entries) {
        if (!backup.empty()) {
            backup += ',';
        }
        backup.append(label).append(":").append(value);
    }
    return backup;
}

std::vector<RecommendedSettingChange> ApplyRecommendedSettings(Values& settings,
                                                               const HostHardware* hardware) {
    const auto saved =
        ParseRecommendedSettingsBackup(settings.recommended_settings_backup.GetValue());
    std::vector<RecommendedSettingChange> changes;
    std::map<std::string, std::string> backup;
    const auto apply = [&](auto& setting, const auto& value) {
        std::string previous = setting.ToStringGlobal();
        // A value backed up earlier stays; a setting the backup does not hold yet, such as one a
        // later build added to the table, joins it.
        const auto it = saved.find(setting.GetLabel());
        backup.emplace(setting.GetLabel(), it == saved.end() ? previous : it->second);
        if (setting.GetValue(true) == value) {
            return;
        }
        const bool using_global = setting.UsingGlobal();
        setting.SetGlobal(true);
        setting.SetValue(value);
        setting.SetGlobal(using_global);
        // A setting both tables set (Mario Kart World's FPS modes) is listed once, from the
        // value it had before the first.
        const auto first = std::find_if(changes.begin(), changes.end(),
                                        [&setting](const RecommendedSettingChange& change) {
                                            return change.setting == &setting;
                                        });
        if (first != changes.end()) {
            previous = std::move(first->previous);
            changes.erase(first);
        }
        if (previous != setting.ToStringGlobal()) {
            changes.push_back({&setting, std::move(previous), setting.ToStringGlobal()});
        }
    };
    ForEachRecommendedSetting(settings, apply);
    if (hardware) {
        ForEachHardwareSetting(settings, *hardware, apply);
    }
    settings.recommended_settings_backup = FormatRecommendedSettingsBackup(backup);
    return changes;
}

std::vector<RecommendedSettingChange> RestorePreviousSettings(Values& settings) {
    const auto saved =
        ParseRecommendedSettingsBackup(settings.recommended_settings_backup.GetValue());
    std::vector<RecommendedSettingChange> changes;
    const auto restore = [&](auto& setting, const auto&) {
        const auto it = saved.find(setting.GetLabel());
        if (it == saved.end()) {
            return;
        }
        std::string previous = setting.ToStringGlobal();
        const bool using_global = setting.UsingGlobal();
        setting.SetGlobal(true);
        setting.LoadString(it->second);
        setting.SetGlobal(using_global);
        if (setting.ToStringGlobal() != previous) {
            changes.push_back({&setting, std::move(previous), setting.ToStringGlobal()});
        }
    };
    ForEachRecommendedSetting(settings, restore);
    // Only the labels matter here: the hardware table's settings are the same for every PC
    ForEachHardwareSetting(settings, HostHardware{}, [&](auto& setting, const auto& value) {
        if (std::none_of(changes.begin(), changes.end(),
                         [&setting](const auto& change) { return change.setting == &setting; })) {
            restore(setting, value);
        }
    });
    settings.recommended_settings_backup = std::string{};
    return changes;
}

} // namespace Settings
