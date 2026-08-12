#include "jbserver_global.h"
#include "jbsettings.h"

#include <mach/mach.h>
#include <libjailbreak/codesign.h>
#include <libjailbreak/libjailbreak.h>
#include <libjailbreak/roothider.h>
#include <libproc.h>

// 2.x had these in its own jbdomain_roothide.c; in the merged 3.x domain layout
// they live here (declared in libjailbreak/src/roothider.h).
int roothide_unsupport_request()
{
	JBLogError("**************************** Unsupported request ****************************");
	return -1;
}

bool roothide_domain_allowed(audit_token_t clientToken)
{
	//its fast enough
	if(isBlacklistedToken(&clientToken)) {
		JBLogDebug("ignore xpc message from blacklisted process (%d),%s", audit_token_to_pid(clientToken), proc_get_path(audit_token_to_pid(clientToken),NULL));
		return false;
	}

	return true;
}

static char *read_file_to_string(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);

    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)size, fp);
    fclose(fp);

    if (read_bytes != (size_t)size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

// Combined permission handler: allow Dopamine app (for dopamine actions) 
// AND any non-blacklisted process (for roothide actions)
bool dopamine_domain_allowed(audit_token_t clientToken)
{
    char path[PATH_MAX];
    if (proc_pidpath_audittoken(&clientToken, path, PATH_MAX) > 0) {
        if (is_dopamine_app(path)) {
            return true;
        }
    }
    // For roothide actions, allow any non-blacklisted process
    if (isBlacklistedToken(&clientToken)) {
        JBLogDebug("ignore xpc message from blacklisted process (%d), %s", 
                   audit_token_to_pid(clientToken), proc_get_path(audit_token_to_pid(clientToken), NULL));
        return false;
    }
    return true;
}

bool dopamine_is_jailbroken(char **outVersion)
{
    *outVersion = read_file_to_string(JBROOT_PATH("/basebin/.version"));
    return true;
}

// The merged domain is open to any non-blacklisted process for roothide actions,
// so the privileged Dopamine actions must re-verify the caller themselves.
static bool caller_is_dopamine_app(audit_token_t *processToken)
{
    char path[PATH_MAX];
    if (proc_pidpath_audittoken(processToken, path, PATH_MAX) <= 0) return false;
    return is_dopamine_app(path);
}

int dopamine_get_root(audit_token_t *processToken)
{
    // Only the Dopamine app may escalate to root (see caller_is_dopamine_app)
    if (!caller_is_dopamine_app(processToken)) {
        JBLogError("dopamine_get_root: denying non-app caller");
        return 1;
    }

    pid_t pid = audit_token_to_pid(*processToken);
    uint64_t proc = proc_find(pid);
    uint64_t ucred = proc_ucred(proc);

    if (kread32(ucred + koffsetof(ucred, uid)) == 501) {
        kwrite32(ucred + koffsetof(ucred, uid), 0);
        kwrite32(ucred + koffsetof(ucred, groups), 0);

        if (gSystemInfo.kernelStruct.proc_ro.exists) {
            uint64_t proc_ro = kread_ptr(proc + koffsetof(proc, proc_ro));

            if (koffsetof(proc_ro, task_tokens)) {
                uint64_t auditToken = proc_ro + koffsetof(proc_ro, task_tokens) + koffsetof(task_token_ro_data, audit_token);
                kwrite32(auditToken + 4, 0); // uid
                kwrite32(auditToken + 8, 0); // gid
            }
        }

        return 0;
    }

    return 1;
}

int dopamine_drop_root(audit_token_t *processToken)
{
    // Only the Dopamine app may drop root (see caller_is_dopamine_app)
    if (!caller_is_dopamine_app(processToken)) {
        JBLogError("dopamine_drop_root: denying non-app caller");
        return 1;
    }

    pid_t pid = audit_token_to_pid(*processToken);
    uint64_t proc = proc_find(pid);
    uint64_t ucred = proc_ucred(proc);

    if (kread32(ucred + koffsetof(ucred, uid)) == 0) {
        kwrite32(ucred + koffsetof(ucred, uid), 501);
        kwrite32(ucred + koffsetof(ucred, groups), 501);

        if (gSystemInfo.kernelStruct.proc_ro.exists) {
            uint64_t proc_ro = kread_ptr(proc + koffsetof(proc, proc_ro));

            if (koffsetof(proc_ro, task_tokens)) {
                uint64_t auditToken = proc_ro + koffsetof(proc_ro, task_tokens) + koffsetof(task_token_ro_data, audit_token);
                kwrite32(auditToken + 4, 501); // uid
                kwrite32(auditToken + 8, 501); // gid
            }
        }

        return 0;
    }

    return 1;
}

// Roothide action handlers
static int roothide_jailbroken_check(audit_token_t *callerToken, bool* jailbroken)
{
    *jailbroken = true;
    return 0;
}

static int roothide_palehide_present(audit_token_t *callerToken, bool* palehide)
{
    static bool result = false;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if(jbinfo(palera1n)=='hide') {
            result = true;
        }
    });

    *palehide = result;
    return 0;
}

static int roothide_blacklist_check(audit_token_t *callerToken, const char* checktype, xpc_object_t checkvalue, bool* blacklisted)
{
    if(strcmp(checktype, "pid")==0) {
        pid_t pid = (pid_t)xpc_uint64_get_value(checkvalue);
        if(pid > 1) {
            *blacklisted = isBlacklistedPid(pid);
            return 0;
        }
    } else if(strcmp(checktype, "path")==0) {
        const char* path = xpc_string_get_string_ptr(checkvalue);
        if(path) {
            *blacklisted = isBlacklistedPath(path);
            return 0;
        }
    } else if(strcmp(checktype, "bundle")==0) {
        const char* bundle = xpc_string_get_string_ptr(checkvalue);
        if(bundle) {
            *blacklisted = isBlacklistedApp(bundle);
            return 0;
        }
    } else {
        JBLogError("Invalid checktype: %s", checktype);
        return -1;
    }
    JBLogError("Failed to check blacklist for %s : %s", checktype, xpc_type_get_name(xpc_get_type(checkvalue)));
    return -1;
}

static int roothide_jailbreakd_lookup(audit_token_t *callerToken, xpc_object_t *portOut)
{
    // 3.x-native: there is no jailbreakd to hand out (initJailbreakd spawns hookd, and
    // the roothide JBD services are no-ops since injection is env-based). Always fail
    // explicitly instead of handing out a live-but-unserved port, which would queue XPC
    // messages forever and hang the client.
    (void)callerToken;
    (void)portOut;
    JBLogError("roothide_jailbreakd_lookup: no jailbreakd on 3.x");
    return -1;
}

static int roothide_jailbreakd_checkin(audit_token_t *callerToken, xpc_object_t *portOut)
{
    pid_t pid = audit_token_to_pid(*callerToken);
    uid_t uid = audit_token_to_euid(*callerToken);

    if(uid != 0) return -1;

    setJailbreakdProcess(pid);

    *portOut = xpc_mach_recv_create(jailbreakdServerPort());
    return 0;
}

static int roothide_dyld_patch_enabled(audit_token_t *callerToken, bool* enabled)
{
    *enabled = jbinfo(dyld_patch_enabled);
    return 0;
}

static int roothide_set_dyld_patch(audit_token_t *callerToken, bool enabled)
{
    pid_t pid = audit_token_to_pid(*callerToken);
    uid_t uid = audit_token_to_euid(*callerToken);

    uint32_t csFlags = 0;
    csops(getpid(), CS_OPS_STATUS, &csFlags, sizeof(csFlags));

    if(uid != 0 && (csFlags & CS_PLATFORM_BINARY)==0) {
        JBLogError("roothide_set_dyld_patch: denying request from %d,%d", pid, uid);
        return -1;
    }
    
#ifdef __arm64e__
    if (!__builtin_available(iOS 16.0, *))
    {
        if(roothide_config_set_spinlock_fix(enabled) != 0) {
            JBLogError("roothide_config_set_spinlock_fix failed");
            return -1;
        }
    }
#endif

    jbinfo(dyld_patch_enabled) = enabled;
    
    return 0;
}

// Trust cache helpers (from signatures.m)
typedef struct {
    uint32_t Count;
    uint32_t* Types;
    uint32_t* Subtypes;
} preferredArchInfo;

extern void recurse_collect_untrusted_cdhashes(const char *path, const char *callerImagePath, const char *callerExecutablePath, const char *workingDir, preferredArchInfo* preferredArch, cdhash_t **cdhashesOut, uint32_t *cdhashCountOut);

static int trust_macho_recurse(const char *machoPath, const char *dlopenCallerImagePath, const char *dlopenCallerExecutablePath, const char *workingDir, xpc_object_t preferredArchsArray)
{
    if(!machoPath || !dlopenCallerExecutablePath) return -1;
    
    size_t preferredArchCount = 0;
    if (preferredArchsArray) preferredArchCount = xpc_array_get_count(preferredArchsArray);
    uint32_t preferredArchTypes[preferredArchCount];
    uint32_t preferredArchSubtypes[preferredArchCount];
    for (size_t i = 0; i < preferredArchCount; i++) {
        preferredArchTypes[i] = 0;
        preferredArchSubtypes[i] = UINT32_MAX;
        xpc_object_t arch = xpc_array_get_value(preferredArchsArray, i);
        if (xpc_get_type(arch) == XPC_TYPE_DICTIONARY) {
            preferredArchTypes[i] = xpc_dictionary_get_uint64(arch, "type");
            preferredArchSubtypes[i] = xpc_dictionary_get_uint64(arch, "subtype");
        }
    }
    
    preferredArchInfo preferredArch = {preferredArchCount, preferredArchTypes, preferredArchSubtypes};

    cdhash_t *cdhashes = NULL;
    uint32_t cdhashesCount = 0;
    recurse_collect_untrusted_cdhashes(machoPath, dlopenCallerImagePath, dlopenCallerExecutablePath, workingDir, &preferredArch, &cdhashes, &cdhashesCount);
    if (cdhashes && cdhashesCount > 0) {
        jb_trustcache_add_cdhashes(cdhashes, cdhashesCount);
        free(cdhashes);
    }
    return 0;
}

// non-static: called directly by launchdhook/src/roothider.m (same process, avoids an XPC roundtrip)
int roothide_trust_executable_recurse(const char *executablePath, const char *processWorkingDir, xpc_object_t preferredArchsArray)
{
    return trust_macho_recurse(executablePath, NULL, executablePath, processWorkingDir, preferredArchsArray);
}

static int roothide_trust_library_recurse(const char *libraryPath, const char *callerLibraryPath, const char *callerExecutablePath, const char *currentWorkingDir)
{
    return trust_macho_recurse(libraryPath, callerLibraryPath, callerExecutablePath, currentWorkingDir, NULL);
}

struct jbserver_domain gDopamineDomain = {
    .permissionHandler = dopamine_domain_allowed,
    .actions = {
        // JBS_DOPAMINE_IS_JAILBROKEN (1)
        {
            .handler = dopamine_is_jailbroken,
            .args = (jbserver_arg[]){
                { .name = "version", .type = JBS_TYPE_STRING, .out = true },
                { 0 },
            },
        },
        // JBS_DOPAMINE_GET_ROOT (2)
        {
            .handler = dopamine_get_root,
            .args = (jbserver_arg[]){
                { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                { 0 },
            },
        },
        // JBS_DOPAMINE_DROP_ROOT (3)
        {
            .handler = dopamine_drop_root,
            .args = (jbserver_arg[]){
                { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                { 0 },
            },
        },
        // JBS_ROOTHIDE_JAILBROKEN_CHECK (4)
        {
            .handler = roothide_jailbroken_check,
            .args = (jbserver_arg[]) {
                    { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                    { .name = "jailbroken", .type = JBS_TYPE_BOOL, .out = true },
                    { 0 },
            },
        },
        // JBS_ROOTHIDE_PALEHIDE_PRESENT (5)
        {
            .handler = roothide_palehide_present,
            .args = (jbserver_arg[]) {
                    { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                    { .name = "palehide", .type = JBS_TYPE_BOOL, .out = true },
                    { 0 },
            },
        },
        // JBS_ROOTHIDE_BLACKLIST_CHECK (6)
        {
            .handler = roothide_blacklist_check,
            .args = (jbserver_arg[]) {
                    { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                    { .name = "checktype", .type = JBS_TYPE_STRING, .out = false },
                    { .name = "checkvalue", .type = JBS_TYPE_XPC_GENERIC, .out = false },
                    { .name = "blacklisted", .type = JBS_TYPE_BOOL, .out = true },
                    { 0 },
            },
        },
        // JBS_ROOTHIDE_JAILBREAKD_LOOKUP (7)
        {
            .handler = roothide_jailbreakd_lookup,
            .args = (jbserver_arg[]) {
                    { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                    // Client reads this via xpc_mach_send_copy_right (jbclient_roothide.c);
                    // declare JBS_TYPE_MACH_SEND so any future non-failing path writes
                    // a proper mach-send. Handler currently returns -1 -> fail-fast.
                    { .name = "port", .type = JBS_TYPE_MACH_SEND, .out = true },
                    { 0 },
            },
        },
        // JBS_ROOTHIDE_JAILBREAKD_CHECKIN (8)
        {
            .handler = roothide_jailbreakd_checkin,
            .args = (jbserver_arg[]) {
                    { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                    { .name = "port", .type = JBS_TYPE_XPC_GENERIC, .out = true },
                    { 0 },
            },
        },
        // JBS_ROOTHIDE_TRUST_LIBRARY_RECURSE (9)
        {
            .handler = roothide_trust_library_recurse,
            .args = (jbserver_arg[]){
                { .name = "library-path", .type = JBS_TYPE_STRING, .out = false },
                { .name = "caller-library-path", .type = JBS_TYPE_STRING, .out = false },
                { .name = "caller-executable-path", .type = JBS_TYPE_STRING, .out = false },
                { .name = "current-working-dir", .type = JBS_TYPE_STRING, .out = false },
                { 0 },
            },
        },
        // JBS_ROOTHIDE_TRUST_EXECUTABLE_RECURSE (10)
        {
            .handler = roothide_trust_executable_recurse,
            .args = (jbserver_arg[]){
                { .name = "executable-path", .type = JBS_TYPE_STRING, .out = false },
                { .name = "process-working-dir", .type = JBS_TYPE_STRING, .out = false },
                { .name = "preferred-archs", .type = JBS_TYPE_ARRAY, .out = false },
                { 0 },
            },
        },
        // JBS_ROOTHIDE_DYLD_PATCH_ENABLED_GET (11)
        {
            .handler = roothide_dyld_patch_enabled,
            .args = (jbserver_arg[]) {
                    { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                    { .name = "enabled", .type = JBS_TYPE_BOOL, .out = true },
                    { 0 },
            },
        },
        // JBS_ROOTHIDE_DYLD_PATCH_ENABLED_SET (12)
        {
            .handler = roothide_set_dyld_patch,
            .args = (jbserver_arg[]) {
                    { .name = "caller-token", .type = JBS_TYPE_CALLER_TOKEN, .out = false },
                    { .name = "enabled", .type = JBS_TYPE_BOOL, .out = false },
                    { 0 },
            },
        },
        { 0 },
    },
};