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

// Release notes as the site shows them: short and for players. Drops the download packages part
// and, in each item, the technical sentences (switches, variables, code), keeping the first one.
function simplifyNotes(markdown) {
  const root = document.createElement("div");
  root.innerHTML = DOMPurify.sanitize(marked.parse(markdown || ""));
  for (const heading of root.querySelectorAll("h1, h2, h3, h4")) {
    if (/packages|paquets|pakete|downloads?|téléchargements/i.test(heading.textContent)) {
      while (heading.nextSibling) {
        heading.nextSibling.remove();
      }
      heading.remove();
      break;
    }
  }
  const technical = /\u0000|EDEN_|NX2_|\b[A-Z][A-Z0-9_]{3,}=|\.(dll|ini|txt|json)\b|0x[0-9a-f]/i;
  for (const item of [...root.querySelectorAll("li")]) {
    if (!root.contains(item)) {
      continue;
    }
    item.querySelectorAll("ul, ol").forEach((list) => list.remove());
    item.querySelectorAll("code").forEach((code) => code.replaceWith("\u0000"));
    const sentences = item.textContent.replace(/\s+/g, " ").trim()
      .split(/(?<=[.!?…])\s+(?=[A-ZÀ-ÝÉ«"“(\u0000])/u)
      .filter((sentence) => !technical.test(sentence));
    let text = (sentences[0] || "").replace(/\s*\([^)]{40,}\)/g, "").trim();
    // A long sentence keeps its first clause
    if (text.length > 170) {
      const cut = Math.max(text.lastIndexOf(" : ", 170), text.lastIndexOf(" ; ", 170), text.lastIndexOf(", ", 170));
      text = (cut > 60 ? text.slice(0, cut) : text.slice(0, 160).replace(/[\s,;:]+\S*$/, "")).trim() + (cut > 60 ? "." : "…");
    }
    if (text) {
      item.textContent = text;
    } else {
      item.remove();
    }
  }
  // Paragraphs that are not group titles and still carry code go too, as do emptied lists
  for (const paragraph of root.querySelectorAll("p")) {
    if (paragraph.querySelector("code") || technical.test(paragraph.textContent)) {
      paragraph.remove();
    }
  }
  root.querySelectorAll("ul, ol").forEach((list) => list.children.length === 0 && list.remove());
  return root;
}
