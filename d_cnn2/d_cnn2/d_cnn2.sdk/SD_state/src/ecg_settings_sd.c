#include "ecg_standalone.h"
#ifdef ECG_SETTINGS_HOST_TEST
#include "standalone_test_ff.h"
#else
#include "ff.h"
#endif

static int read_settings(const char *path, EcgSettings *settings)
{
    FIL file;
    FRESULT fr;
    UINT read_count = 0U;
    char text[ECG_SETTINGS_MAX_BYTES + 1U];
    int rc;
    fr = f_open(&file, path, FA_READ);
    if (fr == FR_NO_FILE) return 1;
    if (fr != FR_OK) return -1;
    fr = f_read(&file, text, sizeof(text), &read_count);
    rc = fr == FR_OK ? ecg_settings_parse(text, read_count, settings) : -1;
    if (f_close(&file) != FR_OK) rc = -1;
    return rc;
}

int ecg_settings_load_sd(EcgSettings *settings, EcgSettingsSource *source)
{
    int rc;
    ecg_settings_defaults(settings);
    rc = read_settings(ECG_SETTINGS_PATH, settings);
    if (rc == 0) { *source = ECG_CFG_SD; return 0; }
    if (rc == 1) {
        rc = read_settings(ECG_SETTINGS_BACKUP, settings);
        if (rc == 1) {
            rc = read_settings(ECG_SETTINGS_NEW, settings);
            if (rc == 1) { *source = ECG_CFG_DEFAULT; return 0; }
            /* An orphan replacement means a save was interrupted. Do not
             * silently autostart with an unrelated factory profile. */
            rc = -1;
        }
        if (rc == 0) {
            settings->auto_record = 0U;
            *source = ECG_CFG_RECOVERED;
            return 0;
        }
    }
    ecg_settings_defaults(settings);
    settings->auto_record = 0U;
    *source = ECG_CFG_INVALID;
    return -1;
}

int ecg_settings_save_sd(const EcgSettings *settings)
{
    FIL file;
    FRESULT fr;
    UINT written = 0U;
    char text[512];
    int length = ecg_settings_format(settings, text, sizeof(text));
    int rc = 0;
    if (length < 0) return -1;
    fr = f_open(&file, ECG_SETTINGS_NEW, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return -2;
    fr = f_write(&file, text, (UINT)length, &written);
    if (fr != FR_OK || written != (UINT)length) rc = -3;
    if (f_sync(&file) != FR_OK) rc = -3;
    if (f_close(&file) != FR_OK) rc = -3;
    if (rc) return rc;
    /* Keep the last profile until a complete replacement is flushed. FAT is
     * not power-loss atomic; the backup is a recovery aid, not a guarantee. */
    fr = f_unlink(ECG_SETTINGS_BACKUP);
    if (fr != FR_OK && fr != FR_NO_FILE) return -4;
    fr = f_rename(ECG_SETTINGS_PATH, ECG_SETTINGS_BACKUP);
    if (fr != FR_OK && fr != FR_NO_FILE) return -4;
    fr = f_rename(ECG_SETTINGS_NEW, ECG_SETTINGS_PATH);
    if (fr != FR_OK) {
        (void)f_rename(ECG_SETTINGS_BACKUP, ECG_SETTINGS_PATH);
        return -5;
    }
    return 0;
}
