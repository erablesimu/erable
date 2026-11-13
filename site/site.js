// Shared by the pages of the Erable site.
const REPO = "erablesimu/erable";
const API = "https://api.github.com/repos/" + REPO;

async function getJson(path) {
  const response = await fetch(API + path, { headers: { Accept: "application/vnd.github+json" } });
  if (!response.ok) {
    throw new Error(response.status === 403 ? "GitHub limite le nombre de visites : réessayez dans une heure." :
                    "GitHub a répondu " + response.status + ".");
  }
  return response.json();
}

function formatDate(iso) {
  return new Date(iso).toLocaleDateString("fr-FR", { day: "numeric", month: "long", year: "numeric" });
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
