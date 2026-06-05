/**
 * NVS backup for settings when TF card is unavailable on JC3248W535.
 */
#include "app_storage_backup.h"

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)

#include <Preferences.h>
#include <stdio.h>
#include <string.h>

static const char * NS = "salesentry";
static const char * KEY_SETTINGS = "settings";

static void trim(char * s)
{
    if(!s) return;
    size_t len = strlen(s);
    while(len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ' || s[len - 1] == '\t'))
        s[--len] = '\0';
    char * start = s;
    while(*start == ' ' || *start == '\t') start++;
    if(start != s) memmove(s, start, strlen(start) + 1);
}

static bool parse_settings_buf(const char * text, app_settings_t * out)
{
    if(!text || !out) return false;

    app_settings_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    strcpy(tmp.system_pin, "1234");
    strcpy(tmp.master_pin, "9999");
    strcpy(tmp.promo_head_name, "Ms. Helen");

    enum { SEC_NONE, SEC_EMP, SEC_DATES, SEC_KEYS } section = SEC_NONE;
    char line[128];
    const char * p = text;

    while(p && *p) {
        size_t i = 0;
        while(p[i] && p[i] != '\n' && i < sizeof(line) - 1) {
            line[i] = p[i];
            i++;
        }
        line[i] = '\0';
        p += i;
        if(*p == '\n') p++;

        trim(line);
        if(line[0] == '\0' || line[0] == '#') continue;

        if(strcmp(line, "@employees") == 0) {
            section = SEC_EMP;
            tmp.employee_count = 0;
            continue;
        }
        if(strcmp(line, "@sent_dates") == 0) {
            section = SEC_DATES;
            tmp.sent_date_count = 0;
            continue;
        }
        if(strcmp(line, "@sent_keys") == 0) {
            section = SEC_KEYS;
            tmp.sent_key_count = 0;
            continue;
        }

        if(section == SEC_EMP) {
            char * sep = strchr(line, '|');
            if(!sep || tmp.employee_count >= APP_MAX_EMPLOYEES) continue;
            *sep = '\0';
            app_employee_t * e = &tmp.employees[tmp.employee_count++];
            strncpy(e->id, line, APP_ID_LEN - 1);
            e->id[APP_ID_LEN - 1] = '\0';
            strncpy(e->name, sep + 1, APP_NAME_LEN - 1);
            e->name[APP_NAME_LEN - 1] = '\0';
            continue;
        }
        if(section == SEC_DATES) {
            if(tmp.sent_date_count >= APP_MAX_SENT_DATES) continue;
            strncpy(tmp.sent_dates[tmp.sent_date_count++], line, APP_DATE_LEN - 1);
            continue;
        }
        if(section == SEC_KEYS) {
            if(tmp.sent_key_count >= APP_MAX_SENT_KEYS) continue;
            strncpy(tmp.sent_keys[tmp.sent_key_count++], line, APP_SENT_KEY_LEN - 1);
            continue;
        }

        char * eq = strchr(line, '=');
        if(!eq) continue;
        *eq = '\0';
        const char * val = eq + 1;
        if(strcmp(line, "system_pin") == 0) strncpy(tmp.system_pin, val, sizeof(tmp.system_pin) - 1);
        else if(strcmp(line, "master_pin") == 0) strncpy(tmp.master_pin, val, sizeof(tmp.master_pin) - 1);
        else if(strcmp(line, "promo_head") == 0) strncpy(tmp.promo_head_name, val, APP_NAME_LEN - 1);
        else if(strcmp(line, "sender_email") == 0) strncpy(tmp.sender_email, val, sizeof(tmp.sender_email) - 1);
        else if(strcmp(line, "recipient_email") == 0) strncpy(tmp.recipient_email, val, sizeof(tmp.recipient_email) - 1);
        else if(strcmp(line, "smtp_app_password") == 0) strncpy(tmp.smtp_app_password, val, sizeof(tmp.smtp_app_password) - 1);
        else if(strcmp(line, "wifi_ssid") == 0) strncpy(tmp.wifi_ssid, val, sizeof(tmp.wifi_ssid) - 1);
        else if(strcmp(line, "wifi_password") == 0) strncpy(tmp.wifi_password, val, sizeof(tmp.wifi_password) - 1);
    }

    if(tmp.employee_count == 0) return false;
    *out = tmp;
    return true;
}

static bool serialize_settings(const app_settings_t * settings, char * buf, size_t buf_sz)
{
    int n = snprintf(buf, buf_sz, "v1\n");
    n += snprintf(buf + n, buf_sz - n, "system_pin=%s\n", settings->system_pin);
    n += snprintf(buf + n, buf_sz - n, "master_pin=%s\n", settings->master_pin);
    n += snprintf(buf + n, buf_sz - n, "promo_head=%s\n", settings->promo_head_name);
    n += snprintf(buf + n, buf_sz - n, "sender_email=%s\n", settings->sender_email);
    n += snprintf(buf + n, buf_sz - n, "recipient_email=%s\n", settings->recipient_email);
    n += snprintf(buf + n, buf_sz - n, "smtp_app_password=%s\n", settings->smtp_app_password);
    n += snprintf(buf + n, buf_sz - n, "wifi_ssid=%s\n", settings->wifi_ssid);
    n += snprintf(buf + n, buf_sz - n, "wifi_password=%s\n", settings->wifi_password);
    n += snprintf(buf + n, buf_sz - n, "@employees\n");
    for(int i = 0; i < settings->employee_count; i++) {
        n += snprintf(buf + n, buf_sz - n, "%s|%s\n",
                      settings->employees[i].id, settings->employees[i].name);
    }
    n += snprintf(buf + n, buf_sz - n, "@sent_dates\n");
    for(int i = 0; i < settings->sent_date_count; i++) {
        n += snprintf(buf + n, buf_sz - n, "%s\n", settings->sent_dates[i]);
    }
    n += snprintf(buf + n, buf_sz - n, "@sent_keys\n");
    for(int i = 0; i < settings->sent_key_count; i++) {
        n += snprintf(buf + n, buf_sz - n, "%s\n", settings->sent_keys[i]);
    }
    return n > 0 && (size_t)n < buf_sz;
}

bool app_storage_backup_save_settings(const app_settings_t * settings)
{
    if(!settings) return false;
    char buf[4096];
    if(!serialize_settings(settings, buf, sizeof(buf))) return false;

    Preferences prefs;
    prefs.begin(NS, false);
    bool ok = prefs.putString(KEY_SETTINGS, buf);
    prefs.end();
    if(ok) printf("[storage] settings saved to NVS backup\n");
    return ok;
}

bool app_storage_backup_load_settings(app_settings_t * out)
{
    if(!out) return false;
    Preferences prefs;
    prefs.begin(NS, true);
    String s = prefs.getString(KEY_SETTINGS, "");
    prefs.end();
    if(s.length() == 0) return false;
    return parse_settings_buf(s.c_str(), out);
}

#else

bool app_storage_backup_save_settings(const app_settings_t * settings)
{
    (void)settings;
    return false;
}

bool app_storage_backup_load_settings(app_settings_t * out)
{
    (void)out;
    return false;
}

#endif /* ESP_PLATFORM || ARDUINO_ARCH_ESP32 */
