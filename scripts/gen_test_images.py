#!/usr/bin/env python3
#
# SimVisionSDK - Generate sample stereo image pairs into DataSource/StereoImage.
# Filenames follow {camera_id}_{index}.jpg where camera_id is L or R.
#
# Usage: python3 scripts/gen_test_images.py [count] [outdir]
#
import os
import struct
import sys

def write_ppm_or_raw(path, width, height):
    # Minimal synthetic "image" payload: header + raw RGB bytes.
    # SimVisionSDK transfers raw file bytes; format is illustrative for testing.
    with open(path, "wb") as f:
        f.write(b"SIMRAW1\n".ljust(16, b"\0"))
        f.write(struct.pack("<II", width, height))
        row = bytes([(x & 0xFF) for x in range(width)])
        for _y in range(height):
            f.write(row)

def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 4
    outdir = sys.argv[2] if len(sys.argv) > 2 else "DataSource/StereoImage"
    os.makedirs(outdir, exist_ok=True)
    width, height = 1280, 720
    for i in range(1, count + 1):
        write_ppm_or_raw(os.path.join(outdir, f"L_{i:03d}.jpg"), width, height)
        write_ppm_or_raw(os.path.join(outdir, f"R_{i:03d}.jpg"), width, height)
    print(f"[SimVisionSDK] generated {count} stereo pair(s) in {outdir}")

if __name__ == "__main__":
    main()
