"""Writes the Erable site's pages (site/*.html, site/tuto/*.html). The texts the pages show
come from site/i18n.js; the tutorials' French, English and German bodies are here.

    python3 tools/site/generate.py [site directory]
"""
import os, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "site")

ICONS = {
    "download": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3v12"/><path d="m7 10 5 5 5-5"/><path d="M5 21h14"/></svg>',
    "arrow": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12h14"/><path d="m13 6 6 6-6 6"/></svg>',
    "back": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M19 12H5"/><path d="m11 18-6-6 6-6"/></svg>',
    "bolt": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M13 2 4 14h7l-1 8 9-12h-7z"/></svg>',
    "refresh": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M21 12a9 9 0 0 1-15.5 6.2L3 16"/><path d="M3 12a9 9 0 0 1 15.5-6.2L21 8"/><path d="M21 3v5h-5"/><path d="M3 21v-5h5"/></svg>',
    "gamepad": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M6 11h4M8 9v4"/><circle cx="15.5" cy="10.5" r=".8" fill="currentColor"/><circle cx="17.5" cy="13" r=".8" fill="currentColor"/><path d="M7 5h10a5 5 0 0 1 4.9 6l-1 5a3 3 0 0 1-5.2 1.4L14 15h-4l-1.7 2.4A3 3 0 0 1 3.1 16l-1-5A5 5 0 0 1 7 5z"/></svg>',
    "globe": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M3 12h18"/><path d="M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18"/></svg>',
    "chat": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M21 12a8 8 0 0 1-11.6 7.1L4 20l1-4.6A8 8 0 1 1 21 12z"/><path d="M9 11h.01M12 11h.01M15 11h.01"/></svg>',
    "flag": '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 21V4"/><path d="M5 4h11l-2 4 2 4H5"/></svg>',
    "github": '<svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M12 .5a11.5 11.5 0 0 0-3.6 22.4c.6.1.8-.3.8-.6v-2c-3.2.7-3.9-1.5-3.9-1.5-.5-1.3-1.3-1.7-1.3-1.7-1-.7.1-.7.1-.7 1.2.1 1.8 1.2 1.8 1.2 1 1.8 2.8 1.3 3.5 1 .1-.8.4-1.3.7-1.6-2.6-.3-5.3-1.3-5.3-5.7 0-1.3.5-2.3 1.2-3.1-.1-.3-.5-1.5.1-3.2 0 0 1-.3 3.3 1.2a11.4 11.4 0 0 1 6 0c2.3-1.5 3.3-1.2 3.3-1.2.7 1.7.2 2.9.1 3.2.8.8 1.2 1.8 1.2 3.1 0 4.4-2.7 5.4-5.3 5.7.4.4.8 1.1.8 2.2v3.3c0 .3.2.7.8.6A11.5 11.5 0 0 0 12 .5z"/></svg>',
}


def head(prefix, title_key, description):
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Erable</title>
<meta name="i18n-title" content="{title_key}">
<meta name="description" content="{description}">
<meta name="theme-color" content="#d2501f">
<link rel="icon" href="{prefix}erable.png">
<link rel="stylesheet" href="{prefix}style.css">
<script src="{prefix}i18n.js"></script>
</head>
<body>
<header class="site">
  <div class="wrap">
    <a class="brand" href="{prefix}./"><img src="{prefix}erable.png" alt="">Erable</a>
    <nav aria-label="Navigation">
      <a href="{prefix}./"{cur('home')} data-i18n="nav.home">Accueil</a>
      <a href="{prefix}tutoriels.html"{cur('tutos')} data-i18n="nav.tutorials">Tutoriels</a>
      <a href="{prefix}changements.html"{cur('changes')} data-i18n="nav.changes">Changements</a>
      <a href="{prefix}bugs.html"{cur('bugs')} data-i18n="nav.bugs">Bugs</a>
      <a href="{prefix}signaler.html"{cur('report')} data-i18n="nav.report">Signaler</a>
    </nav>
    <div class="lang" role="group" data-i18n-attr="aria-label:nav.language">
      <button type="button" data-set-lang="fr" lang="fr" title="Français">FR</button>
      <button type="button" data-set-lang="en" lang="en" title="English">EN</button>
      <button type="button" data-set-lang="de" lang="de" title="Deutsch">DE</button>
    </div>
  </div>
</header>
"""


CURRENT = None


def cur(page):
    return ' aria-current="page"' if page == CURRENT else ""


def foot(prefix, scripts):
    tags = "\n".join(f'<script src="{s if s.startswith("http") else prefix + s}"></script>' for s in scripts)
    return f"""<footer class="site">
  <div class="wrap">
    <a class="brand" href="{prefix}./"><img src="{prefix}erable.png" alt="">Erable</a>
    <p data-i18n-html="footer.text"></p>
    <a class="btn secondary small" href="https://github.com/erablesimu/erable">{ICONS['github']}<span data-i18n="footer.source">Code source</span></a>
  </div>
</footer>
{tags}
</body>
</html>
"""


def write(path, text):
    full = os.path.join(OUT, path)
    os.makedirs(os.path.dirname(full) or ".", exist_ok=True)
    open(full, "w", encoding="utf-8").write(text)


# ---------------------------------------------------------------- home
CURRENT = "home"
features = [("bolt", "f1"), ("refresh", "f2"), ("gamepad", "f3"), ("globe", "f4"), ("chat", "f5"), ("flag", "f6")]
feature_cards = "\n".join(
    f"""      <div class="card feature reveal"><div class="icon">{ICONS[i]}</div><h3 data-i18n="home.{k}t"></h3><p data-i18n="home.{k}p"></p></div>"""
    for i, k in features)
write("index.html", head("", "home.title", "Erable, the family's Nintendo Switch 2 emulator.") + f"""
<section class="hero">
  <div class="leaves" aria-hidden="true"><span></span><span></span><span></span><span></span><span></span><span></span></div>
  <div class="wrap">
    <div>
      <a class="pill" id="version-pill" href="changements.html"><span class="dot"></span><span id="version-text" data-i18n="home.pill">Dernière version</span></a>
      <h1 data-i18n-html="home.h1"></h1>
      <p class="lead" data-i18n="home.lead"></p>
      <div class="actions">
        <a class="btn" id="download" href="https://github.com/erablesimu/erable/releases/latest">{ICONS['download']}<span data-i18n="home.download"></span></a>
        <a class="btn secondary" href="tutoriels.html"><span data-i18n="home.tutorials"></span>{ICONS['arrow']}</a>
      </div>
      <p class="hero-note" data-i18n="home.note"></p>
    </div>
    <div class="hero-art"><img src="erable.png" alt="Erable"></div>
  </div>
</section>
<main>
  <section class="section wrap">
    <div class="section-head reveal">
      <p class="eyebrow" data-i18n="home.featuresEyebrow"></p>
      <h2 data-i18n="home.featuresTitle"></h2>
      <p data-i18n="home.featuresSub"></p>
    </div>
    <div class="grid">
{feature_cards}
    </div>
  </section>

  <section class="section wrap">
    <div class="section-head reveal">
      <p class="eyebrow" data-i18n="home.stepsEyebrow"></p>
      <h2 data-i18n="home.stepsTitle"></h2>
    </div>
    <div class="steps-grid">
      <div class="card step reveal"><h3 data-i18n="home.s1t"></h3><p data-i18n="home.s1p"></p></div>
      <div class="card step reveal"><h3 data-i18n="home.s2t"></h3><p data-i18n="home.s2p"></p></div>
      <div class="card step reveal"><h3 data-i18n="home.s3t"></h3><p data-i18n="home.s3p"></p></div>
    </div>
    <p class="reveal"><a class="more" href="tuto/installer.html"><span data-i18n="home.stepsMore"></span>{ICONS['arrow']}</a></p>
  </section>

  <section class="section wrap">
    <div class="latest card reveal">
      <div>
        <p class="eyebrow" data-i18n="home.latestEyebrow"></p>
        <h2 data-i18n="home.latestTitle"></h2>
        <p class="meta" data-i18n="home.latestSub"></p>
        <p class="meta" id="latest-date"></p>
        <a class="more" href="changements.html"><span data-i18n="home.latestMore"></span>{ICONS['arrow']}</a>
      </div>
      <div id="latest-notes" class="notes"><p class="status" data-i18n="home.latestLoading"></p></div>
    </div>
  </section>
</main>
""" + foot("", ["https://cdnjs.cloudflare.com/ajax/libs/marked/12.0.2/marked.min.js",
                "https://cdnjs.cloudflare.com/ajax/libs/dompurify/3.1.6/purify.min.js",
                "site.js", "home.js"]))

# ---------------------------------------------------------------- changes
CURRENT = "changes"
write("changements.html", head("", "changes.title", "Erable's release notes and code changes.") + """
<main>
  <div class="wrap">
    <div class="page-head">
      <h1 data-i18n="changes.h1"></h1>
      <p class="lead" data-i18n="changes.lead"></p>
    </div>
    <div class="tabs" role="tablist">
      <button role="tab" id="tab-releases" aria-selected="true" aria-controls="releases" data-i18n="changes.tabReleases"></button>
      <button role="tab" id="tab-commits" aria-selected="false" aria-controls="commits" data-i18n="changes.tabCommits"></button>
    </div>
    <p class="meta notes-lang" data-i18n="changes.notesLang"></p>
    <section id="releases" role="tabpanel" aria-labelledby="tab-releases">
      <div class="skeleton"></div><div class="skeleton"></div>
    </section>
    <section id="commits" role="tabpanel" aria-labelledby="tab-commits" hidden>
      <div class="skeleton"></div>
    </section>
  </div>
</main>
""" + foot("", ["https://cdnjs.cloudflare.com/ajax/libs/marked/12.0.2/marked.min.js",
                "https://cdnjs.cloudflare.com/ajax/libs/dompurify/3.1.6/purify.min.js",
                "site.js", "changements.js"]))

# ---------------------------------------------------------------- bugs
CURRENT = "bugs"
write("bugs.html", head("", "bugs.title", "Known problems reported for Erable.") + f"""
<main>
  <div class="wrap">
    <div class="page-head">
      <h1 data-i18n="bugs.h1"></h1>
      <p class="lead" data-i18n="bugs.lead"></p>
      <div class="stats" id="bug-stats"></div>
    </div>
    <div class="bug-tools card">
      <div class="tabs" role="group" id="bug-state">
        <button type="button" data-state="open" aria-pressed="true" data-i18n="bugs.open"></button>
        <button type="button" data-state="closed" aria-pressed="false" data-i18n="bugs.closed"></button>
        <button type="button" data-state="all" aria-pressed="false" data-i18n="bugs.all"></button>
      </div>
      <select id="bug-game" data-i18n-attr="aria-label:bugs.game"></select>
      <select id="bug-kind" data-i18n-attr="aria-label:bugs.kind"></select>
      <input id="bug-search" type="search" data-i18n-attr="placeholder:bugs.search;aria-label:bugs.search">
    </div>
    <div id="bug-list"><div class="skeleton"></div><div class="skeleton"></div></div>
    <p class="meta" id="bug-updated"></p>
    <div class="actions"><a class="btn" href="signaler.html">{ICONS['flag']}<span data-i18n="bugs.report"></span></a></div>
  </div>
</main>
""" + foot("", ["https://cdnjs.cloudflare.com/ajax/libs/sql.js/1.10.3/sql-wasm.js", "site.js", "bugs.js"]))

# ---------------------------------------------------------------- report
CURRENT = "report"
kinds = "\n".join(
    f'            <input type="radio" name="kind" id="k{i}" value="{i}"{" checked" if i == 1 else ""}><label for="k{i}" data-i18n="report.k{i}"></label>'
    for i in range(1, 9))
games = ["The Legend of Zelda: Ocarina of Time", "Hyrule Warriors: Age of Imprisonment", "Mario Kart World",
         "Donkey Kong Bananza", "Kirby Air Riders", "Pokémon Pokopia", "Super Mario Bros. Wonder"]
game_options = "\n".join(f"            <option>{g}</option>" for g in games)
write("signaler.html", head("", "report.title", "Report a problem with Erable.") + f"""
<main>
  <div class="wrap">
    <div class="page-head">
      <h1 data-i18n="report.h1"></h1>
      <p class="lead" data-i18n="report.lead"></p>
    </div>
    <div class="report-layout">
      <aside class="card">
        <h2 class="small-title" data-i18n="report.howTitle"></h2>
        <ol class="howto">
          <li data-i18n-html="report.how1"></li>
          <li data-i18n-html="report.how2"></li>
          <li data-i18n-html="report.how3"></li>
        </ol>
        <p class="meta" data-i18n-html="report.account"></p>
      </aside>
      <form id="report" class="card">
        <div class="field">
          <label for="title" data-i18n="report.summary"></label>
          <input id="title" name="title" required maxlength="120" data-i18n-attr="placeholder:report.summaryPh">
        </div>
        <div class="field">
          <span class="label" data-i18n="report.kind"></span>
          <div class="chips">
{kinds}
          </div>
        </div>
        <div class="row">
          <div class="field">
            <label for="game" data-i18n="report.game"></label>
            <select id="game" name="game">
              <option value="" data-i18n="report.gameNone"></option>
{game_options}
              <option value="other" data-i18n="report.gameOther"></option>
            </select>
            <input id="game-other" name="game-other" data-i18n-attr="placeholder:report.gameOtherPh" hidden>
          </div>
          <div class="field">
            <label for="version"><span data-i18n="report.version"></span> <span class="hint" data-i18n="report.versionHint"></span></label>
            <input id="version" name="version" data-i18n-attr="placeholder:report.versionPh">
          </div>
        </div>
        <div class="field">
          <label for="pc"><span data-i18n="report.pc"></span> <span class="hint" data-i18n="report.optional"></span></label>
          <input id="pc" name="pc" data-i18n-attr="placeholder:report.pcPh">
        </div>
        <div class="field">
          <label for="what" data-i18n="report.what"></label>
          <textarea id="what" name="what" required data-i18n-attr="placeholder:report.whatPh"></textarea>
        </div>
        <div class="field">
          <label for="where"><span data-i18n="report.where"></span> <span class="hint" data-i18n="report.optional"></span></label>
          <textarea id="where" name="where" data-i18n-attr="placeholder:report.wherePh"></textarea>
        </div>
        <p class="notice" data-i18n="report.public"></p>
        <button class="btn" type="submit">{ICONS['github']}<span data-i18n="report.submit"></span></button>
      </form>
    </div>
  </div>
</main>
""" + foot("", ["site.js", "signaler.js"]))

# ---------------------------------------------------------------- tutorials hub
CURRENT = "tutos"
write("tutoriels.html", head("", "tutos.title", "Erable guides: emulator and games.") + """
<main>
  <div class="wrap">
    <div class="page-head">
      <h1 data-i18n="tutos.h1"></h1>
      <p class="lead" data-i18n="tutos.lead"></p>
    </div>
    <section class="tuto-section">
      <div class="section-head">
        <p class="eyebrow">01</p>
        <h2 data-i18n="tutos.emuTitle"></h2>
        <p data-i18n="tutos.emuSub"></p>
      </div>
      <div class="tuto-grid" id="emu-list"></div>
    </section>
    <section class="tuto-section">
      <div class="section-head">
        <p class="eyebrow">02</p>
        <h2 data-i18n="tutos.gamesTitle"></h2>
        <p data-i18n="tutos.gamesSub"></p>
      </div>
      <div class="filter" id="game-filter" role="group"></div>
      <div class="tuto-grid" id="games-list"></div>
    </section>
  </div>
</main>
""" + foot("", ["site.js", "tutos.js"]))


# ---------------------------------------------------------------- tutorial pages
def tutorial(path, title_key, kind_key, game, toc, bodies, heads, leads):
    global CURRENT
    CURRENT = "tutos"
    prefix = "../"
    langs = ["fr", "en", "de"]
    toc_html = "\n".join(
        f'      <nav class="toc" data-lang="{l}"><strong data-i18n="tutos.toc"></strong><ol>' +
        "".join(f'<li><a href="#{a}">{toc[l][i]}</a></li>' for i, a in enumerate(toc["ids"])) + "</ol></nav>"
        for l in langs)
    head_html = "\n".join(
        f'      <h1 data-lang="{l}">{heads[l]}</h1>\n      <p class="lead" data-lang="{l}">{leads[l]}</p>' for l in langs)
    body_html = "\n".join(f'      <article data-lang="{l}">\n{bodies[l]}\n      </article>' for l in langs)
    eyebrow = f'<span data-i18n="{kind_key}"></span>' + (f" · {game}" if game else "")
    write(path, head(prefix, title_key, heads["en"]) + f"""
<main>
  <section class="tuto-hero">
    <div class="wrap">
      <a class="back" href="../tutoriels.html">{ICONS['back']}<span data-i18n="tutos.back"></span></a>
      <p class="eyebrow">{eyebrow}</p>
{head_html}
    </div>
  </section>
  <div class="wrap tuto-layout">
    <aside>
{toc_html}
    </aside>
    <div class="tuto-body">
{body_html}
    </div>
  </div>
</main>
""" + foot(prefix, []))


def fig(name, alt, caption):
    return f'<figure><img src="img/{name}.png" alt="{alt}" loading="lazy"><figcaption>{caption}</figcaption></figure>'


def sec(ident, num, title, inner):
    return f'<section class="card tuto-step" id="{ident}"><h2><span class="num">{num}</span>{title}</h2>\n{inner}\n</section>'


m = lambda s: f'<span class="menu">{s}</span>'

DOWNLOAD = '<a class="btn" href="https://github.com/erablesimu/erable/releases/latest">' + ICONS['download'] + '{}</a>'

install = {
    "fr": "\n".join([
        sec("installer", 1, "Installer Erable", f"""<ol class="steps">
<li>Téléchargez <code>erable-windows-x86_64.zip</code> depuis la dernière version :<br>{DOWNLOAD.format('Télécharger Erable')}</li>
<li>Faites un clic droit sur le zip → {m('Extraire tout…')}</li>
<li>Dans le dossier obtenu, lancez <code>erable.exe</code>.</li>
</ol>
<div class="note">Les mises à jour arrivent toutes seules : au démarrage, Erable propose d'installer la nouvelle version quand il y en a une.</div>
<div class="warn">Si Windows signale qu'il manque des bibliothèques, lancez d'abord <code>vc_redist.x64.exe</code>, qui se trouve dans le même dossier.</div>"""),
        sec("console", 2, "Ajouter le firmware et les clés de votre Switch", f"""<p>L'émulateur a besoin du système (firmware) et des clés (<code>prod.keys</code>) <strong>extraits de votre propre console</strong>. Ces fichiers appartiennent à Nintendo : ils ne sont pas fournis avec Erable, et il ne faut ni les télécharger ni les partager.</p>
<ol class="steps">
<li>Extrayez le firmware et <code>prod.keys</code> de votre Switch.</li>
<li>Dans Erable, en haut à gauche : {m('Tools → Install Firmware → From ZIP')}, puis choisissez le zip du firmware.</li>
<li>Toujours en haut à gauche : {m('Tools → Install Decryption Keys')}, puis choisissez votre fichier <code>prod.keys</code>.</li>
</ol>
{fig('tools-menu', "Menu Tools d'Erable avec Install Decryption Keys et Install Firmware", "Le menu <em>Tools</em> : <em>Install Decryption Keys</em> et <em>Install Firmware</em>.")}
<div class="warn">Ne téléchargez jamais de clés ou de firmware sur Internet : ces fichiers circulent surtout sur des sites qui diffusent aussi des virus.</div>"""),
        sec("langue", 3, "Jeux en français", f"""<ol class="steps">
<li>En haut à gauche : {m('Emulation → Configure')}, puis {m('System')}.</li>
<li><strong>Language</strong> : <code>French (français)</code>.</li>
<li><strong>Region</strong> : <code>Europe</code>.</li>
<li>Validez avec {m('OK')}.</li>
</ol>
{fig('system-language', 'Onglet System de la configuration, langue French (français)', 'Onglet <em>System</em> : la langue des jeux.')}"""),
        sec("manette", 4, "Jouer avec une manette", f"""<ol class="steps">
<li>Branchez votre manette, puis allez dans {m('Emulation → Configure → Controls')}.</li>
<li><strong>Input Device</strong> : choisissez votre manette (en général la dernière de la liste).</li>
<li>Cliquez sur chaque case (A, B, X, Y, sticks, gâchettes…), puis appuyez sur la touche correspondante de la manette.</li>
<li>Validez avec {m('OK')}.</li>
</ol>
{fig('controls', 'Onglet Controls avec une manette connectée et les touches configurées', "Onglet <em>Controls</em> : la case bleue <em>[waiting]</em> attend que vous appuyiez sur une touche.")}
<div class="note">Si la manette n'apparaît pas : fermez Steam (il capture les manettes), essayez un autre câble USB, puis relancez Erable.</div>"""),
        sec("jeux", 5, "Ajouter vos jeux et jouer", f"""<ol class="steps">
<li>Rangez vos jeux (extraits de vos cartouches ou de vos achats) dans un dossier <code>Jeux Switch 2</code>, un sous-dossier par jeu.</li>
<li>Dans la liste des jeux, cliquez sur {m('+ Add New Game Directory')}.</li>
<li>Sélectionnez le dossier du jeu, puis {m('Sélectionner un dossier')}.</li>
<li>Double-cliquez sur le jeu pour le lancer.</li>
</ol>
{fig('game-folder', "Fenêtre de sélection du dossier d'un jeu", "Choisir le dossier d'un jeu.")}
{fig('game-list', 'Liste des jeux avec Zelda Ocarina of Time et Pokémon Pokopia', 'Les jeux apparaissent dans la liste, avec leur temps de jeu et leurs add-ons.')}
<div class="note">La première fois, un jeu peut saccader quelques minutes : l'émulateur prépare ses shaders, puis tout devient plus fluide. Un souci ? <a href="../signaler.html">Signalez-le ici</a>.</div>"""),
    ]),
    "en": "\n".join([
        sec("installer", 1, "Install Erable", f"""<ol class="steps">
<li>Download <code>erable-windows-x86_64.zip</code> from the latest version:<br>{DOWNLOAD.format('Download Erable')}</li>
<li>Right-click the zip → {m('Extract All…')}</li>
<li>In the folder you get, run <code>erable.exe</code>.</li>
</ol>
<div class="note">Updates come on their own: at startup, Erable offers to install the new version when there is one.</div>
<div class="warn">If Windows says libraries are missing, first run <code>vc_redist.x64.exe</code>, which is in the same folder.</div>"""),
        sec("console", 2, "Add your Switch's firmware and keys", f"""<p>The emulator needs the system (firmware) and the keys (<code>prod.keys</code>) <strong>dumped from your own console</strong>. These files belong to Nintendo: they don't come with Erable, and you must neither download nor share them.</p>
<ol class="steps">
<li>Dump the firmware and <code>prod.keys</code> from your Switch.</li>
<li>In Erable, top left: {m('Tools → Install Firmware → From ZIP')}, then pick the firmware zip.</li>
<li>Still top left: {m('Tools → Install Decryption Keys')}, then pick your <code>prod.keys</code> file.</li>
</ol>
{fig('tools-menu', "Erable's Tools menu with Install Decryption Keys and Install Firmware", 'The <em>Tools</em> menu: <em>Install Decryption Keys</em> and <em>Install Firmware</em>.')}
<div class="warn">Never download keys or firmware from the Internet: these files mostly circulate on sites that also spread viruses.</div>"""),
        sec("langue", 3, "Games in your language", f"""<ol class="steps">
<li>Top left: {m('Emulation → Configure')}, then {m('System')}.</li>
<li><strong>Language</strong>: your language, e.g. <code>British English</code>.</li>
<li><strong>Region</strong>: <code>Europe</code> (or <code>USA</code>).</li>
<li>Confirm with {m('OK')}.</li>
</ol>
{fig('system-language', 'System tab of the configuration with the game language', "The <em>System</em> tab: the games' language.")}"""),
        sec("manette", 4, "Play with a controller", f"""<ol class="steps">
<li>Plug in your controller, then go to {m('Emulation → Configure → Controls')}.</li>
<li><strong>Input Device</strong>: pick your controller (usually the last one in the list).</li>
<li>Click each box (A, B, X, Y, sticks, triggers…), then press the matching button on the controller.</li>
<li>Confirm with {m('OK')}.</li>
</ol>
{fig('controls', 'Controls tab with a connected controller and its buttons mapped', 'The <em>Controls</em> tab: the blue <em>[waiting]</em> box waits for you to press a button.')}
<div class="note">If the controller doesn't show up: close Steam (it captures controllers), try another USB cable, then restart Erable.</div>"""),
        sec("jeux", 5, "Add your games and play", f"""<ol class="steps">
<li>Keep your games (dumped from your cartridges or purchases) in a <code>Switch 2 Games</code> folder, one subfolder per game.</li>
<li>In the game list, click {m('+ Add New Game Directory')}.</li>
<li>Select the game's folder, then {m('Select Folder')}.</li>
<li>Double-click the game to start it.</li>
</ol>
{fig('game-folder', "Window for picking a game's folder", "Picking a game's folder.")}
{fig('game-list', 'Game list with Zelda Ocarina of Time and Pokémon Pokopia', 'Games show up in the list with their play time and add-ons.')}
<div class="note">The first time, a game may stutter for a few minutes while the emulator prepares its shaders; then it all gets smoother. A problem? <a href="../signaler.html">Report it here</a>.</div>"""),
    ]),
    "de": "\n".join([
        sec("installer", 1, "Erable installieren", f"""<ol class="steps">
<li>Lade <code>erable-windows-x86_64.zip</code> aus der neuesten Version herunter:<br>{DOWNLOAD.format('Erable herunterladen')}</li>
<li>Rechtsklick auf die ZIP-Datei → {m('Alle extrahieren…')}</li>
<li>Starte im entpackten Ordner <code>erable.exe</code>.</li>
</ol>
<div class="note">Updates kommen von selbst: Beim Start bietet Erable an, eine neue Version zu installieren, sobald es eine gibt.</div>
<div class="warn">Meldet Windows fehlende Bibliotheken, starte zuerst <code>vc_redist.x64.exe</code> aus demselben Ordner.</div>"""),
        sec("console", 2, "Firmware und Schlüssel deiner Switch hinzufügen", f"""<p>Der Emulator braucht das System (Firmware) und die Schlüssel (<code>prod.keys</code>) <strong>aus deiner eigenen Konsole</strong>. Diese Dateien gehören Nintendo: Sie sind nicht in Erable enthalten, und du darfst sie weder herunterladen noch weitergeben.</p>
<ol class="steps">
<li>Lies Firmware und <code>prod.keys</code> aus deiner Switch aus.</li>
<li>In Erable oben links: {m('Tools → Install Firmware → From ZIP')}, dann die ZIP-Datei der Firmware wählen.</li>
<li>Ebenfalls oben links: {m('Tools → Install Decryption Keys')}, dann deine Datei <code>prod.keys</code> wählen.</li>
</ol>
{fig('tools-menu', 'Tools-Menü von Erable mit Install Decryption Keys und Install Firmware', 'Das Menü <em>Tools</em>: <em>Install Decryption Keys</em> und <em>Install Firmware</em>.')}
<div class="warn">Lade nie Schlüssel oder Firmware aus dem Internet herunter: Diese Dateien kursieren vor allem auf Seiten, die auch Viren verbreiten.</div>"""),
        sec("langue", 3, "Spiele auf Deutsch", f"""<ol class="steps">
<li>Oben links: {m('Emulation → Configure')}, dann {m('System')}.</li>
<li><strong>Language</strong>: <code>German (Deutsch)</code>.</li>
<li><strong>Region</strong>: <code>Europe</code>.</li>
<li>Mit {m('OK')} bestätigen.</li>
</ol>
{fig('system-language', 'Reiter System der Einstellungen mit der Spielsprache', 'Reiter <em>System</em>: die Sprache der Spiele.')}"""),
        sec("manette", 4, "Mit einem Controller spielen", f"""<ol class="steps">
<li>Schließe deinen Controller an und öffne {m('Emulation → Configure → Controls')}.</li>
<li><strong>Input Device</strong>: Wähle deinen Controller (meist der letzte in der Liste).</li>
<li>Klicke auf jedes Feld (A, B, X, Y, Sticks, Trigger …) und drücke dann die passende Taste am Controller.</li>
<li>Mit {m('OK')} bestätigen.</li>
</ol>
{fig('controls', 'Reiter Controls mit verbundenem Controller und belegten Tasten', 'Reiter <em>Controls</em>: Das blaue Feld <em>[waiting]</em> wartet auf einen Tastendruck.')}
<div class="note">Taucht der Controller nicht auf: Schließe Steam (es fängt Controller ab), probiere ein anderes USB-Kabel und starte Erable neu.</div>"""),
        sec("jeux", 5, "Spiele hinzufügen und spielen", f"""<ol class="steps">
<li>Lege deine Spiele (aus deinen Modulen oder Käufen ausgelesen) in einen Ordner <code>Switch-2-Spiele</code>, einen Unterordner pro Spiel.</li>
<li>Klicke in der Spieleliste auf {m('+ Add New Game Directory')}.</li>
<li>Wähle den Ordner des Spiels und dann {m('Ordner auswählen')}.</li>
<li>Doppelklicke auf das Spiel, um es zu starten.</li>
</ol>
{fig('game-folder', 'Fenster zur Auswahl des Spielordners', 'Den Ordner eines Spiels auswählen.')}
{fig('game-list', 'Spieleliste mit Zelda Ocarina of Time und Pokémon Pokopia', 'Die Spiele erscheinen in der Liste, mit Spielzeit und Add-ons.')}
<div class="note">Beim ersten Mal kann ein Spiel ein paar Minuten ruckeln, während der Emulator seine Shader vorbereitet; danach läuft alles flüssiger. Ein Problem? <a href="../signaler.html">Hier melden</a>.</div>"""),
    ]),
}

install_toc = {
    "ids": ["installer", "console", "langue", "manette", "jeux"],
    "fr": ["Installer Erable", "Firmware et clés", "Jeux en français", "Manette", "Ajouter vos jeux"],
    "en": ["Install Erable", "Firmware and keys", "Game language", "Controller", "Add your games"],
    "de": ["Erable installieren", "Firmware und Schlüssel", "Spielsprache", "Controller", "Spiele hinzufügen"],
}
tutorial("tuto/installer.html", "tuto.installTitle", "tutos.emuTitle", None, install_toc, install,
         {"fr": "Installer Erable", "en": "Install Erable", "de": "Erable installieren"},
         {"fr": "De zéro à votre premier jeu, pas à pas.",
          "en": "From nothing to your first game, step by step.",
          "de": "Von null bis zum ersten Spiel, Schritt für Schritt."})

ocarina = {
    "fr": "\n".join([
        sec("pret", 1, "Vérifier que le jeu est prêt", f"""<ol class="steps">
<li>Ocarina of Time apparaît dans la liste des jeux d'Erable (voir <a href="installer.html#jeux">Ajouter vos jeux</a>).</li>
<li>Faites un clic droit sur le jeu → {m('Configure Game')}.</li>
</ol>
<div class="note">Tous les réglages ci-dessous se font dans cette fenêtre, et ne concernent qu'Ocarina of Time : vos autres jeux gardent leurs réglages. Le jeu doit être fermé.</div>"""),
        sec("shaders", 2, "Installer le mod de shaders", f"""<p>Le mod <code>ootrshaders</code> limite les saccades : moins de shaders à préparer pendant que vous jouez.</p>
<ol class="steps">
<li>Onglet {m('Add-Ons')} → {m('Import Mod from ZIP')}.</li>
<li>Choisissez <code>ootrshaders.zip</code>, puis le type {m('RomFS')}.</li>
<li>Vérifiez que <strong>ootrshaders</strong> est coché, puis {m('Apply')}.</li>
</ol>
{fig('ocarina-addons', 'Onglet Add-Ons avec ootrshaders, Max Resolution et Dynamic FPS cochés', "Les trois add-ons cochés : <em>ootrshaders</em> (LayeredFS), et les deux intégrés à Erable, <em>Max Resolution</em> et <em>Dynamic FPS by Dark</em>.")}
<div class="note"><strong>Max Resolution</strong> et <strong>Dynamic FPS</strong> sont déjà inclus dans Erable : laissez-les cochés.</div>
<div class="warn">Ne prenez ce zip qu'auprès d'une personne de confiance, et analysez-le avec Microsoft Defender avant de l'importer. Un mod de shaders ne contient jamais de <code>.exe</code>.</div>"""),
        sec("memoire", 3, "Augmenter la mémoire émulée", f"""<ol class="steps">
<li>Onglet {m('System')}, partie <strong>Core</strong>.</li>
<li><strong>Memory Layout</strong> : <code>6GB DRAM</code> ou <code>8GB DRAM</code>.</li>
<li>{m('Apply')}.</li>
</ol>
{fig('ocarina-memory', 'Onglet System, Memory Layout réglé sur 8GB DRAM', 'Onglet <em>System</em> : <em>Memory Layout</em> sur <em>8GB DRAM</em>.')}
<div class="warn">Choisissez <strong>8GB</strong> seulement si votre PC a au moins <strong>16 Go</strong> de mémoire vive, et <strong>6GB</strong> s'il en a 12 Go. Sinon, laissez <em>4GB DRAM (Default)</em> : le PC ralentirait au lieu d'accélérer. Pour connaître votre mémoire : {m('Ctrl + Maj + Échap → Performance → Mémoire')}.</div>"""),
        sec("carte", 4, "Choisir Vulkan et votre carte graphique", """<ol class="steps">
<li>Onglet <span class="menu">Graphics</span>.</li>
<li><strong>API</strong> : <code>Vulkan</code>.</li>
<li><strong>Device</strong> : votre carte graphique (NVIDIA GeForce ou AMD Radeon), pas la puce graphique intégrée au processeur.</li>
<li><strong>Resolution</strong> : <code>1X (720p/1080p)</code>.</li>
</ol>"""),
        sec("dlss", 5, "Activer le DLSS", f"""<p>Ocarina of Time agrandit son image avec son propre réseau « DLSS ». Selon votre carte :</p>
<ol class="steps">
<li><strong>Carte NVIDIA (GeForce RTX)</strong> : cochez {m("Use NVIDIA DLSS in place of a game's DLSS")}. La ligne <em>Ready</em> doit apparaître en vert ; sinon, ouvrez l'application NVIDIA pour qu'elle télécharge les fichiers DLSS, puis rouvrez cette fenêtre.</li>
<li><strong>Autre carte (AMD, Intel)</strong> : cochez {m("Use the game's own DLSS")}.</li>
<li>{m('Apply')}, puis {m('OK')}.</li>
</ol>
{fig('ocarina-graphics', 'Onglet Graphics avec Vulkan, une carte RTX et le DLSS NVIDIA prêt', 'Onglet <em>Graphics</em> sur une carte NVIDIA : Vulkan, résolution 1X et DLSS NVIDIA « Ready ».')}
<div class="note">Le DLSS NVIDIA remplace celui du jeu <strong>uniquement en résolution 1X</strong> : gardez <em>Resolution</em> sur <em>1X</em>, le DLSS se charge d'agrandir l'image.</div>"""),
        sec("precompiler", 6, "Précompiler les shaders", f"""<p>Au premier lancement du jeu, Erable propose de préparer les shaders avant de jouer, ce qui évite la plupart des saccades de la première partie.</p>
<ol class="steps">
<li>Lancez le jeu, puis choisissez {m('Précompiler maintenant')}.</li>
<li>Patientez quelques minutes : le jeu démarre tout seul à la fin.</li>
</ol>
<div class="note">Cette étape n'a lieu qu'une fois. Elle recommence seulement après une mise à jour du pilote de la carte graphique.</div>"""),
        sec("cache", 7, "Reprendre le cache d'un autre PC", f"""<p>Si quelqu'un de la famille a déjà joué, son cache de shaders évite presque toutes les saccades sur un autre PC.</p>
<ol class="steps">
<li>Sur le PC qui a déjà joué : clic droit sur le jeu → {m('Ouvrir le cache de pipelines transférable')}.</li>
<li>Fermez Erable, puis copiez le dossier <code>0400486024dec000</code> sur une clé USB.</li>
<li>Sur l'autre PC : ouvrez le même emplacement de la même façon, fermez Erable, puis collez-y le dossier en remplaçant celui qui existe.</li>
</ol>
<div class="warn">Gardez ce dossier dans la famille : il contient des éléments du jeu et ne doit pas être publié sur Internet.</div>"""),
        sec("ensuite", "✓", "Et ensuite ?", """<p>Lancez le jeu. Les premières minutes, et à chaque nouvelle zone, quelques saccades restent normales le temps que l'émulateur prépare les shaders ; elles disparaissent ensuite. Ne videz pas le cache de shaders entre deux parties.</p>
<p>Un souci ? <a href="../signaler.html">Signalez-le ici</a>.</p>"""),
    ]),
    "en": "\n".join([
        sec("pret", 1, "Check the game is ready", f"""<ol class="steps">
<li>Ocarina of Time shows up in Erable's game list (see <a href="installer.html#jeux">Add your games</a>).</li>
<li>Right-click the game → {m('Configure Game')}.</li>
</ol>
<div class="note">All the settings below are made in this window and only apply to Ocarina of Time: your other games keep their settings. The game must be closed.</div>"""),
        sec("shaders", 2, "Install the shader mod", f"""<p>The <code>ootrshaders</code> mod cuts stutter: fewer shaders to prepare while you play.</p>
<ol class="steps">
<li>{m('Add-Ons')} tab → {m('Import Mod from ZIP')}.</li>
<li>Pick <code>ootrshaders.zip</code>, then the {m('RomFS')} type.</li>
<li>Check that <strong>ootrshaders</strong> is ticked, then {m('Apply')}.</li>
</ol>
{fig('ocarina-addons', 'Add-Ons tab with ootrshaders, Max Resolution and Dynamic FPS ticked', 'The three add-ons ticked: <em>ootrshaders</em> (LayeredFS), and the two built into Erable, <em>Max Resolution</em> and <em>Dynamic FPS by Dark</em>.')}
<div class="note"><strong>Max Resolution</strong> and <strong>Dynamic FPS</strong> already come with Erable: leave them ticked.</div>
<div class="warn">Only take this zip from someone you trust, and scan it with Microsoft Defender before importing it. A shader mod never contains an <code>.exe</code>.</div>"""),
        sec("memoire", 3, "Raise the emulated memory", f"""<ol class="steps">
<li>{m('System')} tab, <strong>Core</strong> section.</li>
<li><strong>Memory Layout</strong>: <code>6GB DRAM</code> or <code>8GB DRAM</code>.</li>
<li>{m('Apply')}.</li>
</ol>
{fig('ocarina-memory', 'System tab with Memory Layout set to 8GB DRAM', 'The <em>System</em> tab: <em>Memory Layout</em> on <em>8GB DRAM</em>.')}
<div class="warn">Choose <strong>8GB</strong> only if your PC has at least <strong>16 GB</strong> of RAM, and <strong>6GB</strong> if it has 12 GB. Otherwise leave <em>4GB DRAM (Default)</em>: the PC would slow down instead of speeding up. To see your memory: {m('Ctrl + Shift + Esc → Performance → Memory')}.</div>"""),
        sec("carte", 4, "Choose Vulkan and your graphics card", """<ol class="steps">
<li><span class="menu">Graphics</span> tab.</li>
<li><strong>API</strong>: <code>Vulkan</code>.</li>
<li><strong>Device</strong>: your graphics card (NVIDIA GeForce or AMD Radeon), not the graphics built into the processor.</li>
<li><strong>Resolution</strong>: <code>1X (720p/1080p)</code>.</li>
</ol>"""),
        sec("dlss", 5, "Turn on DLSS", f"""<p>Ocarina of Time upscales its image with its own "DLSS" network. Depending on your card:</p>
<ol class="steps">
<li><strong>NVIDIA card (GeForce RTX)</strong>: tick {m("Use NVIDIA DLSS in place of a game's DLSS")}. The <em>Ready</em> line should appear in green; if not, open the NVIDIA app so it downloads the DLSS files, then reopen this window.</li>
<li><strong>Other card (AMD, Intel)</strong>: tick {m("Use the game's own DLSS")}.</li>
<li>{m('Apply')}, then {m('OK')}.</li>
</ol>
{fig('ocarina-graphics', 'Graphics tab with Vulkan, an RTX card and NVIDIA DLSS ready', 'The <em>Graphics</em> tab on an NVIDIA card: Vulkan, 1X resolution and NVIDIA DLSS "Ready".')}
<div class="note">NVIDIA DLSS replaces the game's <strong>only at 1X resolution</strong>: keep <em>Resolution</em> on <em>1X</em> and let DLSS upscale the image.</div>"""),
        sec("precompiler", 6, "Precompile the shaders", f"""<p>The first time you start the game, Erable offers to prepare the shaders before you play, which avoids most of the first session's stutter.</p>
<ol class="steps">
<li>Start the game, then choose {m('Précompiler maintenant')} (precompile now).</li>
<li>Wait a few minutes: the game starts by itself at the end.</li>
</ol>
<div class="note">This only happens once. It runs again only after a graphics driver update.</div>"""),
        sec("cache", 7, "Reuse another PC's cache", f"""<p>If someone in the family has already played, their shader cache avoids almost all stutter on another PC.</p>
<ol class="steps">
<li>On the PC that has played: right-click the game → {m('Ouvrir le cache de pipelines transférable')} (open transferable pipeline cache).</li>
<li>Close Erable, then copy the <code>0400486024dec000</code> folder to a USB stick.</li>
<li>On the other PC: open the same place the same way, close Erable, then paste the folder there, replacing the existing one.</li>
</ol>
<div class="warn">Keep this folder in the family: it contains parts of the game and must not be published online.</div>"""),
        sec("ensuite", "✓", "What next?", """<p>Start the game. In the first minutes, and in each new area, a few stutters are normal while the emulator prepares shaders; they go away afterwards. Don't clear the shader cache between sessions.</p>
<p>A problem? <a href="../signaler.html">Report it here</a>.</p>"""),
    ]),
    "de": "\n".join([
        sec("pret", 1, "Prüfen, ob das Spiel bereit ist", f"""<ol class="steps">
<li>Ocarina of Time erscheint in der Spieleliste von Erable (siehe <a href="installer.html#jeux">Spiele hinzufügen</a>).</li>
<li>Rechtsklick auf das Spiel → {m('Configure Game')}.</li>
</ol>
<div class="note">Alle folgenden Einstellungen nimmst du in diesem Fenster vor, und sie gelten nur für Ocarina of Time: Deine anderen Spiele behalten ihre Einstellungen. Das Spiel muss geschlossen sein.</div>"""),
        sec("shaders", 2, "Den Shader-Mod installieren", f"""<p>Der Mod <code>ootrshaders</code> verringert Ruckler: weniger Shader, die während des Spielens vorbereitet werden müssen.</p>
<ol class="steps">
<li>Reiter {m('Add-Ons')} → {m('Import Mod from ZIP')}.</li>
<li>Wähle <code>ootrshaders.zip</code> und dann den Typ {m('RomFS')}.</li>
<li>Prüfe, dass <strong>ootrshaders</strong> angehakt ist, dann {m('Apply')}.</li>
</ol>
{fig('ocarina-addons', 'Reiter Add-Ons mit ootrshaders, Max Resolution und Dynamic FPS angehakt', 'Die drei angehakten Add-ons: <em>ootrshaders</em> (LayeredFS) sowie die beiden in Erable enthaltenen, <em>Max Resolution</em> und <em>Dynamic FPS by Dark</em>.')}
<div class="note"><strong>Max Resolution</strong> und <strong>Dynamic FPS</strong> sind bereits in Erable enthalten: Lass sie angehakt.</div>
<div class="warn">Nimm diese ZIP-Datei nur von einer Person, der du vertraust, und prüfe sie vor dem Import mit Microsoft Defender. Ein Shader-Mod enthält nie eine <code>.exe</code>.</div>"""),
        sec("memoire", 3, "Emulierten Speicher erhöhen", f"""<ol class="steps">
<li>Reiter {m('System')}, Bereich <strong>Core</strong>.</li>
<li><strong>Memory Layout</strong>: <code>6GB DRAM</code> oder <code>8GB DRAM</code>.</li>
<li>{m('Apply')}.</li>
</ol>
{fig('ocarina-memory', 'Reiter System, Memory Layout auf 8GB DRAM', 'Reiter <em>System</em>: <em>Memory Layout</em> auf <em>8GB DRAM</em>.')}
<div class="warn">Wähle <strong>8GB</strong> nur, wenn dein PC mindestens <strong>16 GB</strong> Arbeitsspeicher hat, und <strong>6GB</strong> bei 12 GB. Sonst lass <em>4GB DRAM (Default)</em>: Der PC würde langsamer statt schneller. Deinen Speicher siehst du unter {m('Strg + Umschalt + Esc → Leistung → Arbeitsspeicher')}.</div>"""),
        sec("carte", 4, "Vulkan und deine Grafikkarte wählen", """<ol class="steps">
<li>Reiter <span class="menu">Graphics</span>.</li>
<li><strong>API</strong>: <code>Vulkan</code>.</li>
<li><strong>Device</strong>: deine Grafikkarte (NVIDIA GeForce oder AMD Radeon), nicht die im Prozessor eingebaute Grafik.</li>
<li><strong>Resolution</strong>: <code>1X (720p/1080p)</code>.</li>
</ol>"""),
        sec("dlss", 5, "DLSS einschalten", f"""<p>Ocarina of Time skaliert sein Bild mit einem eigenen „DLSS“-Netz hoch. Je nach Karte:</p>
<ol class="steps">
<li><strong>NVIDIA-Karte (GeForce RTX)</strong>: Hake {m("Use NVIDIA DLSS in place of a game's DLSS")} an. Die Zeile <em>Ready</em> sollte grün erscheinen; falls nicht, öffne die NVIDIA-App, damit sie die DLSS-Dateien lädt, und öffne dieses Fenster erneut.</li>
<li><strong>Andere Karte (AMD, Intel)</strong>: Hake {m("Use the game's own DLSS")} an.</li>
<li>{m('Apply')}, dann {m('OK')}.</li>
</ol>
{fig('ocarina-graphics', 'Reiter Graphics mit Vulkan, einer RTX-Karte und NVIDIA DLSS bereit', 'Reiter <em>Graphics</em> mit einer NVIDIA-Karte: Vulkan, Auflösung 1X und NVIDIA DLSS „Ready“.')}
<div class="note">NVIDIA DLSS ersetzt das des Spiels <strong>nur bei Auflösung 1X</strong>: Lass <em>Resolution</em> auf <em>1X</em>, DLSS übernimmt das Hochskalieren.</div>"""),
        sec("precompiler", 6, "Shader vorkompilieren", f"""<p>Beim ersten Start des Spiels bietet Erable an, die Shader vor dem Spielen vorzubereiten. Das vermeidet die meisten Ruckler der ersten Runde.</p>
<ol class="steps">
<li>Starte das Spiel und wähle {m('Précompiler maintenant')} (jetzt vorkompilieren).</li>
<li>Warte ein paar Minuten: Am Ende startet das Spiel von selbst.</li>
</ol>
<div class="note">Das passiert nur einmal – erneut nur nach einem Update des Grafiktreibers.</div>"""),
        sec("cache", 7, "Den Cache eines anderen PCs übernehmen", f"""<p>Hat jemand aus der Familie schon gespielt, vermeidet sein Shader-Cache auf einem anderen PC fast alle Ruckler.</p>
<ol class="steps">
<li>Auf dem PC, auf dem schon gespielt wurde: Rechtsklick auf das Spiel → {m('Ouvrir le cache de pipelines transférable')} (übertragbaren Pipeline-Cache öffnen).</li>
<li>Schließe Erable und kopiere den Ordner <code>0400486024dec000</code> auf einen USB-Stick.</li>
<li>Auf dem anderen PC: Öffne denselben Ort auf dieselbe Weise, schließe Erable und füge den Ordner dort ein, wobei du den vorhandenen ersetzt.</li>
</ol>
<div class="warn">Behalte diesen Ordner in der Familie: Er enthält Teile des Spiels und darf nicht im Internet veröffentlicht werden.</div>"""),
        sec("ensuite", "✓", "Und dann?", """<p>Starte das Spiel. In den ersten Minuten und in jedem neuen Gebiet sind ein paar Ruckler normal, während der Emulator Shader vorbereitet; danach verschwinden sie. Leere den Shader-Cache nicht zwischen zwei Runden.</p>
<p>Ein Problem? <a href="../signaler.html">Hier melden</a>.</p>"""),
    ]),
}
ocarina_toc = {
    "ids": ["pret", "shaders", "memoire", "carte", "dlss", "precompiler", "cache"],
    "fr": ["Jeu prêt", "Mod de shaders", "Mémoire émulée", "Vulkan et carte", "DLSS", "Précompiler", "Cache d'un autre PC"],
    "en": ["Game ready", "Shader mod", "Emulated memory", "Vulkan and card", "DLSS", "Precompile", "Another PC's cache"],
    "de": ["Spiel bereit", "Shader-Mod", "Emulierter Speicher", "Vulkan und Karte", "DLSS", "Vorkompilieren", "Cache eines anderen PCs"],
}
tutorial("tuto/zelda-ocarina.html", "tuto.ocarinaTitle", "tutos.gamesTitle", "Zelda: Ocarina of Time", ocarina_toc, ocarina,
         {"fr": "Zelda : Ocarina of Time", "en": "Zelda: Ocarina of Time", "de": "Zelda: Ocarina of Time"},
         {"fr": "Moins de saccades et une image plus nette : les bons réglages, étape par étape.",
          "en": "Less stutter and a sharper image: the right settings, step by step.",
          "de": "Weniger Ruckler und ein schärferes Bild: die richtigen Einstellungen, Schritt für Schritt."})
print("ok")
