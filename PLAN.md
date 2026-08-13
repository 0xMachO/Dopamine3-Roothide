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
