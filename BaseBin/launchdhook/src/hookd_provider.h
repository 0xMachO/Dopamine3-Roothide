#ifndef HOOKD_PROVIDER_H
#define HOOKD_PROVIDER_H

#include <mach/mach.h>
#include <sys/types.h>

void hookd_provider_init(void);
void hookd_provider_teardown(void);

/* The running hookd instance managed by the provider. Exposed so the roothide
 * layer (roothider.m / initJailbreakd) can hand over the hookd it spawned and
 * prevent a second instance from being started lazily on iOS 19+/26+. */
extern pid_t gHookdPid;
extern mach_port_t gHookdPort;

#endif /* HOOKD_PROVIDER_H */
