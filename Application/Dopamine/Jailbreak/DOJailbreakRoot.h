//
//  DOJailbreakRoot.h
//  Dopamine
//
//  Roothide hidden-jbroot helpers, ported from Dopamine 2.x roothide.
//  The jailbreak root is stored at a hidden location:
//      /var/containers/Bundle/Application/.jbroot-<16hex>
//      /var/mobile/Containers/Shared/AppGroup/.jbroot-<16hex>
//  instead of the standard rootless preboot path, so that it is not
//  discoverable by standard jailbreak-detection heuristics.
//

#import <Foundation/Foundation.h>
#import <stdint.h>

NS_ASSUME_NONNULL_BEGIN

// The 64-bit random "brand" (with checksum) that identifies the jbroot folder.
uint64_t jbrand_new(void);
uint64_t jbrand_current(void);
uint64_t resolve_jbrand_value(const char *name);

// Returns nonzero if `name` is a valid hidden jbroot folder name (".jbroot-<16hex>").
int is_jbroot_name(char *name);

// Locates the hidden jbroot (searching the app-container + app-group dirs).
// Returns nil if none is found. Caches the result unless `force` is YES.
NSString *_Nullable find_jbroot(BOOL force);

// Prepends the hidden jbroot to an absolute path (App-level path builder).
NSString *_Nullable jbrootPrefix(NSString *_Nullable path);

// Prepends the original rootfs bind-mount ("/rootfs/") to an absolute path.
NSString *_Nullable rootfsPrefix(NSString *_Nullable path);

NS_ASSUME_NONNULL_END
