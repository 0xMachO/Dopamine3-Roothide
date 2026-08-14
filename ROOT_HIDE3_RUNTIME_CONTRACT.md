# RootHide 3 Runtime Contract

## Scope

This design applies to the RootHide adaptation of Dopamine 3 and specifically to the arm64e / iOS 18 capability tier represented by iPhone 11 on iOS 18.3.2. It does not claim to bypass every application-specific integrity or server-side policy. Its purpose is to remove project-created global filesystem mounts and to ensure that a process marked isolated does not receive the project's injection environment.

## Invariants

| Invariant | Owner | Verification |
|---|---|---|
| Normal runtime creates no project-owned `bindfs` mount. | Bootstrap lifecycle | Mount snapshot contains no integration-created bindfs entry. |
| The systemhook injection path is per-jailbreak and dynamically named. | launchdhook RootHide lifecycle | Spawn policy inserts only the configured dynamic path. |
| The dynamic systemhook alias is prepared before injection is enabled. | `roothide_launchd_postinit` | Alias preparation succeeds before `exec_set_patch(true)`. |
| An isolated application and its helpers/extensions receive no project injection path. | Central spawn policy | Policy trace reports `kSpawnConfigTrust` only. |
| Patched dyld remains private to the hidden root. | BaseBin generation | Trusting uses `JBROOT_PATH("/basebin/.fakelib/dyld")`; no FakeLib mount is activated. |
| Every temporary transition is idempotent over full and userspace reboot. | Lifecycle controller | First-load and reboot paths both prepare or verify the dynamic alias. |

## Lifecycle

1. Bootstrap generates the patched dyld and stores it in the hidden root. It does not overlay `/usr/lib`.
2. launchdhook derives a dynamic systemhook filename from the existing jailbreak brand, moves the private systemhook artifact to that hidden-root filename if needed, and publishes an in-kernel namecache alias inside `/usr/lib` through the existing RootHide `unsandbox` capability.
3. The shared spawn policy reads that configured alias. It uses it only for processes approved for injection and removes it from processes that are isolated or safe-mode constrained.
4. The launchd transition verifies that the alias is available before enabling patching. If alias preparation fails, it leaves injection disabled and records a structured error; it does not fall back to a global `/usr/lib` mount.
5. The userspace reboot path repeats the idempotent alias verification. No state requires a global bind mount to survive the transition.

## Capability rules

| Capability tier | Dynamic alias | Global FakeLib mount | Global preboot protection mount | Fallback |
|---|---:|---:|---:|---|
| iOS 16.4+ including iOS 18 / A13 | Required; `unsandbox2` | Forbidden | Forbidden | Disable injection, retain recovery diagnostics. |
| Older supported tier with `unsandbox1` | Required if verified by tier test | Forbidden | Forbidden | Disable injection if alias cannot be prepared. |
| Unsupported or unverified tier | Not enabled | Forbidden | Forbidden | Preserve stock recovery path and reject hardening mode. |

## Non-goals

The design does not change entitlement validation, modify application code, suppress RootHide Manager warnings, or use application-specific workarounds. It also does not remove the hidden-root implementation itself; it removes the project-created global mount dependency around it.
