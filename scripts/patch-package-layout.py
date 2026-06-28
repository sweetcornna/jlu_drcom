#!/usr/bin/env python3
from __future__ import annotations

import argparse
import pathlib


def replace_required(text: str, old: str, new: str) -> str:
    if old not in text:
        raise ValueError(f"expected text not found: {old}")
    return text.replace(old, new)


def write_text_lf(path: pathlib.Path, text: str) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def patch_init(path: pathlib.Path, package_name: str) -> None:
    text = path.read_text(encoding="utf-8")
    text = replace_required(text, 'SERVICE_BIN="/usr/bin/drcom"', f'SERVICE_BIN="/usr/bin/{package_name}"')
    write_text_lf(path, text)


def patch_controller(path: pathlib.Path, package_name: str) -> None:
    text = path.read_text(encoding="utf-8")
    text = replace_required(text, 'module("luci.controller.drcom", package.seeall)', f'module("luci.controller.{package_name}", package.seeall)')
    text = replace_required(text, 'local INIT_PATH = "/etc/init.d/drcom"', f'local INIT_PATH = "/etc/init.d/{package_name}"')
    text = replace_required(text, 'local SERVICE_NAME = "drcom"', f'local SERVICE_NAME = "{package_name}"')
    text = replace_required(text, 'test -L /etc/rc.d/S90drcom', f'test -L /etc/rc.d/S90{package_name}')
    text = replace_required(text, '"admin", "services", "drcom"', f'"admin", "services", "{package_name}"')
    text = replace_required(text, 'tpl.render("drcom/form"', f'tpl.render("{package_name}/form"')
    write_text_lf(path, text)


def patch_view(path: pathlib.Path, package_name: str) -> None:
    text = path.read_text(encoding="utf-8")
    text = replace_required(text, "jludrcom.language", f"{package_name}.language")
    write_text_lf(path, text)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Patch staged drcom files for the release package name.")
    parser.add_argument("--init", required=True, type=pathlib.Path, help="Path to the staged init script.")
    parser.add_argument("--controller", required=True, type=pathlib.Path, help="Path to the staged LuCI controller.")
    parser.add_argument("--view", required=True, type=pathlib.Path, help="Path to the staged LuCI view.")
    parser.add_argument("--package-name", required=True, help="Installed package, service, and binary name.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    patch_init(args.init, args.package_name)
    patch_controller(args.controller, args.package_name)
    patch_view(args.view, args.package_name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
