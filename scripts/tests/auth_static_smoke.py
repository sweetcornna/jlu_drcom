#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re


def assert_contains(text: str, expected: str) -> None:
    if expected not in text:
        raise AssertionError(f"missing expected text: {expected!r}")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\n(?:static\s+)?(?:int|void)\s+{name}\([^)]*\)\s*\{{", source)
    if match is None:
        raise AssertionError(f"function not found: {name}")

    depth = 0
    for index in range(match.end() - 1, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match.end():index]

    raise AssertionError(f"function body not closed: {name}")


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parents[2]
    auth_source = (repo_root / "drcom" / "src" / "auth.c").read_text(encoding="utf-8")

    login_body = function_body(auth_source, "dhcp_login")
    assert_contains(login_body, "sendto(sockfd, login_packet")
    assert_contains(login_body, "drcom_udp_recv_expected_packet")
    if "drcom_udp_send_recv_expected_with_retries" in login_body:
        raise AssertionError("dhcp_login must not retry by resending the login packet")

    logout_helper_body = function_body(auth_source, "logout_before_stop_exit")
    assert_contains(logout_helper_body, "dhcp_logout_challenge")
    assert_contains(logout_helper_body, "dhcp_logout")

    if auth_source.count("logout_before_stop_exit(sockfd, dest_addr, auth_information);") != 2:
        raise AssertionError("expected both logged-in stop exits to logout before closing the socket")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
