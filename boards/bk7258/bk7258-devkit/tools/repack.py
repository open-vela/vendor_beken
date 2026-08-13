#!/usr/bin/env python3
# ============================================================================
# vendor/beken/boards/bk7258/bk7258-devkit/tools/repack.py
#
# Pack the NuttX nuttx.bin into the AP app (app1) of ARMINO's all-app.bin and
# add the Beken 34/32 linear CRC.  Reuses ARMINO's own packer
# (bk_packager_linear_crc) so the CRC/partition layout matches the factory
# exactly.
#
# Prerequisites:
#   1) Build NuttX  -> cmake_out/<board>_nsh/nuttx.bin
#   2) Build ARMINO -> build/bk7258/spi_lcd_example/package/tmp/
#      {bootloader,app}.bin and partitions/bk_package.json
#
# Usage:
#   python3 repack.py [--nuttx-bin <path>]
#
# NUTTX_BIN resolution order (highest priority first):
#   1. --nuttx-bin <path>   command-line argument
#   2. NUTTX_BIN            environment variable
#   3. Auto-detect:         scan cmake_out/*/nuttx.bin, pick newest mtime
#   4. Fallback:            cmake_out/bk7258-devkit_nsh/nuttx.bin
#
# Output:
#   <this dir>/bk_repack_work/all-app-nuttx.bin   (flash this)
#
# NOTE: ARMINO_ROOT / OPENVELA_ROOT below are hard-coded for one machine;
#       adjust them when moving to another environment.
# ============================================================================

import argparse
import os
import shutil
import sys
import time
from pathlib import Path

# ---- Paths (adjust when moving to another machine) ------------------------
ARMINO_ROOT = Path("/home/zhangyan68/miwear-main/vendor/armino/bk_avdk_smp")
OPENVELA_ROOT = Path("/home/zhangyan68/miwear-main/vendor/openvela")

PROJECT_BUILD = ARMINO_ROOT / "build/bk7258/spi_lcd_example"
PKG_TMP = PROJECT_BUILD / "package/tmp"    # bootloader.bin / app.bin / app1.bin
PART_JSON = PROJECT_BUILD / "partitions/bk_package.json"
BK_PY_LIBS = ARMINO_ROOT / "tools/env_tools/bk_py_libs"

NUTTX_BIN_DEFAULT = OPENVELA_ROOT / "cmake_out/bk7258-devkit_nsh/nuttx.bin"

WORKDIR = Path(__file__).resolve().parent / "bk_repack_work"
OUTPUT_NAME = "all-app-nuttx.bin"
STALE_HOURS = 24
# ---------------------------------------------------------------------------


def die(msg):
    print(f"[repack] error: {msg}", file=sys.stderr)
    sys.exit(1)


def find_newest_nuttx_bin():
    """Scan cmake_out/*/nuttx.bin and return the one with newest mtime."""

    cmake_out = OPENVELA_ROOT / "cmake_out"
    if not cmake_out.is_dir():
        return None

    candidates = sorted(
        cmake_out.glob("*/nuttx.bin"),
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )

    if not candidates:
        return None

    if len(candidates) > 1:
        print("[repack] multiple nuttx.bin candidates found:")
        for c in candidates:
            mtime = time.strftime(
                "%Y-%m-%d %H:%M:%S", time.localtime(c.stat().st_mtime)
            )
            print(f"  {c}  (mtime: {mtime})")
        print(f"[repack] selecting newest: {candidates[0]}")

    return candidates[0]


def resolve_nuttx_bin(cli_path):
    """Resolve NUTTX_BIN path using the priority chain."""

    # Priority 1: command-line argument
    if cli_path:
        p = Path(cli_path).resolve()
        if not p.is_file():
            die(f"--nuttx-bin path not found: {p}")
        return p

    # Priority 2: environment variable
    env = os.environ.get("NUTTX_BIN")
    if env:
        p = Path(env).resolve()
        if not p.is_file():
            die(f"NUTTX_BIN env var points to missing file: {p}")
        return p

    # Priority 3: auto-detect newest
    p = find_newest_nuttx_bin()
    if p:
        return p

    # Priority 4: fallback default
    return NUTTX_BIN_DEFAULT


def main():
    parser = argparse.ArgumentParser(
        description="Repack NuttX binary into Beken flashable image"
    )
    parser.add_argument(
        "--nuttx-bin",
        metavar="PATH",
        help="Path to nuttx.bin (overrides env var and auto-detect)",
    )
    args = parser.parse_args()

    # Resolve nuttx.bin
    nuttx_bin = resolve_nuttx_bin(args.nuttx_bin)

    if not nuttx_bin.is_file():
        die(
            f"nuttx.bin not found: {nuttx_bin}\n"
            "  Build NuttX first, or pass --nuttx-bin <path>"
        )

    # Print selected nuttx.bin info with size and mtime
    stat = nuttx_bin.stat()
    mtime_str = time.strftime(
        "%Y-%m-%d %H:%M:%S", time.localtime(stat.st_mtime)
    )
    age_hours = (time.time() - stat.st_mtime) / 3600

    print(f"[repack] NUTTX_BIN = {nuttx_bin}")
    print(f"[repack]   size    = {stat.st_size} bytes")
    print(f"[repack]   mtime   = {mtime_str}")

    # Warn if file is older than STALE_HOURS
    if age_hours > STALE_HOURS:
        print(
            f"\n"
            f"  *** WARNING: nuttx.bin is {age_hours:.0f} hours old "
            f"(>{STALE_HOURS}h) ***\n"
            f"  *** This may be a stale build artifact!          ***\n"
            f"  *** Verify you built with the correct defconfig.  ***\n",
            file=sys.stderr,
        )

    # 1) Check inputs
    for p in (nuttx_bin, PKG_TMP / "bootloader.bin", PKG_TMP / "app.bin",
              PART_JSON):
        if not p.is_file():
            die(f"missing input: {p}\n   (did you rebuild NuttX / ARMINO?)")

    # 2) Prepare workdir: bootloader.bin + app.bin(CP) + app1.bin(=nuttx) + json
    if WORKDIR.exists():
        shutil.rmtree(WORKDIR)
    WORKDIR.mkdir(parents=True)

    shutil.copy2(PKG_TMP / "bootloader.bin", WORKDIR / "bootloader.bin")
    # CPU0 = our NuttX (replaces the ARMINO CP)
    shutil.copy2(nuttx_bin, WORKDIR / "app.bin")
    # Factory AP: no longer started, kept only to preserve the partition
    shutil.copy2(PKG_TMP / "app1.bin", WORKDIR / "app1.bin")
    shutil.copy2(PART_JSON, WORKDIR / "bk_package.json")

    nuttx_size = (WORKDIR / "app.bin").stat().st_size
    print(f"[repack] nuttx.bin -> app.bin (CPU0, {nuttx_size} bytes)")
    print(f"[repack] workdir: {WORKDIR}")

    # 3) Call the ARMINO packer (linear CRC); layout/CRC match the factory
    sys.path.insert(0, str(BK_PY_LIBS))
    try:
        from bk_packager.bk_packager_linear_crc import bk_packager_linear_crc
    except Exception as e:  # noqa: BLE001
        die(f"failed to import bk_packager: {e}\n   check path {BK_PY_LIBS}")

    packer = bk_packager_linear_crc(
        WORKDIR, (WORKDIR / "bk_package.json"), (WORKDIR / OUTPUT_NAME)
    )
    packer.pack()

    out = WORKDIR / OUTPUT_NAME
    if not out.is_file():
        die("packer produced no output file")
    print(f"\n[repack] done -> {out} ({out.stat().st_size} bytes)")
    print("[repack] flash it: bk_loader download -p 0 -b 1500000 -i "
          f"{out}")


if __name__ == "__main__":
    main()
