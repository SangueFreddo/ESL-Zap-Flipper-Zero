#!/usr/bin/env python3
"""ESL Zap CI helper: writes the metadata of one build next to its .fap.
Copyright (c) 2026 SangueFreddo. Licensed under the MIT License."""
import json, os, sys

out_dir, build_id, label, fork, api = sys.argv[1:6]
channel = sys.argv[6] if len(sys.argv) > 6 else "release"
api = api.strip() or "0.0"
meta = {"id": build_id, "label": label, "fork": fork, "api": api, "channel": channel,
        "file": "faps/esl_zap-%s.fap" % build_id}
with open(os.path.join(out_dir, "esl_zap-%s.json" % build_id), "w") as f:
    json.dump(meta, f)
print(meta)
