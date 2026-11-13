// Languages of the Erable site: English unless the visitor picked another (?lang= or the
// switch, remembered). Loaded in <head> to set the language before the page shows, then fills
// [data-i18n] (text), [data-i18n-html] (markup) and [data-i18n-attr] ("attribute:key;...")
// from the dictionary below.
const LANGS = ["fr", "en", "de"];
const LOCALES = { fr: "fr-FR", en: "en-GB", de: "de-DE" };

const I18N = {
  fr: {
    "nav.home": "Accueil",
    "nav.changes": "Changements",
    "nav.report": "Signaler",
    "nav.tutorials": "Tutoriels",
    "nav.language": "Langue",
    "footer.text": "Erable, l'émulateur Nintendo Switch 2 de la famille. Logiciel libre sous licence GPL. Non affilié à Nintendo : jeux, clés et firmware viennent de votre propre console.",
    "footer.source": "Code source",

    "home.title": "Erable – l'émulateur Switch 2 de la famille",
    "home.pill": "Dernière version",
    "home.pillVersion": "Version {tag} · {date}",
    "home.h1": "L'émulateur Switch&nbsp;2 <span class=\"accent\">de la famille</span>",
    "home.lead": "En français, réglé pour nos PC et nos jeux, avec des mises à jour qui arrivent toutes seules.",
    "home.download": "Télécharger pour Windows",
    "home.tutorials": "Bien démarrer",
    "home.note": "Extrayez tout le zip, puis lancez erable.exe.",
    "home.featuresEyebrow": "Pourquoi Erable",
    "home.featuresTitle": "Pensé pour jouer, pas pour bidouiller",
    "home.featuresSub": "Les bons réglages sont déjà faits. Il ne reste qu'à lancer le jeu.",
    "home.f1t": "Fluide dès la première partie",
    "home.f1p": "Les shaders se préparent hors du jeu et peuvent être précompilés : beaucoup moins de saccades en découvrant une zone.",
    "home.f2t": "Mises à jour automatiques",
    "home.f2p": "Au démarrage, Erable propose la nouvelle version avec ses notes, et l'installe en un clic.",
    "home.f3t": "Réglé pour nos jeux",
    "home.f3p": "Zelda Ocarina of Time, Hyrule Warriors, Mario Kart World… des réglages et des correctifs choisis jeu par jeu.",
    "home.f4t": "En français, et au-delà",
    "home.f4p": "Interface et tutoriels en français, ce site aussi en anglais et en allemand.",
    "home.f5t": "Statut Discord",
    "home.f5p": "Vos amis voient à quoi vous jouez, sans rien configurer.",
    "home.f6t": "Un souci ? Deux minutes",
    "home.f6p": "Un formulaire simple prépare le rapport ; il ne reste qu'à joindre le journal.",
    "home.stepsEyebrow": "Démarrer",
    "home.stepsTitle": "Prêt à jouer en trois étapes",
    "home.s1t": "Télécharger",
    "home.s1p": "Récupérez le zip de la dernière version et extrayez-le où vous voulez.",
    "home.s2t": "Ajouter votre console",
    "home.s2p": "Installez le firmware et les clés extraits de votre propre Switch.",
    "home.s3t": "Jouer",
    "home.s3p": "Ajoutez le dossier de vos jeux, double-cliquez, c'est parti.",
    "home.stepsMore": "Le tutoriel complet",
    "home.latestEyebrow": "Nouveautés",
    "home.latestTitle": "Dans la dernière version",
    "home.latestSub": "Les principaux changements, tels qu'annoncés dans la mise à jour.",
    "home.latestMore": "Toutes les versions",
    "home.latestLoading": "Chargement…",

    "changes.title": "Changements – Erable",
    "changes.h1": "Changements",
    "changes.lead": "Ce qui change d'une version à l'autre, et le détail des modifications du code.",
    "changes.tabReleases": "Versions",
    "changes.tabCommits": "Modifications du code",
    "changes.latest": "Dernière",
    "changes.more": "Tout afficher",
    "changes.less": "Réduire",
    "changes.details": "Détails",
    "changes.history": "Tout l'historique sur GitHub",
    "changes.none": "Aucune version publiée pour l'instant.",
    "changes.noNotes": "Pas de notes pour cette version.",
    "changes.notesLang": "",
    "error.load": "Impossible de charger : {message}",
    "error.rate": "GitHub limite le nombre de visites : réessayez dans une heure.",
    "error.status": "GitHub a répondu {status}.",

    "report.title": "Signaler un problème – Erable",
    "report.h1": "Signaler un problème",
    "report.lead": "Décrivez ce qui se passe : le bouton ouvre GitHub avec le rapport déjà rédigé.",
    "report.howTitle": "Comment ça marche",
    "report.how1": "<strong>Fermez Erable</strong>, pour que le journal soit complet.",
    "report.how2": "Remplissez le formulaire, puis cliquez sur <strong>Continuer sur GitHub</strong>.",
    "report.how3": "Sur GitHub, glissez le fichier <code>eden_log.txt</code> du dossier <code>%APPDATA%\\erable\\log</code> dans la description, puis cliquez sur <strong>Create</strong>.",
    "report.account": "Il faut un compte GitHub (gratuit) : <a href=\"https://github.com/signup\">en créer un</a>.",
    "report.summary": "En une phrase",
    "report.summaryPh": "Ex. : écran noir après la cinématique du temple de l'eau",
    "report.game": "Jeu",
    "report.gameNone": "Aucun / pas un jeu en particulier",
    "report.gameOther": "Autre…",
    "report.gameOtherPh": "Nom du jeu",
    "report.kind": "Type de problème",
    "report.k1": "Plantage",
    "report.k2": "Image fausse",
    "report.k3": "Son",
    "report.k4": "Lenteurs",
    "report.k5": "Jeu bloqué",
    "report.k6": "Manette",
    "report.k7": "Installation",
    "report.k8": "Autre",
    "report.version": "Version d'Erable",
    "report.versionHint": "(Aide > À propos)",
    "report.versionPh": "Ex. : build-24",
    "report.pc": "PC",
    "report.optional": "(facultatif)",
    "report.pcPh": "Ex. : Ryzen 5 7600, RTX 4070",
    "report.what": "Ce qui se passe",
    "report.whatPh": "Ce que vous voyez, et ce que vous attendiez à la place.",
    "report.where": "Où et quand",
    "report.wherePh": "Endroit du jeu, ce que vous faisiez juste avant, est-ce que ça revient à chaque fois ?",
    "report.public": "Le rapport est public : n'y mettez ni mot de passe ni information personnelle.",
    "report.submit": "Continuer sur GitHub",
    "report.bodyGame": "Jeu",
    "report.bodyNone": "Aucun en particulier",
    "report.bodyKind": "Type de problème",
    "report.bodyVersion": "Version d'Erable",
    "report.bodyUnknown": "Non précisée",
    "report.bodyPc": "PC",
    "report.bodyWhat": "Ce qui se passe",
    "report.bodyWhere": "Où et quand",
    "report.bodyLog": "Journal",
    "report.bodyLogHint": "_Glissez ici `eden_log.txt` (dossier `%APPDATA%\\erable\\log`, Erable fermé)._",

    "tutos.title": "Tutoriels – Erable",
    "tutos.h1": "Tutoriels",
    "tutos.lead": "Installer Erable pas à pas, puis tirer le meilleur de chaque jeu.",
    "tutos.emuTitle": "Émulateur",
    "tutos.emuSub": "Installation, console, langue, manette : tout ce qu'il faut avant de jouer.",
    "tutos.gamesTitle": "Jeux",
    "tutos.gamesSub": "Les réglages et astuces propres à chaque jeu.",
    "tutos.all": "Tous les jeux",
    "tutos.empty": "Pas encore de tutoriel pour ce jeu.",
    "tutos.read": "Lire",
    "tutos.steps": "{n} étapes",
    "tutos.back": "Tous les tutoriels",
    "tutos.toc": "Au programme",
    "tuto.installTitle": "Installer Erable – Tutoriel",
    "tuto.ocarinaTitle": "Zelda : Ocarina of Time – Tutoriel"
  },

  en: {
    "nav.home": "Home",
    "nav.changes": "Changes",
    "nav.report": "Report",
    "nav.tutorials": "Guides",
    "nav.language": "Language",
    "footer.text": "Erable, the family's Nintendo Switch 2 emulator. Free software under the GPL. Not affiliated with Nintendo: games, keys and firmware come from your own console.",
    "footer.source": "Source code",

    "home.title": "Erable – the family's Switch 2 emulator",
    "home.pill": "Latest version",
    "home.pillVersion": "Version {tag} · {date}",
    "home.h1": "The family's <span class=\"accent\">Switch&nbsp;2 emulator</span>",
    "home.lead": "Tuned for our PCs and our games, with updates that install themselves.",
    "home.download": "Download for Windows",
    "home.tutorials": "Get started",
    "home.note": "Extract the whole zip, then run erable.exe.",
    "home.featuresEyebrow": "Why Erable",
    "home.featuresTitle": "Made for playing, not tinkering",
    "home.featuresSub": "The right settings are already in place. All that's left is to start the game.",
    "home.f1t": "Smooth from the first session",
    "home.f1p": "Shaders are prepared off the game thread and can be precompiled: far fewer stutters when you reach a new area.",
    "home.f2t": "Automatic updates",
    "home.f2p": "At startup, Erable offers the new version with its notes and installs it in one click.",
    "home.f3t": "Tuned for our games",
    "home.f3p": "Zelda Ocarina of Time, Hyrule Warriors, Mario Kart World… settings and fixes picked game by game.",
    "home.f4t": "In your language",
    "home.f4p": "A French interface and guides, and this site in English and German too.",
    "home.f5t": "Discord status",
    "home.f5p": "Your friends see what you're playing, with nothing to set up.",
    "home.f6t": "A problem? Two minutes",
    "home.f6p": "A simple form writes the report for you; just attach the log.",
    "home.stepsEyebrow": "Get started",
    "home.stepsTitle": "Ready to play in three steps",
    "home.s1t": "Download",
    "home.s1p": "Get the latest version's zip and extract it wherever you like.",
    "home.s2t": "Add your console",
    "home.s2p": "Install the firmware and keys dumped from your own Switch.",
    "home.s3t": "Play",
    "home.s3p": "Add your games folder, double-click, and you're off.",
    "home.stepsMore": "The full guide",
    "home.latestEyebrow": "What's new",
    "home.latestTitle": "In the latest version",
    "home.latestSub": "The main changes, as announced in the update (written in French).",
    "home.latestMore": "All versions",
    "home.latestLoading": "Loading…",

    "changes.title": "Changes – Erable",
    "changes.h1": "Changes",
    "changes.lead": "What changes from one version to the next, and every change to the code.",
    "changes.tabReleases": "Versions",
    "changes.tabCommits": "Code changes",
    "changes.latest": "Latest",
    "changes.more": "Show all",
    "changes.less": "Show less",
    "changes.details": "Details",
    "changes.history": "Full history on GitHub",
    "changes.none": "No version published yet.",
    "changes.noNotes": "No notes for this version.",
    "changes.notesLang": "Release notes are written in French.",
    "error.load": "Could not load: {message}",
    "error.rate": "GitHub limits how often the page can ask: try again in an hour.",
    "error.status": "GitHub answered {status}.",

    "report.title": "Report a problem – Erable",
    "report.h1": "Report a problem",
    "report.lead": "Describe what happens: the button opens GitHub with the report already written.",
    "report.howTitle": "How it works",
    "report.how1": "<strong>Close Erable</strong>, so the log is complete.",
    "report.how2": "Fill in the form, then click <strong>Continue on GitHub</strong>.",
    "report.how3": "On GitHub, drop the <code>eden_log.txt</code> file from <code>%APPDATA%\\erable\\log</code> into the description, then click <strong>Create</strong>.",
    "report.account": "You need a (free) GitHub account: <a href=\"https://github.com/signup\">create one</a>.",
    "report.summary": "In one sentence",
    "report.summaryPh": "E.g. black screen after the Water Temple cutscene",
    "report.game": "Game",
    "report.gameNone": "None / not a particular game",
    "report.gameOther": "Other…",
    "report.gameOtherPh": "Game name",
    "report.kind": "Kind of problem",
    "report.k1": "Crash",
    "report.k2": "Wrong image",
    "report.k3": "Sound",
    "report.k4": "Slowness",
    "report.k5": "Game stuck",
    "report.k6": "Controller",
    "report.k7": "Installation",
    "report.k8": "Other",
    "report.version": "Erable version",
    "report.versionHint": "(Help > About)",
    "report.versionPh": "E.g. build-24",
    "report.pc": "PC",
    "report.optional": "(optional)",
    "report.pcPh": "E.g. Ryzen 5 7600, RTX 4070",
    "report.what": "What happens",
    "report.whatPh": "What you see, and what you expected instead.",
    "report.where": "Where and when",
    "report.wherePh": "Where in the game, what you were doing just before, does it happen every time?",
    "report.public": "The report is public: don't put passwords or personal information in it.",
    "report.submit": "Continue on GitHub",
    "report.bodyGame": "Game",
    "report.bodyNone": "None in particular",
    "report.bodyKind": "Kind of problem",
    "report.bodyVersion": "Erable version",
    "report.bodyUnknown": "Not given",
    "report.bodyPc": "PC",
    "report.bodyWhat": "What happens",
    "report.bodyWhere": "Where and when",
    "report.bodyLog": "Log",
    "report.bodyLogHint": "_Drop `eden_log.txt` here (folder `%APPDATA%\\erable\\log`, with Erable closed)._",

    "tutos.title": "Guides – Erable",
    "tutos.h1": "Guides",
    "tutos.lead": "Set up Erable step by step, then get the best out of each game.",
    "tutos.emuTitle": "Emulator",
    "tutos.emuSub": "Installation, console, language, controller: everything you need before playing.",
    "tutos.gamesTitle": "Games",
    "tutos.gamesSub": "Settings and tips for each game.",
    "tutos.all": "All games",
    "tutos.empty": "No guide for this game yet.",
    "tutos.read": "Read",
    "tutos.steps": "{n} steps",
    "tutos.back": "All guides",
    "tutos.toc": "In this guide",
    "tuto.installTitle": "Install Erable – Guide",
    "tuto.ocarinaTitle": "Zelda: Ocarina of Time – Guide"
  },

  de: {
    "nav.home": "Start",
    "nav.changes": "Änderungen",
    "nav.report": "Melden",
    "nav.tutorials": "Anleitungen",
    "nav.language": "Sprache",
    "footer.text": "Erable, der Nintendo-Switch-2-Emulator der Familie. Freie Software unter der GPL. Nicht mit Nintendo verbunden: Spiele, Schlüssel und Firmware stammen von deiner eigenen Konsole.",
    "footer.source": "Quellcode",

    "home.title": "Erable – der Switch-2-Emulator der Familie",
    "home.pill": "Neueste Version",
    "home.pillVersion": "Version {tag} · {date}",
    "home.h1": "Der Switch-2-Emulator <span class=\"accent\">der Familie</span>",
    "home.lead": "Abgestimmt auf unsere PCs und Spiele, mit Updates, die sich selbst installieren.",
    "home.download": "Für Windows herunterladen",
    "home.tutorials": "Los geht's",
    "home.note": "Entpacke die ganze ZIP-Datei und starte dann erable.exe.",
    "home.featuresEyebrow": "Warum Erable",
    "home.featuresTitle": "Zum Spielen gemacht, nicht zum Basteln",
    "home.featuresSub": "Die richtigen Einstellungen sind schon gesetzt. Du musst nur noch das Spiel starten.",
    "home.f1t": "Flüssig ab der ersten Runde",
    "home.f1p": "Shader werden abseits des Spiels vorbereitet und lassen sich vorkompilieren: viel weniger Ruckler in neuen Gebieten.",
    "home.f2t": "Automatische Updates",
    "home.f2p": "Beim Start bietet Erable die neue Version samt Hinweisen an und installiert sie mit einem Klick.",
    "home.f3t": "Auf unsere Spiele abgestimmt",
    "home.f3p": "Zelda Ocarina of Time, Hyrule Warriors, Mario Kart World … Einstellungen und Korrekturen für jedes Spiel einzeln.",
    "home.f4t": "In deiner Sprache",
    "home.f4p": "Oberfläche und Anleitungen auf Französisch, diese Seite auch auf Englisch und Deutsch.",
    "home.f5t": "Discord-Status",
    "home.f5p": "Deine Freunde sehen, was du spielst – ohne Einrichtung.",
    "home.f6t": "Ein Problem? Zwei Minuten",
    "home.f6p": "Ein einfaches Formular schreibt den Bericht für dich; du hängst nur noch das Protokoll an.",
    "home.stepsEyebrow": "Los geht's",
    "home.stepsTitle": "In drei Schritten spielbereit",
    "home.s1t": "Herunterladen",
    "home.s1p": "Lade die ZIP-Datei der neuesten Version herunter und entpacke sie, wo du möchtest.",
    "home.s2t": "Deine Konsole hinzufügen",
    "home.s2p": "Installiere die Firmware und die Schlüssel aus deiner eigenen Switch.",
    "home.s3t": "Spielen",
    "home.s3p": "Füge deinen Spieleordner hinzu, Doppelklick – fertig.",
    "home.stepsMore": "Die ganze Anleitung",
    "home.latestEyebrow": "Neuigkeiten",
    "home.latestTitle": "In der neuesten Version",
    "home.latestSub": "Die wichtigsten Änderungen, wie im Update angekündigt (auf Französisch).",
    "home.latestMore": "Alle Versionen",
    "home.latestLoading": "Wird geladen …",

    "changes.title": "Änderungen – Erable",
    "changes.h1": "Änderungen",
    "changes.lead": "Was sich von Version zu Version ändert, und jede Änderung am Code.",
    "changes.tabReleases": "Versionen",
    "changes.tabCommits": "Code-Änderungen",
    "changes.latest": "Neueste",
    "changes.more": "Alles anzeigen",
    "changes.less": "Weniger anzeigen",
    "changes.details": "Details",
    "changes.history": "Gesamter Verlauf auf GitHub",
    "changes.none": "Noch keine Version veröffentlicht.",
    "changes.noNotes": "Keine Hinweise zu dieser Version.",
    "changes.notesLang": "Die Versionshinweise sind auf Französisch.",
    "error.load": "Laden nicht möglich: {message}",
    "error.rate": "GitHub begrenzt die Anfragen: Versuch es in einer Stunde noch einmal.",
    "error.status": "GitHub antwortete mit {status}.",

    "report.title": "Problem melden – Erable",
    "report.h1": "Problem melden",
    "report.lead": "Beschreibe, was passiert: Der Knopf öffnet GitHub mit dem fertigen Bericht.",
    "report.howTitle": "So funktioniert's",
    "report.how1": "<strong>Schließe Erable</strong>, damit das Protokoll vollständig ist.",
    "report.how2": "Fülle das Formular aus und klicke auf <strong>Weiter zu GitHub</strong>.",
    "report.how3": "Ziehe auf GitHub die Datei <code>eden_log.txt</code> aus <code>%APPDATA%\\erable\\log</code> in die Beschreibung und klicke auf <strong>Create</strong>.",
    "report.account": "Du brauchst ein (kostenloses) GitHub-Konto: <a href=\"https://github.com/signup\">jetzt anlegen</a>.",
    "report.summary": "In einem Satz",
    "report.summaryPh": "Z. B. schwarzer Bildschirm nach der Zwischensequenz im Wassertempel",
    "report.game": "Spiel",
    "report.gameNone": "Keines / kein bestimmtes Spiel",
    "report.gameOther": "Anderes …",
    "report.gameOtherPh": "Name des Spiels",
    "report.kind": "Art des Problems",
    "report.k1": "Absturz",
    "report.k2": "Falsches Bild",
    "report.k3": "Ton",
    "report.k4": "Ruckeln",
    "report.k5": "Spiel hängt",
    "report.k6": "Controller",
    "report.k7": "Installation",
    "report.k8": "Sonstiges",
    "report.version": "Erable-Version",
    "report.versionHint": "(Hilfe > Über)",
    "report.versionPh": "Z. B. build-24",
    "report.pc": "PC",
    "report.optional": "(optional)",
    "report.pcPh": "Z. B. Ryzen 5 7600, RTX 4070",
    "report.what": "Was passiert",
    "report.whatPh": "Was du siehst und was du stattdessen erwartet hast.",
    "report.where": "Wo und wann",
    "report.wherePh": "Wo im Spiel, was du kurz davor gemacht hast, passiert es jedes Mal?",
    "report.public": "Der Bericht ist öffentlich: Gib keine Passwörter oder persönlichen Daten an.",
    "report.submit": "Weiter zu GitHub",
    "report.bodyGame": "Spiel",
    "report.bodyNone": "Kein bestimmtes",
    "report.bodyKind": "Art des Problems",
    "report.bodyVersion": "Erable-Version",
    "report.bodyUnknown": "Nicht angegeben",
    "report.bodyPc": "PC",
    "report.bodyWhat": "Was passiert",
    "report.bodyWhere": "Wo und wann",
    "report.bodyLog": "Protokoll",
    "report.bodyLogHint": "_Ziehe `eden_log.txt` hierher (Ordner `%APPDATA%\\erable\\log`, Erable geschlossen)._",

    "tutos.title": "Anleitungen – Erable",
    "tutos.h1": "Anleitungen",
    "tutos.lead": "Erable Schritt für Schritt einrichten und dann das Beste aus jedem Spiel holen.",
    "tutos.emuTitle": "Emulator",
    "tutos.emuSub": "Installation, Konsole, Sprache, Controller: alles, was du vor dem Spielen brauchst.",
    "tutos.gamesTitle": "Spiele",
    "tutos.gamesSub": "Einstellungen und Tipps für jedes Spiel.",
    "tutos.all": "Alle Spiele",
    "tutos.empty": "Für dieses Spiel gibt es noch keine Anleitung.",
    "tutos.read": "Lesen",
    "tutos.steps": "{n} Schritte",
    "tutos.back": "Alle Anleitungen",
    "tutos.toc": "In dieser Anleitung",
    "tuto.installTitle": "Erable installieren – Anleitung",
    "tuto.ocarinaTitle": "Zelda: Ocarina of Time – Anleitung"
  }
};

function pickLang() {
  const fromUrl = new URLSearchParams(location.search).get("lang");
  if (LANGS.includes(fromUrl)) {
    return fromUrl;
  }
  try {
    const saved = localStorage.getItem("erable-lang");
    if (LANGS.includes(saved)) {
      return saved;
    }
  } catch (e) {}
  return "en";
}

let LANG = pickLang();
document.documentElement.lang = LANG;
document.documentElement.classList.add("js");

function t(key, values) {
  let text = (I18N[LANG] && I18N[LANG][key]) ?? I18N.en[key] ?? key;
  for (const [name, value] of Object.entries(values || {})) {
    text = text.replaceAll("{" + name + "}", value);
  }
  return text;
}

function applyI18n(root) {
  const scope = root || document;
  for (const node of scope.querySelectorAll("[data-i18n]")) {
    node.textContent = t(node.dataset.i18n);
  }
  for (const node of scope.querySelectorAll("[data-i18n-html]")) {
    node.innerHTML = t(node.dataset.i18nHtml);
  }
  for (const node of scope.querySelectorAll("[data-i18n-attr]")) {
    for (const pair of node.dataset.i18nAttr.split(";")) {
      const [attribute, key] = pair.split(":");
      node.setAttribute(attribute.trim(), t(key.trim()));
    }
  }
  const title = document.querySelector("meta[name='i18n-title']");
  if (title) {
    document.title = t(title.content);
  }
  for (const button of document.querySelectorAll(".lang button")) {
    button.setAttribute("aria-pressed", String(button.dataset.setLang === LANG));
  }
}

function setLang(lang) {
  if (!LANGS.includes(lang) || lang === LANG) {
    return;
  }
  LANG = lang;
  document.documentElement.lang = lang;
  try {
    localStorage.setItem("erable-lang", lang);
  } catch (e) {}
  const url = new URL(location.href);
  if (url.searchParams.has("lang")) {
    url.searchParams.set("lang", lang);
    history.replaceState(null, "", url);
  }
  applyI18n();
  document.dispatchEvent(new CustomEvent("langchange", { detail: lang }));
}

document.addEventListener("DOMContentLoaded", () => {
  for (const button of document.querySelectorAll(".lang button")) {
    button.addEventListener("click", () => setLang(button.dataset.setLang));
  }
  applyI18n();
  const observer = "IntersectionObserver" in window ? new IntersectionObserver((entries) => {
    for (const entry of entries) {
      if (entry.isIntersecting) {
        entry.target.classList.add("shown");
        observer.unobserve(entry.target);
      }
    }
  }, { rootMargin: "0px 0px -8% 0px" }) : null;
  for (const node of document.querySelectorAll(".reveal")) {
    if (observer) {
      observer.observe(node);
    } else {
      node.classList.add("shown");
    }
  }
});
