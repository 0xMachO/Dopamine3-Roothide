# تقرير تنفيذ RootHide 3 — تغييرات محلية قيد التحقق

## الحالة

نُفذت التغييرات محلياً فقط فوق النسخة المستنسخة من `main`. لم يُنشأ commit، ولم تُدفع أي تغييرات إلى GitHub، ولم يُبنَ أو يُثبّت TIPA على جهاز. هذا مقصود لأن بيئة العمل الحالية لا توفر Xcode أو iPhoneOS SDK، ولأن التغيير يمس launchd وdyld ويجب أن يمر ببناء macOS واختبار device-gated قبل النشر.

## ما تم تنفيذه

تمت إزالة دورة الـmount العامة من مسار Dopamine الرئيسي. لم يعد `DOJailbreaker` يستدعي حماية preboot أو يركب FakeLib فوق `/usr/lib`. بقي إنشاء dyld الخاص داخل الجذر المخفي وتحميل trustcache الخاص به، ثم صار تحضير `launchdhook` هو المسؤول عن نشر مسار injection ديناميكي وفشل آمن عند عدم توفره.

| طبقة | التغيير | المقصود |
|---|---|---|
| `DOJailbreaker` | استبدال `createFakeLib` بـ`preparePrivateDyld` وإزالة `applyProtection` وFakeLib mount. | إبقاء dyld خاصاً بـJBROOT من دون أثر bindfs عام. |
| `jbctl` | تحويل أوامر `protection` و`fakelib` إلى compatibility no-ops وإلغاء تفعيل الحماية من `startup`. | منع إعادة إنشاء mounts عبر مسار قديم. |
| `launchdhook` | إضافة `prepare_dynamic_systemhook_alias`. | تسمية per-brand عبر `systemhook.dylib.<brand>` وإتاحة alias بواسطة RootHide `unsandbox`. |
| `systemhook/common` | إزالة macro المسار الثابت وإضافة `systemhook_injection_path` و`systemhook_strip_injection`. | حقن alias ديناميكي للتطبيقات المسموحة وحذفه مركزياً من بيئات التطبيقات المعزولة. |
| `dyldhook` | اشتراط اسم alias الديناميكي لعملية check-in. | منع رجوع مسار `/usr/lib/systemhook.dylib` الثابت. |
| `DOEnvironmentManager` | إزالة واجهات FakeLib/preboot وإلغاء إعادة إنشاء `/var/jb`. | جعل سياسة RootHide دائماً hidden في runtime العادي. |
| CLI وRPATH | إزالة حقن CLI الثابت ومسارات `/var/jb` النشطة، وإزالة RPATH `/var/jb/Library/Frameworks` من watchdoghook. | تقليل البصمات الموروثة من Dopamine الفانيلي. |

## ما تم التحقق منه محلياً

| التحقق | النتيجة |
|---|---|
| `Tools/verify_roothide3_static.py` | نجح. يفحص عدم وجود `bindfs` نشط في `jbctl` أو مسار حقن ثابت أو ربط RPATH موروث، ويؤكد وجود alias ديناميكي وتنظيف بيئة blacklist. |
| `git diff --check` | نجح؛ لا توجد أخطاء whitespace في الفروق. |
| فاحص RootHide 3 الساكن | لا توجد نتائج HIGH. بقيت 55 نتيجة MEDIUM، أغلبها مراجع قديمة أو تعليقات أو أدوات رفع واختبار لا تدخل runtime الطبيعي، ويجب معالجتها بمرحلة تنظيف مستقلة لا في تغيير launchd نفسه. |
| بناء iOS محلي | لم ينفذ؛ `xcodebuild` و`xcrun iphoneos` غير متوفرين في بيئة Linux الحالية. |
| اختبار runtime على الجهاز | لم ينفذ بعد؛ إلزامي قبل commit أو إصدار. |

## المخاطر التي لا يجوز تجاهلها

التحويل يستبدل FakeLib mount بآلية RootHide namecache alias الموجودة أصلاً في المشروع. لذلك يجب إثبات أن `unsandbox2` ينجح على iOS 18.3.2 وأن alias الديناميكي يبقى صالحاً عبر userspace reboot. يجب أيضاً التحقق من أن تحديث BaseBin لا يترك alias قديماً أو source غير متزامن. لا تُنشر هذه التغييرات حتى تمر كل صفوف `ROOT_HIDE3_DEVICE_VALIDATION.md`.

لا يمكن الجزم بأن كل تطبيق خارجي سيتوقف عن الإبلاغ عن بيئة معدّلة؛ الهدف الذي أثبته هذا التنفيذ هو إزالة المؤشرات البنيوية التي شخصناها: bindfs العالمي، `/var/jb` وقت التشغيل، ومسار systemhook الثابت. أي نتيجة مرتبطة بخادم خارجي أو attestation أو سياسة تطبيق مستقلة تقع خارج ما يمكن أن يضمنه مشروع محلي.

## الملفات المضافة للمراجعة

- `ROOT_HIDE3_RUNTIME_CONTRACT.md` يعرّف lifecycle المقصود وشروط القبول.
- `ROOT_HIDE3_DEVICE_VALIDATION.md` يعرّف مصفوفة الاختبارات العملية لـiPhone 11 / iOS 18.3.2.
- `Tools/verify_roothide3_static.py` اختبار ساكن قابل لإعادة التشغيل قبل البناء.
- مهارة محلية باسم `roothide3-integration-audit` لفحص الإصدارات اللاحقة، مع فاحص مستقل لقواعد التصميم.

## الخطوة التالية الصحيحة

ينبغي بناء artifact من working tree على macOS مع Xcode/SDK المطابقين لمسار CI، ثم تثبيته على جهاز اختبار مع recovery path جاهز. بعدها فقط تُجمع أدلة mount table وRootHide Manager وسجلات launchd والـblacklist للتمديدات. إن أخفق alias الديناميكي، يجب أن يظل الحقن مغلقاً، لا أن يُعاد FakeLib bind mount كحل سريع.
