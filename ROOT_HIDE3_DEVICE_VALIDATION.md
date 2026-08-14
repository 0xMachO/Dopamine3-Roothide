# RootHide 3 Device Validation Matrix

This checklist validates the refactor on a real device before any release. It is not an application-specific bypass guide; it tests the project's own lifecycle and isolation invariants.

## Test target

| Field | Value |
|---|---|
| Primary device | iPhone 11 (A13 / arm64e) |
| Primary system | iOS 18.3.2 |
| Required build identity | Record the Git SHA and TIPA SHA-256 before installation. |
| Recovery prerequisite | Keep a known-good previous TIPA and a reboot/recovery path available. |

## Installation gate

Install only an artifact built from the reviewed SHA. Before enabling tweaks, capture a baseline after a full reboot and after a userspace reboot. Do not test application behavior until the infrastructure checks below have passed.

| Check | Expected result | Failure action |
|---|---|---|
| Bootstrap and userspace reboot complete | No panic, watchdog timeout, or boot loop. | Reboot to stock state; retain logs; do not retry blindly. |
| RootHide Manager mount inspection | No `Unknown Bindfs Mount(s)` created by this project. | Stop test; collect mount snapshot and launchd log. |
| Mount table | No project-created `bindfs` entry for preboot `System`, preboot `usr`, or `/usr/lib`. | Treat as release blocker. |
| Legacy alias | `/var/jb` is absent during ordinary runtime. | Treat as release blocker. |
| Dynamic alias | The expected per-brand `systemhook.dylib.<brand>` alias resolves only when project infrastructure needs it. | Collect alias, brand, and launchd diagnostics. |

## Reboot and update matrix

Run every row with a freshly started system and retain the result with the build SHA.

| Scenario | Infrastructure check | Functional check |
|---|---|---|
| Fresh bootstrap | Alias preparation succeeds before injection is enabled. | Package manager opens; approved tweak injection works. |
| Userspace reboot | No bindfs is reintroduced. | SpringBoard, network, and core Apple services remain stable. |
| Full reboot then re-jailbreak | Hidden root and dynamic alias are recreated idempotently. | No stale legacy alias or crash on first launch. |
| BaseBin / TIPA update | New systemhook artifact is selected; no stale source blocks boot. | Update completes and installed package managers remain usable. |
| Remove jailbreak | Hidden root, application registrations, and runtime state are cleaned safely. | Stock reboot completes normally. |

## Isolation matrix

Choose only applications you are entitled to test. The objective is to verify the project policy, not to alter any third-party application.

| Process class | Expected injection state | Evidence |
|---|---|---|
| RootHide infrastructure | Controlled dynamic alias allowed. | Launchd diagnostic shows published alias and enabled policy. |
| User app not listed in RootHide blacklist | Policy-controlled injection allowed. | Project diagnostic records the dynamic alias exactly once. |
| Blacklisted user app | No project alias or jailbreak-specific environment inherited. | Spawn diagnostic; RootHide Manager status; normal app launch. |
| Helper, app extension, and XPC extension of blacklisted app | Same isolated policy as parent. | Repeat launch and background/foreground cycles. |
| Safe Mode process | No project alias inherited. | Safe mode launch and crash recovery remain stable. |

## Required evidence bundle

For every failed row, collect the build SHA, boot type, timestamp, RootHide Manager warning text, mount table excerpt, relevant `JBLogError` lines, and whether the process was blacklisted. Never publish device identifiers, account data, or application data with the report.

> **Release rule:** A build that restores a project-owned bindfs mount, exposes `/var/jb`, fails the blacklist-extension check, or causes a userspace-reboot stability regression must not be released.
