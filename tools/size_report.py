#!/usr/bin/env python3
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-mb", type=float, default=15.0)
    parser.add_argument("exe")
    args = parser.parse_args()

    exe = Path(args.exe)
    size = exe.stat().st_size
    size_mb = size / (1024.0 * 1024.0)
    print(f"{exe.name}: {size} bytes ({size_mb:.3f} MB), limit {args.max_mb:.3f} MB")
    if size_mb > args.max_mb:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

