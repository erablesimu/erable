// The known problems: bugs.sqlite (tools/site/export_bugs.py, written when the site is
// published) read in the browser with sql.js.
const KIND_LABELS = {
  crash: "report.k1", graphics: "report.k2", sound: "report.k3", performance: "report.k4",
  stuck: "report.k5", controller: "report.k6", install: "report.k7", other: "report.k8",
};
const filters = { state: "open", game: "", kind: "", search: "" };
let db;

function rows(sql, params) {
  const statement = db.prepare(sql);
  statement.bind(params || {});
  const result = [];
  while (statement.step()) {
    result.push(statement.getAsObject());
  }
  statement.free();
  return result;
}

function fillSelect(select, allKey, values, label) {
  const current = select.value;
  select.replaceChildren(el("option", { value: "" }, t(allKey)),
    ...values.map((value) => el("option", { value }, label(value))));
  select.value = values.includes(current) ? current : "";
}

function renderFilters() {
  fillSelect(document.getElementById("bug-game"), "bugs.allGames",
    rows("SELECT DISTINCT game FROM bugs WHERE game IS NOT NULL ORDER BY game").map((row) => row.game),
    (game) => game);
  fillSelect(document.getElementById("bug-kind"), "bugs.allKinds",
    rows("SELECT DISTINCT kind FROM bugs WHERE kind IS NOT NULL").map((row) => row.kind),
    (kind) => t(KIND_LABELS[kind] || "report.k8"));
  const [counts] = rows("SELECT SUM(state = 'open') AS open, SUM(state = 'closed') AS closed FROM bugs");
  document.getElementById("bug-stats").replaceChildren(
    el("span", { class: "stat open" }, el("strong", {}, String(counts.open || 0)), " ", t("bugs.statOpen")),
    el("span", { class: "stat fixed" }, el("strong", {}, String(counts.closed || 0)), " ", t("bugs.statClosed")));
  const [generated] = rows("SELECT value FROM meta WHERE key = 'generated_at'");
  document.getElementById("bug-updated").textContent =
    generated ? t("bugs.updated", { date: formatDate(generated.value) }) : "";
}

function renderList() {
  const where = [];
  const params = {};
  if (filters.state !== "all") {
    where.push("state = $state");
    params.$state = filters.state;
  }
  if (filters.game) {
    where.push("game = $game");
    params.$game = filters.game;
  }
  if (filters.kind) {
    where.push("kind = $kind");
    params.$kind = filters.kind;
  }
  if (filters.search) {
    where.push("(summary LIKE $search OR details LIKE $search OR game LIKE $search OR CAST(number AS TEXT) = $exact)");
    params.$search = "%" + filters.search + "%";
    params.$exact = filters.search.replace(/^#/, "");
  }
  const bugs = rows("SELECT * FROM bugs" + (where.length ? " WHERE " + where.join(" AND ") : "") +
    " ORDER BY state = 'open' DESC, updated_at DESC", params);
  const list = document.getElementById("bug-list");
  if (bugs.length === 0) {
    list.replaceChildren(el("p", { class: "status card" }, t("bugs.empty")));
    return;
  }
  list.replaceChildren(...bugs.map((bug) => {
    const fixed = bug.state === "closed" && bug.state_reason !== "not_planned";
    const status = bug.state === "open" ? "open" : fixed ? "fixed" : "closed";
    const tags = el("div", { class: "tags" },
      el("span", { class: "status-badge " + status }, t("bugs.status_" + status)));
    if (bug.game) {
      tags.append(el("span", { class: "tag" }, bug.game));
    }
    if (bug.kind) {
      tags.append(el("span", { class: "tag muted" }, t(KIND_LABELS[bug.kind] || "report.k8")));
    }
    for (const label of JSON.parse(bug.labels)) {
      tags.append(el("span", { class: "tag muted" }, label));
    }
    const meta = ["#" + bug.number, formatDate(bug.created_at)];
    if (bug.version) {
      meta.push(bug.version);
    }
    if (bug.comments) {
      meta.push(t("bugs.comments", { n: bug.comments }));
    }
    const card = el("a", { class: "card bug " + status, href: bug.url },
      tags, el("h3", {}, bug.summary), el("p", { class: "meta" }, meta.join(" · ")));
    if (bug.details) {
      card.insertBefore(el("p", { class: "bug-details" }, bug.details), card.lastChild);
    }
    return card;
  }));
}

function render() {
  if (!db) {
    return;
  }
  renderFilters();
  renderList();
}

for (const button of document.querySelectorAll("#bug-state button")) {
  button.addEventListener("click", () => {
    filters.state = button.dataset.state;
    for (const other of document.querySelectorAll("#bug-state button")) {
      other.setAttribute("aria-pressed", String(other === button));
    }
    renderList();
  });
}
document.getElementById("bug-game").addEventListener("change", (event) => {
  filters.game = event.target.value;
  renderList();
});
document.getElementById("bug-kind").addEventListener("change", (event) => {
  filters.kind = event.target.value;
  renderList();
});
document.getElementById("bug-search").addEventListener("input", (event) => {
  filters.search = event.target.value.trim();
  renderList();
});
document.addEventListener("langchange", render);

Promise.all([
  initSqlJs({ locateFile: (file) => "https://cdnjs.cloudflare.com/ajax/libs/sql.js/1.10.3/" + file }),
  fetch("bugs.sqlite", { cache: "no-cache" }).then((response) => {
    if (!response.ok) {
      throw new Error(String(response.status));
    }
    return response.arrayBuffer();
  }),
]).then(([SQL, buffer]) => {
  db = new SQL.Database(new Uint8Array(buffer));
  render();
}).catch(() => {
  document.getElementById("bug-list").replaceChildren(
    el("p", { class: "status card" }, t("bugs.unavailable"), " ",
      el("a", { href: "https://github.com/" + REPO + "/issues" }, "GitHub")));
});
