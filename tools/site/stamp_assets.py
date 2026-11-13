#!/usr/bin/env python3
"""Adds ?v=<content hash> to the site's own .js and .css links in every page of a built site, so
browsers fetch a script or stylesheet again as soon as it changes (GitHub Pages lets them keep
it for 10 minutes).

    stamp_assets.py SITE_DIRECTORY
"""

import hashlib
import re
import sys
from pathlib import Path


def stamp(root):
    root = Path(root)
    pages = list(root.rglob("*.html"))
    for page in pages:
        text = page.read_text(encoding="utf-8")

        def versioned(match):
            attribute, path = match.group(1), match.group(2)
            target = (page.parent / path).resolve()
            if not target.is_file():
                return match.group(0)
            digest = hashlib.sha256(target.read_bytes()).hexdigest()[:10]
            return f'{attribute}="{path}?v={digest}"'

        new = re.sub(r'(src|href)="((?!https?:|//)[^"?#]+\.(?:js|css))"', versioned, text)
        if new != text:
            page.write_text(new, encoding="utf-8")
    return len(pages)


if __name__ == "__main__":
    print(f"{stamp(sys.argv[1])} pages stamped")
