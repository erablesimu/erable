// The tutorials, by section (emulator, games) and game. Add an entry here for each new tutorial.
const TUTORIALS = [
  {
    section: "emu", href: "guides/install.html", image: "guides/img/game-list.png", steps: 5,
    title: { fr: "Installer Erable", en: "Install Erable", de: "Erable installieren" },
    summary: {
      fr: "Télécharger, ajouter firmware et clés, régler la langue et la manette, puis lancer votre premier jeu.",
      en: "Download, add firmware and keys, set the language and controller, then start your first game.",
      de: "Herunterladen, Firmware und Schlüssel hinzufügen, Sprache und Controller einstellen, dann das erste Spiel starten.",
    },
  },
  {
    section: "emu", href: "guides/install.html#console", image: "guides/img/tools-menu.png",
    title: { fr: "Firmware et clés", en: "Firmware and keys", de: "Firmware und Schlüssel" },
    summary: {
      fr: "Installer le système et les clés tirés de votre propre Switch.",
      en: "Install the system and keys dumped from your own Switch.",
      de: "System und Schlüssel aus deiner eigenen Switch installieren.",
    },
  },
  {
    section: "emu", href: "guides/install.html#language", image: "guides/img/system-language.png",
    title: { fr: "Jeux en français", en: "Game language", de: "Spielsprache" },
    summary: {
      fr: "Choisir la langue et la région des jeux.",
      en: "Choose the games' language and region.",
      de: "Sprache und Region der Spiele wählen.",
    },
  },
  {
    section: "emu", href: "guides/install.html#controller", image: "guides/img/controls.png",
    title: { fr: "Configurer une manette", en: "Set up a controller", de: "Controller einrichten" },
    summary: {
      fr: "Associer chaque bouton, et que faire si la manette n'apparaît pas.",
      en: "Map each button, and what to do if the controller doesn't show up.",
      de: "Jede Taste belegen, und was tun, wenn der Controller nicht erscheint.",
    },
  },
  {
    section: "games", game: "ocarina", href: "guides/zelda-ocarina.html", image: "guides/img/ocarina-graphics.png", steps: 7,
    title: { fr: "Optimiser Ocarina of Time", en: "Optimise Ocarina of Time", de: "Ocarina of Time optimieren" },
    summary: {
      fr: "Mod de shaders, mémoire, Vulkan, DLSS, précompilation et partage du cache entre PC.",
      en: "Shader mod, memory, Vulkan, DLSS, precompilation and sharing the cache between PCs.",
      de: "Shader-Mod, Speicher, Vulkan, DLSS, Vorkompilieren und den Cache zwischen PCs teilen.",
    },
  },
];

const GAMES = {
  ocarina: "Zelda: Ocarina of Time",
};

let selectedGame = new URLSearchParams(location.search).get("jeu") || "all";

function card(tutorial) {
  const meta = [];
  if (tutorial.game) {
    meta.push(el("span", { class: "tag" }, GAMES[tutorial.game]));
  }
  if (tutorial.steps) {
    meta.push(el("span", { class: "tag muted" }, t("tutos.steps", { n: tutorial.steps })));
  }
  return el("a", { class: "card tuto-card", href: tutorial.href },
    el("div", { class: "thumb" }, el("img", { src: tutorial.image, alt: "", loading: "lazy" })),
    el("div", { class: "tuto-card-body" },
      el("div", { class: "tags" }, ...meta),
      el("h3", {}, tutorial.title[LANG]),
      el("p", {}, tutorial.summary[LANG]),
      el("span", { class: "more" }, t("tutos.read"), " →")));
}

function render() {
  document.getElementById("emu-list").replaceChildren(
    ...TUTORIALS.filter((tutorial) => tutorial.section === "emu").map(card));

  const filter = document.getElementById("game-filter");
  const choices = [["all", t("tutos.all")], ...Object.entries(GAMES)];
  filter.replaceChildren(...choices.map(([id, name]) => {
    const button = el("button", { type: "button", "aria-pressed": String(id === selectedGame) }, name);
    button.addEventListener("click", () => {
      selectedGame = id;
      const url = new URL(location.href);
      if (id === "all") {
        url.searchParams.delete("jeu");
      } else {
        url.searchParams.set("jeu", id);
      }
      history.replaceState(null, "", url);
      render();
    });
    return button;
  }));

  const games = TUTORIALS.filter((tutorial) =>
    tutorial.section === "games" && (selectedGame === "all" || tutorial.game === selectedGame));
  document.getElementById("games-list").replaceChildren(
    ...(games.length ? games.map(card) : [el("p", { class: "status" }, t("tutos.empty"))]));
}

document.addEventListener("DOMContentLoaded", render);
document.addEventListener("langchange", render);
