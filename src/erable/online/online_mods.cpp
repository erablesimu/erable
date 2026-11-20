// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erable/online/online_mods.h"

#include <algorithm>

#include <QSettings>
#include <QString>
#include <QStringList>

#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"

namespace Online::Mods {

namespace {

constexpr auto Group = "online/paused_mods";

QSettings Store() {
    return QSettings{QStringLiteral("Erable"), QStringLiteral("Erable")};
}

} // Anonymous namespace

std::vector<std::string> Active(Core::System& system, u64 program_id) {
    std::vector<std::string> names;
    if (program_id == 0) {
        return names;
    }
    const FileSys::PatchManager manager{program_id, system.GetFileSystemController(),
                                        system.GetContentProvider()};
    for (const FileSys::Patch& patch : manager.GetPatches()) {
        if (patch.type == FileSys::PatchType::Mod && patch.enabled &&
            !FileSys::PatchManager::IsBuiltinPatch(program_id, patch.name)) {
            names.push_back(patch.name);
        }
    }
    return names;
}

void Pause(u64 program_id, const std::vector<std::string>& names) {
    auto& disabled = Settings::values.disabled_addons[program_id];
    QSettings settings = Store();
    settings.beginGroup(QString::fromLatin1(Group));
    const QString key = QString::number(program_id, 16);
    QStringList paused = settings.value(key).toStringList();
    for (const std::string& name : names) {
        if (std::find(disabled.begin(), disabled.end(), name) == disabled.end()) {
            disabled.push_back(name);
            paused.append(QString::fromStdString(name));
        }
    }
    std::sort(disabled.begin(), disabled.end());
    settings.setValue(key, paused);
}

int Restore() {
    QSettings settings = Store();
    settings.beginGroup(QString::fromLatin1(Group));
    int restored = 0;
    for (const QString& key : settings.childKeys()) {
        bool ok = false;
        const u64 program_id = key.toULongLong(&ok, 16);
        if (ok) {
            auto& disabled = Settings::values.disabled_addons[program_id];
            for (const QString& name : settings.value(key).toStringList()) {
                const auto found = std::find(disabled.begin(), disabled.end(), name.toStdString());
                if (found != disabled.end()) {
                    disabled.erase(found);
                    ++restored;
                }
            }
        }
        settings.remove(key);
    }
    return restored;
}

} // namespace Online::Mods
