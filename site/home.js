// Home page: the latest version's number, download link and the first notes of its changes.
let latest;

function renderLatest() {
  if (!latest) {
    return;
  }
  document.getElementById("version-text").textContent =
    t("home.pillVersion", { tag: latest.tag_name, date: formatDate(latest.published_at) });
  document.getElementById("latest-date").textContent = (latest.name || latest.tag_name) + " · " + formatDate(latest.published_at);
}

getLatestRelease().then((release) => {
  latest = release;
  const zip = release.assets.find((asset) => asset.name === "erable-windows-x86_64.zip");
  if (zip) {
    document.getElementById("download").href = zip.browser_download_url;
  }
  renderLatest();
  // The notes' first section (from "## Nouveautés ..." on), at most eight items
  const html = DOMPurify.sanitize(marked.parse(release.body || ""));
  const notes = document.createElement("div");
  notes.innerHTML = html;
  const items = [...notes.querySelectorAll("li")].slice(0, 8);
  const target = document.getElementById("latest-notes");
  if (items.length === 0) {
    target.replaceChildren(el("p", { class: "status" }, t("changes.noNotes")));
    return;
  }
  const list = el("ul");
  for (const item of items) {
    list.append(item);
  }
  target.replaceChildren(list);
}).catch(() => {
  document.getElementById("latest-notes").replaceChildren(
    el("p", { class: "status" }, el("a", { href: "https://github.com/" + REPO + "/releases" }, "GitHub")));
});

document.addEventListener("langchange", renderLatest);
