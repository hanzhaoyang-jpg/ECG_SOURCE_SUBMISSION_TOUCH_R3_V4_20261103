#ifndef ECG_STANDALONE_H
#define ECG_STANDALONE_H

#include "xil_types.h"

#ifndef ECG_TOUCH_BUILD
#define ECG_TOUCH_BUILD 0
#endif
#ifndef ECG_WORKSTATION_BUILD
#define ECG_WORKSTATION_BUILD 0
#endif
#if ECG_WORKSTATION_BUILD
#define ECG_FIRMWARE_REVISION "ecg_touch_workstation_20261103_r3"
#elif ECG_TOUCH_BUILD
#define ECG_FIRMWARE_REVISION "ecg_touch_offline_20261103_r3"
#else
#define ECG_FIRMWARE_REVISION "standalone_nav_780_4hz_20261028"
#endif
#define ECG_SETTINGS_PATH "0:/ECGCFG.INI"
#define ECG_SETTINGS_BACKUP "0:/ECGCFG.BAK"
#define ECG_SETTINGS_NEW "0:/ECGCFG.NEW"
#define ECG_SETTINGS_MAX_BYTES 1024U

typedef struct {
    u16 gain;
    u16 hp_x100;
    u16 lp_hz;
    u8 auto_record;
    u8 key_profile; /* 0 disabled; 1 verified Navigator PS_KEY0/1 only. */
    u32 start_delay_s;
    u32 record_seconds; /* 0 unlimited, otherwise up to 72 hours. */
} EcgSettings;

typedef enum {
    ECG_CFG_DEFAULT = 0, ECG_CFG_SD, ECG_CFG_RECOVERED,
    ECG_CFG_INVALID, ECG_CFG_NO_SD
} EcgSettingsSource;

typedef struct {
    u8 candidate, stable, armed, long_sent;
    u32 changed_ms, pressed_ms;
} EcgButton;

typedef struct {
    EcgButton key[2];
    u8 menu;
    u8 auto_pending;
    u32 boot_ms;
} EcgStandalone;

enum { ECG_MENU_GAIN_DOWN = 0, ECG_MENU_GAIN_UP, ECG_MENU_HP,
       ECG_MENU_LP, ECG_MENU_SAVE, ECG_MENU_COUNT };
enum { ECG_BUTTON_NONE = 0, ECG_BUTTON_SHORT, ECG_BUTTON_LONG };

void ecg_settings_defaults(EcgSettings *settings);
int ecg_settings_parse(const char *text, u32 size, EcgSettings *settings);
int ecg_settings_format(const EcgSettings *settings, char *text, u32 size);
u8 ecg_settings_chset(const EcgSettings *settings);
int ecg_settings_load_sd(EcgSettings *settings, EcgSettingsSource *source);
int ecg_settings_save_sd(const EcgSettings *settings);
const char *ecg_settings_source_name(EcgSettingsSource source);
void ecg_standalone_init(EcgStandalone *control, u32 now_ms, u8 auto_record);
void ecg_standalone_cancel_auto(EcgStandalone *control);
u8 ecg_standalone_auto_due(EcgStandalone *control, const EcgSettings *settings,
                           u32 now_ms);
u8 ecg_standalone_stop_due(const EcgSettings *settings, u64 samples, u32 fs_hz);
u8 ecg_button_step(EcgButton *button, u8 pressed, u32 now_ms);

/* Optional board-specific adapter; never configured by a guessed PL pin. */
int ecg_board_keys_init(u8 profile);
u8 ecg_board_keys_read(void);

#endif
