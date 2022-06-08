<div dir="rtl">

# تمرین گروهی ۱٫۰ - آشنایی با pintos

### شماره گروه: ۲

پارسا صلواتی - parsa.salavati@gmail.com  
سپهر صفری - sepehr.safari8731@gmail.com  
علیرضا شاطری - alirezashateri7@gmail.com  
محمدحسین قیصریه - mgheysariyeh@gmail.com  

## مقدمات

۱. [آموزش markdown](https://markdownguide.org)  
۲. https://meta.stackexchange.com/questions/150147/how-to-escape-the-angled-brackets-in-a-code-block  
۳. http://heather.cs.ucdavis.edu/~matloff/cgdb.html  
۴. https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_24.html  
۵. https://stackoverflow.com/questions/18088955/markdown-continue-numbered-list  
۶. http://intel80386.com/386htm/toc.htm  
۷. https://stackoverflow.com/questions/14745631/what-does-a-double-percent-sign-do-in-gcc-inline-assembly  
۸. https://stackoverflow.com/questions/1604968/what-does-a-colon-in-a-struct-declaration-mean-such-as-1-7-16-or-32  
۹. https://cgdb.github.io/docs/cgdb.html  
۱۰. http://www.jamesmolloy.co.uk/tutorial_html/  
۱۱. https://en.wikipedia.org/wiki/Data_structure_alignment  
۱۲. https://stackoverflow.com/questions/2748995/struct-memory-layout-in-c   

------
## آشنایی با pintos

### یافتن دستور معیوب


**۱.** `0xc0000008`  
**۲.** `0x8048624`  
**۳.** ```_start: mov 0xc(%ebp),%eax```  
**۴.** ```void _start(int argc, char* argv[]) { exit(main(argc, argv)); }```  
دستورات اسمبلی تابع:
<div dir="ltr">

```
0804861e <_start>:
 804861e:       55                      push   %ebp
 804861f:       89 e5                   mov    %esp,%ebp
 8048621:       83 ec 18                sub    $0x18,%esp
 8048624:       8b 45 0c                mov    0xc(%ebp),%eax
 8048627:       89 44 24 04             mov    %eax,0x4(%esp)
 804862b:       8b 45 08                mov    0x8(%ebp),%eax
 804862e:       89 04 24                mov    %eax,(%esp)
 8048631:       e8 6a fa ff ff          call   80480a0 &lt;main&gt;
 8048636:       89 04 24                mov    %eax,(%esp)
 8048639:       e8 5b 18 00 00          call   8049e99 &lt;exit&gt;
```

<div dir="rtl">

دو دستور اول، آدرس پایهٔ استک تابع قبل را (که برای استک خود تنظیم کرده بود) ذخیره می‌کند و مقدار جدید آن را برابر مقدار فعلی استک پوینتر می‌گذارد.  
پنج دستور بعدی آرگومان‌های تابع `main` را بر روی استک می‌گذارد تا تابع را صدا بزند.  
در نهایت در سه دستور آخر، تابع `main`صدا زده می‌شود و خروجی آن روی استک گذاشته شده و به تابع `exit` داده می‌شود.  
**۵.** یکی آرگومان‌های ورودی تابع `_start` که برای تابع `main` مورد نیاز بود، در آن آدرس قرار داشت.

### به سوی crash

**۶.** آدرس ترد همان مقدار چاپ شده روبه‌روی صفرمین ترد است: `0xc000e000`
<div dir="ltr">

```
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000edec "\020", priority = 31, allelem = {prev = 0
xc0036c10 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036c20 <ready_list>, next = 0xc0036c28 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000
e020, next = 0xc0036c18 <all_list+8>}, elem = {prev = 0xc0036c20 <ready_list>, next = 0xc0036c28 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
```
<div dir="rtl">

**۷.** خروجی gdb:
<div dir="ltr">

```
#0  process_execute (file_name=file_name@entry=0xc0007d50 "do-nothing") at ../../userprog/process.c:34
#1  0xc0020936 in run_task (argv=0xc0036acc <argv+12>) at ../../threads/init.c:271
#2  0xc00204df in run_actions (argv=0xc0036acc <argv+12>, argv@entry=0xc0036ac8 <argv+8>) at ../../threads/init.c:318
#3  0xc0020a17 in main () at ../../threads/init.c:130
```
<div dir="rtl">

قطعه کدهای c به ترتیب:
<div dir="ltr">

```c
#0 sema_init(&temporary, 0);
#1 process_wait(process_execute(task));
#2 a->function(argv);
#3 run_actions(argv);
```
<div dir="rtl">

**۸.** نام ریسهٔ اجرایی `main` است.
<div dir="ltr">

```
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000eeac "\001", priority = 31, allelem = {prev = 0
xc0036c10 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0038614 <temporary+4>, next = 0xc003861c <temporary+12>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000
e020, next = 0xc010a020}, elem = {prev = 0xc0036c20 <ready_list>, next = 0xc0036c28 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #2: 0xc010a000 {tid = 3, status = THREAD_RUNNING, name = "do-nothing\000\000\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0xc010
4020, next = 0xc0036c18 <all_list+8>}, elem = {prev = 0xc0036c20 <ready_list>, next = 0xc0036c28 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
```
<div dir="rtl">

**۹.** در تابع `process execute`:
<div dir="ltr">

```
tid = thread_create(file_name, PRI_DEFAULT, start_process, fn_copy);
```

<div dir="rtl">

**۱۰.** اطلاعات `if_`:  
<div dir="ltr">

```
{edi = 0x0, esi = 0x0, ebp = 0x0, esp_dummy = 0x0, ebx = 0x0, edx = 0x0, ecx = 0x0, eax = 0x0, gs = 0x23, fs = 0x23, es = 0x23, ds = 0x23, vec_no = 0x0, error_code = 0x0,
frame_pointer = 0x0, eip = 0x0, cs = 0x1b, eflags = 0x202, esp = 0x0, ss = 0x23}
```
<div dir="rtl">

**۱۱.** به نظر می‌رسد به خاطر مقادیر داخل `if_` (که الان در رجیسترها هستند) به ویژه دو مقدار `if_.cs` و `if_.ss` است. در واقع دو مقدار `SEL_UCSEG` و `SEL_UDSEG` به ترتیب به معنای قسمت کد کاربر و قسمت دادهٔ کاربر هستند. این ساختار و به ویژه این دو مقدار باعث تغییر از حالت کرنل به کاربر شده‌اند.  
**۱۲.**
<div dir="ltr">

```
eax            0x0                 0
ecx            0x0                 0
edx            0x0                 0
ebx            0x0                 0
esp            0xc0000000          0xc0000000
ebp            0x0                 0x0
esi            0x0                 0
edi            0x0                 0
eip            0x804861e           0x804861e
eflags         0x202               [ IF ]
cs             0x1b                27
ss             0x23                35
ds             0x23                35
es             0x23                35
fs             0x23                35
gs             0x23                35

```
<div dir="rtl">

همان مقدارها هستند. مقادیر ساختار `if_` در استک ریخته شده و سپس توسط دستور `iret` وارد رجیسترها شدند.  
**۱۳.**

<div dir="ltr">

```
#0  _start (argc=-268370093, argv=0xf000ff53) at ../../lib/user/entry.c:6
#1  0xf000ff53 in ?? ()
```
<div dir="rtl">

### دیباگ

**۱۴.** مقدار esp بلافاصله بعد از شروع برنامهٔ کاربر برابر `PHYS_BASE` است، در صورتی که دو آرگومان `argv` و `argc` ورودی برنامهٔ کاربر هستند. بنابراین اولاً باید آدرس متغیر `argv` را در استک بنویسیم. دوماً باید پس از نوشتن تمام آدرس‌ها، مقدار esp مضربی از ۱۶ باشد. بنابراین دو خط زیر را به کد اضافه کردیم:

<div dir="ltr">

```
process.c:load
if (!setup_stack(esp))
  goto done;

*((char***) (*esp - 0xf)) = (char**) (*esp - 0xb);
*esp -= 0x14;
```

<div dir="rtl">

توجه کنید که باید بعد از همهٔ آدرس‌ها، یک مقدار ساختگی (مثلاً صفر) برای آدرس بازگشت نیز قرار دهیم. و می‌دانیم که کل صفحهٔ حافظه، قبل از اختصاص به برنامهٔ صفر شده است.

**۱۵.** ۱۲. قبل از اضافه کردن آدرس بازگشت به ابتدای استک، `esp` مضربی از ۱۶ است. آدرس بازگشتی هم ۴بایت دارد بنابراین در اول اجرای برنامه این مقدار باید ۱۲ باشد.  
**۱۶.**

<div dir="ltr">

```
0xbfffffa8:	0x00000001	0x000000a2
```
<div dir="rtl">

**۱۷.** دقیقاً همان مقدارها به ترتیب هستند.  
**۱۸.** می‌توان منتظر ماند تا یک ترد بعد از تمام شدن اطلاع‌رسانی کند. چون در کل process.c متغیر temporary استاتیک وجود دارد، باید طوری مطمئن شد که ۲ ترد در صورت تمام شدن همزمان، به درستی اطلاع‌رسانی می‌کنند. برای اینکار از سمافور استفاده شده است. `sema_down` در تابع `process_wait` قرار دارد.  
**۱۹.** نام ترد `main` و آدرسش `0xc000e000` است. ترد `idle` همچنان در سیستم‌عامل وجود دارد.
