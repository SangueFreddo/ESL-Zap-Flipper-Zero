#!/usr/bin/env python3
"""ESL Zap NFC import tests: builds Flipper .nfc files holding label ids and
checks the decoder, plus a cross-check against the community reference."""
import os, random, subprocess, sys

HOST, WORK = sys.argv[1], sys.argv[2]
REF = sys.argv[3] if len(sys.argv) > 3 else None
ALPHA = "MOS8H57iPcvXWalBDxeKAws61mNy0f2RnZJ-tL3b9QpdETFqIu_zgrCUoY4hVjkG"

def check_digit(first16):
    return str(sum(ord(c) for c in first16.upper()) % 10)

def encode_id(barcode):
    digits = "%02d" % (ord(barcode[0]) - 65) + barcode[1:]
    high, low = int(digits[:9]), int(digits[9:])
    grp = lambda v: "".join(ALPHA[(v >> (6 * i)) & 63] for i in range(5))
    return grp(low) + grp(high)

def random_barcode():
    b = random.choice("ABCDEFGHJKLMNPQRSTUVWXYZ") + "4" + "".join(random.choice("0123456789") for _ in range(14))
    return b + check_digit(b)

def nfc_file(uri, prefix_code=4, ndef=True):
    payload = bytes([prefix_code]) + uri.encode()
    record = bytes([0xD1, 1, len(payload)]) + b"U" + payload
    tlv = bytes([0x03, len(record)]) + record + b"\xFE" if ndef else b"\xFE"
    mem = bytearray(4 * 45)
    mem[0:4] = b"\x04\x85\x92\x9A"; mem[12:16] = b"\xE1\x10\x12\x00"
    mem[16:16 + len(tlv)] = tlv
    lines = ["Filetype: Flipper NFC device", "Version: 4", "# Device type can be ISO14443-3A, NTAG/Ultralight...",
             "Device type: NTAG/Ultralight", "UID: 04 85 92 9A 12 34 56", "ATQA: 00 44", "SAK: 00",
             "NTAG/Ultralight type: NTAG213", "Pages total: 45", "Pages read: 45"]
    for p in range(45):
        lines.append("Page %d: %s" % (p, " ".join("%02X" % b for b in mem[p * 4:p * 4 + 4])))
    return "\n".join(lines) + "\n"

def run(*args):
    return subprocess.run([HOST, *args], capture_output=True, text=True).stdout.strip()

ok = True
random.seed(11)
# 1) round trips through complete .nfc files
for i in range(25):
    bc = random_barcode()
    path = os.path.join(WORK, "t%d.nfc" % i)
    open(path, "w").write(nfc_file("example.com/esl/" + encode_id(bc) + ("?s=1" if i % 3 == 0 else "")))
    out = run("nfc", path)
    if "result=0 barcode=%s" % bc not in out:
        ok = False; print("FAIL roundtrip", bc, out)
print("round trips via .nfc files:", "OK" if ok else "FAIL")

# 2) error cases
cases = [
    ("radio tag", nfc_file("nfc.imagotag.com/ABC123"), "result=3"),
    ("no NDEF", nfc_file("", ndef=False), "result=2"),
    ("bad check digit", nfc_file("example.com/" + encode_id((lambda b: b[:16] + str((int(b[16]) + 1) % 10))(random_barcode()))), "result=5"),
    ("not a page dump", "Filetype: Flipper NFC device\nDevice type: Mifare Classic\nBlock 0: 00\n", "result=1"),
]
for name, text, want in cases:
    path = os.path.join(WORK, "e.nfc"); open(path, "w").write(text)
    out = run("nfc", path)
    if want is None:
        good = "result=0" in out or "result=5" in out
    else:
        good = want in out
    print("%-16s -> %s %s" % (name, out, "OK" if good else "FAIL")); ok &= good

# 3) cross-check with the community decoder on random ids
if REF:
    agree = 0
    for _ in range(2000):
        tag = "".join(random.choice(ALPHA) for _ in range(10))
        a = run("nfcid", tag)
        b = subprocess.run([REF, tag], capture_output=True, text=True).stdout.strip()
        a_norm = a if a not in ("BADCHECK",) else "INVALID"
        # reference additionally requires digit 2 == '4'
        if b == "INVALID" and a_norm != "INVALID" and a_norm[1] != "4":
            agree += 1; continue
        if a_norm == b: agree += 1
        else: print("MISMATCH", tag, a, b); ok = False; break
    print("cross-check with community decoder: %d/2000 agree" % agree)
print("ALL NFC TESTS OK" if ok else "NFC TESTS FAILED")
