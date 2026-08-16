#include <spawn.h>
#include "../systemhook/src/common/common.h"
#include "boomerang.h"
#include "crashreporter.h"
#include "update.h"
#include <libjailbreak/util.h>
#include <libjailbreak/roothider.h>
#include <substrate.h>
#include <mach-o/dyld.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <litehook.h>
#include "jbserver/jbserver_local.h"
#include "hookd_provider.h"
extern char **environ;

void abort_with_reason(uint32_t reason_namespace, uint64_t reason_code, const char *reason_string, uint64_t reason_flags);
extern int roothide_launchd_trust_executable(const char* path);
extern int roothide_launchd___posix_spawn_prehook(pid_t *restrict pidp, const char *restrict path, struct _posix_spawn_args_desc *desc, char *const argv[restrict], char *const envp[restrict]);
extern int roothide_launchd___posix_spawn_posthook(pid_t *restrict pidp, const char *restrict path, struct _posix_spawn_args_desc *desc, char *const argv[restrict], char *const envp[restrict]);

extern int systemwide_trust_file_by_path(const char *path);
extern int platform_set_process_debugged(uint64_t pid, bool fullyDebugged);
extern void systemwide_domain_set_enabled(bool enabled);

#define LOG_PROCESS_LAUNCHES 0

extern bool gInEarlyBoot;
extern bool gFreeBootLogoBeforeBackboardd;
void free_boot_logo(void);

void early_boot_done(void)
{
	gInEarlyBoot = false;
}

/*
void ensure_fakelib_mounted(void)
{
	struct statfs fsb;
	if (statfs("/usr/lib", &fsb) != 0) return;
	if (strcmp(fsb.f_mntonname, "/usr/lib") != 0) {
		systemwide_domain_set_enabled(true);

		// The jailbreak server is not reachable at this point in the launchd lifecycle
		// So we need to host our own, just so that jbctl can talk to it
		mach_port_t serverPort = jbserver_local_start();
		jbctl_earlyboot(serverPort, "internal", "fakelib", "mount", NULL);
		jbserver_local_stop();

		// Note down that the jailbreak was hidden
		// So that after the userspace reboot, we can unmount fakelib again
		setenv("DOPAMINE_IS_HIDDEN", "1", true);
	}
}
*/

int __posix_spawn_orig_wrapper(pid_t *restrict pid, const char *restrict path,
					   struct _posix_spawn_args_desc *desc,
					   char *const argv[restrict],
					   char *const envp[restrict])
{
short flags = -1;
if (desc && desc->attrp) {
	posix_spawnattr_t attr = desc->attrp;
	posix_spawnattr_getflags(&attr, &flags);
}
JBLogDebug("launchd spawn path=%s flags=%x", path, flags);
if (argv) for (int i = 0; argv[i]; i++) JBLogDebug("\targs[%d] = %s", i, argv[i]);
if (envp) for (int i = 0; envp[i]; i++) JBLogDebug("\tenvp[%d] = %s", i, envp[i]);

pid_t pidval = 0;
if (!pid) pid = &pidval;

	// we need to disable the crash reporter during the orig call
	// otherwise the child process inherits the exception ports
	// and this would trip jailbreak detections
	int key = crashreporter_pause();	
	/*
	 * GOT-rebind mode deliberately calls the imported symbol itself.  The
	 * launchdhook image is excluded from global rebinding by litehook, so this
	 * resolves to the unmodified libc implementation rather than recursing
	 * through __posix_spawn_hook.  This avoids the PAC-sensitive inline
	 * trampoline that is unsafe in launchd on iOS 18 arm64e.
	 */
	int r = __posix_spawn(pid, path, desc, argv, envp);
	crashreporter_resume(key);

JBLogDebug("__posix_spawn ret=%d pid=%d", r, *pid);

	return r;
}

int __posix_spawn_hook(pid_t *restrict pid, const char *restrict path,
					   struct _posix_spawn_args_desc *desc,
					   char *const argv[restrict],
					   char *const envp[restrict])
{
	if (path) {
		char executablePath[1024];
		uint32_t bufsize = sizeof(executablePath);
		_NSGetExecutablePath(&executablePath[0], &bufsize);
		if (!strcmp(path, executablePath)) {
			// This spawn will perform a userspace reboot...
			// Instead of the ordinary hook, we want to reinsert this dylib
			// This has already been done in envp so we only need to call the original posix_spawn

			// We are back in "early boot" for the remainder of this launchd instance
			// Mainly so we don't lock up while spawning boomerang
			gInEarlyBoot = true;

			hookd_provider_teardown();

			// If the jailbreak is currently hidden, fakelib is not mounted
			// It needs to be mounted to regain launchd code execution after the userspace reboot
//			ensure_fakelib_mounted();

#if LOG_PROCESS_LAUNCHES
			FILE *f = fopen("/var/mobile/launch_log.txt", "a");
			fprintf(f, "==== USERSPACE REBOOT ====\n");
			fclose(f);
#endif

			// Before the userspace reboot, we want to stash the primitives into boomerang
			boomerang_stashPrimitives();

			// Fix Xcode debugging being broken after the userspace reboot
			unmount("/Developer", MNT_FORCE);

			// If there is a pending jailbreak update, apply it now
			const char *stagedJailbreakUpdate = getenv("STAGED_JAILBREAK_UPDATE");
			if (stagedJailbreakUpdate) {
				int r = jbupdate_basebin(stagedJailbreakUpdate);
				if (r != 0) {
					char msg[1000];
					snprintf(msg, 1000, "Failed updating basebin (error %d).", r);
					abort_with_reason(7, 1, msg, 0);
				}
				unsetenv("STAGED_JAILBREAK_UPDATE");
			}

			// Always use environ instead of envp, as boomerang_stashPrimitives calls setenv
			// setenv / unsetenv can sometimes cause environ to get reallocated
			// In that case envp may point to garbage or be empty
			// Say goodbye to this process
			return __posix_spawn_orig_wrapper(pid, path, desc, argv, environ);
		}
	}

#if LOG_PROCESS_LAUNCHES
	if (path) {
		FILE *f = fopen("/var/mobile/launch_log.txt", "a");
		fprintf(f, "%s", path);
		int ai = 0;
		while (argv) {
			if (argv[ai]) {
				if (ai >= 1) {
					fprintf(f, " %s", argv[ai]);
				}
				ai++;
			}
			else {
				break;
			}
		}
		fprintf(f, "\n");
		fclose(f);

		// if (!strcmp(path, "/usr/libexec/xpcproxy")) {
		// 	const char *tmpBlacklist[] = {
		// 		"com.apple.logd"
		// 	};
		// 	size_t blacklistCount = sizeof(tmpBlacklist) / sizeof(tmpBlacklist[0]);
		// 	for (size_t i = 0; i < blacklistCount; i++)
		// 	{
		// 		if (!strcmp(tmpBlacklist[i], firstArg)) {
		// 			FILE *f = fopen("/var/mobile/launch_log.txt", "a");
		// 			fprintf(f, "blocked injection %s\n", firstArg);
		// 			fclose(f);
		// 			return __posix_spawn_orig_wrapper(pid, path, file_actions, desc, envp);
		// 		}
		// 	}
		// }
	}
#endif

	// We can't support injection into processes that get spawned before the launchd XPC server is up
	// (Technically we could but there is little reason to, since it requires additional work)
	if (gInEarlyBoot) {
		if (!strcmp(path, "/usr/libexec/xpcproxy")) {
			// The spawned process being xpcproxy indicates that the launchd XPC server is up
			// All processes spawned including this one should be injected into
			early_boot_done();
		}
		else {
			return __posix_spawn_orig_wrapper(pid, path, desc, argv, envp);
		}
	}

	// If we're drawing a boot logo, free up it's resources before backboardd starts
	if (gFreeBootLogoBeforeBackboardd) {
		if (!strcmp(path, "/usr/libexec/xpcproxy")) {
			if (argv[0]) {
				if (argv[1]) {
					if (!strcmp(argv[1], "com.apple.backboardd\n")) {
						free_boot_logo();
						gFreeBootLogoBeforeBackboardd = false;
					}
				}
			}
		}
	}

	return posix_spawn_hook_shared(pid, path, desc, argv, envp, __posix_spawn_orig_wrapper, systemwide_trust_file_by_path, platform_set_process_debugged, jbsetting(jetsamMultiplier));
}

/*
 * iOS 18 arm64e safety gate.
 *
 * The arm64e launchdhook artifact contains a PAC return-address guard in
 * roothide_launchd___posix_spawn_prehook which traps with brk #0xc471 when
 * an inline trampoline does not preserve the authenticated LR. launchd is
 * PID 1, so a trap here escalates into an initproc kernel panic. Until the
 * trampoline is independently validated for this ABI, do not install an
 * inline hook in launchd on iOS 18 arm64e. Use the authenticated GOT path
 * above instead, and retain inline mode only for older/other targets.
 */
static bool spawn_rebind_filter(const mach_header_u *header)
{
	/*
	 * The global API walks every loaded image.  launchd's own call sites are
	 * in the main executable, so do not fault in or modify unrelated shared
	 * cache GOTs.  The replacement image is already excluded internally by
	 * litehook.
	 */
	return header == (const mach_header_u *)_dyld_get_image_header(0);
}

void initSpawnHooks(void)
{
#if defined(__arm64e__)
	if (__builtin_available(iOS 18.0, *)) {
		/*
		 * Do not patch launchd text on iOS 18 arm64e.  The old inline hook
		 * corrupts the authenticated return address and trips launchd's PAC
		 * guard, which escalates into an initproc panic.  Current litehook
		 * handles __auth_got entries and signs replacements for the slot, so
		 * rebind only the imported function pointer instead.
		 */
		JBLogDebug("launchd: installing PAC-safe __posix_spawn GOT rebind on iOS 18 arm64e");
		litehook_rebind_symbol(LITEHOOK_REBIND_GLOBAL, (void *)__posix_spawn, (void *)__posix_spawn_hook, spawn_rebind_filter);
		return;
	}
#endif
	litehook_hook_function(__posix_spawn, __posix_spawn_hook);
}
