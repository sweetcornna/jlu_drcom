#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


def write(path: pathlib.Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")


def assert_contains(text: str, expected: str) -> None:
    if expected not in text:
        raise AssertionError(f"missing expected text: {expected!r}\n{text}")


def assert_not_contains(text: str, unexpected: str) -> None:
    if unexpected in text:
        raise AssertionError(f"unexpected text present: {unexpected!r}\n{text}")


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]

    with tempfile.TemporaryDirectory() as temp_dir:
        root = pathlib.Path(temp_dir)
        init_path = root / "init"
        controller_path = root / "controller.lua"
        view_path = root / "form.htm"

        write(
            init_path,
            '\n'.join(
                [
                    'CONF_PATH="/etc/drcom.conf"',
                    'LOG_PATH="/tmp/drcom.log"',
                    'PORT_STATE_PATH="/tmp/drcom-port-state"',
                    'SERVICE_BIN="/usr/bin/drcom"',
                    'echo "[drcom-init]"',
                    "",
                ]
            ),
        )
        write(
            controller_path,
            '\n'.join(
                [
                    'module("luci.controller.drcom", package.seeall)',
                    'local CONF_PATH = "/etc/drcom.conf"',
                    'local INIT_PATH = "/etc/init.d/drcom"',
                    'local LOG_PATH = "/tmp/drcom.log"',
                    'local SERVICE_NAME = "drcom"',
                    'local PORT_STATE_PATH = "/tmp/drcom-port-state"',
                    'local enabled = shell_ok("test -L /etc/rc.d/S90drcom")',
                    'local hint = "dogcom-style syntax stays literal"',
                    'entry({"admin", "services", "drcom"}, call("render_form"), translate("DrCOM"), 10)',
                    'entry({"admin", "services", "drcom", "status"}, call("status_json")).leaf = true',
                    'local status_url = disp.build_url("admin", "services", "drcom", "status")',
                    'tpl.render("drcom/form", {})',
                    "",
                ]
            ),
        )
        write(view_path, "<script>var key = 'jludrcom.language';</script>\n")

        subprocess.run(
            [
                sys.executable,
                str(repo_root / "scripts" / "patch-package-layout.py"),
                "--init",
                str(init_path),
                "--controller",
                str(controller_path),
                "--view",
                str(view_path),
                "--package-name",
                "drcom_openwrt",
            ],
            check=True,
        )

        init_text = init_path.read_text(encoding="utf-8")
        controller_text = controller_path.read_text(encoding="utf-8")
        view_text = view_path.read_text(encoding="utf-8")

    assert_contains(init_text, 'CONF_PATH="/etc/drcom.conf"')
    assert_contains(init_text, 'LOG_PATH="/tmp/drcom.log"')
    assert_contains(init_text, 'PORT_STATE_PATH="/tmp/drcom-port-state"')
    assert_contains(init_text, 'SERVICE_BIN="/usr/bin/drcom_openwrt"')

    assert_contains(controller_text, 'module("luci.controller.drcom_openwrt", package.seeall)')
    assert_contains(controller_text, 'local CONF_PATH = "/etc/drcom.conf"')
    assert_contains(controller_text, 'local INIT_PATH = "/etc/init.d/drcom_openwrt"')
    assert_contains(controller_text, 'local LOG_PATH = "/tmp/drcom.log"')
    assert_contains(controller_text, 'local SERVICE_NAME = "drcom_openwrt"')
    assert_contains(controller_text, 'local PORT_STATE_PATH = "/tmp/drcom-port-state"')
    assert_contains(controller_text, 'test -L /etc/rc.d/S90drcom_openwrt')
    assert_contains(controller_text, 'entry({"admin", "services", "drcom_openwrt"}, call("render_form"), translate("DrCOM"), 10)')
    assert_contains(controller_text, 'entry({"admin", "services", "drcom_openwrt", "status"}, call("status_json")).leaf = true')
    assert_contains(controller_text, 'local status_url = disp.build_url("admin", "services", "drcom_openwrt", "status")')
    assert_contains(controller_text, 'tpl.render("drcom_openwrt/form", {})')
    assert_contains(controller_text, 'dogcom-style syntax stays literal')
    assert_not_contains(controller_text, "dogcom_openwrt-style")
    assert_not_contains(controller_text, '{"admin", "services", "drcom"}')
    assert_not_contains(controller_text, 'tpl.render("drcom/form", {})')

    assert_contains(view_text, "drcom_openwrt.language")
    assert_not_contains(view_text, "jludrcom.language")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
