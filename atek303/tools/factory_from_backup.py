#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Javier (Animatek)
"""Javier's presets as the ATEK303's factory presets: app/factory_presets.h from what an FM-1 saved.

    python3 tools/factory_from_backup.py BACKUP [-o app/factory_presets.h]

BACKUP is either
  - a backup taken over USB with fm1-base's tools/fm1_backup.py (the FM-1 with ATEK303, stopped):
        python3 base/tools/fm1_backup.py backup mis-presets.bak --magic AT64
  - or a 1 MiB flash image: the simulator's "flash_dump FILE", or a whole FM-1 read with fm1_unbrick.py.

What goes in: the four preset banks (OBJ_PRESETS_A..D: 8 presets each, the 32 slots as they are on the FM-1, deleted
ones included as empty) and the settings (OBJ_SETTINGS: the state the FM-1 was in, which a fresh FM-1 starts with).
The firmware takes them only where the store has nothing (a fresh FM-1, or after "wipe"): a unit with presets of its
own keeps them. The layout has to be the firmware's (its SET_MAGIC, AT64): the firmware checks every preset and drops
any from another layout, so rebuild the header after a change of the settings' layout.

Then ./build.sh (and --web): the header is picked up by app.c (__has_include). Delete it to go back to the table of 8
in app.c.
"""
import argparse
import struct
import sys
import zlib
from pathlib import Path

OBJ_SETTINGS, OBJ_PANEL, OBJ_PRESETS_A = 0, 1, 2
NBANKS = 4
MAGIC = b"AT64"
SET_MAGIC = 0x41543634                     # app.c SET_MAGIC: the settings' first word
ST_MAGIC = 0x30583058                      # fm1-base device/storage.c: "X0X0"
STORE_MAP = {OBJ_SETTINGS: 0xFC000, OBJ_PANEL: 0xDC000, 2: 0xD8000, 3: 0xDA000, 4: 0xD4000, 5: 0xD6000}   # app.h


def from_backup(data):
    """fm1_backup.py's file: magic, u16 version, u16 count, u32 flags, u32 CRC; objects (u16 id, u16 len, u32 CRC,
    payload); u32 CRC of all before."""
    if data[:4] != MAGIC:
        raise SystemExit(f"not an ATEK303 backup (magic {data[:4]!r}, wanted {MAGIC!r})")
    if zlib.crc32(data[:-4]) != struct.unpack_from("<I", data, len(data) - 4)[0]:
        raise SystemExit("the backup is damaged (CRC)")
    _, count, _, _ = struct.unpack_from("<HHII", data, 4)
    off, objs = 16, {}
    for _ in range(count):
        oid, n, crc = struct.unpack_from("<HHI", data, off)
        payload = data[off + 8:off + 8 + n]
        if zlib.crc32(payload) != crc:
            raise SystemExit(f"object {oid} damaged (CRC)")
        objs[oid] = payload
        off += 8 + n
    return objs


def from_flash(img):
    """A 1 MiB image: each object's A/B sectors from APP_STORE_MAP, the valid copy with the highest seq."""
    objs = {}
    for oid, base in STORE_MAP.items():
        best = None
        for off in (base, base + 4096):
            magic, typ, slot, seq, n, crc, r0, r1, hcrc = struct.unpack_from("<IHHIIIIII", img, off)
            if magic != ST_MAGIC or typ != oid or n > 3840:
                continue
            if zlib.crc32(img[off:off + 28]) != hcrc:
                continue
            payload = img[off + 256:off + 256 + n]
            if zlib.crc32(payload) != crc:
                continue
            if best is None or seq > best[0]:
                best = (seq, payload)
        if best:
            objs[oid] = best[1]
    return objs


def c_bytes(b, indent="    "):
    lines = []
    for i in range(0, len(b), 24):
        lines.append(indent + ", ".join(f"0x{v:02X}" for v in b[i:i + 24]) + ",")
    return "\n".join(lines) or indent + "0"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("backup")
    ap.add_argument("-o", "--out", default=str(Path(__file__).resolve().parent.parent / "app" / "factory_presets.h"))
    a = ap.parse_args()
    data = Path(a.backup).read_bytes()
    objs = from_flash(data) if len(data) == 1 << 20 else from_backup(data)
    banks = [objs.get(OBJ_PRESETS_A + b, b"") for b in range(NBANKS)]
    if not any(banks):
        raise SystemExit("no presets in it (saved with this firmware? the backup's objects: %s)" % sorted(objs))
    sizes = {len(b) for b in banks if b}
    if len(sizes) != 1:
        raise SystemExit(f"the banks differ in size: {sorted(sizes)}")
    bank_len = sizes.pop()
    preset_len = bank_len // 8
    names = []
    for bi, b in enumerate(banks):
        for k in range(8 if b else 0):
            p = b[k * preset_len:(k + 1) * preset_len]
            used = p[15]
            magic = struct.unpack_from("<I", p, 16)[0]
            if used == 1 and magic == SET_MAGIC:
                names.append(f"{bi * 8 + k + 1:2d} {p[:15].split(b'\0')[0].decode('ascii', 'replace')}")
            elif used == 1:
                print(f"  slot {bi * 8 + k + 1}: another layout (magic {magic:#x}): the firmware will drop it")
    settings = objs.get(OBJ_SETTINGS, b"")
    if settings and struct.unpack_from("<I", settings, 0)[0] != SET_MAGIC:
        print("  the settings are from another layout: a fresh FM-1 will start with the defaults")
        settings = b""
    out = ["/* Generated by tools/factory_from_backup.py from %s: Javier's presets, the ATEK303's factory ones." % Path(a.backup).name,
           " * Do not edit: make the presets on an FM-1, take a backup, run the tool again. */",
           "#define FACTORY_BANKS 1",
           "static const uint32_t FACTORY_BANK_LEN[%d] = {%s};" % (NBANKS, ", ".join(str(len(b)) for b in banks))]
    out.append("static const uint8_t FACTORY_BANK[%d][%d] = {" % (NBANKS, bank_len))
    for b in banks:
        out.append("  {\n" + c_bytes(b or b"\0") + "\n  },")
    out.append("};")
    out.append("static const uint8_t FACTORY_SETTINGS[%d] = {\n%s\n};" % (max(1, len(settings)), c_bytes(settings or b"\0")))
    Path(a.out).write_text("\n".join(out) + "\n")
    print(f"{a.out}: {len(names)} presets" + (", and the start state" if settings else ""))
    for n in names:
        print("  " + n)


if __name__ == "__main__":
    sys.exit(main())
