// Shared by the pages of the Erable site (after i18n.js).
const REPO = "erablesimu/erable";
const API = "https://api.github.com/repos/" + REPO;

async function getJson(path) {
  const response = await fetch(API + path, { headers: { Accept: "application/vnd.github+json" } });
  if (!response.ok) {
    throw new Error(response.status === 403 ? t("error.rate") : t("error.status", { status: response.status }));
  }
  return response.json();
}

function formatDate(iso) {
  return new Date(iso).toLocaleDateString(LOCALES[LANG], { day: "numeric", month: "long", year: "numeric" });
}

function el(tag, attributes, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(attributes || {})) {
    node.setAttribute(key, value);
  }
  for (const child of children) {
    node.append(child);
  }
  return node;
}

// The latest release, fetched once per page
let latestRelease;
function getLatestRelease() {
  latestRelease ??= getJson("/releases/latest");
  return latestRelease;
}
