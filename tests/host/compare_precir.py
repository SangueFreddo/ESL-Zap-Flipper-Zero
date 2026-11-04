#!/usr/bin/env python3
"""Compares ESL Zap frames with furrtek's PrecIR reference (tools_python/pr.py)."""
import subprocess, sys, os
from PIL import Image, ImageDraw

PRECIR = sys.argv[1]      # path to PrecIR/tools_python
HOST = sys.argv[2]        # host_test binary
WORK = sys.argv[3]
sys.path.insert(0, PRECIR)
import pr  # noqa: E402

def precir_frames(img, barcode, color_mode, page):
    """Same steps as PrecIR img2dm.py, without the transmitter."""
    PLID = pr.get_plid(barcode)
    w, h = img.size
    px = img.load()
    def convert(color_pass):
        out = []
        for y in range(h):
            for x in range(w):
                r, g, b = [c / 255 for c in px[x, y][:3]]
                is_red = r > 0.5 and g < 0.5 and b < 0.5
                is_yellow = r > 0.5 and g > 0.5 and b < 0.5
                if color_pass:
                    out.append(0 if (is_red or is_yellow) else 1)
                elif is_red and color_mode:
                    out.append(1)
                elif is_yellow and color_mode:
                    out.append(0)
                else:
                    luma = 0.21 * r + 0.72 * g + 0.07 * b
                    out.append(0 if luma < 0.5 else 1)
        return out
    pixels = convert(0)
    if color_mode:
        pixels += convert(1)
    comp = []
    def record(n):
        bits = []
        while n:
            bits.insert(0, n & 1); n >>= 1
        comp.extend([0] * (len(bits) - 1)); comp.extend(bits)
    comp.append(pixels[0]); run_px = pixels[0]; run = 1
    for p in pixels[1:]:
        if p == run_px: run += 1
        else: record(run); run = 1; run_px = p
    record(run)  # PrecIR only does this when run > 1; test images end with long runs
    if len(comp) < len(pixels): data, ctype = comp, 2
    else: data, ctype = list(pixels), 0
    pad = 160 - (len(data) % 160)
    data += [0] * pad   # PrecIR always pads (even a full extra frame when already aligned)
    frames = [pr.make_ping_frame(PLID, 0, 400)]
    f = pr.make_mcu_frame(PLID, 0x05)
    pr.append_word(f, len(data) // 8); f.append(0); f.append(ctype); f.append(page)
    for v in (w, h, 0, 0, 0): pr.append_word(f, v)
    f.append(0x88); pr.append_word(f, 0); f.extend([0, 0, 0, 0]); pr.terminate_frame(f, 0, 1)
    frames.append(f)
    i = 0
    for n in range(len(data) // 160):
        f = pr.make_mcu_frame(PLID, 0x20); pr.append_word(f, n)
        for _ in range(20):
            v = 0
            for k in range(8): v = (v << 1) | data[i + k]
            f.append(v); i += 8
        pr.terminate_frame(f, 0, 1); frames.append(f)
    frames.append(pr.make_refresh_frame(PLID, 0))
    return [" ".join("%02X" % b for b in fr[:-2]) for fr in frames]  # drop repeat bytes

def make_image(w, h, red):
    img = Image.new("RGB", (w, h), (255, 255, 255))
    d = ImageDraw.Draw(img)
    d.rectangle([4, 4, w // 2, h // 2], fill=(0, 0, 0))
    d.ellipse([w // 3, h // 4, w - 10, h - 20], outline=(0, 0, 0), width=3)
    for x in range(0, w - 20, 7): d.line([x, h - 12, x + 3, h - 9], fill=(0, 0, 0))
    if red: d.rectangle([w - 60, 6, w - 8, 30], fill=(255, 0, 0))
    return img

ok = True
for (w, h, color, bc) in [(208, 112, 0, "A4012345678913189"), (296, 128, 1, "N4987650001213283"),
                          (152, 152, 1, "B4000017999913391"), (64, 40, 0, "C4655356553513001")]:
    img = make_image(w, h, color)
    path = os.path.join(WORK, "t_%dx%d.bmp" % (w, h))
    img.save(path)
    ref = precir_frames(img, bc, color, 1)
    out = subprocess.run([HOST, "frames", bc, path, str(w), str(h), "1" if color else "0", "1"],
                         capture_output=True, text=True).stdout.strip().splitlines()
    ours = [l.strip() for l in out]
    # PrecIR pads a whole extra frame when already aligned; ESL Zap does not.
    same = ours == ref or (ours[:-1] == ref[:len(ours) - 1] and ref[-1] == ours[-1])
    first_diff = next((i for i, (a, b) in enumerate(zip(ours, ref)) if a != b), None)
    print("%dx%d color=%d frames ours=%d ref=%d -> %s" % (w, h, color, len(ours), len(ref),
          "MATCH" if ours == ref else "DIFF at %s" % first_diff))
    if ours != ref:
        ok = False
        if first_diff is not None:
            print("  ours:", ours[first_diff]); print("  ref :", ref[first_diff])
print("ALL MATCH" if ok else "MISMATCH")

# edge cases: noise (raw is smaller than RLE) and yellow
import random
random.seed(7)
extra_ok = True
for (w, h, color, bc, kind) in [(96, 48, 0, "A4999991234513189", "noise"), (128, 64, 2, "Z4123459999913449", "yellow")]:
    img = Image.new("RGB", (w, h), (255, 255, 255))
    px = img.load()
    for y in range(h - 2):
        for x in range(w):
            if kind == "noise":
                px[x, y] = (0, 0, 0) if random.random() < 0.5 else (255, 255, 255)
            elif (x // 16 + y // 16) % 3 == 0:
                px[x, y] = (255, 255, 0)
            elif (x // 16 + y // 16) % 3 == 1:
                px[x, y] = (0, 0, 0)
    path = os.path.join(WORK, "e_%s.bmp" % kind)
    img.save(path)
    ref = precir_frames(img, bc, 1 if color else 0, 0)
    out = [l.strip() for l in subprocess.run([HOST, "frames", bc, path, str(w), str(h), str(color), "0"],
                                             capture_output=True, text=True).stdout.strip().splitlines()]
    print(kind, "header", out[1][:60], "->", "MATCH" if out == ref else "DIFF")
    extra_ok &= out == ref
print("EDGE MATCH" if extra_ok else "EDGE MISMATCH")
