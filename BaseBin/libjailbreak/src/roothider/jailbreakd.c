#include <spawn.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <xpc/xpc.h>
#include <mach/mach.h>
#include <bsm/libbsm.h>
#include <sys/param.h>

#include "../libjailbreak.h"
#include "jailbreakd.h"
#include "common.h"
#include "log.h"

#ifdef ENABLE_LOGS
static void (*JBDLogDebugFunction)(const char *format, ...);
static void (*JBDLogErrorFunction)(const char *format, ...);

#define JBLogDebug(...) do { if(JBDLogDebugFunction)JBDLogDebugFunction(__VA_ARGS__); } while(0)
#define JBLogError(...) do { if(JBDLogErrorFunction)JBDLogErrorFunction(__VA_ARGS__); } while(0)

void enableJBDLog(void* debugLog, void* errorLog)
{
	JBDLogDebugFunction = debugLog;
	JBDLogErrorFunction = errorLog;
}
#endif

int posix_spawnattr_setspecialport_np(posix_spawnattr_t *attr, mach_port_t new_port, int which);
int posix_spawnattr_set_registered_ports_np(posix_spawnattr_t * __restrict attr, mach_port_t portarray[], uint32_t count);

static bool __jailbreakd_initialized = false;
mach_port_t gJailbreakdPort = MACH_PORT_NULL;

/* 3.x-native: initJailbreakd spawns hookd (there is no jailbreakd binary on 3.x).
 * These export the spawned hookd instance so launchdhook can sync it into
 * hookd_provider's globals and avoid spawning a second hookd on iOS 19+/26+,
 * where litehook routes memory hooks through hookd. */
pid_t gSpawnedHookdPid = -1;
mach_port_t gSpawnedHookdPort = MACH_PORT_NULL;

#define JAILBREAKD_CLIENT_PORT_FAST_GET

int registerServerPort()
{
	if (getpid() != 1) {
		// 3.x-native: never assert/abort from pid 1; a non-launchd caller is a
		// programming error, not a reason to panic the kernel.
		JBLogError("registerServerPort: called outside launchd");
		return -1;
	}

	// deallocate the previous port if it exists
	if(MACH_PORT_VALID(gJailbreakdPort)) {
		mach_port_deallocate(mach_task_self(), gJailbreakdPort);
		gJailbreakdPort = MACH_PORT_NULL;
	}

	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &gJailbreakdPort);
	mach_port_insert_right(mach_task_self(), gJailbreakdPort, gJailbreakdPort, MACH_MSG_TYPE_MAKE_SEND);

	JBLogDebug("jailbreakd server port: %x", gJailbreakdPort);

#ifdef JAILBREAKD_CLIENT_PORT_FAST_GET
	mach_port_t self_host = mach_host_self();
	kern_return_t kr = host_set_special_port(self_host, HOST_LAUNCHCTL_PORT, gJailbreakdPort);
	mach_port_deallocate(mach_task_self(), self_host);
#endif

	return kr==KERN_SUCCESS ? 0 : -1;
}

#ifdef JAILBREAKD_CLIENT_PORT_FAST_GET
mach_port_t jailbreakdClientPortFastGet()
{
	mach_port_t port = MACH_PORT_NULL;
	mach_port_t self_host = mach_host_self();
	kern_return_t kr = host_get_special_port(self_host, HOST_LOCAL_NODE, HOST_LAUNCHCTL_PORT, &port);
	mach_port_deallocate(mach_task_self(), self_host);
	if(kr != KERN_SUCCESS) {
		JBLogError("jailbreakdClientPortFastGet failed: %x,%s", kr, mach_error_string(kr));
		return MACH_PORT_NULL;
	}
	return port;
}
#endif

void setJailbreakdProcess(pid_t pid)
{
	//Reclaim the previous jailbreakd zombie process
	const char *pidenv = getenv("JAILBREAKD_PID");
	if (pidenv) 
	{
		pid_t oldpid = atoi(pidenv);
		if(oldpid != pid)
		{
			waitpid(oldpid, NULL, 0);
			unsetenv("JAILBREAKD_PID");
		}
	}

	char buf[32];
	snprintf(buf, sizeof(buf), "%d", pid);
	setenv("JAILBREAKD_PID", buf, 1);
}

int spawnJailbreakd()
{
	if (getpid() != 1) {
		JBLogError("spawnJailbreakd: called outside launchd");
		return -1;
	}

	/* 3.x-native: spawn /basebin/hookd instead of jailbreakd.
	 * hookd speaks the hookd_mach_msg checkin protocol: it looks up its
	 * registered ports and sends its server port to registeredPorts[2].
	 * Mirrors launchdhook/src/hookd_provider.c (hookd_start). */
	mach_port_t checkinPort = MACH_PORT_NULL;
	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &checkinPort);
	mach_port_insert_right(mach_task_self(), checkinPort, checkinPort, MACH_MSG_TYPE_MAKE_SEND);
	JBLogDebug("hookd checkin port: %x", checkinPort);

	pid_t pid;
	posix_spawnattr_t attr = NULL;
	posix_spawnattr_init(&attr);
	posix_spawnattr_set_registered_ports_np(&attr, (mach_port_t[]){ MACH_PORT_NULL, MACH_PORT_NULL, checkinPort }, 3);
	/* 3.x-native: spawn hookd with a controlled environment (mirrors
	 * hookd_provider.c hookd_start). Never inherit launchd's
	 * DYLD_INSERT_LIBRARIES: loading launchdhook.dylib into hookd would run its
	 * constructor (boomerang primitive recovery, xpc hooks) before hookd's own
	 * main() can send its checkin — deadlock/panic risk at boot. */
	const char *envp[] = { "_SafeMode=1", NULL };
	int ret = posix_spawn(&pid, JBROOT_PATH("/basebin/hookd"), NULL, &attr, (char*[]){"hookd",NULL}, (char *const *)envp);
	posix_spawnattr_destroy(&attr);

	if (ret != 0) {
		JBLogError("posix_spawn hookd failed: %d\n", ret);
		mach_port_deallocate(mach_task_self(), checkinPort);
		return ret;
	}

	JBLogDebug("hookd spawned, pid=%d\n", pid);

	/* Bound the checkin wait: this runs on launchd's main thread during its
	 * boot constructor. A wedged hookd must never hang launchd's boot
	 * (watchdog panic -> bootloop). mach_msg timeout unit = milliseconds. */
	mach_msg_header_t hdr = { 0 };
	hdr.msgh_size = sizeof(hdr) + MAX_TRAILER_SIZE;
	kern_return_t kr = mach_msg(&hdr, MACH_RCV_MSG, 0, hdr.msgh_size, checkinPort, 10 * 1000, 0);
	if (kr != KERN_SUCCESS) {
		JBLogError("hookd checkin receive failed: %x, %s", kr, mach_error_string(kr));
		kill(pid, SIGKILL);
		mach_port_deallocate(mach_task_self(), checkinPort);
		gSpawnedHookdPort = MACH_PORT_NULL;
		gSpawnedHookdPid = -1;
		return kr;
	}

	gSpawnedHookdPort = hdr.msgh_remote_port;
	mach_port_mod_refs(mach_task_self(), gSpawnedHookdPort, MACH_PORT_RIGHT_SEND, 1);
	gSpawnedHookdPid = pid;
	JBLogDebug("hookd checkin received, server port=%x", gSpawnedHookdPort);

	mach_msg_destroy(&hdr);
	mach_port_deallocate(mach_task_self(), checkinPort);

	setJailbreakdProcess(pid);

	return 0;
}

int initJailbreakd(bool firstLoad)
{
	if (getpid() != 1) {
		JBLogError("initJailbreakd: called outside launchd");
		return -1;
	}
	(void)firstLoad; /* 3.x-native: the RESPAWN_REQUIRED distinction is gone (hookd has no init flow) */

	if (__jailbreakd_initialized) {
		JBLogError("initJailbreakd: already initialized");
		return -1;
	}

	if(registerServerPort() != 0) {
		JBLogError("registerServerPort failed");
		return -1;
	}

	__jailbreakd_initialized = true;

	return spawnJailbreakd();
}

mach_port_t reactiveJailbreakdPort()
{
/* restarting jailbreakd may cause it to lose its previous internal state, 
	so we only use it during development. */
#ifndef ENABLE_LOGS
	/* 3.x-native: there is no jailbreakd to restart; aborting launchd (pid 1)
	 * here would panic the kernel and boot-loop the device. Fail fast instead. */
	JBLogError("jailbreakdClientPort: port dead, no jailbreakd to restart on 3.x");
	return MACH_PORT_NULL;
#endif

	if (getpid() != 1) {
		JBLogError("reactiveJailbreakdPort: called outside launchd");
		return MACH_PORT_NULL;
	}

	//prevent jailbreakdClientPort from calling before initJailbreakd
	if (!__jailbreakd_initialized) {
		JBLogError("reactiveJailbreakdPort: not initialized");
		return MACH_PORT_NULL;
	}

	mach_port_t port = MACH_PORT_NULL;

	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);

	// lock and check if another thread has reactivated the port

	kern_return_t kr = mach_port_mod_refs(mach_task_self(), gJailbreakdPort, MACH_PORT_RIGHT_SEND, 1);
	if(kr == KERN_SUCCESS) {
		port = gJailbreakdPort;
	}
	else
	{
		//make jailbreakd crashes perceptible
		sleep(5);

		/* 3.x-native: initJailbreakd already spawned the single hookd instance
		 * (gSpawnedHookdPid). Never spawn a second one — that would violate
		 * hookd_provider's single-instance assumption. Unlock before returning
		 * (the caller holds the mutex; a locked-forever mutex deadlocks the
		 * next jailbreakdClientPort call). */
		if (gSpawnedHookdPid != -1) {
			JBLogError("jailbreakdClientPort: hookd already spawned, not restarting");
			pthread_mutex_unlock(&mutex);
			return MACH_PORT_NULL;
		}

		//register server port before spawn jailbreakd
		if(registerServerPort() == 0)
		{
			//acquire the send right first
			kr = mach_port_mod_refs(mach_task_self(), gJailbreakdPort, MACH_PORT_RIGHT_SEND, 1);
			if(kr == KERN_SUCCESS)
			{
				port = gJailbreakdPort;

				// Try to restart jailbreakd
				if(spawnJailbreakd() != 0) {
					JBLogError("loadJailbreakd failed");
				}
			}
			else
			{
				JBLogError("jailbreakdClientPort failed");
			}
		}
		else
		{
			JBLogError("registerServerPort failed");
		}
	}

	pthread_mutex_unlock(&mutex);

	return port;
}

mach_port_t jailbreakdServerPort()
{
	if (getpid() != 1) {
		JBLogError("jailbreakdServerPort: called outside launchd");
		return MACH_PORT_NULL;
	}

	return gJailbreakdPort;
}

mach_port_t jailbreakdClientPort()
{
	mach_port_t port = MACH_PORT_NULL;

	if(getpid() == 1)
	{
		kern_return_t kr = mach_port_mod_refs(mach_task_self(), gJailbreakdPort, MACH_PORT_RIGHT_SEND, 1);
		if(kr == KERN_SUCCESS) {
			port = gJailbreakdPort;
		} else {
			JBLogError("jailbreakd port dead: %x,%s port=%x", kr, mach_error_string(kr), gJailbreakdPort);		
			port = reactiveJailbreakdPort();
		}
	}
	else
	{

#ifdef JAILBREAKD_CLIENT_PORT_FAST_GET
		port = jailbreakdClientPortFastGet();
		if(!MACH_PORT_VALID(port))
		{
#endif

			port = jbclient_jailbreakd_lookup();

#ifdef JAILBREAKD_CLIENT_PORT_FAST_GET
		}
#endif

	}

	return port;
}

// xpc_object_t jailbreakdRequestViaLaunchd(xpc_object_t xdict)
// {
// 	// to do
// }

xpc_object_t jailbreakdXpcRequest(xpc_object_t xdict)
{
	/* 3.x-native: there is no jailbreakd to serve JBD XPC messages (the roothide
	 * spawn-patch / spinlock-fix / exec-trace services are no-ops, see below).
	 * Fail fast instead of sending XPC into launchd's own registered port, which
	 * would queue the message forever and hang the caller. The original
	 * implementation is preserved under #if 0 for reference. */
	(void)xdict;
	return NULL;
#if 0
	mach_port_t port = jailbreakdClientPort();
	if (!MACH_PORT_VALID(port)) {
		JBLogError("invalid jailbreakdClientPort: %x", port);
		return NULL;
	}
	
	xpc_object_t xreply = NULL;
	xpc_object_t pipe = xpc_pipe_create_from_port(port, 0);
	if (pipe) {
		int err = xpc_pipe_routine(pipe, xdict, &xreply);
		if (err != 0) {
			char *desc = NULL;
			JBLogError("xpc_pipe_routine error on sending message to jailbreakd: %d / %s\n%s", err, xpc_strerror(err), (desc=xpc_copy_description(xdict)));
			if(desc) free(desc);
			if(xreply) xpc_release(xreply);
			xreply = NULL;
		};
	} else {
		JBLogError("xpc_pipe_create_from_port failed");
	}

	mach_port_deallocate(mach_task_self(), port);

	xpc_release(pipe);
	return xreply;
#endif
}

int jbdTestCall(int value)
{
	(void)value;
	/* 3.x-native: no jailbreakd; report healthy so callers don't treat the
	 * absence of a patch daemon as a fatal failure. */
	return 0;
}

int jbdSystemwideLog(const char* fmt, ...)
{
	(void)fmt;
	/* 3.x-native: no jailbreakd to log through; drop the message. */
	return 0;
}

int jbdSpawnPatchChild(int pid, bool resume)
{
	/* 3.x-native no-op: injection is env-based on 3.x (systemhook via
	 * DYLD_INSERT_LIBRARIES, dyldhook via the .fakelib dyld replacement), so
	 * there is no kernel-side dyld patch to apply. The child was spawned
	 * START_SUSPENDED by the caller; resume it here (mirrors what jailbreakd
	 * used to do on success) so no spawned process is left hanging. */
	if (resume) {
		kill(pid, SIGCONT);
	}
	return 0;
}

int jbdSpinlockFixOnly(int pid, bool resume)
{
	/* 3.x-native no-op: the iOS 15 arm64e spinlock fix is handled by
	 * dyldhook's spinlock_fix + the SPINLOCK_FIX_DISABLED env (see
	 * roothider.m posthook), not by a daemon. Just resume the child. */
	if (resume) {
		kill(pid, SIGCONT);
	}
	return 0;
}

int jbdSpawnExecStart(const char* execfile, bool resume)
{
	(void)execfile;
	(void)resume;
	/* 3.x-native no-op: exec'd images get env-based injection, so no
	 * spawnExecPatch registry is needed. */
	return 0;
}

int jbdSpawnExecCancel(const char* execfile)
{
	(void)execfile;
	return 0;
}

int jbdExecTraceStart(const char* execfile, bool* traced)
{
	(void)execfile;
	/* 3.x-native no-op: exec tracing was needed for kernel-side dyld patching
	 * of exec'd images; on 3.x the new image gets systemhook/dyldhook via env.
	 * Callers busy-wait on *traced (roothide_systemhook___execve_posthook), so
	 * report "already traced" to avoid a hang. */
	if (traced) *traced = true;
	return 0;
}

int jbdExecTraceCancel(const char* execfile, bool* detached)
{
	(void)execfile;
	/* See jbdExecTraceStart: report "already detached" so the caller's wait loop
	 * exits immediately. */
	if (detached) *detached = true;
	return 0;
}
