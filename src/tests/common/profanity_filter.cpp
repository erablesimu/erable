// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "common/profanity_filter.h"

using Common::Profanity::Contains;
using Common::Profanity::Mask;

TEST_CASE("Profanity: insults of several languages", "[common]") {
    for (const char* text :
         {"connard", "fils de pute", "fuck you", "hijo de puta", "vaffanculo", "Arschloch", "kurwa",
          "caralho", "kurva", "vittu", "сука", "БЛЯДЬ", "μαλάκας", "motherfucker", "scheiße"}) {
        INFO(text);
        REQUIRE(Contains(text));
    }
}

TEST_CASE("Profanity: spellings that get around a filter", "[common]") {
    for (const char* text :
         {"CONNARD !!", "c0nn4rd",   "c.o.n.n.a.r.d", "c o n n a r d", "connnnnaaard", "conanrd",
          "s4l0pe",     "$alope",    "p.u.t.e",       "putaaain",      "merde!",       "ENCULÉ",
          "encul3",     "fuuuuck",   "f u c k",       "phuck",         "sh1t",         "b1tch",
          "kurw4",      "k.u.r.w.a", "gilipoyas"}) {
        INFO(text);
        REQUIRE(Contains(text));
    }
}

TEST_CASE("Profanity: ordinary words stay", "[common]") {
    for (const char* text : {"Salut, on lance une course ?",
                             "Le conseil municipal a voté",
                             "J'ai un cancer de la patience",
                             "good night",
                             "Pick a car",
                             "cut the corner",
                             "Maria et Conrad arrivent",
                             "en carbone",
                             "assassin's creed",
                             "Scunthorpe United",
                             "classe ! bien joué",
                             "Nigeria",
                             "le score est 3-1",
                             "GG wp",
                             "schießen",
                             "fuktig",
                             "meerdere",
                             "conseguir",
                             "Typhus",
                             "bavard",
                             "galoper",
                             "salopette",
                             "computer"}) {
        INFO(text);
        REQUIRE_FALSE(Contains(text));
    }
}

TEST_CASE("Profanity: masking keeps the rest", "[common]") {
    REQUIRE(Mask("t'es qu'un connard, bien joué") == "t'es qu'un *******, bien joué");
    REQUIRE(Mask("ENCULÉ") == "******");
    REQUIRE(Mask("c.o.n.n.a.r.d !") == "************* !");
    REQUIRE(Mask("сука") == "****");
    REQUIRE(Mask("merci pour la partie") == "merci pour la partie");
    REQUIRE(Mask("") == "");
}
