# ROOTHIDE DIFF ANALYSIS — Dopamine2-roothide (2.4.9-beta) مقابل upstream Dopamine 2.4.9

**الهدف:** عزل "ماذا غيّر roothide بالضبط فوق Dopamine الأصلي" بدقة،
وتصنيفه إلى (أ) تعديلات roothide يجب نقلها لـ 3.0.4، و(ب) تغييرات استقرار
Dopamine نفسها لا يجب المساس بها، ومطابقة كل عنصر بحالته في مشروعنا 3.x.

**منهج الـ diff:**
- `git diff upstream/2.4.9 → roothide2x/2.4.9-beta` لعزل دلتا roothide.
- ملاحظة: الـ merge-base قديم (فبراير 2024) لأن roothide دمج upstream بشكل متقطع،
  فتظهر بعض فروقات (palera1n، أيقونات، تعديلات exploits) هي في الحقيقة **drift من
  upstream** وليست من roothide. تم تمييزها بالفحص المضموني لكل ملف.

---

## 1. الخلاصة التنفيذية

| الطبقة | حالة النقل في 3.x |
|---|---|
| **الطبقة المنخفضة (roothide core)** — launchdhook/dyldhook/systemhook/roothidehooks/libjailbreak-roothider | ✅ **منقولة بالكامل** |
| **طبقة التطبيق (الواجهات + تدفق الجيلبريك + الإخفاء)** | ⚠️ **منقولة جزئيًا — فجوات محددة أدناه** |
| **التخزين المخفي (`.jbroot-`)** | ❌ **غير منقولة** — موضوع PLAN.md (7 مراحل) |

**المحصلة:** الجوهر المنخفض سليم ومكتمل. الفجوات المتبقية كلها في طبقة التطبيق،
وهي بالضبط "واجهات roothide" التي طلبت نقلها الآن.

---

## 2. ملفات roothide الجديدة (غير موجودة في upstream) — وحالتها عندنا

### A. مكوّنات منقولة بالكامل ✅
| المكوّن | الدور | عندنا |
|---|---|---|
| `BaseBin/launchdhook/**` | حقن launchd + jbserver | ✅ |
| `BaseBin/dyldhook/**` | fakelib + إعادة توجيه المسارات | ✅ |
| `BaseBin/systemhook/**` | hooks النظام (posix_spawn/execve/fcntl) | ✅ |
| `BaseBin/roothidehooks/**` | hooks cfprefsd/installd/lsd/springboard | ✅ |
| `BaseBin/libjailbreak/src/roothider/**` (~20 ملف) | unsandbox/xpc_hook/exec_patch/dyld_patch | ✅ |
| `libjailbreak/src/jbclient_roothide.c` | عميل roothide | ✅ |
| `libjailbreak/src/trustcache_fs.c/h` | trustcache عبر نظام الملفات (حقن env) | ✅ |
| `libjailbreak/src/stock_fixes.c/h` | إصلاحات نظام | ✅ |
| `libjailbreak/src/roothider.h` | تصريحات الدوال | ✅ |
| `libjailbreak/src/dyldhook/src/roothider.c/S` | إعادة كتابة `@loader_path/.jbroot` | ✅ |
| حقول `info.h` (jbrand/palera1n/dyld_patch_enabled/nchashtbl/inpcb/socket/protosw…) | offsets للنواة | ✅ (كلها موجودة + تُحل على iOS 18.3.2 كما ظهر في سجل الجيلبريك) |

### B. مكوّنات لا تُنقل (محسوم)
| المكوّن | السبب |
|---|---|
| `BaseBin/jailbreakd/**` (server.m/main.m) | ثنائي jailbreakd المستقل في 2.x → **استُبدل بـ hookd** في 3.x (منقول سليمًا) |
| `BaseBin/bootstrapper/**` | **stub فارغ** (`return 0` + TODO) — لا قيمة وظيفية |

### C. تعديلات على ملفات مشتركة — تم فحصها
| الملف | ما فعله roothide | القرار عندنا |
|---|---|---|
| `BaseBin/forkfix/src/main.c` | `dlopen(systemhook.dylib)+dlsym(litehook_hook_function)` بدل الربط المباشر | ⚠️ عندنا ربط مباشر `litehook.h` — **وظيفيًا سليم** (البناء نجح، لا تعارض hooks) — ملاحظة فقط |
| `BaseBin/boomerang/src/main.c` | أضاف `unrestrict(1, roothide_patch_proc, true)` | ✅ **لا يُنقل** — هذا مسار dyld_patch (معطّل في 3.x)، الحقن عندنا بيئي |
| `BaseBin/jbctl/src/main.m` | `trustcache add /path/to/macho` + bundle ID `-roothide` | ⚠️ **نصف منقول**: منطق المسار منقول، لكن معرّف `-roothide` لم يُوائم |
| `BaseBin/libjailbreak/src/info.h` | حقول roothide (jbrand + kernelStruct إضافية) | ✅ منقول |
| الـ exploits (badRecovery/kfd/dmaFail/multicast/weightBufs) | **drift من upstream وليس roothide** | ✅ **لا يُلمس** (استقرار Dopamine) |

---

## 3. الفجوات المكتشفة (غير موجودة في خطة PLAN.md السابقة)

### GAP-1 — معرّف التطبيق (Bundle ID) — ✅ محسوم: يُبقى `com.opa334.Dopamine`
- **قرار المستخدم:** البقاء على `com.opa334.Dopamine` (لا تحويل إلى `-roothide`).
- **التحقق:**
  - `jbctl/src/main.m:170` يستخدم `com.opa334.Dopamine` أصلًا → متوافق.
  - `DOPreferenceManager.m:27` — `com.opa334.Dopamine.plist` → متوافق.
  - `SENSITIVE_APP_IDENTIFIERS` (common.h) تحوي المعرّفين معًا → متوافقة.
- **تعديل واحد (أُنجز):** `blacklist.m` `builtinApps` أُضيف لها `com.opa334.Dopamine`
  بجوار `-roothide` (قائمة الإعفاء من الحجب تطابق المعرّف الفعلي).

### GAP-2 — تدفق الجيلبريك في DOJailbreaker.m 🔴
تعديلات roothide على `DOJailbreaker.m` غير منقولة بالكامل. المقارنة:

| خطوة roothide | عندنا |
|---|---|
| `randomizeAndLoadBasebinTrustcache(JBROOT_PATH("/basebin/"))` | ❌ **مفقودة** (0) |
| `ensure_dyld_trustcache(JBROOT_PATH("/basebin/.fakelib/dyld"))` | ❌ **مفقودة** (0) |
| `setenv("DISABLE_TWEAKS", "1")` (لا tweaks أثناء الجيلبريك) | ❌ **مفقودة** (0) |
| `setenv("DYLD_INSERT_LIBRARIES", .../systemhook.dylib)` | ✅ موجودة |
| `setenv("PATH", ...:/rootfs/sbin:...)` | ❌ مرتبط بـ rootfs (غير منقول) |
| `hideJailbreak` / `dyldPatchEnabled` preference | ❌ **مفقودة** (0) |
| كشف "جيلبريك آخر نشط" → رفض | ⚠️ يحتاج فحص |

### GAP-3 — منطق hideJailbreak في DOEnvironmentManager.m 🟡
- `hideJailbreak` (0) + `jbrootPrefix` (0) + `rootfsPrefix` (0) + `find_jbroot` (0) + `jbrand` (0) — كلها غير موجودة.
- **ملاحظة:** `find_jbroot`/`jbrand` مشمولة في PLAN.md المرحلة 1 (بدأناها)، لكن
  `hideJailbreak` (زر الإخفاء: إخفاء `/var/jb` + العمليات + unmount fakelib) **غير مشمولة** في الخطة.
- **لاحظ:** زر `hideJailbreak` موجود في واجهة `DOSettingsController.m` (13 إشارة — من
  upstream 3.x)، لكن **المنطق الفعلي** في `DOEnvironmentManager.m` غير موجود → الزر
  موجود بلا فعل حقيقي في طبقة roothide. يجب ربطهما.

### GAP-4 — إعداد "dyld patch" في الواجهة 🟢 (ثانوي)
- 2.x roothide أضاف toggle "Enable dyld patch / Spinlock Fix" في `DOSettingsController.m`.
- عندنا: `dyldPatch` (0) — غير موجود.
- **القرار:** بما أن الحقن عندنا بيئي (`dyld_patch_enabled = false` افتراضيًا)، هذا
  الإعداد **vestigial**. يمكن: (أ) حذفه نهائيًا، أو (ب) نقله للتوافق الشكلي فقط. لا أثر على السلامة.

---

## 4. التصنيف النهائي (roothide يُنقل vs استقرار dopamine لا يُلمس)

### ✅ يُنقل (roothide)
| العنصر | الحالة |
|---|---|
| الطبقة المنخفضة كاملة | ✅ منقول |
| `find_jbroot`/`jbrand`/`jbrootPrefix`/`rootfsPrefix` | ⏳ PLAN المرحلة 1 (بدأنا) |
| إزالة `/var/jb` + إضافة `/rootfs/` | ⏳ PLAN المرحلة 5 |
| تفعيل `@loader_path/.jbroot` | ⏳ PLAN المرحلة 3 |
| **bundle ID** | ✅ محسوم: يُبقى `com.opa334.Dopamine` |
| **تدفق DOJailbreaker (randomize/trustcache/DISABLE_TWEAKS)** | ❌ GAP-2 (المرحلة 8b) |
| **hideJailbreak (زر الإخفاء)** | ❌ GAP-3 (المرحلة 8c) |
| **روابط التحديث → مستودعنا** | ❌ GAP-5 (المرحلة 9a) — حاليًا تشير لـ 2.x القديم |
| **كشف "جيلبريك آخر نشط"** | ❌ (المرحلة 9b) |
| **`roothideapp.deb` (تطبيق RootHide)** | ❌ مفقود (المرحلة 9c — قرار) |
| **credits + الأيقونة + الترجمة** | ⏳ (المرحلة 9d) |

### ✅ لا يُلمس (استقرار Dopamine نفسه)
- الـ exploits (ClearSword/DarkSword/momentarius/Titan/dmaFail/weightBufs) — كل تعديلات 2.x عليها drift من upstream.
- `boomerang`/`forkfix` (الربط المباشر لـ litehook سليم).
- طبقة النواة (kernel.c/physrw/primitives) في 3.x — نسخة upstream، والحقول التي يحتاجها roothide موجودة وتحُل صحيحًا.

---

## 5. الإجراء المقترح

1. **GAP-1 (bundle ID):** ✅ محسوم — يُبقى `com.opa334.Dopamine` + `builtinApps` أُصلحت.
2. **GAP-2 (تدفق DOJailbreaker):** المرحلة 8b — `randomizeAndLoadBasebinTrustcache` + `ensure_dyld_trustcache` + `DISABLE_TWEAKS`.
3. **GAP-3 (hideJailbreak):** المرحلة 8c — يعتمد جزئيًا على المراحل 1-5.
4. **GAP-5 (روابط التحديث):** المرحلة 9a — تغيير `Dopamine2-roothide` → `Phantom-fahad/Dopamine3-Roothide`.
5. **الموارد (roothideapp.deb):** المرحلة 9c — قرار نقل/استغناء.
