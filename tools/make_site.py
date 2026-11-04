#!/usr/bin/env python3
"""ESL Zap helper: builds the installer page with every successful build
embedded in it (base64), so it works online, offline and opened from a file.
The .fap files and faps/index.json are copied too, for direct download links.
Copyright (c) 2026 SangueFreddo. Licensed under the MIT License.
Usage: make_site.py BUILDS_DIR WEB_DIR OUT_DIR
BUILDS_DIR holds esl_zap-<id>.fap + esl_zap-<id>.json (see build_meta.py)."""
import base64, glob, json, os, shutil, sys

builds_dir, web_dir, site_dir = sys.argv[1:4]
shutil.rmtree(site_dir, ignore_errors=True)
shutil.copytree(web_dir, site_dir, ignore=shutil.ignore_patterns("faps"))
os.makedirs(os.path.join(site_dir, "faps"), exist_ok=True)

ORDER = ["official-release", "official-rc", "official-dev", "momentum-release", "momentum-dev",
         "unleashed-release", "unleashed-dev", "roguemaster-release", "roguemaster"]
entries = []
for meta_path in glob.glob(os.path.join(builds_dir, "esl_zap-*.json")):
    with open(meta_path) as f:
        meta = json.load(f)
    fap = os.path.join(builds_dir, "esl_zap-%s.fap" % meta["id"])
    if not os.path.exists(fap):
        continue
    shutil.copy(fap, os.path.join(site_dir, "faps"))
    entries.append(meta)
entries.sort(key=lambda e: ORDER.index(e["id"]) if e["id"] in ORDER else 99)

with open(os.path.join(site_dir, "faps", "index.json"), "w") as f:
    json.dump({"builds": entries}, f, indent=1)

embedded = []
for e in entries:
    with open(os.path.join(builds_dir, "esl_zap-%s.fap" % e["id"]), "rb") as f:
        embedded.append(dict(e, data=base64.b64encode(f.read()).decode()))
page = os.path.join(site_dir, "index.html")
html = open(page, encoding="utf-8").read()
marker = '<script id="ez-builds" type="application/json">{"builds":[]}</script>'
assert marker in html, "build data marker missing in index.html"
html = html.replace(marker, '<script id="ez-builds" type="application/json">%s</script>'
                    % json.dumps({"builds": embedded}).replace("</", "<\\/"))
open(page, "w", encoding="utf-8").write(html)
print("site ready with %d builds (%d KB page)" % (len(entries), len(html) // 1024))
for e in entries:
    print("  %-20s %-8s API %s" % (e["id"], e.get("channel", ""), e["api"]))
