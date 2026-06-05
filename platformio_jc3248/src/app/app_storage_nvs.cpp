/**
 * ESP32 NVS persistence — same text format as PC app_storage.c
 */
#include "app_storage.h"
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

#ifndef APP_MAX_ENTRIES
#define APP_MAX_ENTRIES 40
#endif
#ifndef APP_MAX_ARCHIVED
#define APP_MAX_ARCHIVED 20
#endif

static Preferences s_prefs;
static const char *NS = "salesentry";
static const char *KEY_SETTINGS = "settings";
static const char *KEY_SALES = "sales";

static char s_settings_path[] = "nvs://settings";

const char *app_storage_settings_path(void)
{
    return s_settings_path;
}

static void trim(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ' || s[len - 1] == '\t'))
        s[--len] = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

static bool parse_settings_buf(const char *text, app_settings_t *out)
{
    if (!text || !out) return false;

    app_settings_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    strcpy(tmp.system_pin, "1234");
    strcpy(tmp.master_pin, "9999");
    strcpy(tmp.promo_head_name, "Ms. Helen");

    enum { SEC_NONE, SEC_EMP, SEC_DATES, SEC_KEYS } section = SEC_NONE;
    char line[128];
    const char *p = text;

    while (p && *p) {
        size_t i = 0;
        while (p[i] && p[i] != '\n' && i < sizeof(line) - 1) {
            line[i] = p[i];
            i++;
        }
        line[i] = '\0';
        p += i;
        if (*p == '\n') p++;

        trim(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        if (strcmp(line, "@employees") == 0) {
            section = SEC_EMP;
            tmp.employee_count = 0;
            continue;
        }
        if (strcmp(line, "@sent_dates") == 0) {
            section = SEC_DATES;
            tmp.sent_date_count = 0;
            continue;
        }
        if (strcmp(line, "@sent_keys") == 0) {
            section = SEC_KEYS;
            tmp.sent_key_count = 0;
            continue;
        }

        if (section == SEC_EMP) {
            char *sep = strchr(line, '|');
            if (!sep || tmp.employee_count >= APP_MAX_EMPLOYEES) continue;
            *sep = '\0';
            app_employee_t *e = &tmp.employees[tmp.employee_count++];
            strncpy(e->id, line, APP_ID_LEN - 1);
            strncpy(e->name, sep + 1, APP_NAME_LEN - 1);
            continue;
        }
        if (section == SEC_DATES) {
            if (tmp.sent_date_count >= APP_MAX_SENT_DATES) continue;
            strncpy(tmp.sent_dates[tmp.sent_date_count++], line, APP_DATE_LEN - 1);
            continue;
        }
        if (section == SEC_KEYS) {
            if (tmp.sent_key_count >= APP_MAX_SENT_KEYS) continue;
            strncpy(tmp.sent_keys[tmp.sent_key_count++], line, APP_SENT_KEY_LEN - 1);
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *val = eq + 1;
        if (strcmp(line, "system_pin") == 0) strncpy(tmp.system_pin, val, sizeof(tmp.system_pin) - 1);
        else if (strcmp(line, "master_pin") == 0) strncpy(tmp.master_pin, val, sizeof(tmp.master_pin) - 1);
        else if (strcmp(line, "promo_head") == 0) strncpy(tmp.promo_head_name, val, APP_NAME_LEN - 1);
        else if (strcmp(line, "sender_email") == 0) strncpy(tmp.sender_email, val, sizeof(tmp.sender_email) - 1);
        else if (strcmp(line, "recipient_email") == 0) strncpy(tmp.recipient_email, val, sizeof(tmp.recipient_email) - 1);
        else if (strcmp(line, "wifi_ssid") == 0) strncpy(tmp.wifi_ssid, val, sizeof(tmp.wifi_ssid) - 1);
        else if (strcmp(line, "wifi_password") == 0) strncpy(tmp.wifi_password, val, sizeof(tmp.wifi_password) - 1);
    }

    if (tmp.employee_count == 0) return false;
    *out = tmp;
    return true;
}

bool app_storage_save_settings(const app_settings_t *settings)
{
    if (!settings) return false;

    char buf[3072];
    int n = snprintf(buf, sizeof(buf), "v1\n");
    n += snprintf(buf + n, sizeof(buf) - n, "system_pin=%s\n", settings->system_pin);
    n += snprintf(buf + n, sizeof(buf) - n, "master_pin=%s\n", settings->master_pin);
    n += snprintf(buf + n, sizeof(buf) - n, "promo_head=%s\n", settings->promo_head_name);
    n += snprintf(buf + n, sizeof(buf) - n, "sender_email=%s\n", settings->sender_email);
    n += snprintf(buf + n, sizeof(buf) - n, "recipient_email=%s\n", settings->recipient_email);
    n += snprintf(buf + n, sizeof(buf) - n, "wifi_ssid=%s\n", settings->wifi_ssid);
    n += snprintf(buf + n, sizeof(buf) - n, "wifi_password=%s\n", settings->wifi_password);
    n += snprintf(buf + n, sizeof(buf) - n, "@employees\n");
    for (int i = 0; i < settings->employee_count; i++) {
        n += snprintf(buf + n, sizeof(buf) - n, "%s|%s\n",
                      settings->employees[i].id, settings->employees[i].name);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "@sent_dates\n");
    for (int i = 0; i < settings->sent_date_count; i++) {
        n += snprintf(buf + n, sizeof(buf) - n, "%s\n", settings->sent_dates[i]);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "@sent_keys\n");
    for (int i = 0; i < settings->sent_key_count; i++) {
        n += snprintf(buf + n, sizeof(buf) - n, "%s\n", settings->sent_keys[i]);
    }
    if (n <= 0 || (size_t)n >= sizeof(buf)) return false;

    s_prefs.begin(NS, false);
    bool ok = s_prefs.putString(KEY_SETTINGS, buf);
    s_prefs.end();
    return ok;
}

bool app_storage_load_settings(app_settings_t *out)
{
    if (!out) return false;
    s_prefs.begin(NS, true);
    String s = s_prefs.getString(KEY_SETTINGS, "");
    s_prefs.end();
    if (s.length() == 0) return false;
    return parse_settings_buf(s.c_str(), out);
}

static void append_entry(char *buf, size_t buf_sz, int *n, const char *tag, const app_sales_entry_t *e)
{
    *n += snprintf(buf + *n, buf_sz - *n, "%s\n", tag);
    *n += snprintf(buf + *n, buf_sz - *n, "id=%s\n", e->id);
    *n += snprintf(buf + *n, buf_sz - *n, "emp_id=%s\n", e->employee_id);
    *n += snprintf(buf + *n, buf_sz - *n, "emp_name=%s\n", e->employee_name);
    *n += snprintf(buf + *n, buf_sz - *n, "date=%s\n", e->date);
    *n += snprintf(buf + *n, buf_sz - *n, "week=%s\n", e->week_key);
    *n += snprintf(buf + *n, buf_sz - *n, "closed=%d\n", e->closed ? 1 : 0);
    *n += snprintf(buf + *n, buf_sz - *n, "grand=%.2f\n", e->grand_total);
    *n += snprintf(buf + *n, buf_sz - *n, "manual=%.2f\n", e->has_manual_total ? e->manual_total : 0.0f);
    *n += snprintf(buf + *n, buf_sz - *n, "items=%d\n", e->item_count);
    for (int i = 0; i < e->item_count; i++) {
        const app_sales_item_t *it = &e->items[i];
        *n += snprintf(buf + *n, buf_sz - *n, "item=%s|%.2f|%.2f|%d|%.2f\n",
                        it->barcode, it->price, it->discount, it->quantity, it->line_total);
    }
}

static bool parse_entry_line(app_sales_entry_t *e, const char *key, const char *val)
{
    if (strcmp(key, "id") == 0) strncpy(e->id, val, APP_ID_LEN - 1);
    else if (strcmp(key, "emp_id") == 0) strncpy(e->employee_id, val, APP_ID_LEN - 1);
    else if (strcmp(key, "emp_name") == 0) strncpy(e->employee_name, val, APP_NAME_LEN - 1);
    else if (strcmp(key, "date") == 0) strncpy(e->date, val, APP_DATE_LEN - 1);
    else if (strcmp(key, "week") == 0) strncpy(e->week_key, val, APP_DATE_LEN - 1);
    else if (strcmp(key, "closed") == 0) e->closed = (atoi(val) != 0);
    else if (strcmp(key, "grand") == 0) e->grand_total = (float)atof(val);
    else if (strcmp(key, "manual") == 0) {
        e->manual_total = (float)atof(val);
        e->has_manual_total = true;
    } else if (strcmp(key, "items") == 0) e->item_count = atoi(val);
    else if (strcmp(key, "item") == 0) {
        if (e->item_count < APP_MAX_ITEMS) {
            app_sales_item_t *it = &e->items[e->item_count++];
            sscanf(val, "%[^|]|%f|%f|%d|%f",
                   it->barcode, &it->price, &it->discount, &it->quantity, &it->line_total);
        }
    } else return false;
    return true;
}

static bool parse_sales_buf(const char *text, app_sales_entry_t *active, int *active_count,
                            app_sales_entry_t *archived, int *archived_count)
{
    if (!text || !active || !active_count || !archived || !archived_count) return false;
    *active_count = 0;
    *archived_count = 0;

    app_sales_entry_t cur;
    memset(&cur, 0, sizeof(cur));
    bool in_entry = false;
    bool is_archive = false;
    char line[160];
    const char *p = text;

    while (p && *p) {
        size_t i = 0;
        while (p[i] && p[i] != '\n' && i < sizeof(line) - 1) {
            line[i] = p[i];
            i++;
        }
        line[i] = '\0';
        p += i;
        if (*p == '\n') p++;

        trim(line);
        if (line[0] == '\0') continue;
        if (strcmp(line, "@active") == 0 || strcmp(line, "@archive") == 0) {
            if (in_entry) {
                if (is_archive && *archived_count < APP_MAX_ARCHIVED)
                    archived[(*archived_count)++] = cur;
                else if (!is_archive && *active_count < APP_MAX_ENTRIES)
                    active[(*active_count)++] = cur;
            }
            memset(&cur, 0, sizeof(cur));
            in_entry = true;
            is_archive = (strcmp(line, "@archive") == 0);
            continue;
        }
        char *eq = strchr(line, '=');
        if (!eq || !in_entry) continue;
        *eq = '\0';
        parse_entry_line(&cur, line, eq + 1);
    }
    if (in_entry) {
        if (is_archive && *archived_count < APP_MAX_ARCHIVED)
            archived[(*archived_count)++] = cur;
        else if (!is_archive && *active_count < APP_MAX_ENTRIES)
            active[(*active_count)++] = cur;
    }
    return (*active_count > 0 || *archived_count > 0);
}

bool app_storage_save_sales(const app_sales_entry_t *active, int active_count,
                            const app_sales_entry_t *archived, int archived_count)
{
    static char buf[16384];
    int n = snprintf(buf, sizeof(buf), "v1\n");
    for (int i = 0; i < active_count && n < (int)sizeof(buf) - 256; i++)
        append_entry(buf, sizeof(buf), &n, "@active", &active[i]);
    for (int i = 0; i < archived_count && n < (int)sizeof(buf) - 256; i++)
        append_entry(buf, sizeof(buf), &n, "@archive", &archived[i]);
    if (n <= 0) return false;

    s_prefs.begin(NS, false);
    bool ok = s_prefs.putString(KEY_SALES, buf);
    s_prefs.end();
    return ok;
}

bool app_storage_load_sales(app_sales_entry_t *active, int *active_count,
                            app_sales_entry_t *archived, int *archived_count)
{
    s_prefs.begin(NS, true);
    String s = s_prefs.getString(KEY_SALES, "");
    s_prefs.end();
    if (s.length() == 0) return false;
    return parse_sales_buf(s.c_str(), active, active_count, archived, archived_count);
}

bool app_storage_save_all(const app_settings_t *settings,
                          const app_sales_entry_t *active, int active_count,
                          const app_sales_entry_t *archived, int archived_count)
{
    bool ok1 = settings ? app_storage_save_settings(settings) : true;
    bool ok2 = app_storage_save_sales(active, active_count, archived, archived_count);
    return ok1 && ok2;
}
