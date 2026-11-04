#include "ecg_standalone.h"
#include "cn121_ps.h"
#include <string.h>
#include <stdio.h>

void ecg_settings_defaults(EcgSettings *s)
{
    memset(s, 0, sizeof(*s));
    s->gain = 60U * ((CN121_CHSET_ECG_DEFAULT & 15U) + 1U);
    s->hp_x100 = (CN121_CHSET_ECG_DEFAULT & 64U) ? 5U : 400U;
    s->lp_hz = 40U;
    s->auto_record = 1U;
    s->start_delay_s = 3U;
    s->record_seconds = 120U;
    /* The user confirmed the Navigator board; profile 0 remains an SD override. */
    s->key_profile = 1U;
}

static char *trim(char *text)
{
    char *end;
    while (*text == ' ' || *text == '\t') ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) --end;
    *end = '\0';
    return text;
}

static int decimal(const char *text, u32 *value)
{
    u32 result = 0U;
    if (!*text) return -1;
    for (; *text; ++text) {
        u32 digit;
        if (*text < '0' || *text > '9') return -1;
        digit = (u32)(*text - '0');
        if (result > (0xFFFFFFFFU - digit) / 10U) return -1;
        result = result * 10U + digit;
    }
    *value = result;
    return 0;
}

int ecg_settings_parse(const char *text, u32 size, EcgSettings *settings)
{
    static const char *names[] = {"version", "gain", "hp_x100", "lp_hz",
        "auto_record", "start_delay_s", "record_seconds", "key_profile"};
    EcgSettings parsed;
    char buffer[ECG_SETTINGS_MAX_BYTES + 1U];
    char *line, *next;
    u32 seen = 0U, i;
    if (!text || !settings || !size || size > ECG_SETTINGS_MAX_BYTES) return -1;
    for (i = 0; i < size; ++i) if (text[i] == '\0') return -2;
    memcpy(buffer, text, size);
    buffer[size] = '\0';
    line = buffer;
    if (size >= 3U && (u8)line[0] == 0xEFU && (u8)line[1] == 0xBBU && (u8)line[2] == 0xBFU)
        line += 3;
    ecg_settings_defaults(&parsed);
    while (line) {
        char *key, *equals, *value;
        u32 number;
        int index;
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        key = trim(line);
        if (!*key || *key == '#' || *key == ';') { line = next; continue; }
        equals = strchr(key, '=');
        if (!equals) return -3;
        *equals = '\0';
        value = trim(equals + 1);
        key = trim(key);
        for (index = 0; index < 8 && strcmp(key, names[index]); ++index) { }
        if (index == 8 || (seen & (1U << index)) || decimal(value, &number)) return -4;
        seen |= 1U << index;
        switch (index) {
        case 0: if (number != 1U) return -5; break;
        case 1:
            if (number < 60U || number > 960U || number % 60U) return -5;
            parsed.gain = (u16)number; break;
        case 2:
            if (number != 5U && number != 400U) return -5;
            parsed.hp_x100 = (u16)number; break;
        case 3:
            /* 150/300 Hz are not admitted as this 360-Hz recording profile. */
            if (number != 40U && number != 55U) return -5;
            parsed.lp_hz = (u16)number; break;
        case 4: if (number > 1U) return -5; parsed.auto_record = (u8)number; break;
        case 5: if (number > 60U) return -5; parsed.start_delay_s = number; break;
        case 6: if (number > 259200U) return -5; parsed.record_seconds = number; break;
        case 7: if (number > 1U) return -5; parsed.key_profile = (u8)number; break;
        }
        line = next;
    }
    if (!(seen & 1U)) return -6;
    *settings = parsed; /* Reject the entire file, never apply half a profile. */
    return 0;
}

int ecg_settings_format(const EcgSettings *s, char *text, u32 size)
{
    int length;
    EcgSettings checked;
    if (!s || !text || !size) return -1;
    length = snprintf(text, size, "version=1\r\ngain=%u\r\nhp_x100=%u\r\nlp_hz=%u\r\n"
        "auto_record=%u\r\nstart_delay_s=%lu\r\nrecord_seconds=%lu\r\nkey_profile=%u\r\n",
        s->gain, s->hp_x100, s->lp_hz, s->auto_record, (unsigned long)s->start_delay_s,
        (unsigned long)s->record_seconds, s->key_profile);
    if (length < 0 || (u32)length >= size) return -1;
    if (ecg_settings_parse(text, (u32)length, &checked)) return -1;
    return length;
}

u8 ecg_settings_chset(const EcgSettings *s)
{
    return (u8)((s->gain / 60U - 1U) | (s->hp_x100 == 5U ? 64U : 0U) |
                (s->lp_hz == 55U ? 16U : 0U));
}

const char *ecg_settings_source_name(EcgSettingsSource source)
{
    static const char *names[] = {"DEFAULT", "SD", "BACKUP", "BAD", "NO-SD"};
    return (unsigned)source < 5U ? names[source] : "BAD";
}

void ecg_standalone_init(EcgStandalone *c, u32 now_ms, u8 auto_record)
{
    memset(c, 0, sizeof(*c));
    c->boot_ms = now_ms;
    c->auto_pending = auto_record ? 1U : 0U;
}

void ecg_standalone_cancel_auto(EcgStandalone *c) { c->auto_pending = 0U; }

u8 ecg_standalone_auto_due(EcgStandalone *c, const EcgSettings *s, u32 now_ms)
{
    if (!c->auto_pending || (u32)(now_ms - c->boot_ms) < s->start_delay_s * 1000U) return 0U;
    c->auto_pending = 0U; /* A failed start is not retried in a busy loop. */
    return 1U;
}

u8 ecg_standalone_stop_due(const EcgSettings *s, u64 samples, u32 fs_hz)
{
    return (s->record_seconds && fs_hz && samples >= (u64)s->record_seconds * fs_hz) ? 1U : 0U;
}

u8 ecg_button_step(EcgButton *b, u8 pressed, u32 now_ms)
{
    pressed = pressed ? 1U : 0U;
    if (pressed != b->candidate) {
        b->candidate = pressed;
        b->changed_ms = now_ms;
    }
    if ((u32)(now_ms - b->changed_ms) < 30U) return ECG_BUTTON_NONE;
    if (!b->armed) {
        if (!pressed) b->armed = 1U; /* Holding a key at boot cannot start a record. */
        return ECG_BUTTON_NONE;
    }
    if (pressed != b->stable) {
        b->stable = pressed;
        if (pressed) {
            b->pressed_ms = now_ms;
            b->long_sent = 0U;
        } else if (!b->long_sent) {
            return ECG_BUTTON_SHORT;
        }
    }
    if (b->stable && !b->long_sent && (u32)(now_ms - b->pressed_ms) >= 1500U) {
        b->long_sent = 1U;
        return ECG_BUTTON_LONG;
    }
    return ECG_BUTTON_NONE;
}
