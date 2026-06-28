#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import subprocess
import tempfile


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    script = repo_root / "scripts" / "build-openwrt-sdk-ipk.sh"
    script_text = script.read_text(encoding="utf-8")
    makefile_text = (repo_root / "drcom" / "Makefile").read_text(encoding="utf-8")

    if "+luci-lua-runtime" not in makefile_text or "Depends: libc, luci-base, luci-lua-runtime" not in script_text:
        raise AssertionError("missing LuCI Lua runtime dependency")
    if 'chmod 0600 "$STAGE_DIR/etc/drcom.conf"' not in script_text:
        raise AssertionError("release package should install /etc/drcom.conf as 0600")
    if "default_postinst" in script_text or "default_prerm" in script_text:
        raise AssertionError("release package should not auto-enable or auto-start through OpenWrt defaults")
    if "/etc/init.d/$OUTPUT_PKG_NAME restart" in script_text or "/etc/init.d/$OUTPUT_PKG_NAME stop" in script_text:
        raise AssertionError("generated control sidecars should not duplicate init lifecycle actions")

    with tempfile.TemporaryDirectory() as temp_dir:
        payload = pathlib.Path(temp_dir) / "payload.txt"
        payload.write_text("drcom checksum smoke\n", encoding="utf-8")
        expected = subprocess.check_output(["shasum", "-a", "256", str(payload)], text=True).split()[0]
        subprocess.run(["bash", str(script), "--self-test-verify-sha256", expected, str(payload)], check=True)
        subprocess.run(["bash", str(script), "--self-test-host-supported", "Linux", "x86_64"], check=True)
        unsupported = subprocess.run(["bash", str(script), "--self-test-host-supported", "Darwin", "arm64"], text=True, capture_output=True)
        if unsupported.returncode == 0 or "Linux x86_64" not in unsupported.stderr:
            raise AssertionError(unsupported.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
