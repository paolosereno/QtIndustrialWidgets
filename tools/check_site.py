#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

"""Checks the GitHub Pages site assembled by the CMake target "site" (Python standard library only).

  check_site.py SITE_DIR          checks the files (run in CI after "cmake --build build --target site"):
    - every local href and src of the landing page resolves to a file of the site, with its #anchor;
    - every other HTML file at the root is a redirect stub whose target exists under api/;
    - the old pages of the classes in MOVED_CLASSES (api/class<Name>.html and class<Name>.html, published until #144)
      are redirect stubs to api/classQtIndustrialWidgets_1_1<Name>.html, and no other page under api/ is a stub;
    - every src (images, scripts) of every page under api/ resolves;
    - the widget grid of the landing page lists the widgets of the README table, with the same captions and images;
    - the GIT_TAG of the landing page's quick start equals the README's;
    - the landing page's meta description is the README's opening sentence;
    - a Google Search Console verification file at the root (google<token>.html) names itself;
    - sitemap.xml lists the landing page and every page under api/ that is not a redirect stub, each existing.
  check_site.py --url BASE_URL    crawls a served copy (python3 -m http.server, or the published site) over HTTP:
    the landing page, every page and image it links to, every redirect stub and its target (including those of
    MOVED_CLASSES), every src of the Doxygen pages; every request must answer 200.

Exit code 0 when everything holds, 1 otherwise; each problem is printed on its own line.
"""

import html
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from html.parser import HTMLParser

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README = os.path.join(REPO_ROOT, "README.md")
# Classes whose page Doxygen made outside the namespace until #144 (class<Name>.html): those addresses were published,
# under api/ and at the root, and must stay redirect stubs to the namespaced page. Kept apart from the list in
# docs/site/assemble_site.cmake on purpose: this one is what was published, that one is what the generator writes.
MOVED_CLASSES = ("AnnunciatorPanel", "IndustrialSwitch", "LedIndicator", "LevelMeter", "LinearGauge", "RadialGauge",
                 "SevenSegmentDisplay")


def moved_class_stubs():
    """(old address relative to the site, target relative to the site) of every moved class page."""
    out = []
    for name in MOVED_CLASSES:
        target = f"api/classQtIndustrialWidgets_1_1{name}.html"
        out += [(f"api/class{name}.html", target), (f"class{name}.html", target)]
    return out


class PageParser(HTMLParser):
    """Collects the local links, the sources, the ids and the meta refresh target of a page."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.hrefs = []
        self.srcs = []
        self.ids = set()
        self.refresh = None

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        for key in ("id", "name"):
            if a.get(key):
                self.ids.add(a[key])
        if tag == "a" and a.get("href"):
            self.hrefs.append(a["href"])
        if tag == "link" and a.get("href") and a.get("rel") == "stylesheet":
            self.srcs.append(a["href"])
        if tag in ("img", "script") and a.get("src"):
            self.srcs.append(a["src"])
        if tag == "meta" and (a.get("http-equiv") or "").lower() == "refresh":
            m = re.search(r"url=(.+)$", a.get("content", ""), re.I)
            self.refresh = m.group(1).strip() if m else None


def parse(text):
    p = PageParser()
    p.feed(text)
    return p


def is_local(link):
    return not re.match(r"^[a-z][a-z0-9+.-]*:", link, re.I) and not link.startswith("//")


def readme_widgets():
    """(name, caption, image file) of every row of the README widget table."""
    text = open(README, encoding="utf-8").read()
    rows = re.findall(r'^\| \[`(\w+)`\]\(docs/widgets\.md#\w+\)<br>(.*?) \| <img src="docs/images/([\w.-]+)"',
                      text, re.M)
    return [(n, c.strip(), i) for n, c, i in rows]


def readme_sentence():
    m = re.search(r"^# QtIndustrialWidgets\n\n(.*?)\n\n", open(README, encoding="utf-8").read(), re.S | re.M)
    return " ".join(m.group(1).split()) if m else None


def landing_description(text):
    m = re.search(r'<meta name="description" content="([^"]*)"', text)
    return " ".join(html.unescape(m.group(1)).split()) if m else None


def readme_git_tag():
    m = re.search(r"GIT_TAG\s+(\S+)", open(README, encoding="utf-8").read())
    return m.group(1) if m else None


def landing_widgets(text):
    cards = re.findall(r'<li class="widget" id="[\w-]+">\s*<img src="api/images/([\w.-]+)"[^>]*>\s*'
                       r'<h3>(\w+)</h3>\s*<p>(.*?)</p>', text, re.S)
    return [(n, html.unescape(c).strip(), i) for i, n, c in cards]


def landing_git_tag(text):
    m = re.search(r"GIT_TAG\s+(\S+)", html.unescape(text))
    return m.group(1) if m else None


def check_files(site):
    problems = []
    index = os.path.join(site, "index.html")
    if not os.path.isfile(index):
        return ["no index.html in " + site]
    landing_text = open(index, encoding="utf-8").read()
    page_cache = {}

    def ids_of(path):
        if path not in page_cache:
            page_cache[path] = parse(open(path, encoding="utf-8", errors="replace").read()).ids
        return page_cache[path]

    def resolve(base_dir, link, where, check_anchor=True):
        url = urllib.parse.urlsplit(link)
        if not url.path:
            target = os.path.join(base_dir, "index.html") if not url.fragment else None
        else:
            target = os.path.normpath(os.path.join(base_dir, urllib.parse.unquote(url.path)))
        if target is None:
            target = index if where == "index.html" else os.path.join(base_dir, where)
        if not os.path.isfile(target):
            problems.append(f"{where}: {link}: no such file")
        elif check_anchor and url.fragment and target.endswith(".html") and url.fragment not in ids_of(target):
            problems.append(f"{where}: {link}: no anchor '{url.fragment}'")

    # Landing page: every local link and source
    landing = parse(landing_text)
    for link in landing.hrefs + landing.srcs:
        if is_local(link):
            resolve(site, link, "index.html")

    # Root: every other HTML file is a redirect stub to an existing page under api/
    stubs = 0
    verification = re.compile(r"^google[0-9a-f]+\.html$")
    for name in sorted(os.listdir(site)):
        if not name.endswith(".html") or name == "index.html":
            continue
        if verification.match(name):
            content = open(os.path.join(site, name), encoding="utf-8").read().strip()
            if content != "google-site-verification: " + name:
                problems.append(f"{name}: not a Google Search Console verification file for its own name")
            continue
        page = parse(open(os.path.join(site, name), encoding="utf-8").read())
        if not page.refresh or not page.refresh.startswith("api/"):
            problems.append(f"{name}: not a redirect stub to api/")
            continue
        resolve(site, page.refresh, name, check_anchor=False)
        stubs += 1
    if stubs == 0:
        problems.append("no redirect stubs at the root")

    # Doxygen pages: every source (images, scripts, style sheets); the only stubs under api/ are the moved classes'
    api = os.path.join(site, "api")
    pages = [n for n in os.listdir(api) if n.endswith(".html")] if os.path.isdir(api) else []
    if "index.html" not in pages:
        problems.append("no api/index.html")
    moved = dict(moved_class_stubs())
    api_stubs = set()
    images = 0
    for name in sorted(pages):
        page = parse(open(os.path.join(api, name), encoding="utf-8", errors="replace").read())
        if page.refresh:
            api_stubs.add(name)
            if "api/" + name not in moved:
                problems.append(f"api/{name}: a redirect stub, but not the old page of a class in MOVED_CLASSES")
            continue
        for src in page.srcs:
            if is_local(src):
                resolve(api, src, "api/" + name, check_anchor=False)
                images += src.endswith(".png")

    # The old pages of the moved classes: stubs at both published addresses, leading to the namespaced page
    for old, target in moved_class_stubs():
        path = os.path.join(site, old)
        if not os.path.isfile(path):
            problems.append(f"{old}: missing (redirect stub to {target} expected)")
            continue
        page = parse(open(path, encoding="utf-8").read())
        resolved = os.path.relpath(os.path.normpath(os.path.join(os.path.dirname(path), page.refresh or "")), site)
        if not page.refresh or resolved.replace(os.sep, "/") != target:
            problems.append(f"{old}: not a redirect stub to {target} (refresh: {page.refresh})")
        elif not os.path.isfile(os.path.join(site, target)):
            problems.append(f"{old}: {target}: no such file")
        elif target.rsplit("/", 1)[1] in api_stubs:
            problems.append(f"{old}: {target} is itself a redirect stub")

    # Same widgets, captions, images and GIT_TAG as the README
    expected = readme_widgets()
    actual = landing_widgets(landing_text)
    if len(expected) != 12:
        problems.append(f"README.md: {len(expected)} widget rows found, 12 expected")
    if actual != expected:
        missing = [w for w in expected if w not in actual]
        extra = [w for w in actual if w not in expected]
        problems.append(f"index.html: widget grid differs from the README table; missing {missing}, extra {extra}")
    if landing_git_tag(landing_text) != readme_git_tag():
        problems.append(f"index.html: GIT_TAG {landing_git_tag(landing_text)} differs from README.md's "
                        f"{readme_git_tag()}")

    if landing_description(landing_text) != readme_sentence():
        problems.append("index.html: the meta description differs from the README's opening sentence")

    # Sitemap: the landing page and every page under api/, nothing else
    sitemap_path = os.path.join(site, "sitemap.xml")
    locs = []
    if not os.path.isfile(sitemap_path):
        problems.append("no sitemap.xml")
    else:
        locs = re.findall(r"<loc>([^<]+)</loc>", open(sitemap_path, encoding="utf-8").read())
        prefix = "https://paolosereno.github.io/QtIndustrialWidgets/"
        listed = set()
        for loc in locs:
            if not loc.startswith(prefix):
                problems.append(f"sitemap.xml: {loc}: not under {prefix}")
                continue
            rel = loc[len(prefix):]
            listed.add(rel)
            if rel and not rel.startswith("api/"):
                problems.append(f"sitemap.xml: {loc}: not the landing page or a page under api/")
            elif not os.path.isfile(os.path.join(site, rel or "index.html")):
                problems.append(f"sitemap.xml: {loc}: no such file")
        for rel in sorted(listed & {"api/" + n for n in api_stubs}):
            problems.append(f"sitemap.xml: {prefix}{rel}: a redirect stub")
        expected_locs = {""} | {"api/" + n for n in pages if n not in api_stubs}
        for rel in sorted(expected_locs - listed):
            problems.append(f"sitemap.xml: {prefix}{rel} missing")

    print(f"checked: landing page ({len(landing.hrefs)} links, {len(landing.srcs)} sources), {stubs} redirect "
          f"stubs at the root and {len(api_stubs)} under api/, {len(pages) - len(api_stubs)} pages under api/ "
          f"({images} image references), {len(actual)} widgets, {len(locs)} sitemap URLs")
    return problems


def check_url(base):
    if not base.endswith("/"):
        base += "/"
    problems = []
    fetched = {}

    def get(url):
        if url not in fetched:
            try:
                with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "check_site"}),
                                            timeout=60) as r:
                    body = r.read()
                    fetched[url] = (r.status, body.decode("utf-8", "replace") if url.endswith((".html", "/"))
                                    else "")
            except urllib.error.HTTPError as e:
                fetched[url] = (e.code, "")
            except (urllib.error.URLError, TimeoutError) as e:
                fetched[url] = (str(e), "")
        return fetched[url]

    def links_of(url, text, include_hrefs):
        page = parse(text)
        out = []
        for link in (page.hrefs if include_hrefs else []) + page.srcs:
            if is_local(link):
                out.append(urllib.parse.urldefrag(urllib.parse.urljoin(url, link))[0])
        return page, out

    status, text = get(base)
    if status != 200:
        return [f"{base}: {status}"]
    _, linked = links_of(base, text, include_hrefs=True)
    pages = set()
    for url in linked:
        status, body = get(url)
        if status != 200:
            problems.append(f"index.html -> {url}: {status}")
        elif url.endswith(".html"):
            pages.add((url, body))
    # The Doxygen main page and the pages the landing page links to: every source must load
    api_index = urllib.parse.urljoin(base, "api/index.html")
    pages.add((api_index, get(api_index)[1]))
    for url, body in sorted(pages):
        _, srcs = links_of(url, body, include_hrefs=False)
        for src in srcs:
            status, _ = get(src)
            if status != 200:
                problems.append(f"{url} -> {src}: {status}")
    # Redirect stubs: the old address of every page the landing page links to under api/, plus the old guide names
    stub_names = {u.rsplit("/", 1)[1] for u in linked if "/api/" in u and u.endswith(".html")}
    stub_names |= {"md__2home_2runner_2work_2QtIndustrialWidgets_2QtIndustrialWidgets_2docs_2" + n[len("md_docs_2"):]
                   for n in stub_names if n.startswith("md_docs_2")}
    stub_names -= {"index.html"}
    expected_targets = {urllib.parse.urljoin(base, old): urllib.parse.urljoin(base, target)
                        for old, target in moved_class_stubs()}
    for url in sorted({urllib.parse.urljoin(base, n) for n in stub_names} | set(expected_targets)):
        status, body = get(url)
        page = parse(body) if status == 200 else None
        if status != 200 or not page.refresh:
            problems.append(f"{url}: {status}, not a redirect stub")
            continue
        target = urllib.parse.urljoin(url, page.refresh)
        if url in expected_targets and target != expected_targets[url]:
            problems.append(f"{url} -> {target}, expected {expected_targets[url]}")
        if get(target)[0] != 200:
            problems.append(f"{url} -> {target}: {get(target)[0]}")
    print(f"crawled {len(fetched)} URLs under {base} ({len(stub_names) + len(expected_targets)} redirect stubs)")
    return problems


def main(argv):
    if len(argv) == 3 and argv[1] == "--url":
        problems = check_url(argv[2])
    elif len(argv) == 2 and not argv[1].startswith("-"):
        problems = check_files(argv[1])
    else:
        print(__doc__)
        return 2
    for p in problems:
        print("error: " + p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
