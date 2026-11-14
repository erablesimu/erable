// Turns the form into a prefilled "new issue" page on GitHub.
const form = document.getElementById("report");
const game = document.getElementById("game");
const gameOther = document.getElementById("game-other");

game.addEventListener("change", () => {
  gameOther.hidden = game.value !== "other";
  if (!gameOther.hidden) {
    gameOther.focus();
  }
});

form.addEventListener("submit", (event) => {
  event.preventDefault();
  const value = (id) => document.getElementById(id).value.trim();
  const gameName = game.value === "other" ? value("game-other") : game.value;
  const kind = t("report.k" + form.querySelector("input[name=kind]:checked").value);
  const section = (heading, text) => (text ? "### " + t(heading) + "\n\n" + text + "\n\n" : "");
  const body =
    section("report.bodyGame", gameName || t("report.bodyNone")) +
    section("report.bodyKind", kind) +
    section("report.bodyVersion", value("version") || t("report.bodyUnknown")) +
    section("report.bodyPc", value("pc")) +
    section("report.bodyWhat", value("what")) +
    section("report.bodyWhere", value("where")) +
    "### " + t("report.bodyLog") + "\n\n" + t("report.bodyLogHint") + "\n";
  const title = (gameName ? "[" + gameName + "] " : "") + "[" + kind + "] " + value("title");
  const url = "https://github.com/" + REPO + "/issues/new?" + new URLSearchParams({ title, body }).toString();
  window.open(url, "_blank", "noopener");
});
