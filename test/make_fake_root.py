#!/usr/bin/env python3
"""Builds a fake PS3 filesystem under the given directory for host tests."""
import os, struct, sys

def sfo(title, tid):
    entries = [("TITLE", title), ("TITLE_ID", tid)]
    keys = b""; data = b""; ents = b""
    for k, v in entries:
        kb = k.encode() + b"\0"; vb = v.encode() + b"\0"
        ents += struct.pack("<HHIII", len(keys), 0x0204, len(vb), len(vb), len(data))
        keys += kb; data += vb
    kt = 20 + 16 * len(entries)
    dt = kt + len(keys)
    return struct.pack("<4sIIII", b"\0PSF", 0x101, kt, dt, len(entries)) + ents + keys + data

def iso(sfo_bytes):
    SECT = 2048
    img = bytearray(SECT * 40)
    def rec(lba, size, flags, name):
        nb = name
        n = 33 + len(nb); n += n % 2
        r = bytearray(n)
        r[0] = n
        struct.pack_into("<I", r, 2, lba); struct.pack_into(">I", r, 6, lba)
        struct.pack_into("<I", r, 10, size); struct.pack_into(">I", r, 14, size)
        r[25] = flags; r[32] = len(nb); r[33:33+len(nb)] = nb
        return bytes(r)
    root_lba, game_lba, sfo_lba = 20, 21, 22
    pvd = bytearray(SECT); pvd[0] = 1; pvd[1:6] = b"CD001"; pvd[6] = 1
    pvd[156:156+34] = rec(root_lba, SECT, 2, b"\0")[:34].ljust(34, b"\0")
    img[16*SECT:17*SECT] = pvd
    img[root_lba*SECT:root_lba*SECT+60] = rec(game_lba, SECT, 2, b"PS3_GAME")
    img[game_lba*SECT:game_lba*SECT+60] = rec(sfo_lba, len(sfo_bytes), 0, b"PARAM.SFO;1")
    img[sfo_lba*SECT:sfo_lba*SECT+len(sfo_bytes)] = sfo_bytes
    return bytes(img)

def w(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "wb").write(data)

root = sys.argv[1]
mm = [("Gran Racer 7", "BLES00001", "GRANRACER"), ("Skyline Run", "BLUS30002", "SKYLINE"),
      ("Dust & Dunes: Extreme Edition of the Long Title Test", "BLJS10003", "DUNES"),
      ("Moon Rally", "BCES00004", "MOON"), ("Neon Drift", "BCUS98005", "NEON"), ("Zed Quest", "BLUS30006", "ZED")]
for t, i, d in mm:
    w(f"{root}/dev_hdd0/GAMES/{d}/PS3_GAME/PARAM.SFO", sfo(t, i))
w(f"{root}/dev_usb000/GAMES/USBGAME/PS3_GAME/PARAM.SFO", sfo("USB Racer", "BLES00099"))
os.makedirs(f"{root}/dev_hdd0/GAMES/NOT_A_GAME", exist_ok=True)
for t, i, f in [("Deep Reef ISO", "BLES20001", "DeepReef.iso"), ("Iron Peak ISO", "BLUS20002", "IronPeak.ISO")]:
    w(f"{root}/dev_hdd0/PS3ISO/{f}", iso(sfo(t, i)))
w(f"{root}/dev_hdd0/PS3ISO/Broken_Title.iso", b"not an iso at all")
for t, i, d in [("Star Harbor", "NPUB30011", "NPUB30011"), ("Reef Runner", "NPEB00012", "NPEB00012")]:
    w(f"{root}/dev_hdd0/game/{d}/PARAM.SFO", sfo(t, i)); w(f"{root}/dev_hdd0/game/{d}/USRDIR/EBOOT.BIN", b"x")
for t, i, d in [("MS5 LUNCHER", "MS5LNCHR1", "CARC00001"), ("Media Player", "MPLAY0001", "MPLAY0001"), ("File Manager", "FMGR00001", "FMGR00001")]:
    w(f"{root}/dev_hdd0/game/{d}/PARAM.SFO", sfo(t, i)); w(f"{root}/dev_hdd0/game/{d}/USRDIR/EBOOT.BIN", b"x")
w(f"{root}/dev_hdd0/game/NPXS10008/PARAM.SFO", sfo("System", "NPXS10008")); w(f"{root}/dev_hdd0/game/NPXS10008/USRDIR/EBOOT.BIN", b"x")
w(f"{root}/dev_hdd0/game/BLUS99999/PARAM.SFO", sfo("DataOnly", "BLUS99999"))
print("fake root ready")
