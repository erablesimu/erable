// Home page: the latest version (download link, size, downloads, first changes), the last
// versions one line each, and the main guides.
let releases;

const HOME_GUIDES = ["guides/install.html", "guides/saves.html", "guides/zelda-ocarina.html"];

function zipOf(release) {
  return release.assets.find((asset) => asset.name === "erable-windows-x86_64.zip");
}

function sizeText(bytes) {
  return new Intl.NumberFormat(LOCALES[LANG], { maximumFractionDigits: 0 }).format(bytes / 1048576) + " " + t("home.mb");
}

function number(value) {
  return new Intl.NumberFormat(LOCALES[LANG]).format(value);
}

function timeAgo(iso) {
  const seconds = (new Date(iso).getTime() - Date.now()) / 1000;
  const format = new Intl.RelativeTimeFormat(LOCALES[LANG], { numeric: "auto" });
  const steps = [["year", 31536000], ["month", 2592000], ["week", 604800], ["day", 86400], ["hour", 3600], ["minute", 60]];
  for (const [unit, size] of steps) {
    if (Math.abs(seconds) >= size) {
      return format.format(Math.round(seconds / size), unit);
    }
  }
  return format.format(0, "minute");
}

/// A release's changes, as plain sentences
function items(release) {
  return [...simplifyNotes(release.body).querySelectorAll("li")].map((item) => item.textContent);
}

/// What a release brought: its notes keep the earlier versions' lines, so only the lines the
/// release before it didn't have
function added(index) {
  const own = items(releases[index]);
  if (index + 1 >= releases.length) {
    return own;
  }
  const before = new Set(items(releases[index + 1]));
  return own.filter((text) => !before.has(text));
}

function renderLatest() {
  if (!releases || releases.length === 0) {
    return;
  }
  const latest = releases[0];
  const zip = zipOf(latest);
  if (zip) {
    document.getElementById("download").href = zip.browser_download_url;
    document.getElementById("download-meta").textContent = latest.tag_name + " · " + sizeText(zip.size);
    document.getElementById("stat-size").textContent = sizeText(zip.size);
    document.getElementById("stat-downloads").textContent = number(zip.download_count);
  }
  const total = releases.reduce((sum, release) => sum + (zipOf(release)?.download_count || 0), 0);
  document.getElementById("stat-total").textContent = number(total);
  document.getElementById("stat-tag").textContent = latest.name || latest.tag_name;
  document.getElementById("stat-when").textContent = t("home.published", { when: timeAgo(latest.published_at) });
  const notes = added(0).slice(0, 4);
  document.getElementById("stat-notes").replaceChildren(
    ...(notes.length ? notes.map((text) => el("li", {}, text)) : [el("li", { class: "status" }, t("changes.noNotes"))]));

  document.getElementById("release-rows").replaceChildren(...releases.slice(0, 6).map((release, index) => {
    const news = added(index);
    const changes = news.length;
    const first = news[0] || t("home.fixes");
    return el("a", { class: "d-row" + (index === 0 ? " latest" : ""), href: "changelog.html" },
      el("span", { class: "d-chip" }, release.tag_name),
      el("span", { class: "d-row-main" },
        el("strong", {}, first),
        el("span", { class: "meta" }, changes === 1 ? t("home.change1") : t("home.changes", { n: changes }))),
      el("span", { class: "d-row-side meta" }, timeAgo(release.published_at)));
  }));
}

function renderGuides() {
  document.getElementById("guide-rows").replaceChildren(...HOME_GUIDES.map((href) => {
    const guide = TUTORIALS.find((tutorial) => tutorial.href === href);
    return el("a", { class: "d-row", href },
      el("img", { class: "d-thumb", src: guide.image, alt: "", loading: "lazy" }),
      el("span", { class: "d-row-main" }, el("strong", {}, guide.title[LANG]), el("span", { class: "meta" }, guide.summary[LANG])),
      el("span", { class: "d-row-side d-arrow", "aria-hidden": "true" }, "→"));
  }));
}

getJson("/releases?per_page=100").then((list) => {
  releases = list.filter((release) => !release.draft && !release.prerelease);
  renderLatest();
}).catch((error) => {
  const fallback = el("p", { class: "status" }, el("a", { href: "https://github.com/" + REPO + "/releases" }, "GitHub"));
  document.getElementById("release-rows").replaceChildren(fallback);
  document.getElementById("stat-notes").replaceChildren(el("li", { class: "status" }, error.message));
});

renderGuides();
document.addEventListener("langchange", () => {
  renderLatest();
  renderGuides();
});
