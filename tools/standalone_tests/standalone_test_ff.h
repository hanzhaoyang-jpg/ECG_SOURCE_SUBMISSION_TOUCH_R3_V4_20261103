#ifndef STANDALONE_TEST_FF_H
#define STANDALONE_TEST_FF_H
#include <stddef.h>
typedef unsigned int UINT;
typedef int FRESULT;
typedef struct { int slot; } FIL;
enum { FR_OK, FR_DISK_ERR, FR_NO_FILE, FR_EXIST };
enum { FA_READ = 1, FA_WRITE = 2, FA_CREATE_ALWAYS = 4 };
FRESULT f_open(FIL *, const char *, unsigned char);
FRESULT f_read(FIL *, void *, UINT, UINT *);
FRESULT f_write(FIL *, const void *, UINT, UINT *);
FRESULT f_close(FIL *);
FRESULT f_sync(FIL *);
FRESULT f_unlink(const char *);
FRESULT f_rename(const char *, const char *);
#endif
