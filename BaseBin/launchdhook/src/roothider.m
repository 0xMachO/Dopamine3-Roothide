#import <Foundation/Foundation.h>

#include <spawn.h>
#include <signal.h>
#include <sys/sysctl.h>

#include <libjailbreak/libjailbreak.h>
#include <libjailbreak/roothider.h>

#include "../systemhook/src/common/common.h"
#include "../systemhook/src/common/envbuf.h"
#include "hookd_provider.h"
#include <litehook.h>

#define POSIX_SPAWN_PROC_TYPE_DRIVER 0x700
extern int posix_spawnattr_getprocesstype_np(const posix_spawnattr_t *__restrict, int *__restrict) __API_AVAILABLE(macos(10.8), ios(6.0));

//from launchdhook/spawn_hook.c
extern int systemwide_trust_file_by_path(const char *path);
extern int platform_set_process_debugged(uint64_t pid, bool fullyDebugged);
extern int __posix_spawn_hook(pid_t *restrict pid, const char *restrict path, struct _posix_spawn_args_desc *desc, char *const argv[restrict], char *const envp[restrict]);
extern int __posix_spawn_orig_wrapper(pid_t *restrict pid, const char *restrict path, struct _posix_spawn_args_desc *desc, char *const argv[restrict], char *const envp[restrict]);

//from launchdhook/src/jbserver/jbdomain_dopamine.c (roothide actions)
int roothide_trust_executable_recurse(const char *executablePath, const char *processWorkingDir, xpc_object_t preferredArchsArray);

//from libjailbreak/src/roothider/jailbreakd.c — the hookd spawned by initJailbreakd,
//synced into hookd_provider's globals so no second hookd is lazily spawned
extern pid_t gSpawnedHookdPid;
extern mach_port_t gSpawnedHookdPort;

//from systemhook/src/roothider_common.c (compiled into launchdhook)
bool isRemovableBundlePath(const char* path);
bool hasTrollstoreMarker(const char* path);
int __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, const void *newp, size_t newlen);
int __sysctl_hook(int *name, u_int namelen, void *oldp, size_t *oldlenp, const void *newp, size_t newlen);
int __sysctlbyname(const char *name, size_t namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);
int __sysctlbyname_hook(const char *name, size_t namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen);

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
int (*orig_bind)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int new_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (addr->sa_family == AF_INET && addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in addr_in = *(struct sockaddr_in*)addr;
        in_port_t port = ntohs(addr_in.sin_port);
        if (port == 0) {
			int ret = -1;
			for(port=IPPORT_HIFIRSTAUTO; port<=IPPORT_HILASTAUTO; port++)
			{
				addr_in.sin_port = htons(port);
				ret = orig_bind(sockfd, (struct sockaddr*)&addr_in, addrlen);
				if(ret==0 || errno!=EADDRINUSE) {
					break;
				}
			}
			return ret;
        }
    } else if (addr->sa_family == AF_INET6 && addrlen >= sizeof(struct sockaddr_in6)) {
        struct sockaddr_in6 addr_in6 = *(struct sockaddr_in6*)addr;
        in_port_t port = ntohs(addr_in6.sin6_port);
        if (port == 0) {
			int ret = -1;
			for(port=IPPORT_HIFIRSTAUTO; port<=IPPORT_HILASTAUTO; port++)
			{
				addr_in6.sin6_port = htons(port);
				ret = orig_bind(sockfd, (struct sockaddr*)&addr_in6, addrlen);
				if(ret==0 || errno!=EADDRINUSE) {
					break;
				}
			}
			return ret;
        }
    }
    return orig_bind(sockfd, addr, addrlen);
}

//new_/orig_ symbols are defined in libjailbreak/src/roothider/xpc_hook.m and exported by libjailbreak
extern xpc_object_t (*orig_xpc_dictionary_create_reply)(xpc_object_t original);
extern xpc_object_t new_xpc_dictionary_create_reply(xpc_object_t original);
extern int (*orig_xpc_pipe_routine_reply)(xpc_object_t reply);
extern int new_xpc_pipe_routine_reply(xpc_object_t reply);

void roothide_launchd_preinit()
{
	JBLogDebug("roothide_launchd_preinit");

#ifdef ENABLE_LOGS
	enableJBDLog(JBLogDebugFunction, JBLogErrorFunction);
#endif

	exec_set_patch(false);
}

void roothide_launchd_postinit(bool firstLoad)
{
	JBLogDebug("roothide_launchd_postinit: firstLoad=%d", firstLoad);

	launchdhookFirstLoad = firstLoad;

	exec_set_patch(true);

	if(firstLoad)
	{
		if (__builtin_available(iOS 16.0, *))
		{
			hideDeveloperMode();
		}
#ifdef __arm64e__
			if (!__builtin_available(iOS 16.0, *))
			{
				if(roothide_config_set_spinlock_fix(dyld_patch_enabled()) != 0) {
					/* Non-fatal: log and continue degraded. (iOS 15 arm64e path only,
					 * not reached on the A13/iOS 18.3.2 target.) */
					JBLogError("roothide_config_set_spinlock_fix failed (continuing degraded)");
				}
			}
#endif
	}

	if (__builtin_available(iOS 16.0, *))
	{
		litehook_hook_function(__sysctl, __sysctl_hook);
		litehook_hook_function(__sysctlbyname, __sysctlbyname_hook);
		orig_bind = bind;
		litehook_rebind_symbol(LITEHOOK_REBIND_GLOBAL, (void *)bind, (void *)new_bind, NULL); //fix network issues on iOS16+
	}
#ifdef __arm64e__
	else 
	{
		// iOS15 arm64e only
		// MSHookFunction(sysctlbyname, (void *)sysctlbyname_hook, (void **)&sysctlbyname_orig);
	}
#endif

	if(!firstLoad)
	{
		int ret = ensure_dyld_trustcache(JBROOT_PATH("/basebin/.fakelib/dyld"));
		if (ret != 0) {
			/* Non-fatal: a missing dyld trustcache degrades dyld replacement, but
			 * panicking here converts a fixable state into a bootloop. */
			JBLogError("ensure dyld trustcache failed: %d (continuing degraded)", ret);
		}
	}

	loadAppStoredIdentifiers();

	// The SDK's xpc_dictionary_create_reply carries XPC_RETURNS_RETAINED +
	// nullability attributes; cast to our plain pointer type (clang 16+ treats
	// the mismatch as an error).
	orig_xpc_dictionary_create_reply = (typeof(orig_xpc_dictionary_create_reply))xpc_dictionary_create_reply;
	litehook_rebind_symbol(LITEHOOK_REBIND_GLOBAL, (void *)xpc_dictionary_create_reply, (void *)new_xpc_dictionary_create_reply, NULL);
	orig_xpc_pipe_routine_reply = xpc_pipe_routine_reply;
	litehook_rebind_symbol(LITEHOOK_REBIND_GLOBAL, (void *)xpc_pipe_routine_reply, (void *)new_xpc_pipe_routine_reply, NULL);

	// load the daemon after applying hooks.
	// 3.x-native: initJailbreakd spawns /basebin/hookd (there is no jailbreakd on 3.x;
	// the roothide spawn-patch / spinlock-fix / exec-trace services are no-ops since
	// injection is env-based: systemhook via DYLD_INSERT_LIBRARIES, dyldhook via the
	// .fakelib dyld replacement). The launchdhook xpc_hook hosts the jbserver, so no
	// XPC server daemon is needed. A failing spawn must not take launchd down (that
	// would boot-loop the device); degrade gracefully.
	// On iOS 19+/26+ litehook routes memory hooks through hookd (hookd_provider); hand
	// the hookd we just spawned over so it reuses this instance instead of spawning a
	// second one lazily.
	// Only hand the spawned hookd over when its checkin delivered a valid server
	// port; otherwise keep hookd_provider's lazy respawn as fallback (a NULL port
	// would make launchd_hookd_send_msg skip the respawn and then send to NULL).
	if(initJailbreakd(firstLoad) == 0 && MACH_PORT_VALID(gSpawnedHookdPort)) {
		gHookdPid = gSpawnedHookdPid;
		gHookdPort = gSpawnedHookdPort;
	} else {
		JBLogError("initJailbreakd failed, continuing without hookd");
	}
}

#include <dlfcn.h>
#include <IOKit/IOKitLib.h>
void fix__iosConnect()
{
	// Make sure the image is actually loaded, otherwise its globals aren't accessible
	void *image = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_NOW);
	JBLogDebug("IOSurface image=%p\n", image);
	if (!image) {
		// Non-fatal: this runs inside launchd (spawn prehook); an assert here
		// would abort pid 1 and panic the device.
		JBLogError("fix__iosConnect: IOSurface load failed, skipping");
		return;
	}

	io_service_t* __iosService = litehook_find_dsc_symbol("/System/Library/Frameworks/IOSurface.framework/IOSurface", "__iosService");
	io_connect_t* __iosConnect = litehook_find_dsc_symbol("/System/Library/Frameworks/IOSurface.framework/IOSurface", "__iosConnect");
	if(!__iosService || !__iosConnect) {
		// The dsc/.symbols lookup is best-effort (it reads the shared cache from disk);
		// failing to find the IOSurface globals must not take launchd down.
		JBLogError("fix__iosConnect: unable to locate IOSurface globals in dsc");
		return;
	}

	JBLogDebug("__iosService=%p __iosConnect=%p\n", __iosService, __iosConnect);
	JBLogDebug("*__iosService=%d *__iosConnect=%d\n", *__iosService, *__iosConnect);

	kern_return_t (*IOServiceClose)(io_connect_t connect);
	kern_return_t (*IOServiceOpen)(io_service_t service, task_port_t owningTask, uint32_t type, io_connect_t* connect);

	*(void **)&IOServiceOpen = dlsym(RTLD_DEFAULT, "IOServiceOpen");
	*(void **)&IOServiceClose = dlsym(RTLD_DEFAULT, "IOServiceClose");
	if (!IOServiceOpen || !IOServiceClose) {
		JBLogError("fix__iosConnect: IOService symbols unavailable, skipping");
		return;
	}
    
    io_connect_t old__iosConnect = *__iosConnect;

    if(old__iosConnect) {

        if(*__iosService == 0) {
            JBLogError("fix__iosConnect: __iosService invalid, skipping");
            return;
        }

        kern_return_t kr = IOServiceOpen(*__iosService, mach_task_self(), 0, __iosConnect);
        JBLogDebug("IOServiceOpen kr=%x, new iosConnect=%d\n", kr, *__iosConnect);
        if (kr != KERN_SUCCESS) {
            // Close the saved old connection so the failure path does not leak it.
            JBLogError("fix__iosConnect: IOServiceOpen failed %x, closing old connect", kr);
            IOServiceClose(old__iosConnect);
            return;
        }

        kr = IOServiceClose(old__iosConnect);
        if (kr != KERN_SUCCESS) {
            JBLogError("fix__iosConnect: IOServiceClose failed %x", kr);
        }
    }
}

int roothide_launchd_trust_executable(const char* path)
{
	return dyld_patch_enabled() ? systemwide_trust_file_by_path(path) : roothide_trust_executable_recurse(path, "/", NULL);
}

int roothide_launchd___posix_spawn_posthook(pid_t *restrict pidp, const char *restrict path, struct _posix_spawn_args_desc *desc, char *const argv[restrict], char *const envp[restrict])
{
	//spawn_prehook ensure this is always available
	posix_spawnattr_t *attrp = &desc->attrp;

	short flags = 0;
	posix_spawnattr_getflags(attrp, &flags);

	int proctype = 0;
	posix_spawnattr_getprocesstype_np(attrp, &proctype);

	bool should_suspend = (proctype != POSIX_SPAWN_PROC_TYPE_DRIVER);
	bool should_resume = should_suspend && (flags & POSIX_SPAWN_START_SUSPENDED)==0;

	if (should_suspend) {
		posix_spawnattr_setflags(attrp, flags | POSIX_SPAWN_START_SUSPENDED);
	}

	// on some devices dyldhook may fail due to vm_protect(VM_PROT_READ|VM_PROT_WRITE), 2, (os/kern) protection failure in dsc::__DATA_CONST:__const, 
	// so we need to disable dyld-in-cache here. (or we can use VM_PROT_READ|VM_PROT_WRITE|VM_PROT_COPY)
	char **envc = envbuf_mutcopy((const char **)envp);
	if(envbuf_getenv(envc, "DYLD_INSERT_LIBRARIES")) {
		envbuf_setenv(&envc, "DYLD_IN_CACHE", "0");
	}

#ifdef __arm64e__
	if (!__builtin_available(iOS 16.0, *))
	{
		if(!dyld_patch_enabled() && process_force_dyld_patch(path, argv)) {
			envbuf_setenv(&envc, "SPINLOCK_FIX_DISABLED", "1");
		}
	}
#endif

	int pid = 0;
	int ret = __posix_spawn_orig_wrapper(&pid, path, desc, argv, envc);
	if(pidp) *pidp = pid;

	envbuf_free(envc);
	
	posix_spawnattr_setflags(attrp, flags); // maybe caller will use it again?

	if (ret == 0 && pid > 0) {
		if(should_suspend) {
			// 3.x-native: jbdSpawnPatchChild is a no-op that always resumes the child.
			// The kill-on-failure branch can never be reached and must not be revived
			// (it would SIGKILL arbitrary spawned processes from inside launchd).
			(void)jbdSpawnPatchChild(pid, should_resume);
		}
	} else {
		JBLogError("spawn failed: %d %s, pid=%d", ret, strerror(ret), pid);
	}

	return ret;
}

int roothide_launchd___posix_spawn__spinlock_fix_only(pid_t *restrict pidp, const char *restrict path, struct _posix_spawn_args_desc *desc, char *const argv[restrict], char *const envp[restrict])
{
	//spawn_prehook ensure this is always available
	posix_spawnattr_t *attrp = &desc->attrp;

	short flags = 0;
	posix_spawnattr_getflags(attrp, &flags);

	bool should_resume = (flags & POSIX_SPAWN_START_SUSPENDED)==0;

	posix_spawnattr_setflags(attrp, flags | POSIX_SPAWN_START_SUSPENDED);

	int pid = 0;
	int ret = __posix_spawn_orig_wrapper(&pid, path, desc, argv, envp);
	if(pidp) *pidp = pid;
	
	posix_spawnattr_setflags(attrp, flags); // maybe caller will use it again?

	if (ret == 0 && pid > 0) {
		// 3.x-native: jbdSpinlockFixOnly is a no-op that always resumes the child.
		// The kill-on-failure branch can never be reached and must not be revived.
		(void)jbdSpinlockFixOnly(pid, should_resume);
	} else {
		JBLogError("spawn failed: %d %s, pid=%d", ret, strerror(ret), pid);
	}

	return ret;
}

int roothide_launchd___posix_spawn_prehook(pid_t *restrict pidp, const char *restrict path, struct _posix_spawn_args_desc *desc, char *const argv[restrict], char *const envp[restrict])
{
	if(!desc || !desc->attrp) {
		posix_spawnattr_t attr=NULL;
		posix_spawnattr_init(&attr);
		int ret = posix_spawn(pidp, path, (desc && desc->file_actions) ? &desc->file_actions : NULL, &attr, argv, envp);
		posix_spawnattr_destroy(&attr);
		return ret;
	}
	posix_spawnattr_t *attrp = &desc->attrp;

	if(!path) {
		return __posix_spawn_hook(pidp, path, desc, argv, envp);
	}

	if(isRemovableBundlePath(path)) {
		static dispatch_once_t onceToken = {0};
		dispatch_once(&onceToken, ^{
			fix__iosConnect();
		});
	}

	if(strcmp(path, "/sbin/launchd") == 0) {
		short flags = 0;
		posix_spawnattr_getflags(attrp, &flags);
		posix_spawnattr_setflags(attrp, flags | POSIX_SPAWN_START_SUSPENDED);
		return __posix_spawn_hook(pidp, path, desc, argv, envp);
	}

	if(path && string_has_suffix(path, "/Dopamine.app/Dopamine"))
	{
		/* if the jailbreak activation is interrupted for some reason, 
			we prevent the app from relaunching to prevent the system from being in an unknown state */
		if(launchdhookFirstLoad) {
#ifdef ENABLE_LOGS
			launchd_panic("reboot device due to jailbreak failure!");
#endif
			return EPERM;
		}

		char roothidefile[PATH_MAX];
		snprintf(roothidefile, sizeof(roothidefile), "%s.roothide", path);
		if(access(roothidefile, F_OK) != 0) {
			return EPERM;
		}
	}
	
	// 3.x-native: the daemon spawned by initJailbreakd is /basebin/hookd (there is no
	// jailbreakd on 3.x). Give it a clean spawn like 2.x gave jailbreakd: no suspension,
	// no injection dance — hookd checks in via its registered port and idles.
	if(string_has_suffix(path, "/basebin/hookd")) {
		return __posix_spawn_orig_wrapper(pidp, path, desc, argv, envp);
	}


	// mitigate spinlock panic for ios15(A12+) devices

	bool iOS15Arm64e = false;
	bool choicyBlocked = false;
#ifdef __arm64e__
	if (!__builtin_available(iOS 16.0, *))
	{
		iOS15Arm64e = true;
		if(envbuf_getenv(envp, "_SafeMode") || envbuf_getenv(envp, "_MSSafeMode")) {
			if(path && isRemovableBundlePath(path) && !hasTrollstoreMarker(path)) {
				choicyBlocked = true;
			}
		}
	}
#endif

	bool roothideBlacklisted = isBlacklistedPath(path);
	if (choicyBlocked || roothideBlacklisted)
	{
		int ret;

		JBLogDebug("blacklisted app %s", path);

		if(dyld_patch_enabled() && iOS15Arm64e && roothideBlacklisted && (strstr(path, "/PlugIns/") || strstr(path, "/Extensions/") || strstr(path, ".appex/"))) {
			JBLogDebug("prevent blacklisted app's extension from running: ", path);
			ret = EPERM;
		}
		else if(dyld_patch_enabled() && iOS15Arm64e && roothideBlacklisted && (envbuf_getenv(envp, "ActivePrewarm") || envbuf_getenv(envp, "DYLD_USE_CLOSURES"))) {
			JBLogDebug("prevent blacklisted app from prewarming: ", path);
			ret = EPERM;
		}
		else
		{
			char **envc = envbuf_mutcopy((const char **)envp);

			//choicy may set these 
			envbuf_unsetenv(&envc, "_SafeMode");
			envbuf_unsetenv(&envc, "_MSSafeMode");
	
			/* According to xnu, the new thread in new process will not run in userland until after copyout pid
			https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/bsd/kern/kern_exec.c#L4321
			https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/bsd/kern/kern_exec.c#L4882
			https://github.com/apple-oss-distributions/xnu/blob/8d741a5de7ff4191bf97d57b9f54c2f6d4a15585/bsd/kern/kern_exec.c#L4933
			*/

			/* and posix_spawn->kernel->amfid->launchd may cause xpc dead loop so we can't use lock-spawn-unlock here */

			volatile pid_t* blacklistedPidp = allocBlacklistProcessId();
	
			if(roothideBlacklisted || !dyld_patch_enabled() || !iOS15Arm64e) {
				ret = __posix_spawn_orig_wrapper(blacklistedPidp, path, desc, argv, envc);
			} else {
				ret = roothide_launchd___posix_spawn__spinlock_fix_only(blacklistedPidp, path, desc, argv, envc);
			}
	
			pid_t pid = *blacklistedPidp;
			if(pidp) *pidp = *blacklistedPidp;

			commitBlacklistProcessId(blacklistedPidp); // will release blacklistedPidp
			blacklistedPidp = NULL;

			envbuf_free(envc);
				
			if(ret==0 && pid>0) {
				short flags = 0;
				posix_spawnattr_getflags(attrp, &flags);
				if((flags & POSIX_SPAWN_START_SUSPENDED) != 0) {
					platform_set_process_debugged(pid, false);
				}
			}
		}
	
		return ret;
	}

	if(launchdhookFirstLoad) 
	{
		//we should not enable system-wide injection until the jailbreak is finalized (userspace reboot).
		return __posix_spawn_orig_wrapper(pidp, path, desc, argv, envp);
	}
	
	return __posix_spawn_hook(pidp, path, desc, argv, envp);
}
