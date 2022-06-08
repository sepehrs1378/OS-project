تمرین گروهی ۳ - مستند طراحی
======================

گروه ۲
-----

>>‫نام و آدرس پست الکترونیکی اعضای گروه را در این قسمت بنویسید.

پارسا صلواتی - parsa.salavati@gmail.com

سپهر صفری - sepehr.safari8731@gmail.com

علیرضا شاطری - alirezashateri7@gmail.com

محمدحسین قیصریه - mgheysariyeh@gmail.com

مقدمات
----------

>>‫ ‫‫اگر نکات اضافه‌ای در مورد تمرین یا برای دستیاران آموزشی دارید در این قسمت  ‫بنویسید.

>>‫ لطفا در این قسمت تمامی منابعی (غیر از مستندات Pintos، اسلاید‌ها و دیگر منابع‫ ‫درس) را که برای تمرین از آن‌ها استفاده کرده‌اید در این قسمت بنویسید.

بافر کش
============

داده‌ساختار‌ها و توابع
---------------------

>>‫ در این قسمت تعریف هر یک از `struct` ها، اعضای `struct` ها، متغیرهای سراسری یا ایستا، `typedef` ها یا `enum` هایی که ایجاد کرده‌اید یا تغییر داده‌اید را‫ بنویسید و دلیل هر کدام را در حداکثر ۲۵ کلمه توضیح دهید.

```c
#define MAX_CACHE_SIZE 64 + 10
hash_table ht;
struct cache_block {
	struct lock lock;
	int data[BLOCK_SECTOR_SIZE];
	bool valid;
};
struct cache_block cache[MAX_CACHE_SIZE];
```


الگوریتم‌ها
------------

>>‫ توضیح دهید که الگوریتم مورد استفاده‌ی شما به چه صورت یک بلاک را برای جایگزین ‫ شدن انتخاب می‌کند؟

LRU

>>‫ روش پیاده‌سازی `read-ahead` را توضیح دهید.

پس از خواندن یک بلاک، چند بلاک بعدی را هم می‌خوانیم. برای بهینه‌سازی باید در عمل چند مقدار را امتحان کنیم تا کمترین دسترسی به دیسک را پیدا کنیم.

همگام سازی
-------------

>>‫ هنگامی که یک پردازه به طور مستمر در حال خواندن یا نوشتن داده در یک بلاک بافرکش‫ می‌باشد به چه صورت از دخالت سایر پردازه‌ها جلوگیری میشود؟

هر کدام از سکتورهای کش‌شده، یک لاک دارند که هر کس مشغول کار با آن شد لاک را در اختیار بگیرد.

>>‫ در حین خارج شدن یک بلوک از حافظه‌ی نهان، چگونه از پروسه‌های دیگر جلوگیری می‌شود تا‫ به این بلاک دسترسی پیدا نکنند؟

لاک اشاره شده برای زمانی که یک بلوک از حافظه خارج یا وارد می‌شود نیز استفاده می‌شود.

منطق طراحی
-----------------

>>‫ یک سناریو را توضیح دهید که از بافر کش، `read-ahead` و یا از `write-behind` استفاده کند.

متوجه منظور سؤال نشدم. استفاده از هر کدام از این دو قابلیت، ممکن است. مثلاً فرض کنید سیستم‌عامل مدت زیادی روشن باشد: اگر از write-behind استفاده نکنیم آنگاه برای نوشته شدن هر بار اطلاعات روی دیسک یا باید تعدادی برنامه باز کنیم که آن‌ها نیاز به دسترسی به دیسک داشته باشند و یا باید سیستم را دوباره راه‌اندازی کنیم.

فایل‌های قابل گسترش
=====================

داده‌ساختار‌ها و توابع
---------------------

>>‫ در این قسمت تعریف هر یک از `struct` ها، اعضای `struct` ها، متغیرهای سراسری‫ یا ایستا، `typedef` ها یا `enum` هایی که ایجاد کرده‌اید یا تغییر داده‌اید را بنویسید و‫ دلیل هر کدام را در حداکثر ۲۵ کلمه توضیح دهید.
```c
#define DIRECT_BLOCKS 124
#define INDIRECT_BLOCKS 128
```

```c
struct inode_disk {
  block_sector_t direct_blocks[DIRECT_BLOCKS]; // array of direct pointers
  block_sector_t indirect_blocks; // single indirect_block pointer
  block_sector_t doubly_indirect_blocks; // double indirec_block pointer

  off_t length; // file size in bytes
  unsigned magic; // magic number
  uint32_t unused; // this field removed because no space left for struct
}

struct indirect_block {
  block_sector_t blocks[INDIRECT_BLOCKS];
};

struct inode {
  ...
  struct lock inode_lock; // Lock for inode synchronization
  ... 
}

static bool inode_alloc (struct inode_disk *inode_disk, off_t length); // try to allocate new sectors in the order of direct -> indirect -> doubly-indirect
static void inode_dealloc (struct inode *inode); // deallocate blocks

```

در پیاده سازی فعلی `struct inode_disk`، یک آرایه‌ی `uint32_t unused[125]` و یک `block_sector_t start` وجود دارد. می‌دانیم آرایه‌ی `unused` تنها برای ایجاد سایز ۵۱۲ بایت استفاده می‌شود که با توجه به تعریف متغیرهای جدید، دیگر احتیاجی به آن نیست چون هم اکنون اندازه هر بلاک دقیقا ۵۱۲ بایت خواهد شد. محاسبات:          
block size = 124 * 4 + 4 + 4 + 4 + 4 = 128 * 4 =512 bytes

>>‫ بیشترین سایز فایل پشتیبانی شده توسط ساختار inode شما چقدر است؟

با توجه به طراحی جدید می‌دانیم هر `indirect_block` به ۱۲۸ بلوک دیسک و هر `doubly_indirect_block` به ۱۲۸ `indirect_block` اشاره خواهد کرد.  یعنی در مجموع داریم:

total blocks = 124 + 128 + 128 * 128 = 16636
total size = 16636 * 512 / 1024^2 = 8.123 MB

مطابق محاسبات بالا تقریبا فایل با سایز ۸.۱۲ مگابایت را می‌توانیم پشتیبانی کنیم. 

همگام سازی
----------

>>‫ توضیح دهید که اگر دو پردازه بخواهند یک فایل را به طور همزمان گسترش دهند، کد شما چگونه از‫ حالت مسابقه جلوگیری می‌کند.

با قرار دادن `inode_lock` در ساختار `inode`، هر پردازه‌ای که قصد نوشتن داشته باشد، ابتدا باید بتوان این قفل را بگیرد و اگر این قفل قبلا توسط پردازه‌ای دیگر گرفته شده باشد، باید صبر کند تا کار آن پردازه تمام شود.

>>‫ فرض کنید دو پردازه‌ی A و B فایل F را باز کرده‌اند و هر دو به end-of-file اشاره کرده‌اند.‫ اگر  همزمان A از F بخواند و B روی آن بنویسد، ممکن است که A تمام، بخشی یا هیچ چیز از‫ اطلاعات نوشته شده توسط B را بخواند. همچنین A نمی‌تواند چیزی جز اطلاعات نوشته شده توسط B را‫ بخواند. مثلا اگر B تماما ۱ بنویسد، A نیز باید تماما ۱ بخواند. توضیح دهید کد شما چگونه از‫ این حالت مسابقه جلوگیری می‌کند.

با توجه نیازمندی‌های پروژه، هر عمل `I/O` روی یک فایل نیاز به گرفتن یک قفل یعنی `inode_lock` دارد در غیر این صورت نمی‌تواند کارش را انجام دهد، برای مثال اگر پرداز `A` در حال نوشتن است و قفل را دارد، پردازه `B` دیگر نمی‌تواند از فایل بخواند و برعکس.

>>‫ توضیح دهید همگام سازی شما چگونه "عدالت" را برقرار می‌کند. فایل سیستمی "عادل" است که‫ خواننده‌های اطلاعات به صورت ناسازگار نویسنده‌های اطلاعات را مسدود نکنند و برعکس. بدین ترتیب‫ اگر تعدادی بسیار زیاد پردازه‌هایی که از یک فایل می‌خوانند نمی‌توانند تا ابد مانع نوشده شدن‫ اطلاعات توسط یک پردازه‌ی دیگر شوند و برعکس.

همون قفلی که در بالا گفته شده، هر فرایندی که درخواست خواندن یا نوشتن داره، با گرفتن قفل در انتهای صف پوش می‌شود، و در نهایت نوبتش خواهد شد و پردازه‌ای تا ابد در صف باقی نمی‌ماند و می‌تواند بنویسد یا بخواند.

منطق طراحی
----------

>>‫ آیا ساختار `inode` شما از طبقه‌بندی چند سطحه پشتیبانی می‌کند؟ اگر بله، دلیل خود را برای‫ انتخاب این ترکیب خاص از بلوک‌های مستقیم، غیر مستقیم و غیر مستقیم دوطرفه توضیح دهید.‌‫ اگر خیر، دلیل خود برای انتخاب ساختاری غیر از طبقه‌بندی چند سطحه و مزایا و معایب ساختار‫ مورد استفاده خود نسبت به طبقه‌بندی چند سطحه را توضیح دهید.

بله پشتیبانی می‌کند. داشتن ۱۲۴ `direct_blocks` و ۱ `indirect_blocks` و ۱ `double-indirect blocks` کمک می‌کند که فایل با سایز ۸مگابایت را پشتیبانی کنیم، همچنین دقیقا اندازه `stuct inode_disk` برابر ۵۱۲ بایت بماند. با این کار به راحتی نیز می‌توان تعداد سکتورهای موردنیاز برای افزایش حجم فایل را اختصاص داد.

زیرمسیرها
============

داده‌ساختار‌ها و توابع
---------------------

>>‫ در این قسمت تعریف هر یک از `struct` ها، اعضای `struct` ها، متغیرهای سراسری‫ یا ایستا، `typedef` ها یا `enum` هایی که ایجاد کرده‌اید یا تغییر داده‌اید را بنویسید و‫ دلیل هر کدام را در حداکثر ۲۵ کلمه توضیح دهید.

```
struct fd {
    -- struct file* file; //remove it
    void* content; // instead of file*
    bool is_file; // to know it is a dir or file
}

struct dir_entry {
  ...
  bool is_file; // file or dir
};

struct thread {
  ...
  char[200] cwd; // cwd
};

struct inode {
  ...
  struct lock* lock; // used for synchronization, (e.g. 2 threads can't delete same file or dir of a dir, they also can't create same file in a dir. These were usages for dir, it may be used for files too, as we know every file and dir has exactly one inode) 
};
```


چون استراکت fd باید علاوه بر فایل، دیرکتوری را هم پشتیبانی کند، به حای file* از void* استفاده میکنیم و به کمک is_file هم معلوم میکنیم که فایل هست و یا دیرکتوری.

درون استراکت dir_entry باید is_file اضافه شود تا معلوم شود که به فایل اشاره میکند یا دیرکتوری.

درون استراکت thread هم باید رشته cwd را اضافه کنیم که مسیر فعلی پردازه را نشان میدهد، این مقدار باید در سیستم کال های مختلف مانند cd به روز شود.

درون استراکت inode باید یک قفل قرار دهیم. وظیفه آن همگام سازی است. به این صورت که مثلا در هنگام پاک کردن یک فایل یا اضافه کردن آن باید قفل مربوط به inode مذکور گرفته شود تا race پیش نیاید. درواقع مثلا اگر در یک مسیر میخواهیم فایلی را پاک کنیم، قفل inode آن مسیر را میگیریم تا race پیش نیاید.

الگوریتم‌ها
-----------

>>‫ کد خود را برای طی کردن یک مسیر گرفته‌شده از کاربر را توضیح دهید.‫ آیا عبور از مسیرهای absolute و relative تفاوتی دارد؟

شیوه ما به این صورت است که اگر کاربر آدرس نسبی داد باید با cwd پردازه کانکت شود، در غیر این صورت خود مسیر ورودی میشود مسیر نهایی. در نهایت باید با این مسیر نهایی کار کنیم. بنابراین بین مسیر نسبی و مطلق تفاوت چندانی وجود ندارد.

همگام سازی
-------------

>>‫ چگونه از رخ دادن race-condition در مورد دایرکتوری ها پیشگیری می‌کنید؟‫ برای مثال اگر دو درخواست موازی برای حذف یک فایل وجود داشته باشد و ‫ تنها یکی از آنها باید موفق شود یا مثلاً دو ریسه موازی بخواهند فایلی‫ یک اسم در یک مسیر ایجاد کنند و مانند آن.‫ آیا پیاده سازی شما اجازه می‌دهد مسیری که CWD یک ریسه شده یا پردازه‌ای‫ از آن استفاده می‌کند حذف شود؟ اگر بله، عملیات فایل سیستم بعدی روی آن‫ دایرکتوری چه نتیجه‌ای می‌دهند؟ اگر نه، چطور جلوی آن را می‌گیرید؟

در قسمت داده‌ساختارها هم این توضیح داده شده است، اما به طور کلی به این شکل است که در هنگام پاک کردن یا ایجاد یک فایل باید قفل دیرکتوری که در آن هستیم را بگیریم تا race پیش نیاید.

منطق طراحی
-----------------

>>‫ توضیح دهید چرا تصمیم گرفتید CWD یک پردازه را به شکلی که طراحی کرده‌اید‫ پیاده‌سازی کنید؟

چون cwd صرفا یک رشته است کار با آن ساده‌تر است. همچنین به خاطر رشته بودن، در نهایت ما باید صرفا با یک مسیر کار کنیم که همیشه از ریشه هم شروع می‌شود، به همین خاطر این نوع پیاده سازی را انتخاب کردیم. همچنین چون معمولا عمق دیرکتوری‌ها (تعداد دیرکتوری‌های تودرتو) معمولا آن قدر زیاد نیست، این روش هزینه محاسباتی خاصی نیز ندارد.

### سوالات نظرسنجی

پاسخ به این سوالات دلخواه است، اما به ما برای بهبود این درس در ادامه کمک خواهد کرد.

نظرات خود را آزادانه به ما بگوئید—این سوالات فقط برای سنجش افکار شماست.

ممکن است شما بخواهید ارزیابی خود از درس را به صورت ناشناس و در انتهای ترم بیان کنید.

>>‫ به نظر شما، این تمرین گروهی، یا هر کدام از سه وظیفه آن، از نظر دشواری در چه سطحی بود؟ خیلی سخت یا خیلی آسان؟

>> چه مدت زمانی را صرف انجام این تمرین کردید؟ نسبتا زیاد یا خیلی کم؟

>>‫ آیا بعد از کار بر روی یک بخش خاص از این تمرین (هر بخشی)، این احساس در شما به وجود آمد که اکنون یک دید بهتر نسبت به برخی جنبه‌های سیستم عامل دارید؟

>>‫ آیا نکته یا راهنمایی خاصی وجود دارد که بهتر است ما آنها را به توضیحات این تمرین اضافه کنیم تا به دانشجویان ترم های آتی در حل مسائل کمک کند؟

>> متقابلا، آیا راهنمایی نادرستی که منجر به گمراهی شما شود وجود داشته است؟

>>‫ آیا پیشنهادی در مورد دستیاران آموزشی درس، برای همکاری موثرتر با دانشجویان دارید؟

این پیشنهادات میتوانند هم برای تمرین‌های گروهی بعدی همین ترم و هم برای ترم‌های آینده باشد.

>>‫ آیا حرف دیگری دارید؟