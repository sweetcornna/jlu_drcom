#!/usr/bin/env python3
from __future__ import annotations

import pathlib


def assert_contains(text: str, expected: str) -> None:
    if expected not in text:
        raise AssertionError(f"missing expected text: {expected!r}")


def assert_not_contains(text: str, unexpected: str) -> None:
    if unexpected in text:
        raise AssertionError(f"unexpected text present: {unexpected!r}")


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    controller = (repo_root / "drcom" / "files" / "usr" / "lib" / "lua" / "luci" / "controller" / "drcom.lua").read_text(encoding="utf-8")
    view = (repo_root / "drcom" / "files" / "usr" / "lib" / "lua" / "luci" / "view" / "drcom" / "form.htm").read_text(encoding="utf-8")
    sample = (repo_root / "drcom" / "files" / "etc" / "drcom.conf").read_text(encoding="utf-8")

    assert_contains(controller, "invalid_keys = invalid_keys")
    assert_contains(controller, "valid = #missing_keys == 0 and #invalid_keys == 0")
    assert_contains(controller, "Configuration has invalid values")
    assert_contains(controller, 'value == "True" or value == "False" or value == "1" or value == "0"')
    assert_contains(controller, 'fs.chmod(CONF_PATH, 384)')
    assert_contains(view, "keepalive1_mod should be True, False, 1, or 0.")
    assert_contains(sample, "# Try this on older JLU gateways")
    assert_not_contains(sample, "# profile='jlu-legacy'   #")
    assert_not_contains(sample, "# startup_delay_seconds=0  #")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
