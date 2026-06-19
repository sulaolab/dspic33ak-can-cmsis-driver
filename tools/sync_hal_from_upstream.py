#!/usr/bin/env python3
"""Synchronize the vendored dsPIC33AK CAN FD HAL from the upstream HAL repo."""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


UPSTREAM_REPO = "https://github.com/sulaolab/dspic33ak-can-hal.git"
UPSTREAM_BRANCH = "main"
UPSTREAM_SOURCE_DIR = "src"
DESTINATION_DIR = "src/hal_can"

HAL_FILES = (
    "dspic33ak_canfd.h",
    "dspic33ak_canfd_reg.h",
    "dspic33ak_canfd_device.c",
    "dspic33ak_canfd_device.h",
    "dspic33ak_canfd_common.c",
    "dspic33ak_canfd_common.h",
    "dspic33ak_canfd_node.c",
    "dspic33ak_canfd_node.h",
    "dspic33ak_canfd_isr.c",
    "dspic33ak_canfd_isr.h",
)


def run(command: list[str], cwd: Path | None = None) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, end="", file=sys.stdout)
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
        raise SystemExit(result.returncode)
    return result.stdout.strip()


def require_repo_root(repo_root: Path) -> None:
    required_paths = (
        repo_root / "README.md",
        repo_root / DESTINATION_DIR / "UPSTREAM.md",
    )
    missing = [str(path.relative_to(repo_root)) for path in required_paths if not path.exists()]
    if missing:
        joined = ", ".join(missing)
        raise SystemExit(f"Run this script from the repository root; missing: {joined}")


def clone_upstream(work_dir: Path) -> tuple[Path, str]:
    upstream_dir = work_dir / "upstream"
    run(
        [
            "git",
            "clone",
            "--depth",
            "1",
            "--branch",
            UPSTREAM_BRANCH,
            UPSTREAM_REPO,
            str(upstream_dir),
        ]
    )
    upstream_commit = run(["git", "rev-parse", "HEAD"], cwd=upstream_dir)
    return upstream_dir, upstream_commit


def copy_hal_files(upstream_dir: Path, repo_root: Path) -> None:
    source_dir = upstream_dir / UPSTREAM_SOURCE_DIR
    destination_dir = repo_root / DESTINATION_DIR
    destination_dir.mkdir(parents=True, exist_ok=True)

    for file_name in HAL_FILES:
        source_path = source_dir / file_name
        if not source_path.is_file():
            raise SystemExit(f"Upstream file not found: {source_path}")
        shutil.copy2(source_path, destination_dir / file_name)


def update_upstream_md(repo_root: Path, upstream_commit: str) -> None:
    upstream_md = repo_root / DESTINATION_DIR / "UPSTREAM.md"
    text = upstream_md.read_text(encoding="utf-8")
    updated, replacements = re.subn(
        r"- Upstream commit: [0-9a-fA-F]+",
        f"- Upstream commit: {upstream_commit}",
        text,
        count=1,
    )
    if replacements != 1:
        raise SystemExit("Could not update upstream commit line in src/hal_can/UPSTREAM.md")
    upstream_md.write_text(updated, encoding="utf-8", newline="\n")


def main() -> int:
    repo_root = Path.cwd().resolve()
    require_repo_root(repo_root)

    with tempfile.TemporaryDirectory(prefix="dspic33ak_can_hal_") as temp_dir:
        upstream_dir, upstream_commit = clone_upstream(Path(temp_dir))
        copy_hal_files(upstream_dir, repo_root)
        update_upstream_md(repo_root, upstream_commit)

    print(
        "Synchronized HAL from "
        f"sulaolab/dspic33ak-can-hal @ {upstream_commit}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
