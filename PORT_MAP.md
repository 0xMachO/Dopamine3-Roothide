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

1. **jailbreakd → hookd:** هل ننقل خدمات roothider (unsandbox, exec_patch, xpc_hook, dyld_patch, blacklist) داخل hookd الجديد أم نحوّل jailbreakd القديم بجانبه؟ — التوصية: **دمج في hookd** (لأن 3.x بنى hookd ونقل إليه كل منطق jailbreakd، والنقل المزدوج سيكون انفصاميًا)
2. **bootstrap integration:** كيف يحصل roothide Bootstrap على الجذر — عبر DOBootstrapper 3.x (procursus) أم عبر ملفات Resources/bootstrap_*.tar.zst القديمة؟
3. **استغلال palera1n** — مؤكد الإسقاط
4. **الإصدار:** basebin/.version = قيمته الجديدة في نسختنا

## 4. المخاطر الحاسمة (من فحص الكود الفعلي)

- **dyldhook** في 3.x لديه `CHECK_SELF_CONTAINED` و `FILES_IOS16-17`/`FILES_IOS18+` generated — roothider.c يخطف `expandAtLoaderPath` عبر mangled symbol لـ dyld 4/5/6 — الرمز نفسه موجود في 3.x لكن البنية متغيرة (لتشغيله على ios26/18+ تغيّرت توقيعات الدوال). **يحتاج إعادة كتابة فعلية لكل متغير
- **libjailbreak** في 3.x غيّر واجهات (`jbclient_root_trustcache_add_cdhash` معطّلة في 2.x-roothide، و3.x أضافت `hookd_external`) — دمج دقيق مطلوب
- **jbserver 3.x** أكبر وأحدث (216 سطر jbserver_mach vs 175) — إعادة تنفيذ فتحة roothide داخل dopamine domain
- **primitives_IOSurface.m / signatures.c / trustcache.c / util.*** في 3.x** — اختلفت بالكامل، يجب أخذ نسخ 3.x ودمج شيفرة roothide فوقها