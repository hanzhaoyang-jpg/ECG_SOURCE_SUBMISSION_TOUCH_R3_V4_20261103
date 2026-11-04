#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ecg_standalone.h"
#include "standalone_test_ff.h"

typedef struct { char name[32], data[1200]; UINT size; int used; } MockFile;
static MockFile files[6];
static int sync_failure, close_failure, rename_failure, partial_write;

static int slot(const char *name)
{
    int i;
    for (i = 0; i < 6; ++i) if (files[i].used && !strcmp(files[i].name, name)) return i;
    return -1;
}
static void put(const char *name, const char *text)
{
    int i = slot(name), j;
    if (i < 0) for (j = 0; j < 6; ++j) if (!files[j].used) { i = j; break; }
    assert(i >= 0);
    files[i].used = 1;
    strcpy(files[i].name, name);
    strcpy(files[i].data, text);
    files[i].size = (UINT)strlen(text);
}
FRESULT f_open(FIL *f, const char *name, unsigned char mode)
{
    int i = slot(name);
    if (mode & FA_CREATE_ALWAYS) { put(name, ""); i = slot(name); }
    if (i < 0) return FR_NO_FILE;
    f->slot = i;
    return FR_OK;
}
FRESULT f_read(FIL *f, void *text, UINT max, UINT *n)
{
    *n = files[f->slot].size < max ? files[f->slot].size : max;
    memcpy(text, files[f->slot].data, *n);
    return FR_OK;
}
FRESULT f_write(FIL *f, const void *text, UINT count, UINT *n)
{
    *n = partial_write ? count - 1U : count;
    assert(*n < sizeof(files[f->slot].data));
    memcpy(files[f->slot].data, text, *n);
    files[f->slot].data[*n] = 0;
    files[f->slot].size = *n;
    return FR_OK;
}
FRESULT f_close(FIL *f) { (void)f; return close_failure ? FR_DISK_ERR : FR_OK; }
FRESULT f_sync(FIL *f) { (void)f; return sync_failure ? FR_DISK_ERR : FR_OK; }
FRESULT f_unlink(const char *name)
{
    int i = slot(name);
    if (i < 0) return FR_NO_FILE;
    files[i].used = 0;
    return FR_OK;
}
FRESULT f_rename(const char *old, const char *name)
{
    int i = slot(old);
    if (rename_failure && !strcmp(name, ECG_SETTINGS_PATH)) return FR_DISK_ERR;
    if (i < 0) return FR_NO_FILE;
    if (slot(name) >= 0) return FR_EXIST;
    strcpy(files[i].name, name);
    return FR_OK;
}

static void parser(void)
{
    EcgSettings s, old;
    char text[512];
    const char *bad[] = {"", "gain=780\n", "version=2\n", "version=1\ngain=781",
        "version=1\ngain=-1", "version=1\ngain=42949672960", "version=1\ngain=0",
        "version=1\ngain=961", "version=1\nhp_x100=4", "version=1\nlp_hz=300",
        "version=1\nauto_record=2", "version=1\nstart_delay_s=61",
        "version=1\nrecord_seconds=259201", "version=1\nkey_profile=2",
        "version=1\ngain=780\ngain=420", "version=1\nunknown=1", "version=1\ngain=780#x"};
    unsigned i;
    ecg_settings_defaults(&s);
    assert(s.gain == 780 && s.hp_x100 == 400 && s.lp_hz == 40 && s.key_profile == 1);
    assert(s.record_seconds == 120 && ecg_settings_chset(&s) == 0x0C);
    old = s;
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        assert(ecg_settings_parse(bad[i], (u32)strlen(bad[i]), &s) != 0);
        assert(!memcmp(&s, &old, sizeof(s)));
    }
    strcpy(text, "\xEF\xBB\xBF# test\r\n version = 1\r\ngain=420\nhp_x100=5\nlp_hz=55\nrecord_seconds=7200\nkey_profile=1");
    assert(ecg_settings_parse(text, (u32)strlen(text), &s) == 0);
    assert(s.gain == 420 && s.hp_x100 == 5 && s.record_seconds == 7200 && s.key_profile == 1);
    assert(ecg_settings_chset(&s) == 0x56);
    i = (unsigned)ecg_settings_format(&s, text, sizeof(text));
    assert(i > 0 && i < sizeof(text));
    assert(ecg_settings_parse(text, i, &old) == 0 && !memcmp(&s, &old, sizeof(s)));
    assert(ecg_settings_format(&s, text, 5) < 0);
    assert(ecg_settings_parse("version=1\0gain=1", 16, &s) != 0);
    assert(ecg_settings_parse("version=1\nkey_profile=0", 23, &s) == 0 && !s.key_profile);
    puts("PASS parser: defaults, transactional validation, BOM/CRLF, ranges, duplicates, overflow, roundtrip");
}

static void storage(void)
{
    EcgSettings s;
    EcgSettingsSource source;
    memset(files, 0, sizeof(files));
    assert(ecg_settings_load_sd(&s, &source) == 0 && source == ECG_CFG_DEFAULT);
    assert(s.auto_record);
    put(ECG_SETTINGS_NEW, "version=1\ngain=360");
    assert(ecg_settings_load_sd(&s, &source) < 0 && !s.auto_record);
    assert(f_unlink(ECG_SETTINGS_NEW) == FR_OK);
    put(ECG_SETTINGS_PATH, "version=1\ngain=360");
    assert(ecg_settings_load_sd(&s, &source) == 0 && s.gain == 360 && source == ECG_CFG_SD);
    s.gain = 780;
    assert(ecg_settings_save_sd(&s) == 0);
    assert(slot(ECG_SETTINGS_BACKUP) >= 0);
    assert(ecg_settings_load_sd(&s, &source) == 0 && s.gain == 780);
    sync_failure = 1;
    s.gain = 240;
    assert(ecg_settings_save_sd(&s) < 0);
    sync_failure = 0;
    assert(ecg_settings_load_sd(&s, &source) == 0 && s.gain == 780);
    partial_write = 1;
    assert(ecg_settings_save_sd(&s) < 0);
    partial_write = 0;
    rename_failure = 1;
    s.gain = 420;
    assert(ecg_settings_save_sd(&s) < 0);
    rename_failure = 0;
    assert(ecg_settings_load_sd(&s, &source) == 0 && source == ECG_CFG_RECOVERED);
    assert(s.gain == 780 && !s.auto_record);
    put(ECG_SETTINGS_PATH, "version=1\ngain=666");
    assert(ecg_settings_load_sd(&s, &source) < 0 && source == ECG_CFG_INVALID && !s.auto_record);
    put(ECG_SETTINGS_PATH, "version=1");
    close_failure = 1;
    assert(ecg_settings_load_sd(&s, &source) < 0 && !s.auto_record);
    close_failure = 0;
    memset(files[slot(ECG_SETTINGS_PATH)].data, 'x', 1100);
    files[slot(ECG_SETTINGS_PATH)].size = 1100;
    assert(ecg_settings_load_sd(&s, &source) < 0 && !s.auto_record);
    puts("PASS storage: load, save, backup, partial writes, sync/close failures, interrupted replacement, oversized file");
}

static void control(void)
{
    EcgSettings s;
    EcgStandalone c;
    EcgButton b = {0};
    ecg_settings_defaults(&s);
    ecg_standalone_init(&c, 0xFFFFFF00U, 1U);
    assert(!ecg_standalone_auto_due(&c, &s, 0x100U));
    assert(ecg_standalone_auto_due(&c, &s, 0x1000U));
    assert(!ecg_standalone_auto_due(&c, &s, 0x2000U));
    ecg_standalone_init(&c, 0, 1);
    ecg_standalone_cancel_auto(&c);
    assert(!ecg_standalone_auto_due(&c, &s, 99999));
    assert(!ecg_standalone_stop_due(&s, 43199, 360));
    assert(ecg_standalone_stop_due(&s, 43200, 360));
    s.record_seconds = 0;
    assert(!ecg_standalone_stop_due(&s, 999999999ULL, 360));
    assert(!ecg_button_step(&b, 1, 0));
    assert(!ecg_button_step(&b, 1, 3000));
    assert(!ecg_button_step(&b, 0, 3001));
    assert(!ecg_button_step(&b, 0, 3031));
    assert(!ecg_button_step(&b, 1, 3040));
    assert(!ecg_button_step(&b, 0, 3050)); /* Bounce: never became a press. */
    assert(!ecg_button_step(&b, 0, 3080));
    assert(!ecg_button_step(&b, 1, 3100));
    assert(!ecg_button_step(&b, 1, 3130));
    assert(!ecg_button_step(&b, 0, 3200));
    assert(ecg_button_step(&b, 0, 3230) == ECG_BUTTON_SHORT);
    assert(!ecg_button_step(&b, 1, 3300));
    assert(!ecg_button_step(&b, 1, 3330));
    assert(ecg_button_step(&b, 1, 4830) == ECG_BUTTON_LONG);
    assert(!ecg_button_step(&b, 1, 5000));
    assert(!ecg_button_step(&b, 0, 5100));
    assert(!ecg_button_step(&b, 0, 5130));
    puts("PASS control: one-shot autostart, cancellation, wraparound, exact sample limit, startup-held keys, debounce, long/short exclusion");
}

static void profile_file(const char *path)
{
    char text[ECG_SETTINGS_MAX_BYTES + 1U];
    EcgSettings actual, expected;
    FILE *file = fopen(path, "rb");
    size_t count;
    assert(file);
    count = fread(text, 1, sizeof(text), file);
    assert(!ferror(file) && count <= ECG_SETTINGS_MAX_BYTES);
    assert(fclose(file) == 0);
    assert(ecg_settings_parse(text, (u32)count, &actual) == 0);
    ecg_settings_defaults(&expected);
    assert(!memcmp(&actual, &expected, sizeof(actual)));
    puts("PASS delivery INI: real firmware parser, matches compiled Navigator defaults");
}

int main(int argc, char **argv)
{
    parser(); storage(); control();
    if (argc == 2) profile_file(argv[1]);
    return 0;
}
