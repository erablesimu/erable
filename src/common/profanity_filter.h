// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

/// Insults and swear words of the European languages, for the room chat and the names shown in
/// it. The text is compared after folding what people do to get around a filter: capitals,
/// accents, letters written as digits or symbols ("s4l0pe", "@$$"), letters repeated ("fuuuck"),
/// spaced or dotted out ("f u c k", "c.o.n.n.a.r.d"), common sound-alike spellings ("phuck",
/// "kurva"), and two letters swapped in the longer words ("conanrd").
namespace Common::Profanity {

/// Whether `text` (UTF-8) holds an insult or swear word
bool Contains(std::string_view text);

/// `text` (UTF-8) with each word holding an insult or swear word replaced by as many '*' as it
/// has characters; the rest is kept as it was
std::string Mask(std::string_view text);

} // namespace Common::Profanity
