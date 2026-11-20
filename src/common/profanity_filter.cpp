// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/profanity_filter.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Common::Profanity {

namespace {

/// How a listed word is found in a word of the text, once both are folded
enum class Kind {
    Word,   ///< The whole word (a trailing "s" allowed)
    Prefix, ///< Its start: the word and what is built on it ("connard", "connards")
    Infix,  ///< Anywhere in it ("motherfucker")
};

struct Entry {
    std::string_view word;
    Kind kind;
    /// Compared as written, accents and double letters kept, where the loose folding would
    /// catch an ordinary word of another language ("pute" and the Danish "putte", "pussy" and
    /// the Lithuanian "pusiau")
    bool strict = false;
};

constexpr Kind W = Kind::Word;
constexpr Kind P = Kind::Prefix;
constexpr Kind I = Kind::Infix;
constexpr bool S = true;

// Spelled as people write them; folded like the text before comparing. A word of a language that
// is ordinary in another one stays out ("con" is "with" in Spanish, "fan" a fan in English).
constexpr Entry Words[] = {
    // English
    {"fuck", I, S},
    {"fuck", W},
    {"fucking", W},
    {"fucker", P},
    {"fucked", W},
    {"shit", P, S},
    {"bullshit", P, S},
    {"horseshit", W, S},
    {"batshit", W, S},
    {"dipshit", W, S},
    {"bitch", P},
    {"bastard", P},
    {"cunt", W, S},
    {"dick", W, S},
    {"dickhead", P},
    {"cock", W, S},
    {"cocksuck", P},
    {"pussy", P, S},
    {"whore", P},
    {"asshole", P},
    {"arsehole", P},
    {"wanker", P, S},
    {"twat", P, S},
    {"prick", W, S},
    {"bollock", P},
    {"nigg", P, S},
    {"faggot", P, S},
    {"tranny", W, S},
    {"kike", W, S},
    {"spic", W},
    {"chink", W},
    {"retarded", W},
    // French
    {"merde", W, S},
    {"merdeux", P},
    {"merdique", P},
    {"putain", P},
    {"pute", W, S},
    {"salope", P},
    {"salopard", P},
    {"salaud", P},
    {"connard", P},
    {"connasse", P},
    {"conne", W, S},
    {"pédé", W, S},
    {"tarlouze", P},
    {"tapette", W, S},
    {"encule", P},
    {"enculer", P},
    {"batard", P},
    {"fdp", W},
    {"ntm", W},
    {"nique", W},
    {"niquer", P},
    {"niktamer", P},
    {"couille", P},
    {"couillon", P},
    {"chier", W},
    {"petasse", P},
    {"pouffiasse", P},
    {"grognasse", P},
    {"enfoire", P},
    {"bougnoule", P},
    {"nègre", W, S},
    {"abruti", P},
    {"cretin", P},
    {"trouduc", W},
    // Spanish
    {"puta", W, S},
    {"puto", W, S},
    {"mierda", P},
    {"joder", P, S},
    {"jodido", P},
    {"coño", W, S},
    {"cabron", P},
    {"gilipollas", P},
    {"gilipoyas", P},
    {"pendejo", P},
    {"maricon", P},
    {"polla", W, S},
    {"culero", W},
    {"chingar", P},
    {"chingada", P},
    {"verga", W, S},
    {"carajo", W},
    {"cojones", W},
    {"zorra", W, S},
    // Italian
    {"cazzo", W, S},
    {"cazzi", W, S},
    {"cazzata", P, S},
    {"cazzone", P, S},
    {"merda", W},
    {"stronzo", P},
    {"stronza", P},
    {"vaffanculo", P},
    {"fanculo", P},
    {"puttana", P},
    {"coglione", P},
    {"minchia", P},
    {"frocio", P},
    {"froci", W},
    {"bastardo", P},
    // German
    {"scheiße", P, S},
    {"scheisse", P, S},
    {"scheiss", W, S},
    {"arschloch", P},
    {"arsch", W},
    {"fotze", P},
    {"wichser", P},
    {"wixer", W},
    {"hurensohn", P},
    {"hure", W, S},
    {"ficken", P, S},
    {"schlampe", P},
    {"missgeburt", P},
    {"spast", W},
    {"schwuchtel", P},
    {"kanake", W},
    // Portuguese
    {"porra", W, S},
    {"caralho", P},
    {"foda", W, S},
    {"fodase", W},
    {"fodido", P},
    {"cuzão", W, S},
    {"viado", W},
    {"buceta", P},
    {"arrombado", P},
    // Dutch
    {"kut", W, S},
    {"lul", W, S},
    {"klootzak", P},
    {"godverdomme", P},
    {"hoer", W},
    {"neuken", P},
    {"mongool", W, S},
    {"flikker", P, S},
    {"tering", P},
    {"tyfus", W, S},
    // Polish
    {"kurwa", P},
    {"chuj", P},
    {"pierdol", P},
    {"spierdal", P},
    {"wypierdal", P},
    {"jebac", P},
    {"jebany", P},
    {"zajebac", P},
    {"pizda", P},
    {"pizdz", P},
    {"skurwysyn", P},
    {"cipa", W, S},
    {"dupek", W},
    {"cwel", W},
    // Czech, Slovak
    {"kurva", P},
    {"kokot", P},
    {"hovno", W},
    {"zmrd", W},
    {"čurák", W},
    {"prdel", W},
    // Hungarian
    {"geci", W, S},
    {"picsa", P},
    {"fasz", W, S},
    {"faszt", W},
    {"faszom", W},
    {"fasza", W},
    {"bazmeg", P},
    {"baszd", P},
    {"buzi", W, S},
    {"köcsög", P},
    // Romanian
    {"muie", W},
    {"futu", W},
    {"futui", W},
    {"căcat", W, S},
    {"poponar", W},
    // Scandinavian
    {"fitta", W, S},
    {"kuk", W, S},
    {"jävla", W},
    {"jævla", W},
    {"knulla", P},
    {"faen", W},
    {"pikk", W, S},
    {"kuksuger", P},
    {"fisse", W, S},
    {"luder", W},
    // Finnish
    {"vittu", P, S},
    {"vitun", W, S},
    {"vitut", W, S},
    {"vituttaa", P, S},
    {"perkele", W, S},
    {"paska", W, S},
    {"kyrpä", W},
    {"huora", W},
    // Croatian, Serbian, Bosnian, Slovene
    {"jebem", P},
    {"jebote", P},
    {"jebi", W},
    {"kurac", W},
    {"kurca", W, S},
    {"kurcu", W, S},
    {"govno", P},
    // Turkish
    {"orospu", P},
    {"siktir", P},
    {"yarrak", P, S},
    // Greek, and as written in Latin letters
    {"μαλάκας", W},
    {"πούστης", W},
    {"μουνί", W},
    {"σκατά", W},
    {"πουτάνα", W},
    {"γαμώ", W},
    {"γαμώτο", W},
    {"γαμήσου", W},
    {"malakas", W},
    {"poustis", W},
    {"poutana", W},
    // Russian, Ukrainian, Bulgarian, and as written in Latin letters
    {"бля", W},
    {"бляд", P},
    {"блят", P},
    {"хуй", P},
    {"хуе", P},
    {"хуя", P},
    {"пизд", P},
    {"ебан", P},
    {"ебат", P},
    {"ебал", P},
    {"ебло", P},
    {"уеба", P},
    {"уебк", P},
    {"уебо", P},
    {"заеб", P},
    {"выеб", P},
    {"долбоеб", P},
    {"сука", W},
    {"суки", W},
    {"сучка", P},
    {"сучар", P},
    {"мудак", P},
    {"мудил", P},
    {"пидор", P},
    {"пидар", P},
    {"пидр", P},
    {"гандон", P},
    {"шлюх", P},
    {"говн", P},
    {"курв", P},
    {"blyat", P},
    {"blyad", P},
    {"suka", W, S},
    {"cyka", W, S},
    {"pizdec", P},
    {"nahui", W, S},
    {"nahuy", W, S},
    {"pidor", P},
    {"mudak", P},
};

/// Ordinary words a listed one would otherwise catch, folded like the text
constexpr std::string_view Allowed[] = {
    "assassin",   "assassins", "assassinat", "bassin",    "classe",    "scunthorpe",
    "shitake",    "shiitake",  "cocktail",   "cocktails", "cockpit",   "peacock",
    "hancock",    "dickens",   "pussycat",   "snigger",   "sniggered", "salopette",
    "salopettes", "kurvy",     "prickly",    "dickson",   "putative",  "computer",
};

constexpr std::size_t FuzzyLength = 7; ///< Listed words this long match with two letters swapped

/// A letter written as a digit or symbol ("s4l0p3"), when the word goes on after it
std::optional<char32_t> Leet(char32_t c) {
    switch (c) {
    case U'0':
        return U'o';
    case U'1':
    case U'!':
    case U'|':
        return U'i';
    case U'3':
    case U'€':
        return U'e';
    case U'4':
    case U'@':
        return U'a';
    case U'5':
    case U'$':
        return U's';
    case U'7':
        return U't';
    case U'8':
        return U'b';
    default:
        return std::nullopt;
    }
}

/// Characters people put inside a word to break it up ("c.o.n", "f-u-c-k", "c*nnard")
bool IsJoiner(char32_t c) {
    switch (c) {
    case U'.':
    case U'-':
    case U'_':
    case U'*':
    case U'~':
    case U'·':
    case U'​':
    case U'‌':
    case U'‍':
    case U'⁠':
    case U'﻿':
    case U'­':
        return true;
    default:
        return false;
    }
}

/// The lower-case, accent-free letters `c` stands for (several for "æ", "ß"...), empty when it is
/// no letter
std::u32string_view Fold(char32_t c, char32_t* buffer) {
    const auto one = [&](char32_t letter) {
        buffer[0] = letter;
        return std::u32string_view{buffer, 1};
    };
    if (c >= U'A' && c <= U'Z') {
        return one(c - U'A' + U'a');
    }
    if (c >= U'a' && c <= U'z') {
        return one(c);
    }
    if (c >= 0xC0 && c <= 0xFF) {
        // À..ÿ; '?' marks the ones handled below
        constexpr std::u32string_view Latin1 =
            U"aaaaaa?ceeeeiiiidnooooo?ouuuuy??aaaaaa?ceeeeiiiidnooooo?ouuuuy?y";
        static_assert(Latin1.size() == 0x40);
        switch (c) {
        case 0xC6:
        case 0xE6:
            return U"ae";
        case 0xDE:
        case 0xFE:
            return U"th";
        case 0xDF:
            return U"ss";
        case 0xD7:
        case 0xF7:
            return {};
        default:
            return one(Latin1[c - 0xC0]);
        }
    }
    if (c >= 0x100 && c <= 0x17F) {
        if (c == 0x152 || c == 0x153) {
            return U"oe";
        }
        constexpr std::u32string_view ExtendedA =
            U"aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiiiijjkkklllllllllln"
            U"nnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";
        static_assert(ExtendedA.size() == 0x80);
        return one(ExtendedA[c - 0x100]);
    }
    if (c == 0x218 || c == 0x219) { // Ș ș
        return one(U's');
    }
    if (c == 0x21A || c == 0x21B) { // Ț ț
        return one(U't');
    }
    // Greek: lower case, without the accents and the final sigma
    if (c >= 0x391 && c <= 0x3A9) {
        return one(c + 0x20);
    }
    switch (c) {
    case 0x386:
    case 0x3AC:
        return one(0x3B1);
    case 0x388:
    case 0x3AD:
        return one(0x3B5);
    case 0x389:
    case 0x3AE:
        return one(0x3B7);
    case 0x38A:
    case 0x3AF:
    case 0x3CA:
    case 0x390:
        return one(0x3B9);
    case 0x38C:
    case 0x3CC:
        return one(0x3BF);
    case 0x38E:
    case 0x3CD:
    case 0x3CB:
    case 0x3B0:
        return one(0x3C5);
    case 0x38F:
    case 0x3CE:
        return one(0x3C9);
    case 0x3C2:
        return one(0x3C3);
    default:
        break;
    }
    if (c >= 0x3B1 && c <= 0x3C9) {
        return one(c);
    }
    // Cyrillic: lower case, "ё" as "е"
    if (c == 0x401 || c == 0x451) {
        return one(0x435);
    }
    if (c >= 0x410 && c <= 0x42F) {
        return one(c + 0x20);
    }
    if (c >= 0x400 && c <= 0x40F) {
        return one(c + 0x50);
    }
    if (c >= 0x430 && c <= 0x45F) {
        return one(c);
    }
    return {};
}

/// `c` in lower case, accents kept: the strict entries are compared so
char32_t Lower(char32_t c) {
    if ((c >= U'A' && c <= U'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) {
        return c + 0x20;
    }
    if (c >= 0x100 && c <= 0x17F) {
        // Pairs, upper case first, except the two runs that start on an odd code point
        const bool odd_first = (c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E);
        const bool upper = odd_first ? (c % 2 == 1) : (c % 2 == 0 && c != 0x138);
        return upper ? c + 1 : c;
    }
    if (c >= 0x391 && c <= 0x3A9) {
        return c + 0x20;
    }
    if (c >= 0x410 && c <= 0x42F) {
        return c + 0x20;
    }
    if (c >= 0x400 && c <= 0x40F) {
        return c + 0x50;
    }
    return c;
}

/// Spellings that sound the same written one way ("phuck", "kurwa"), then each run of
/// a letter as one ("fuuuck"): applied to the text's words and to the listed ones alike
std::u32string Simplify(std::u32string_view letters) {
    std::u32string out;
    out.reserve(letters.size());
    for (std::size_t i = 0; i < letters.size(); ++i) {
        char32_t c = letters[i];
        const char32_t next = i + 1 < letters.size() ? letters[i + 1] : 0;
        if (c == U'p' && next == U'h') {
            c = U'f';
            ++i;
        } else if (c == U'c' && next == U'k') {
            c = U'k';
            ++i;
        } else if (c == U'y') {
            c = U'i';
        } else if (c == U'w') {
            c = U'v';
        }
        if (out.empty() || out.back() != c) {
            out.push_back(c);
        }
    }
    return out;
}

/// The code points of UTF-8 text, each with where it starts; a bad byte stands for itself
struct CodePoint {
    char32_t value;
    std::size_t offset;
};

std::vector<CodePoint> Decode(std::string_view text) {
    std::vector<CodePoint> points;
    points.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const auto byte = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        char32_t value = byte;
        if (byte >= 0xF0 && byte < 0xF8) {
            length = 4;
            value = byte & 0x07;
        } else if (byte >= 0xE0) {
            length = 3;
            value = byte & 0x0F;
        } else if (byte >= 0xC0) {
            length = 2;
            value = byte & 0x1F;
        }
        if (length > 1) {
            bool valid = i + length <= text.size();
            for (std::size_t k = 1; valid && k < length; ++k) {
                const auto continuation = static_cast<unsigned char>(text[i + k]);
                valid = (continuation & 0xC0) == 0x80;
                value = (value << 6) | (continuation & 0x3F);
            }
            if (!valid) {
                length = 1;
                value = byte;
            }
        }
        points.push_back({value, i});
        i += length;
    }
    return points;
}

/// A word of the text: its letters, folded and simplified, and the bytes it covers
struct Token {
    std::u32string letters;
    std::u32string strict; ///< The letters in lower case, as written
    std::size_t begin;
    std::size_t end;
};

std::vector<Token> Tokenize(std::string_view text) {
    const std::vector<CodePoint> points = Decode(text);
    std::vector<Token> tokens;
    std::u32string letters;
    std::u32string strict;
    std::size_t begin = 0;
    std::size_t end = 0;
    const auto finish = [&] {
        if (!letters.empty()) {
            tokens.push_back({Simplify(letters), strict, begin, end});
            letters.clear();
            strict.clear();
        }
    };
    char32_t buffer[2];
    for (std::size_t i = 0; i < points.size(); ++i) {
        const char32_t c = points[i].value;
        const std::size_t next_offset = i + 1 < points.size() ? points[i + 1].offset : text.size();
        std::u32string_view folded = Fold(c, buffer);
        if (folded.empty()) {
            // A digit or symbol is a letter inside a word ("s4lope") or ending one ("kurw4"), but
            // "!" and "|" ending a word are punctuation ("merde!")
            const auto leet = Leet(c);
            char32_t next_buffer[2];
            const bool more =
                i + 1 < points.size() &&
                (!Fold(points[i + 1].value, next_buffer).empty() || Leet(points[i + 1].value));
            if (leet && (more || (!letters.empty() && c != U'!' && c != U'|'))) {
                buffer[0] = *leet;
                folded = std::u32string_view{buffer, 1};
                strict.push_back(*leet);
            }
        } else {
            strict.push_back(Lower(c));
        }
        if (!folded.empty()) {
            if (letters.empty()) {
                begin = points[i].offset;
            }
            letters.append(folded);
            end = next_offset;
            continue;
        }
        if (IsJoiner(c) && !letters.empty()) {
            continue; // The word goes on past it, if a letter follows
        }
        finish();
    }
    finish();

    // Letters spaced out one by one ("f u c k") read as one word
    std::vector<Token> merged;
    for (std::size_t i = 0; i < tokens.size();) {
        std::size_t run = i;
        while (run < tokens.size() && tokens[run].letters.size() == 1) {
            ++run;
        }
        if (run - i >= 3) {
            Token joined{{}, {}, tokens[i].begin, tokens[run - 1].end};
            for (std::size_t k = i; k < run; ++k) {
                joined.letters += tokens[k].letters;
                joined.strict += tokens[k].strict;
            }
            joined.letters = Simplify(joined.letters);
            merged.push_back(std::move(joined));
            i = run;
        } else {
            merged.push_back(std::move(tokens[i]));
            ++i;
        }
    }
    return merged;
}

std::u32string FoldWord(std::string_view word, bool strict) {
    std::u32string letters;
    char32_t buffer[2];
    for (const CodePoint& point : Decode(word)) {
        if (strict) {
            if (!Fold(point.value, buffer).empty()) {
                letters.push_back(Lower(point.value));
            }
        } else {
            letters.append(Fold(point.value, buffer));
        }
    }
    return strict ? letters : Simplify(letters);
}

/// Whether two words differ by two neighbouring letters swapped ("salpoe")
bool IsSwap(std::u32string_view a, std::u32string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    std::size_t i = 0;
    while (i < a.size() && a[i] == b[i]) {
        ++i;
    }
    return i + 1 < a.size() && a[i] == b[i + 1] && a[i + 1] == b[i] &&
           a.substr(i + 2) == b.substr(i + 2);
}

struct Folded {
    std::u32string word;
    Kind kind;
    bool strict;
    std::u32string written; ///< Its letters as listed, in lower case: swaps are found on these
};

struct Lists {
    std::vector<Folded> words;
    std::unordered_set<std::u32string> allowed;
};

const Lists& FoldedLists() {
    static const Lists lists = [] {
        Lists folded;
        for (const Entry& entry : Words) {
            // A listed phrase ("ta gueule") is found word by word: its last word stands for it
            std::string_view word = entry.word;
            if (const auto space = word.rfind(' '); space != std::string_view::npos) {
                word = word.substr(space + 1);
            }
            folded.words.push_back(
                {FoldWord(word, entry.strict), entry.kind, entry.strict, FoldWord(word, true)});
        }
        for (const std::string_view word : Allowed) {
            folded.allowed.insert(FoldWord(word, false));
        }
        return folded;
    }();
    return lists;
}

bool Matches(const Token& token, const Folded& entry) {
    const std::u32string& text = entry.strict ? token.strict : token.letters;
    const std::u32string& word = entry.word;
    if (word.empty()) {
        return false;
    }
    // Two letters swapped in a long word, as written: "conanrd" is "connard" mistyped
    const std::u32string& written = entry.written;
    const bool swappable = !entry.strict && written.size() >= FuzzyLength;
    switch (entry.kind) {
    case Kind::Word:
        if (text == word || (text.size() == word.size() + 1 && text.back() == U's' &&
                             text.compare(0, word.size(), word) == 0)) {
            return true;
        }
        return swappable && IsSwap(token.strict, written);
    case Kind::Prefix:
        if (text.compare(0, word.size(), word) == 0) {
            return true;
        }
        return swappable && token.strict.size() >= written.size() &&
               IsSwap(std::u32string_view{token.strict}.substr(0, written.size()), written);
    case Kind::Infix:
        return text.find(word) != std::u32string::npos;
    }
    return false;
}

bool IsOffensive(const Token& token) {
    const Lists& lists = FoldedLists();
    if (lists.allowed.contains(token.letters)) {
        return false;
    }
    return std::any_of(lists.words.begin(), lists.words.end(),
                       [&](const Folded& entry) { return Matches(token, entry); });
}

} // Anonymous namespace

bool Contains(std::string_view text) {
    const std::vector<Token> tokens = Tokenize(text);
    return std::any_of(tokens.begin(), tokens.end(), IsOffensive);
}

std::string Mask(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t copied = 0;
    for (const Token& token : Tokenize(text)) {
        if (!IsOffensive(token)) {
            continue;
        }
        out.append(text.substr(copied, token.begin - copied));
        // One '*' per character of the word, separators inside it included
        for (std::size_t i = token.begin; i < token.end; ++i) {
            if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
                out.push_back('*');
            }
        }
        copied = token.end;
    }
    out.append(text.substr(copied));
    return out;
}

} // namespace Common::Profanity
