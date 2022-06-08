# تقسیم بندی‌ کارها

+ کش: پارسا صلواتی و محمدحسین قیصریه

+ فایل قابل گسترش: علیرضا شاطری

+ زیرمسیرها: سپهر صفری

## کش

با این دو تابع از کش `sector` مربوطه را میخوانیم. 

 اگر در کش سکتور مربوطه وجود نداشته باشد، از دیسک خوانده میشود. 

و به کمک `remove_lru ` یک خانه کش حذف میشود. 

```c
void read_cache(block_sector_t sector, void* buffer) 
static void remove_lru()
```


به کمک این تابع هم اطلاعات در داخل کش نوشته می‌شود. 
```c
void write_cache(block_sector_t sector, void* buffer) 
```

## فایل قابل گسترش

تغییرات نسبت به داک طراحی:

افزودن توابع کمکی بیشتر برای `allocate` کردن سکتورها در  ساختار جدید `inode_disk`. این توابع شامل موارد زیر است:

```c

static bool inode_allocate_sector(block_sector_t* sector);
static bool inode_allocate_indirect(block_sector_t* sector, size_t count);
static bool inode_allocate_doubly_indirect(block_sector_t* sector, size_t count);

static void inode_deallocate_indirect(block_sector_t sector, size_t count);
static void inode_deallocate_doubly_indirect(block_sector_t sector, size_t count);


```
این توابع تنها برای تمیز تر شدن کد، اضافه شدند.

اضافه شدن ۲تابع کمکی برای گرفتن و رها کردن قفل:

```c

void inode_lock_acquire(struct inode* inode);
void inode_lock_release(struct inode* inode);

```

## زمانبند اولویت دار

تغییراتی که در این قسمت دادیم از قرار زیر بودند:
در ابتدا اندازه `cwd` هر پردازه را عدد ثابت ۲۰۰ در نظر گرفته بودیم که فهمیدیم ایراد دارد، چرا که هم اگر مسیر کوچک باشد بیش از حد نیاز فضا در نظر گرفتیم، و هم اینکه ممکن است در بعضی مواقع، مثلا در تست dir-vine اندازه آن بیش از ۲۰۰ شود.

