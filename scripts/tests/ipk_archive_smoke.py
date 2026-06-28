#!/usr/bin/env python3
from __future__ import annotations

import gzip
import io
import pathlib
import subprocess
import sys
import tarfile
import tempfile


def extract_member(archive_bytes: bytes, member_name: str) -> bytes:
    with gzip.GzipFile(fileobj=io.BytesIO(archive_bytes), mode="rb") as gz_file:
        with tarfile.open(fileobj=gz_file, mode="r:") as archive:
            member = archive.getmember(member_name)
            extracted = archive.extractfile(member)
            if extracted is None:
                raise AssertionError(f"member is not a file: {member_name}")
            return extracted.read()


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    builder = repo_root / "scripts" / "build-legacy-ipk.py"

    with tempfile.TemporaryDirectory() as temp_dir:
        root = pathlib.Path(temp_dir)
        stage = root / "stage"
        control = root / "control"
        output = root / "test.ipk"

        (stage / "etc").mkdir(parents=True)
        (stage / "usr" / "bin").mkdir(parents=True)
        control.mkdir()

        conf = stage / "etc" / "drcom.conf"
        binary = stage / "usr" / "bin" / "drcom_openwrt"
        postinst = control / "postinst"

        conf.write_text("password='secret'\n", encoding="utf-8")
        binary.write_text("#!/bin/sh\n", encoding="utf-8")
        postinst.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        conf.chmod(0o600)
        binary.chmod(0o755)
        postinst.chmod(0o755)

        subprocess.run(
            [
                sys.executable,
                str(builder),
                "--stage-dir",
                str(stage),
                "--control-dir",
                str(control),
                "--output",
                str(output),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
        )

        with gzip.open(output, "rb") as outer_gz:
            with tarfile.open(fileobj=outer_gz, mode="r:") as outer:
                data_member = outer.extractfile("./data.tar.gz")
                control_member = outer.extractfile("./control.tar.gz")
                if data_member is None or control_member is None:
                    raise AssertionError("ipk archive missing data/control tarballs")
                data_bytes = data_member.read()
                control_bytes = control_member.read()

        with gzip.GzipFile(fileobj=io.BytesIO(data_bytes), mode="rb") as data_gz:
            with tarfile.open(fileobj=data_gz, mode="r:") as data_archive:
                conf_info = data_archive.getmember("./etc/drcom.conf")
                bin_info = data_archive.getmember("./usr/bin/drcom_openwrt")
                if conf_info.mode != 0o600:
                    raise AssertionError(f"drcom.conf mode should be 0600, got {conf_info.mode:o}")
                if bin_info.mode != 0o755:
                    raise AssertionError(f"binary mode should be 0755, got {bin_info.mode:o}")

        postinst_payload = extract_member(control_bytes, "./postinst").decode("utf-8")
        if "default_postinst" in postinst_payload:
            raise AssertionError("postinst should not call default_postinst")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
