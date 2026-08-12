# PORT_MAP — خريطة نقل طبقة roothide من 2.x إلى 3.x

**المصدر:** `roothide2x/2.x` (Dopamine2-roothide) — فرق صافٍ مع `upstream/2.x`
**الوجهة:** `main` (Dopamine 3.0.4-3-g43b1388)
**الفرق الصافي:** 198 ملف، +12291/-301
**الجهاز المرجعي:** iPhone 11 (A13/arm64e) iOS 18.3.2

---

## 0. البنية التحتية والاكتشاف الحاسم

بنية jbserver متطابقة بين 2.x و3.x، والفتحة 5 (JBS_DOMAIN) هي نفسها:

```
2.x-roothide:  JBS_DOMAIN_ROOTHIDE  5   ← jbdomain_roothide.c (276 سطر)
3.x:           JBS_DOMAIN_DOPAMINE  5   ← jbdomain_dopamine.c (137 سطر)
```

**الاستنتاج:** 3.x رثّت بنية 2.x وأعادت تسمية فتحة roothide إلى "dopamine" — نحتاج **دمج** منطق الاثنين في فتحة 5، ليس إعادة بناء.

**API العميل الجديدة (2.x-roothide) التي يجب نقلها:**
```
jbclient_palehide_present / jbclient_roothide_jailbroken
jbclient_jailbreakd_lookup / jbclient_jailbreakd_checkin
jbclient_blacklist_check_pid/path/bundle
jbclient_trust_library_recurse / jbclient_trust_executable_recurse
jbclient_dyld_patch_enabled / jbclient_set_dyld_patch
```

---

## 1. جدول النقل الكامل

| # | المكوّن | الملفات في 2.x-roothide | العملية | الصعوبة | ملاحظات |
|---|---|---|---|---|---|
| 1 | **roothider library** | `libjailbreak/src/roothider/` (23 ملف) | **adapt** | ⭐⭐⭐ | يعتمد على jailbreakd (XPC) — 3.x استبدلها بـ hookd |
| 2 | **roothidehooks** | `BaseBin/roothidehooks/` (13 ملف: pathhook.x, cfprefsd.x, installd.x, lsd.x, springboard.x, main.x, palera1n.x) | **copy** + تكييف | ⭐⭐ | الأقل تأثرًا بالنسخة؛ palera1n.x يُسقط (قرار) |
| 3 | **jbdomain_roothide** | `launchdhook/src/jbserver/jbdomain_roothide.c` (276 سطر) | **merge** مع jbdomain_dopamine | ⭐⭐⭐ | الفتحة 5 في 3.x مشغولة بـ dopamine domain |
| 4 | **dyldhook/roothider** | `dyldhook/src/roothider.{c,S}` + تعديلات main.c/fakelib_redirect.c/lv_bypass.c/spinlock_fix.c | **rewrite** | ⭐⭐⭐⭐ | يجب إعادة بنائه على متغيرات ios15/16-17/18+ (مخالف للنموذج القديم ios15/16) |
| 5 | **systemhook** | `systemhook/src/roothider_main.c`, `roothider_common.c`, `roothider.h` + تعديلات common.c/common.h/main.c | **adapt** | ⭐⭐⭐ | 3.x أعاد هيكلة إلى `src/common/` مع hookd_external |
| 6 | **launchdhook** | `launchdhook/src/roothider.m` + تعديلات jbserver_*.c/jbdomain_platform/root/systemwide/watchdog + main.m/spawn_hook.c/update.m | **adapt** | ⭐⭐⭐ | بنية 3.x أضافت hookd_provider — تكامل مزدوج |
| 7 | **XPF** | submodule fork roothide ("add roothide support" commit) | **re-apply** | ⭐⭐ | 3.x يستخدم opa334/XPF الأحدث (iOS 27) — نعيد تطبيق الدعم فوقه |
| 8 | **jbctl** | `jbctl/src/internal.m` (قسم roothide: launchctl bootstrap + uicache) + main.m + entitlements | **adapt** | ⭐⭐ | مسارات JBROOT_PATH بدل /var/jb |
| 9 | **jailbreakd vs hookd** | 2.x: BaseBin/jailbreakd (main.m/server.m ObjC) | **decision** | ⭐⭐⭐⭐ | 3.x: hookd (main.c C). خياران: (أ) نقل خدمات roothider لـ hookd، (ب) إبقاء jailbreakd منفصلاً |
| 10 | **Application** | DOBootstrapper/DOEnvironmentManager/DOJailbreaker/DOPreferenceManager/DOExploitManager/DOUIManager/DOMainViewController/DOSettingsController/DOUpdateViewController/main.m/NSString+Version/DOGlobalAppearance/DOCreditsViewController | **adapt** | ⭐⭐⭐ | marker `Dopamine.roothide`, plist الـ roothide، تكامل Bootstrap |
| 11 | **Makefiles** | BaseBin/Makefile, dyldhook/Makefile, forkfix/Makefile, watchdoghook/Makefile, launchdhook/Makefile, systemhook/Makefile, libjailbreak/Makefile, MachOMerger, Application/Makefile, جذر Makefile, Packages/* | **adapt** | ⭐⭐ | توقيع `ldid -S` بدل `-Cadhoc`، `-lc++`، إزالة BUILD_STANDALONE، MARKETING_VERSION |
| 12 | **التغليف** | `_external/basebin/.version`, LaunchDaemons plists, .gitignore, .gitmodules | **copy/adapt** | ⭐ | نسخة roothide 2.4.9.x → نسخة 3.x جديدة |
| 13 | **Assets/UI** | AppIcon المخصص + Credits.plist | **copy** | ⭐ | اختياري — أيقونة roothide |
| 14 | **palera1n** | `Exploits/palera1n/` (كامل) | **skip** ❌ | — | قرار: 3.x أسقطه — يتوافق جهازنا (A13) مع momentarius |
| 15 | **bootstrapper** | `BaseBin/bootstrapper/` (stub) + Resources/bootstrap_*.tar.zst + roothideapp.deb | **تحقيق** | ⭐⭐ | 3.x استبدل آلية bootstrap — نتحقق هل نحتاج هذه الملفات |
| 16 | **Workflow** | `.github/workflows/roothide.yml` (+BUILD.md, ISSUE_TEMPLATE) | **adapt** | ⭐⭐ | دمج إعداد roothide (theos fork, trustcache) مع 3.x main.yml (Xcode latest) |
| 17 | **ChOma** | submodule (الاختلافات مع نسخة 3.x) | **adapt** | ⭐⭐ | فحص diff: roothide عدّل arm64.c/MachO.c إلخ — النقل الأفضل من fork roothide على نسخة 3.x |

---

## 2. ترتيب التنفيذ المقترح (كل خطوة = commit مستقل + اختبار بناء)

| الترتيب | المكوّن | معيار القبول |
|---|---|---|
| 1 | libjailbreak + roothider library + jbroot.h | يُجمع بدون أخطاء، الروابط سليمة |
| 2 | roothidehooks | يتجمع ويُحزم في basebin |
| 3 | dyldhook/roothider (لكل متغير iOS) | كل الأنواع الستة تُنتج (arm64/arm64e × ios15/ios16-17/ios18+) |
| 4 | systemhook | يتجمع مع بنية common/ الجديدة |
| 5 | launchdhook + jbdomain merge | تتجمع مع hookd_provider |
| 6 | XPF patch | xpf_test on 18.3.2 kernelcache |
| 7 | jbctl | يتجمع |
| 8 | Application | tipa يُنتج |
| 9 | workflow + التغليف | build عبر Actions ناجح |

## 3. قرارات مفتوحة (تُحسم أثناء التنفيذ)

1. ~~**jailbreakd → hookd**~~ — **تم الحسم (خيار 3.x-native):** `initJailbreakd` يسبن `/basebin/hookd`، وخدمات roothide (spawn-patch/spinlock-fix/exec-trace) أصبحت no-ops لأن حقن 3.x يعمل عبر env (`DYLD_INSERT_LIBRARIES` لـ systemhook + `.fakelib/dyld` لـ dyldhook) — لا حاجة لـ krw spawn patch. التفاصيل في سجل الجلسة (بـ) أسفل.
2. **bootstrap integration:** كيف يحصل roothide Bootstrap على الجذر — عبر DOBootstrapper 3.x (procursus) أم عبر ملفات Resources/bootstrap_*.tar.zst القديمة؟
3. **استغلال palera1n** — مؤكد الإسقاط
4. **الإصدار:** basebin/.version = قيمته الجديدة في نسختنا

## 4. المخاطر الحاسمة (من فحص الكود الفعلي)

- **dyldhook** في 3.x لديه `CHECK_SELF_CONTAINED` و `FILES_IOS16-17`/`FILES_IOS18+` generated — roothider.c يخطف `expandAtLoaderPath` عبر mangled symbol لـ dyld 4/5/6 — الرمز نفسه موجود في 3.x لكن البنية متغيرة (لتشغيله على ios26/18+ تغيّرت توقيعات الدوال). **يحتاج إعادة كتابة فعلية لكل متغير
- **libjailbreak** في 3.x غيّر واجهات (`jbclient_root_trustcache_add_cdhash` معطّلة في 2.x-roothide، و3.x أضافت `hookd_external`) — دمج دقيق مطلوب
- **jbserver 3.x** أكبر وأحدث (216 سطر jbserver_mach vs 175) — إعادة تنفيذ فتحة roothide داخل dopamine domain
- **primitives_IOSurface.m / signatures.c / trustcache.c / util.*** في 3.x** — اختلفت بالكامل، يجب أخذ نسخ 3.x ودمج شيفرة roothide فوقها

---

## 5. سجل التقدم (آخر تحديث: 2026-08-12)

**الوضع العام:** تم التزام 4 commits فوق 3.0.4 (الخطوات 1، 2، 4 + هذا المستند). الجاري حاليًا: الخطوة 3 (dyldhook) والخطوة 5 (دمج jbdomain) — تعديلات غير ملتزمة. لم يُبنَ أي مكوّن بعد.

### اكتشاف حاسم يصحّح بند المخاطر 4

فحص مصدر dyld الرسمي يؤكد أن `Loader::expandAtLoaderPath` **لم يتغيّر توقيعه** في:
- `dyld-1160.6` (iOS 18.x) و `dyld-1162` و `dyld-1378` (iOS 26):
```cpp
bool Loader::expandAtLoaderPath(RuntimeState&, const char*, const LoadOptions&, const Loader*, bool, char[]);
// نفس الـ mangled symbol: _ZN5dyld46Loader18expandAtLoaderPathERNS_12RuntimeStateEPKcRKNS0_11LoadOptionsEPKS0_bPc
```
**الاستنتاج:** نسخة 2.x الحرفية من `roothider.c` صحيحة ABI لكل المتغيرات الستة — لا حاجة لإعادة كتابة الرمز، فقط إصلاحات الدمج أدناه. (الفرع المحلي `2.4.9-beta` من roothide يبني نفس الرمز على متغير ios18+ أيضًا.)

### إصلاحات تمت هذا الجلسة (غير ملتزمة)

| الملف | الإصلاح | لماذا؟ |
|---|---|---|
| `dyldhook/src/roothider.S` | إزالة `MAKE_TRAMPOLINE(strlcpy/strlcat)` | main.S في 3.x يعرّفها أصلًا → رمز مكرر يكسر الرابط |
| `dyldhook/src/spinlock_fix.c` | `header->` → `header.` (بنية على الاستاك) + `strcpy/strcat` → `strlcpy/strlcat` | خطأ تجميع + main.S 3.x يوفر strlcpy/strlcat فقط → كان سيفشل `CHECK_SELF_CONTAINED` |
| `dyldhook/src/lv_bypass.c` | hook dlopen مقصور على `#if IOS >= 18` + strlcpy | fakelib_redirect.c (upstream) يغطي IOS < 18 → بدون تكرار رمز |
| `dyldhook/Makefile` | إعادة `fakelib_redirect.c` إلى FILES | أُبعد سابقًا دون داعٍ |
| `BaseBin/Makefile` | إعادة نسخ `libjailbreak/src/roothider/*.h` إلى `.include/libjailbreak/roothider` + ربط roothidehooks | بدونه كل `#include <libjailbreak/roothider.h>` يفشل تجميعًا |
| `launchdhook/Makefile` | إضافة `../systemhook/src/roothider_common.c` | مثل 2.x — وظائف sysctl/ppid المطلوبة للـ launchdhook |
| `jbdomain_dopamine.c` | تحصين: `get_root`/`drop_root` مقصورة على تطبيق Dopamine + حارس `MACH_PORT_NULL` في `jailbreakd_lookup` | الدمج فتح الفتحة لأي عملية غير محظورة؛ أفعال الجذر لم تكن موجودة في roothide domain في 2.x أصلًا |

**تم التحقق منه:** كل رموز الدمج موجودة في 3.x — `jbinfo(palera1n/dyld_patch_enabled/jbrand/appIdentifier/rootPath)` في info.h، و`JBS_TYPE_*` في jbserver.h، و`isBlacklisted*`/`jailbreakd*Port`/`setJailbreakdProcess`/`roothide_config_set_spinlock_fix`/`recurse_collect_untrusted_cdhashes`/`jb_trustcache_add_cdhashes`.

### جلسة 2026-08-12 (ب): إكمال الخطوة 5 (launchdhook) + CI + إصلاح خلل جوهري في الدمج

#### 🐛 خلل جوهري اكتُشف وصُحّح: ترقيم أفعال roothide (كان سيكسر كل IPC)

2.x يعرّف أفعال roothide بأرقام 1-9 (فهارس 1-أساسية داخل مصفوفة `actions` للـ domain) — والعميل `jbclient_roothide.c` يرسل **القيمة الخام** كحقل `action`، و`jbserver_received_xpc_message` يمشي `domain->actions[actionIdx-1]`.

دمج WIP السابق نقل الأرقام إلى `101-109` (لتفادي التصادم مع أفعال dopamine 1-3) — **لكن ذلك يكسر الـ dispatch**: `action=101` → `actions[100]` → إنهاء مصفوفة → `-1` لكل استدعاء roothide.

**الإصلاح (`jbserver_domains.h`)**: `JBS_ROOTHIDE_JAILBROKEN_CHECK = 4` … `JBS_ROOTHIDE_DYLD_PATCH_ENABLED_SET = 12` — فهارس متصاعدة داخل الـ domain المدمج (4-12 تطابق مواضع الـ handlers في `jbdomain_dopamine.c`). تعليق توثيقي يشرح العقد (قيمة الـ enum = موضع 1-أساسي).

#### ما نُقل (11 ملف launchdhook + ملف مكتبة + 2 ملف CI)

| الملف | التحويل |
|---|---|
| `launchdhook/src/roothider.m` (**جديد**) | substrate → **litehook** (`litehook_hook_function(__sysctl/__sysctlbyname)` كـ 2.x systemhook؛ `orig + litehook_rebind_symbol` لـ `bind`/`xpc_dictionary_create_reply`/`xpc_pipe_routine_reply`؛ `fix__iosConnect` عبر `dlopen` + `litehook_find_dsc_symbol` بدل MSGetImageByName/MSFindSymbol). حُذف `HOOK_DYLIB_PATH` + فرع النقل/unsandbox (بائد في 2.x وغير موجود في 3.x؛ `basebin_gen` يتكفل بـ systemhook عبر `.fakelib`). `posix_spawnattr_t attrp = &desc->attrp` → `posix_spawnattr_t *attrp` (نوع صحيح بدل تحذير) |
| `main.m` | ماكرو `abort_with_reason` → `launchd_panic` + `roothide_launchd_preinit/postinit` + `vm.shared_region_pivot` (arm64e/iOS<16) + تعليق boot logo وكتلة DOPAMINE_IS_HIDDEN |
| `spawn_hook.c` | ماكرو + تعليق `ensure_fakelib_mounted` + تسجيل spawn في `__posix_spawn_orig_wrapper` + `crashreporter_pause/resume(key)` + ربط `roothide_launchd___posix_spawn_prehook` كـ hook رئيسي + `posix_spawn_hook_shared` بوسائط roothide + `#include <libjailbreak/roothider.h>` |
| `update.m` | `randomizeAndLoadBasebinTrustcache` بدل trustcache_file_build/upload + `sets[99]` + `namecache`/`amfi_oids` |
| `ipc_hook.c` | فرع `isBlacklistedToken` في sandbox hook |
| `daemon_hook.m` | تعطيل مسارات `/Library/LaunchDaemons` (يُحمّل عبر procursus launchctl) |
| `crashreporter.h/.m` | `#if 0` كامل — تجنّب رموزًا مكررة مع `libjailbreak/src/roothider/crashreporter.{h,m}` (التي توفّر `crashreporter_pause → int key` / `resume(key)`) |
| `jbdomain_systemwide.c` | تعطيل `systemwide_domain_allowed/set_enabled` + `roothide_domain_allowed` كـ permissionHandler + `generate_sandbox_extensions(processToken, isPlatformProcess)` بدل extensions المباشرة + `isRemovableBundlePath/isSubPathOf` لـ fullyDebugged + `CS_INSTALLER` في platformize + إعادة ترتيب fork_fix (بنسخة 2.4.9-beta المُصحّحة: `childNentries` من `childHeader`) + إلغاء إعفاء `/usr/lib` |
| `jbdomain_root.c` | `JBS_ROOT_ADD_CDHASH` → `roothide_unsupport_request` |
| `jbdomain_platform.c` | `SET_SYSTEMWIDE_DOMAIN_ENABLED` → `roothide_unsupport_request` |
| `jbdomain_watchdog.c` | إضافة include roothider.h (crashreporter_open_outfile من libjailbreak) |
| `jbserver_mach.c` | `roothide_domain_allowed` + تسجيل JBLogDebug |
| `jbdomain_dopamine.c` | تعريف `roothide_domain_allowed`/`roothide_unsupport_request` (كانتا في jbdomain_roothide.c في 2.x) + إلغاء static عن `roothide_trust_executable_recurse` (يستدعيه roothider.m مباشرة) + تحديث تعليقات الأفعال إلى 4-12 |
| `.github/workflows/roothide.yml` (**جديد**) | دمج 2.x roothide.yml (theos fork roothide, Credits, تسمية tipa) مع 3.x main.yml (setup-xcode, checkout submodules, NIGHTLY, bootstraps) — **بدون** حذف xpc الخاص بالـ SDK (3.x يحتاجه لـ `xpc_dictionary_set_mach_recv` في jbserver.c — بخلاف 2.x الذي يملك نسخة معدّلة في `_external`) |

**قرارات التكييف:** `unsandbox()` غير موجودة في 3.x (`unsandbox1/2` فقط) → حُذف فرع نقل systemhook. النظام الفرعي لـ systemhook في 3.x يعرّف `HOOK_DYLIB_PATH` كثابت → لا حاجة لمتغير roothider.m. دوال `__sysctl`/`__sysctlbyname` تُخطف الآن بنمط `litehook_hook_function` المطابق لـ 2.x systemhook.

### جلسة 2026-08-12 (ج): الخطوة 6 — حسم jailbreakd → hookd (خيار 3.x-native)

**القرار (بموافقة المستخدم):** `initJailbreakd` يسبن `/basebin/hookd` بدل jailbreakd، وخدمات JBD تصبح no-ops. الأساس الواقعي:
- 3.x **لا يبني jailbreakd** إطلاقًا؛ حقن العمليات يعمل عبر env: systemhook عبر `DYLD_INSERT_LIBRARIES` (في `spawn_exec_hook_common`), وdyldhook عبر استبدال dyld بـ `.fakelib/dyld` + trustcache — **لا يوجد krw spawn patch**. الـ jbserver يعيش داخل launchdhook (`xpc_hook.c`) لا في daemon منفصل.
- `hookd` (بروتوكول `hookd_mach_msg` لترميز الـ hooks الميموري، iOS 19+/26+ عبر `litehook_hook_memory_hookd`) يُسبن أصلًا lazily بواسطة `hookd_provider`.
- `roothide_patch_proc` (krw) يُستدعى فقط من `exec_patch.m`/`exec_trace.m` اللذين لا يُفعَّلان إلا عبر مسارات jbd* (التي أصبحت no-ops) → krw يصبح غير قابل للوصول آمنًا.

| الملف | التغيير |
|---|---|
| `libjailbreak/src/roothider/jailbreakd.c` | `spawnJailbreakd` يسبن `/basebin/hookd` عبر بروتوكول checkin (`registeredPorts[2]` + استقبال منفذ الخادم) — يطابق `hookd_provider.hookd_start`؛ أُزيل `__firstLoad`/dispatch-source القديم (و`abort()` لغم فيه). دوال `jbdSpawnPatchChild`/`jbdSpinlockFixOnly` → no-op مع `SIGCONT` عند `resume` (الطفل المعلّق لا يُترك معلقًا أبدًا)؛ `jbdSpawnExecStart/Cancel` → 0؛ `jbdExecTraceStart/Cancel` → 0 مع `*traced/*detached = true` (المستدعي busy-wait — بدونها تعليق أبدي)؛ `jbdSystemwideLog`/`jbdTestCall` → 0؛ `jailbreakdXpcRequest` → fail-fast (`#if 0` للجسم) — إرسال XPC لمنفذ launchd الخاص كان سيعلّق المرسل للأبد. تصدير `gSpawnedHookdPid/gSpawnedHookdPort` |
| `launchdhook/src/hookd_provider.h` | extern لـ `gHookdPid/gHookdPort` (كانا بلا تصريح) |
| `launchdhook/src/roothider.m` | بعد نجاح `initJailbreakd`، مزامنة `gHookdPid/gHookdPort` من `gSpawnedHookdPid/gSpawnedHookdPort` → **منع double-spawn** على iOS 19+/26+ (حيث litehook يمرر الـ memory hooks عبر hookd). تحديث التعليق (لا boot-loop: الفشل يُسجَّل ويُتابع) |
| `launchdhook/src/jbserver/jbdomain_dopamine.c` | `roothide_jailbreakd_lookup` → فشل صريح دائم (منفذ حيّ بلا خادم = XPC يتراكم للأبد ويعلّق العميل) |

**لماذا لا خيار "دمج خدمات JBD في hookd"؟** نقل `server.m` (roothide_patch_proc عبر krw) يتطلب تشغيل 3.x krw داخل hookd (في 2.x كانت تأتي عبر `jbclient_initialize_primitives` من launchd — غير موجود على 3.x) + XPC server + threads exec_trace — مشروع مستقل بمخاطرة boot-loop عالية، بينما 3.x لا يحتاج الـ patch أصلًا (حقن env).

**نافذة double-spawn معروفة (iOS 19+/26+):** `hookd_provider_init()` + `litehook_hook_function(mach_vm_protect, ...)` في main.m يعملان **قبل** `roothide_launchd_postinit` — أي استدعاء متزامن لـ mach_vm_protect من خيط آخر بين تلك النقطة والمزامنة قد يفعّل `launchd_hookd_send_msg` مع `gHookdPid == -1` ويسبن hookd ثانيًا. المزامنة تُضيّق النافذة إلى لحظة إقلاع ضئيلة، وأسوأ الحالات = hookd يتيم خامل (لا تعليق). المزامنة محروسة بـ `MACH_PORT_VALID(gSpawnedHookdPort)` — لا تُحذفها.

### جلسة 2026-08-12 (د): الخطوة 7 — إعادة تطبيق دعم roothide على XPF

**الوضع قبل:** 2.x يستخدم fork roothide/XPF عند commit وحيد `3fb4bb3` ("add roothide support" على قاعدة `bc9c880` القديمة). 3.x يستخدم opa334/XPF الأحدث (`b703bf3`، دعم iOS 27/SPTM).

**اكتشاف حاسم:** نسخة 3.x من `src/common.c` (1633 سطرًا) تحتوي **أصلًا على معظم** finders الداعمة (start_first_cpu, allproc, task_itk_space, developer_mode_enabled, iorvbar, str_x8_x0...). ما ينقص فعلًا من دعم roothide (4 عناصر kernel + setّان + مقطعان) نقلته فوق نسخة 3.x:

| الملف | التغيير |
|---|---|
| `XPF/src/xpf.h` | حقلان جديدان: `kernelPrelinkDataSection` + `kernelAMFIDataSection` |
| `XPF/src/xpf.c` | تهيئة المقطعين في فرعي fileset (`com.apple.driver.AppleMobileFileIntegrity`, `__DATA`, `__data`) و prelink (`__PRELINK_DATA`, `__data`) + تنظيفهما في `xpf_stop` + `gNameCacheSet` (namecache: nchashtbl/nchashmask) و `gAMFIOidsSet` (amfi_oids: launch_env_logging/developer_mode_status) + إضافتهما إلى `gSets` |
| `XPF/src/common.c` | `xpf_find_namecache` + `xpf_find_amfi_oid` (منسوختان حرفيًا من 2.x — كل APIs: `pfsec_read_pointer`, `LDR_STR_TYPE_ANY`, `pfmetric_run_in_range` متوفرة في 3.x/ChOma) + تسجيل العناصر الأربعة في `xpf_common_init` |
| `XPF/src/cli/main.c` | `char *sets[99]` + إضافة namecache/amfi_oids (بنمط 2.x) |

**لم يُنقل:** `Makefile` + رؤوس `external/ios/include` المفرودة — كانت مطلوبة في 2.x لأن نسخة XPF القديمة لم تكن تُجمّع مع رؤوس xpc الخاصة بالـ SDK؛ 3.x يجمعها أصلًا (SDK يوفر xpc.modulemap).

**المستهلكون (مؤكد):** `launchdhook/src/update.m` (نقل الخطوة 5) يستدعي `xpf_construct_offset_dictionary` مع namecache/amfi_oids؛ `libjailbreak/src/roothider/unsandbox1.m` (إخفاء الاسم) يحتاج `kernelSymbol.nchashtbl/nchashmask`؛ `roothider.m` `hideDeveloperMode` يحتاج OIDs الـ AMFI.

**ملاحظة للخطوة 8 (Application):** `DOJailbreaker.m` في 3.x **لا يضم** namecache/amfi_oids في قائمته (2.x-roothide أضافهما هناك) — بدونها لن يُنتج `dopamine` daemon هذه الـ offsets عند الجمع عبر `gatherSystemInformation`. يُنقل في الخطوة 8 مع تكييف نفس النمط.

#### المتبقي بالترتيب
1. **اختبار بناء** (الآن ممكن عبر `roothide.yml` على GitHub Actions — أو macOS محلي): BaseBin كامل ثم tipa.
2. **الخطوة 8**: Application (Dopamine.roothide + تكامل DOBootstrapper/DOEnvironmentManager).
3. **الخطوة 9**: `_external/basebin/.version` (3.0.4 → إصدار roothide) + بقية التغليف.
4. القرار المفتوح المتبقي: bootstrap integration.

### جلسة 2026-08-12 (هـ): الخطوة 8 — ربط sets الـ XPF الجديدة في DOJailbreaker.m

**الملف:** `Application/Dopamine/Jailbreak/DOJailbreaker.m` (دالة `gatherSystemInformation`)

| التغيير | التفاصيل |
|---|---|
| `char *sets[]` (VLA 12 خانة) → `char *sets[99]` | الـ VLA القديمة كانت **سعة ناقصة**: 7 مجموعات أساسية + حتى 4 اختيارية (devmode/badRecovery/arm64kcall/perfkrw) = 11، ثم namecache/amfi_oids = 13 → **تجاوز سعة مصفوفة** كان سيقع حتمًا. |
| `while(sets[++idx])` → `uint32_t idx = 7;` | idx ثابت = عدد المداخل الأساسية (7) — مطابق حرفيًا لنمط 2.x-roothide |
| كتلة roothide | `sets[idx++] = "namecache"` (مدعوم دائمًا) + `amfi_oids` خلف `xpf_set_is_supported("amfi_oids")` (أي iOS < 16 لا يُضاف) + `sets[idx] = NULL` — العلامة النهائية التي تجعل `xpf_construct_offset_dictionary` (حلقة `for (i = 0; sets[i]; i++)`) تتوقف ولا تقرأ خانات غير مهيأة |

**لماذا هذا كافٍ (لا حاجة لشيء آخر):**
- `libjailbreak/src/info.h` في 3.x **يضم أصلًا** `kernelSymbol.nchashtbl/nchashmask/launch_env_logging/developer_mode_status` في قائمة `SYSTEM_INFO_ITERATE` (54 رمزًا) → `jbinfo_initialize_dynamic_offsets(_systemInfoXdict)` يفككها و`ksymbol(name)` يقرؤها
- المستهلكون مطابقون لـ 2.x: `unsandbox1.m` (nchashtbl/nchashmask لإخفاء الاسم من namecache) + `common.m` (`hideDeveloperMode` عبر OIDs الـ AMFI)
- `xpf_construct_offset_dictionary` يرفض الـ set غير المدعوم → الحارس `xpf_set_is_supported` يمنع فشل الجمع على iOS 15

**مراجعة الكود:** موافقة — فقط ملاحظة `idx = 7` رقم سحري مرتبط بعدد المداخل الأساسية (مقبول لاتساقنا الحرفي مع 2.x).

**تم بذلك إغلاق: بند الخطوة 8 في جدول النقل (Application جزئيًا — الباقي تكامل bootstrap في الخطوة 10)**

### جلسة 2026-08-12 (و): الخطوة 9 — تكامل Application المتبقي (marker + bootstrap)

**الأساس:** فرق `origin/2.x..origin/2.4.9-beta` في Application = 13 ملف (+1158/−33). **اكتشاف محوري: معظم تغييرات roothide دخلت upstream 3.x أصلًا** (`getCFMajorVersion→"1900"`, `chmod jbctl S_ISUID`, سلسلة الدعم `18.7.1`, `IOSurface_map_cleanup`, `cleanUpPostExploitation`, `availableExploitIdentifiersForType` + fallback الـ exploits, `readExploitPreferenceValue`). ما نُقل فعليًا:

| الملف | التغيير |
|---|---|
| `UI/DOUIManager.m` | قناة التحديث: `api.github.com/repos/opa334/Dopamine` → `roothide/Dopamine2-roothide` (مطابق حرفيًا لـ 2.x) |
| `UI/Update/DOUpdateViewController.m` | زر الفتح عند غياب TrollStore: `github.com/opa334/Dopamine/releases` → `roothide/Dopamine2-roothide/releases` |
| `Jailbreak/DOEnvironmentManager.m` | بوابة `if (!jbclient_roothide_jailbroken()) return;` في `updateJailbreakState` قبل `jbclient_dopamine_is_jailbroken` — تكرار بوابة 2.x-roothide في `isJailbroken`. آمنة: fail-fast عند غياب launchdhook (لا تعليق)، والتطبيق يضبط `setJailbroken:YES` يدويًا بعد `injectLaunchdHook` فجلسة الجيلبريك الحالية لا تتأثر |

**قرار bootstrap (مُحسم — 3.x-native):** **لا يُنقل** `DOBootstrapper(roothide)` category الخاص بـ 2.x (jbrand/jbrootPrefix + `roothideapp.deb`). الأسباب: (1) 3.x يستخدم **procursus الأصلي** — `DOBootstrapper.prepareBootstrap` ينزّل من `apt.procurs.us` و`download_bootstraps.sh` (الذي يشغّله roothide.yml المدمج)؛ (2) آلية jbrand (`.jbroot-%016llX` العشوائي) لا وجود لها في 3.x — الجذر ثابت عبر `jbclient_get_jbroot()`؛ (3) تركيب `roothideapp.deb` في 2.x كان مقترنًا ببنية Bootstrap app الخاصة بـ 2.x. **المتبقي المقبول:** تثبيت roothide Manager اختياريًا لاحقًا عبر Sileo/Zebra من repo رروثايد (قرار تشغيلي خارج نطاق الكود).

**قرار marker (مُحسم):** **لا يُنقل** كتابة/قراءة `/basebin/.AppIdentifier` (2.x: يكتبه DOBootstrapper أثناء bootstrap، يقرؤه `blacklist.m:builtinApps()` ليُعفي تطبيق Dopamine من الـ blacklist). 3.x يحقق نفس الإعفاء عبر: `builtinApps` الثابت (`com.opa334.Dopamine-roothide`) + `jbinfo(appIdentifier)` / `is_dopamine_app` لحماية أفعال رفع الصلاحية في jbdomain. **ملاحظة مراجعة (حدود معروفة):** إذا غُيّر bundle ID عن الافتراضي، يفقد التطبيق إعفاء blacklist (رفع الصلاحية محمي بآلية أخرى).

**تحقق:** لا بقايا URLs لـ opa334 في مسارات التحديث، `jbclient_roothide_jailbroken` متاح عبر `util.h → jbclient_xpc.h`، معالج `JBS_ROOTHIDE_JAILBROKEN_CHECK(4)` يعيد `*jailbroken=true` (نُقل في الخطوة 5). مراجعة الكود: موافقة (ملاحظتان: XPC مزدوج مقبول داخل dispatch_once، وحدود bundle-ID المذكورة).

**الخطوة 9 مكتملة — طبقة Application roothide مكتملة. النقل البرمجي كامل.**

### جلسة 2026-08-13 (ز): الإصلاحات — خطة `DOPAMINE3_ROOTHIDE_REMEDIATION_PLAN.md`

بعد التدقيق الجنائي (`DOPAMINE3_ROOTHIDE_AUDIT_REPORT.md` — BLOCKED حتى إصلاح بوابات الإقلاع)، نُفّذت خطة الإصلاح:

| Commit | الاكتشاف | الملف | الإصلاح |
|---|---|---|---|
| C1 | **CRIT-01** | `roothider/jailbreakd.c` | مهلة 10 ثانية (mach_msg بالملي ثانية) على استقبال checkin + قتل الطفل + إعادة تعيين `gSpawnedHookdPort/gSpawnedHookdPid` — لا تجميد إقلاع أبدًا |
| C2 | **HIGH-01** | `roothider/jailbreakd.c` | spawn لـ hookd بـ envp `_SafeMode=1` (مطابق `hookd_provider`) — لا وراثة لـ `DYLD_INSERT_LIBRARIES` (لا constructor لـ launchdhook داخل hookd) |
| C3 | **HIGH-02** | `roothider/jailbreakd.c` | `abort()` في `reactiveJailbreakdPort` → `JBLogError` + `return MACH_PORT_NULL` |
| C4 | **HIGH-03** | `main.m` + `roothider.m` | `boomerang_recoverPrimitives`/`ensure_dyld_trustcache`/`set_spinlock_fix` → غير قاتلة (log + متابعة متدهورة) — لا bootloop تحت SideStore |
| C5 | **HIGH-04** | `roothider.m` | كتلا kill الميتة (SIGQUIT+SIGKILL+ret=202) → `(void)` + تعليق |
| C6 | **MED-7** | `roothider.m` | `assert()` في `fix__iosConnect` (سياق launchd) → حراس مع `JBLogError` + return |
| C7 | **MED-8** | `jbdomain_dopamine.c` | نوع وسيط `port` في `jailbreakd_lookup` → `JBS_TYPE_MACH_SEND` (يُقرأ بـ `xpc_mach_send_copy_right`). **لم تُحذف `roothide_domain_allowed`** — مستخدمة في `jbserver_mach.c:52,184` و`jbdomain_systemwide.c:564` (توصية LOW-6 الأولى كانت خاطئة) |
| C8 | **MED-9** | `xpc_hook.m` | مؤكد آمن بالدليل (strchr يضمن `end>=bundle`) — تعليق توثيقي فقط |
| C9 | **MED-10** | `unsandbox1.m` | حراس `ksymbol(nchashtbl/nchashmask)` قبل `kread64` (لا قراءة عناوين kernel 0) |
| C9b | **MED-14** 🆕 | `unsandbox1.m` | `abort()` مكتشف في المراجعة (فرع `nc_entry` في سياق launchd) → `JBLogError` + `goto failed` |
| C10 | **MED-13** | `_external/basebin/.version` | `3.0.4` → `3.0.4-r1` (إصدار roothide + تسمية tipa + قناة التحديث) |
| C11 | LOW | `DOJailbreaker.m` | تعليق `idx = 7` (رقم سحري مطابق لـ 2.x) |
| OPT-C | — | `roothider/jailbreakd.c` | حارس منع spawn مزدوج لـ hookd في بناءات ENABLE_LOGS (`gSpawnedHookdPid != -1` → return) |

**موجة الفحص §8 أكملت الفحص عبر مكتبة roothider كاملة وكشفت نطاقات إضافية نُفذت بنفس المبدأ:**

| الملف | الإصلاح |
|---|---|
| `jailbreakd.c` | تحويل الـ 7 `assert()` (getpid==1 + init guards) إلى حراس ناعمة + **إصلاح باغ مراجعة**: `pthread_mutex_unlock` قبل `return` في حارس OPT-C (كان سيُجمّد mutex → deadlock في بناءات ENABLE_LOGS) |
| `common.m` | `abort()` في `loadAppStoredIdentifiers` (launchd كل إقلاع) → تهيئة القائمة أولًا ثم return ناعم؛ `assert(StoredAppIdentifiers != nil)` → حارس nil؛ `assert` الأربعة في `ensure_jbroot_symlink` (سياق launchd للثقة بالثنائيات) → حراس ناعمة؛ `assert(!postexploit)` → إزالة |
| `unsandbox2.m` | نفس إصلاح unsandbox1 (abortان في nc_entry → `goto failed` + حراس `ksymbol(nchashtbl/nchashmask)`) |
| `dyld_patch.m` | إزالة 4 asserts + abortين (مسارات krw غير قابلة للوصول عبر jbd no-ops) مع الحفاظ على فحص NULL والمنطق |
| `exec_patch.m` | 3 asserts → ناعمة + تحييد فخ kill (SIGQUIT+SIGKILL) |
| `exec_trace.m` | تحييد فخ kill في `finish_process_trace` |

**Panic مقبولة (موثقة):** `spawn_hook.c:131` (`abort_with_reason` عند فشل تحديث basebin أثناء userspace reboot — upstream 3.x مقصود لحالة غير قابلة للاسترداد، خارج نطاق المنفذ) + `roothider.m:359` (مقيدة بـ `#ifdef ENABLE_LOGS` — تطويرية فقط).

**تحقق:** كل فحوصات §8 ساكنة خضراء (صفر `abort()`/`assert(`/`kread64(ksymbol(`/فخوخ kill في مسارات launchd)؛ مراجعة الكود: موافقة بعد إصلاح الـ mutex. المتبقي: بوابة البناء (`roothide.yml` على Actions) ثم بروتوكول الجهاز (جهاز تجريبي + تدريب فشل: غياب `/basebin/hookd` يجب أن يقلع متدهورًا لا حلقيًا).