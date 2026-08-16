# EXPERIMENTAL — DO NOT INSTALL OR USE

> **This repository is an active development workspace, not a jailbreak release. It is public only to make development and continuous-integration work visible. Do not download, install, build, sideload, or run any artifact from this repository unless you are an authorized maintainer testing on your own device and accepting all risk.**

## Current status: unsafe and incomplete

This fork is under active investigation and is **not stable, verified, supported, or suitable for normal users**. Core launchd/bootstrap and process-injection behavior is being redesigned for newer arm64e iOS versions. A build may boot while the jailbreak remains inactive; Sileo and RootHide Manager may fail to start; other core functionality may be absent or broken.

The code and any workflow artifact may cause application crashes, repeated resprings, failed jailbreak attempts, loss of jailbreak functionality, boot loops, kernel panics, data loss, or a need to restore the device. No compatibility claim is made for any iOS version, device, or configuration.

| Audience | Permitted use |
|---|---|
| General users | **Do not use this repository or its artifacts.** |
| Researchers and contributors | Source review and development discussion only; do not treat commits or Actions artifacts as releases. |
| Authorized maintainers | Controlled testing only, on personally owned test devices, with a current backup and a recovery/restore plan. |

## No release channel

There are **no supported releases** in this repository. GitHub Actions artifacts are transient development outputs, not downloads for end users. Their successful compilation does **not** mean that the resulting TIPA is safe, functional, or compatible with a device.

Do not open support requests for installation, compatibility, failed jailbreak attempts, or data recovery based on this fork. Issues and pull requests may be ignored, closed, or changed without notice while the architecture is under investigation.

## Safety requirements for maintainers

Before any controlled test, maintainers must independently verify the exact source commit, preserve a device backup, understand the applicable recovery path, and avoid using a primary device or data that cannot be restored. Never represent this work as an official Dopamine or RootHide release.

For an end-user jailbreak release, use only the official project channels and releases explicitly designated by their maintainers. This repository is **not affiliated with, endorsed by, or a replacement for an official stable release**.

## Development scope

The present work focuses on diagnosing and safely restructuring RootHide bootstrap and process-injection behavior without reintroducing launchd instability. Until maintainers explicitly replace this warning with a release notice, every branch, commit, workflow run, log, and artifact must be considered **experimental and unsafe**.

---

**By viewing, cloning, building, downloading, or testing this repository, you acknowledge that you are acting at your own risk.**
