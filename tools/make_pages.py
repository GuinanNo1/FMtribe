#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Ian Guinan
"""Make FMtribe's site for GitHub Pages (https://guinanno1.github.io/FMtribe/): fm1-base's site (tools/make_site.py:
the front page, installer, emulator, manual, screen mirror, words, firmware download), plus a quickstart page, and
every page's header linking all of them.

  tools/make_pages.py PKG VERSION OUT_DIR

PKG must be a release build (./build.sh --release X.Y, identity FM-1_59XXYYZZ), and app/build/emu must hold the
browser build (./build.sh --web --release X.Y)."""
import html
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "base"
APP = ROOT / "app"
sys.path.insert(0, str(BASE / "tools"))
import make_site  # noqa: E402  (fm1-base's)
from build import read_conf  # noqa: E402

PAGES = [("emu/", "Emulator"), ("install/", "Install"), ("quickstart/", "Quickstart"), ("manual/", "Manual"),
         ("mirror/", "Mirror")]


def header(name, repo, here, depth):
    """the site's header for a page DEPTH folders down: home, every page, the source (manual_html.py's markup)"""
    up = "../" * depth
    cur = ' aria-current="page"'
    nav = "".join(f'<a href="{up}{p}"{cur if p == here else ""}>{t}</a>' for p, t in PAGES)
    if repo:
        nav += f'<a href="{repo}">Source</a>'
    return f'<header class="site"><a class="home" href="{up or "./"}"{cur if here == "" else ""}>{html.escape(name)}</a><nav>{nav}</nav></header>'


def main(pkg, version, out):
    out = Path(out)
    conf = read_conf(APP / "fm1.toml")
    name, repo = conf["name"], conf.get("repo", "")
    make_site.main(APP, pkg, version, out)
    # the quickstart: the manual's look (fm1-base's tools/manual_html.py)
    (out / "quickstart").mkdir()
    qs = out / "quickstart" / "index.html"
    subprocess.run([sys.executable, str(BASE / "tools" / "manual_html.py"), str(APP / "site" / "quickstart.md"), str(qs),
                    "--name", name, "--repo", repo, "--tagline", conf.get("tagline", "")], check=True)
    h = qs.read_text(encoding="utf-8")
    title_link = f'<a class="home" href="../">{name}</a>\n<h1>'
    if h.count(title_link) != 1:
        raise SystemExit("quickstart: its title not found once")
    qs.write_text(h.replace(title_link, header(name, repo, "quickstart/", 1) + "\n<h1>"), encoding="utf-8")
    # every Markdown page's header: all the pages
    for f, here, depth in ((out / "index.html", "", 0), (out / "manual" / "index.html", "manual/", 1),
                           (qs, "quickstart/", 1)):
        h = f.read_text(encoding="utf-8")
        h, n = re.subn(r'<header class="site">.*?</header>', lambda _: header(name, repo, here, depth), h, count=1,
                       flags=re.S)
        if n != 1:
            raise SystemExit(f"{f}: no site header")
        f.write_text(h, encoding="utf-8")
    print(f"site: {out}: + quickstart/")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
