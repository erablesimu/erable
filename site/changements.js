// Versions (GitHub releases, with their notes) and the latest commits of the default branch.
const tabs = { releases: document.getElementById("tab-releases"), commits: document.getElementById("tab-commits") };
const data = {};

function show(name) {
  for (const [key, tab] of Object.entries(tabs)) {
    tab.setAttribute("aria-selected", String(key === name));
    document.getElementById(key).hidden = key !== name;
  }
  document.querySelector(".notes-lang").hidden = name !== "releases" || !t("changes.notesLang");
  if (!(name in data)) {
    data[name] = null;
    (name === "releases" ? loadReleases : loadCommits)();
  }
  history.replaceState(null, "", location.search + (name === "commits" ? "#code" : ""));
}

function fail(section, error) {
  section.replaceChildren(el("p", { class: "status error" }, t("error.load", { message: error.message })));
}

function renderReleases() {
  const section = document.getElementById("releases");
  const releases = data.releases;
  if (!releases) {
    return;
  }
  if (releases.length === 0) {
    section.replaceChildren(el("p", { class: "status" }, t("changes.none")));
    return;
  }
  const timeline = el("div", { class: "timeline" });
  releases.forEach((release, index) => {
    const notes = el("div", { class: "notes" });
    notes.innerHTML = DOMPurify.sanitize(marked.parse(release.body || t("changes.noNotes")));
    const title = el("h3", {}, el("a", { href: release.html_url }, release.name || release.tag_name));
    if (index === 0) {
      title.append(el("span", { class: "badge" }, t("changes.latest")));
    }
    const card = el("article", { class: "card release" },
      el("header", {}, title, el("span", { class: "meta" }, formatDate(release.published_at))),
      notes);
    if (index > 0 || (release.body || "").length > 1500) {
      notes.classList.add("collapsed");
      const toggle = el("button", { class: "toggle", type: "button" }, t("changes.more"));
      toggle.addEventListener("click", () => {
        const collapsed = notes.classList.toggle("collapsed");
        toggle.textContent = t(collapsed ? "changes.more" : "changes.less");
      });
      card.append(toggle);
    }
    timeline.append(card);
  });
  section.replaceChildren(timeline);
}

async function loadReleases() {
  try {
    data.releases = (await getJson("/releases?per_page=15")).filter((release) => !release.draft);
    renderReleases();
  } catch (error) {
    fail(document.getElementById("releases"), error);
  }
}

function renderCommits() {
  const section = document.getElementById("commits");
  const commits = data.commits;
  if (!commits) {
    return;
  }
  const list = el("ul", { class: "commits card" });
  for (const commit of commits) {
    const [first, ...rest] = commit.commit.message.split("\n");
    const match = first.match(/^([\w./, -]{2,40}): (.+)$/);
    const scope = match ? match[1] : "erable";
    const title = match ? match[2] : first;
    const body = rest.join("\n").replace(/^(Co-Authored-By|Claude-Session|Signed-off-by): .*$/gim, "").trim();
    const item = el("li", {},
      el("span", { class: "scope", title: scope }, scope),
      el("div", { class: "title" }, el("a", { href: commit.html_url }, title)),
      el("div", { class: "meta" }, formatDate(commit.commit.author.date) + " · " + commit.sha.slice(0, 7)));
    if (body) {
      item.append(el("details", {}, el("summary", { class: "meta" }, t("changes.details")), el("p", {}, body)));
    }
    list.append(item);
  }
  section.replaceChildren(list,
    el("p", {}, el("a", { class: "more", href: "https://github.com/" + REPO + "/commits" }, t("changes.history"))));
}

async function loadCommits() {
  try {
    data.commits = await getJson("/commits?per_page=40");
    renderCommits();
  } catch (error) {
    fail(document.getElementById("commits"), error);
  }
}

tabs.releases.addEventListener("click", () => show("releases"));
tabs.commits.addEventListener("click", () => show("commits"));
document.addEventListener("langchange", () => {
  renderReleases();
  renderCommits();
  document.querySelector(".notes-lang").hidden = tabs.releases.getAttribute("aria-selected") !== "true" || !t("changes.notesLang");
});
show(location.hash === "#code" ? "commits" : "releases");
