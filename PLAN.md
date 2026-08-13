# PLAN — تحويل Dopamine3-Roothide إلى roothide كامل (تخفي فيزيائي)

**الهدف:** جعل المشروع "roothide حرفيًا" — نفس معمارية التخفي الفيزيائي لـ roothide 2.x —
لكن مبنيًا على نواة Dopamine 3 لدعم **كل الأجهزة** المدعومة في Dopamine الأصلية (A12→A17).

**الوضع الحالي (هجين):** معمارية حقن roothide (launchdhook/dyldhook/fakelib/systemhook) لكن
تخزين rootless قياسي (`/private/preboot/<hash>/dopamine-XXX/procursus/`) + symlink `/var/jb`.

---

## 1. الركائز الأربع للتخفي الكامل (من 2.x roothide)

| الركيزة | 2.x roothide | نسختنا الحالية |
|---|---|---|
| موقع التخزين | `/var/containers/Bundle/Application/.jbroot-<16hex>/` | `/private/preboot/<hash>/dopamine-XXX/procursus/` |
| بناء المسارات | `jbrootPrefix()` + `rootfsPrefix()` (`/rootfs/`) | `JBROOT_PATH()` (المسار المطلق) |
| `/var/jb` | غير موجود نهائيًا | symlink موجود |
| الـ brand | `jbrand` (رقم عشوائي بفحص checksum) | `dopamine-<6أحرف>` فقط |

> ملاحظة: حقل `jbrand` موجود فعلًا في `info.h` (سطر 47) — النية كانت موجودة منذ البداية.

---

## 2. خطة التنفيذ (7 مراحل)

### المرحلة 1 — طبقة التخزين المخفي (الأساس)
- نقل `find_jbroot` / `is_jbroot_name` / `jbrand_new` / `jbrand_current` / `resolve_jbrand_value`.
- نقل `jbrootPrefix` / `rootfsPrefix`.
- تعديل `JBROOT_PATH`/`ROOTFS_PATH` في `jbroot.h` لتُرجع المسار المخفي.
- تحديث `locateJailbreakRoot` و`ensureJailbreakRootExists`.

### المرحلة 2 — الـ Bootstrap
- تكييف `DOBootstrapper` ليفك الـ bootstrap إلى المسار المخفي.
- **القرار (محسوم):** نعتمد **الـ procursus القياسي** (`bootstrap_1800/1900.tar.zst`) — نفس ما فعله 2.x roothide (لم يستخدم procursus مخصصًا للـ bootstrap الأساسي؛ `roothide.github.io/procursus` كان لمصادر apt فقط). لا اعتماد على مستودع خارجي.

### المرحلة 3 — تفعيل `@loader_path/.jbroot`
- إعادة التوجيه موجودة في `expandAtLoaderPath` لكنها غير مستخدمة (الثنائيات تشير للمسار المطلق).
- جعل basebin + bootstrap يشيران إلى `.jbroot`.

### المرحلة 4 — اكتشاف الـ jbroot في launchdhook
- launchdhook يشتق `rootPath` من مسار الـ dylib — سيستخدم `find_jbroot` (أو marker الـ jbrand).

### المرحلة 5 — إزالة `/var/jb` + إضافة `/rootfs/`
- حذف منطق symlink `/var/jb` نهائيًا.
- إضافة bind-mount لـ `/rootfs/` (الوصول للـ rootfs الأصلي).

### المرحلة 6 — دعم كل الأجهزة (A12→A17)
- سلسلة الـ exploit تدعم كل الأجهزة أصلًا (ClearSword/DarkSword + momentarius/Titan/dmaFail).
- التحقق من المسارات الخاصة (spinlock_fix = iOS 15 arm64e فقط).

### المرحلة 7 — البناء والاختبار
- بناء كامل + فحص الـ tipa + اختبار على الجهاز.

### المرحلة 8 — توحيد المعرّف + تدفق الجيلبريك roothide (جديد — من ROOTHIDE_DIFF_ANALYSIS.md)

> اكتُشفت عبر الـ diff المفصّل مع 2.4.9. هذه الفجوات **مستقلة** عن التخزين المخفي
> (المراحل 1-5) ويجب إنجازها أيضًا لنصل إلى "roothide حرفيًا".

#### 8a. Bundle ID — ✅ محسوم: يُبقى `com.opa334.Dopamine` (بدون تغيير)
- **قرار المستخدم:** البقاء على `com.opa334.Dopamine` (لا تحويل إلى `-roothide`).
- `jbctl/src/main.m:170` يستخدم `com.opa334.Dopamine` أصلًا → متوافق.
- `SENSITIVE_APP_IDENTIFIERS` (common.h) تحوي المعرّفين معًا → متوافقة.
- **تعديل واحد مطلوب (أُنجز):** `blacklist.m` `builtinApps` أُضيف لها `com.opa334.Dopamine`
  بجوار `-roothide` (قائمة الإعفاء من الحجب تطابق المعرّف الفعلي).

#### 8b. تدفق الجيلبريك في DOJailbreaker.m (خطوات roothide المفقودة)
- `randomizeAndLoadBasebinTrustcache(JBROOT_PATH("/basebin/"))` — تحميل trustcache عشوائي (إخفاء).
- `ensure_dyld_trustcache(JBROOT_PATH("/basebin/.fakelib/dyld"))` — رفع cdhash الـ fakelib dyld للنواة.
- `setenv("DISABLE_TWEAKS", "1", 1)` — منع تحميل tweaks أثناء الجيلبريك.
- `setenv("PATH", "/sbin:...:/rootfs/sbin:...")` — يعتمد على المرحلة 5.
- قراءة `dyldPatchEnabled` من التفضيلات (أو إبقاؤها معطّلة = المسار البيئي).
- كشف "جيلبريك آخر نشط" → رفض بأمان.

#### 8c. منطق hideJailbreak (زر الإخفاء)
- `DOEnvironmentManager.m` — إخفاء `/var/jb` + العمليات + unmount fakelib مؤقتًا.
- يعتمد جزئيًا على المراحل 1-5 (`/rootfs/` + `.jbroot`)، لكن الهيكل العام يمكن نقله الآن.

### المرحلة 9 — واجهات roothide + الموارد (جديد — من ROOTHIDE_DIFF_ANALYSIS.md)

> واجهات التطبيق (UI) وموارده — ما طلبه المستخدم ولم يكن مدرجًا سابقًا.

#### 9a. روابط التحديث → مستودعنا 🔴
- `DOUIManager.m:74` — `api.github.com/repos/roothide/Dopamine2-roothide/releases` → `Phantom-fahad/Dopamine3-Roothide`.
- `DOUpdateViewController.m:122` — `github.com/roothide/Dopamine2-roothide/releases` → مستودعنا.
- ملاحظة: الحالي يشير لمستودع 2.x القديم (خاطئ).

#### 9b. كشف "جيلبريك آخر نشط" (تنبيه)
- `DOMainViewController.m` + `DOJailbreaker.m` — تنبيه "Your device currently has another jailbreak activated, please reboot device." غير موجود عندنا.
- مرتبط بـ 8b (كشف جيلبريك آخر قبل البدء).

#### 9c. الموارد: `roothideapp.deb` (تطبيق RootHide Manager) 🟡
- 2.x يثبّت تطبيق RootHide (واجهة blacklist التطبيقات + إخفاء الجيلبريك) عبر deb.
- عندنا: **مفقود** — غير موجود في `Resources/`.
- **القرار (محسوم):** البناء من المصدر `roothide/RootHideManagerApp` (Objective-C، MIT، نشط حتى 2026).
- **خطوات ملموسة:**
  1. إضافة submodule `roothide/RootHideManagerApp` (أو vendoring).
  2. بناؤه عبر THEOS (`Makefile` موجود → ينتج deb `com.roothide.manager`، arm64+arm64e، iOS 15+).
  3. ضمّ الـ deb الناتج إلى `Application/Dopamine/Resources/roothideapp.deb`.
  4. إضافة خطوة البناء في CI (roothide.yml).
- **ملاحظة توافق:** التطبيق يكتب `RootHideConfig.plist` ويخاطب domain الـ roothide في jbserver — بما أننا حافظنا على نفس البروتوكول، سيعمل كما هو.

#### 9d. الـ credits + الأيقونة + الترجمة
- `Credits.plist` + `DOCreditsViewController.m` (رابط repo → مستودعنا).
- أيقونة التطبيق (branding roothide) — قرار تجميلي.
- `Localizable.strings` (نصوص roothide الجديدة).

---

## 3. تقدير الجهد

| المرحلة | الجهد | التعقيد |
|---|---|---|
| 1 (التخزين المخفي) | متوسط | ⭐⭐⭐ |
| 2 (bootstrap) | كبير | ⭐⭐⭐⭐ |
| 3 (تفعيل .jbroot) | كبير | ⭐⭐⭐⭐ |
| 4 (launchdhook) | صغير-متوسط | ⭐⭐ |
| 5 (/var/jb + /rootfs) | متوسط | ⭐⭐⭐ |
| 6 (كل الأجهزة) | تحقق فقط | ⭐ |
| 7 (بناء+اختبار) | مستمر | ⭐⭐ |
| 8a (bundle ID) | ✅ منجز | ⭐ |
| 8b (تدفق DOJailbreaker) | متوسط | ⭐⭐⭐ |
| 8c (hideJailbreak) | متوسط | ⭐⭐⭐ |
| 9a (روابط التحديث) | صغير جدًا | ⭐ |
| 9b (كشف جيلبريك آخر) | صغير | ⭐ |
| 9c (roothideapp.deb) | قرار + متوسط | ⭐⭐ |
| 9d (credits/أيقونة/ترجمة) | صغير-تجميلي | ⭐ |

---

## 4. قرارات حاسمة

1. **الـ Bootstrap:** ✅ **محسوم — القياسي + تكييف** (نفس نهج 2.x).
   - نقطة معماريّة: `jbroot.h` متطابق مع 2.x (`JBROOT_PATH` = `get_jbroot() + path`)، فالتوجيه مجرّد عبر `get_jbroot()` — التغيير الجوهري هو جعلها ترجع المسار المخفي.
2. **الـ rootfs (`/rootfs/`):** هل هو مطلوب فعلًا أم يمكن الاستغناء عنه؟
3. **ترتيب التنفيذ:** المرحلة 1+4 أساسًا صلبًا، ثم 2+3.

---

## 5. المخاطر

- **الـ procursus الخارجي:** قد لا يكون محدثًا لأحدث حزم iOS 18.
- **تفعيل `.jbroot`:** يتطلب إعادة بناء basebin بمسارات نسبية (ليس مجرد تبديل ماكرو).
- **التوافق العكسي:** التخزين الجديد يلغي المسار القديم — يجب ترحيل المستخدمين الحاليين.
