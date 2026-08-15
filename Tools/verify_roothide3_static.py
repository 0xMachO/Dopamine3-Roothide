#!/usr/bin/env python3
"""Static release checks for the RootHide 3 runtime contract.

This check is intentionally source-only: it validates invariants that must hold
before a device build is installed. It never edits the working tree.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def main() -> int:
    failures: list[str] = []

    jbctl = read("BaseBin/jbctl/src/internal.m")
    jailbreaker = read("Application/Dopamine/Jailbreak/DOJailbreaker.m")
    launchd = read("BaseBin/launchdhook/src/roothider.m")
    common = read("BaseBin/systemhook/src/common/common.c")
    dyldhook = read("BaseBin/dyldhook/src/main.c")
    env_manager = read("Application/Dopamine/Jailbreak/DOEnvironmentManager.m")
    watchdog_makefile = read("BaseBin/watchdoghook/Makefile")
    environment_manager = read("Application/Dopamine/Jailbreak/DOEnvironmentManager.m")

    require("mount_unsandboxed(\"bindfs\"" not in jbctl,
            "jbctl still contains a bindfs mount operation", failures)
    require("setFakelibMounted" not in jailbreaker and "setPrivatePrebootProtected" not in jailbreaker,
            "Dopamine jailbreak flow still activates a global mount lifecycle", failures)
    require("/usr/lib/systemhook.dylib\"" not in common,
            "shared spawn policy still contains the fixed systemhook path", failures)
    require("systemhook.dylib.%016llX" in launchd and "unsandbox(\"/usr/lib\"" in launchd,
            "launchd does not publish a dynamic RootHide systemhook alias", failures)
    require("if(firstLoad)" in launchd and "exec_set_patch(true);" in launchd
            and "skipping developer-mode sysctl mutation on iOS 18+" in launchd
            and "else {\n\t\t// Only after userspace reboot is it safe to stage and publish the\n\t\t// per-jailbreak alias. Keep injection disabled on any failure.\n\t\tbool systemhookReady = prepare_dynamic_systemhook_alias();" in launchd,
            "launchd does not preserve firstLoad trust or safely defer the dynamic alias", failures)
    require("systemhook_strip_injection(&envc)" in launchd,
            "launchd blacklist path does not strip inherited injection", failures)
    require("systemhook_strip_injection(&envc)" in common,
            "shared spawn policy does not strip injection for isolated processes", failures)
    require("/systemhook.dylib." in dyldhook,
            "dyldhook does not require the dynamic RootHide alias", failures)
    require("/usr/lib/systemhook.dylib\"" not in env_manager,
            "environment manager still probes a fixed systemhook path", failures)
    require("/var/jb/Library/Frameworks" not in watchdog_makefile,
            "watchdoghook retains a legacy /var/jb RPATH", failures)
    require("posix_spawn(&pid, argBuf[0], &act, &attr" in environment_manager
            and "posix_spawnattr_destroy(&attr);" in environment_manager
            and "return cmd_wait_for_exit(pid);" in environment_manager,
            "spawnJbctlAsRootWithArgs may race Bootstrap finalization", failures)

    # Block accidental reintroduction in the production sources. Dynamic aliases
    # deliberately include a suffix and are allowed by this expression.
    fixed_path = re.compile(r"/usr/lib/systemhook\.dylib(?!\.)")
    for base in (ROOT / "Application", ROOT / "BaseBin"):
        for path in base.rglob("*"):
            if path.suffix not in {".c", ".h", ".m", ".mm", ".x", ".xm", ".S"}:
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                stripped = line.strip()
                if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                    continue
                if fixed_path.search(line):
                    failures.append(f"fixed systemhook path in {path.relative_to(ROOT)}:{number}")

    if failures:
        print("RootHide 3 static verification failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("RootHide 3 static verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
