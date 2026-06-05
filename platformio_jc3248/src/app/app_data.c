#include "app_data.h"
#include "app_settings_defaults.h"
#include "app_storage.h"
#include "app_display.h"
#if !APP_EMAIL_SIMULATE
#include "app_mail.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(ARDUINO_ARCH_ESP32)
#define APP_USE_LIBC_TIME  1
#include <esp_heap_caps.h>
#else
#define APP_USE_LIBC_TIME  1
#endif

static const char APP_DEFAULT_DATE[] = "2026-05-20";

typedef struct {
    app_settings_t settings;
    app_sales_entry_t entries[APP_MAX_ENTRIES];
    int entry_count;
    app_sales_entry_t archived[APP_MAX_ARCHIVED];
    int archived_count;
    app_reconciliation_t reconciliation[APP_MAX_RECON];
    int recon_count;
    char selected_date[APP_DATE_LEN];
    char sales_employee_id[APP_ID_LEN];
    uint32_t id_counter;
} app_state_t;

#if defined(ARDUINO_ARCH_ESP32)
static app_state_t * g_st;
#define g  (*g_st)
#else
static app_state_t g;
#endif

static bool state_alloc(void)
{
#if defined(ARDUINO_ARCH_ESP32)
    if(g_st)
        return true;
    g_st = (app_state_t *)heap_caps_calloc(1, sizeof(app_state_t),
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(!g_st)
        g_st = (app_state_t *)calloc(1, sizeof(app_state_t));
    if(!g_st) {
        printf("[data] FATAL: no RAM for app state\n");
        return false;
    }
    return true;
#else
    return true;
#endif
}

static bool persist_sales(void);

static void today_str(char * out, size_t sz)
{
#if !APP_USE_LIBC_TIME
    strncpy(out, APP_DEFAULT_DATE, sz - 1);
    out[sz - 1] = '\0';
    return;
#else
    time_t t = time(NULL);
    struct tm tm_buf;
    const struct tm * tm = localtime_r(&t, &tm_buf);
    if(!tm) {
        strncpy(out, APP_DEFAULT_DATE, sz - 1);
        out[sz - 1] = '\0';
        return;
    }
    snprintf(out, sz, "%04d-%02d-%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
#endif
}

static bool name_equal_ci(const char * a, const char * b)
{
    if(!a || !b) return false;
    while(*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if(ca != cb) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool key_in_list(const char * key)
{
    for(int i = 0; i < g.settings.sent_key_count; i++) {
        if(strcmp(g.settings.sent_keys[i], key) == 0) return true;
    }
    return false;
}

void app_data_init(void)
{
    static bool initialized;
    if(initialized) return;
    if(!state_alloc())
        return;
    initialized = true;

    memset(&g, 0, sizeof(g));
    strcpy(g.settings.system_pin, "1234");
    strcpy(g.settings.master_pin, "9999");
    strcpy(g.settings.promo_head_name, "Ms. Helen");
    app_settings_apply_defaults(&g.settings);

    strcpy(g.settings.employees[0].id, "1");
    strcpy(g.settings.employees[0].name, "John");
    strcpy(g.settings.employees[1].id, "2");
    strcpy(g.settings.employees[1].name, "Howard lee");
    g.settings.employee_count = 2;

    today_str(g.selected_date, sizeof(g.selected_date));
    strcpy(g.sales_employee_id, g.settings.employees[0].id);

    app_data_reload_from_storage();
    app_data_sync_selected_to_today();
    app_clamp_selected_date();

    if(app_settings_apply_defaults(&g.settings))
        app_storage_save_settings(&g.settings);
}

void app_data_reload_from_storage(void)
{
    static app_settings_t loaded;
    if(app_storage_load_settings(&loaded)) {
        g.settings = loaded;
        if(g.settings.employee_count > 0)
            strcpy(g.sales_employee_id, g.settings.employees[0].id);
        printf("[data] settings loaded (%s)\n", app_storage_settings_path());
    }

    int ac = 0, arc = 0;
    if(app_storage_load_sales(g.entries, &ac, g.archived, &arc)) {
        g.entry_count = ac;
        g.archived_count = arc;
        bool repaired = false;
        for(int i = 0; i < g.entry_count; i++) {
            if(app_entry_compact_items(&g.entries[i]))
                repaired = true;
        }
        for(int i = 0; i < g.archived_count; i++) {
            if(app_entry_compact_items(&g.archived[i]))
                repaired = true;
        }
        if(repaired) {
            persist_sales();
            printf("[data] sales repaired (removed ghost items)\n");
        }
        printf("[data] sales loaded active=%d archived=%d\n", g.entry_count, g.archived_count);
    }
    else {
        printf("[data] no sales.cfg on storage yet\n");
    }
}

static bool persist_sales(void)
{
    if(app_storage_save_sales(g.entries, g.entry_count, g.archived, g.archived_count))
        return true;
    printf("[data] WARN: sales NOT saved — check SD card / DailySales folder\n");
    return false;
}

app_settings_t * app_settings(void)
{
    return &g.settings;
}

const char * app_today_date(void)
{
    static char today[APP_DATE_LEN];
    today_str(today, sizeof(today));
    return today;
}

const char * app_selected_date(void)
{
    return app_today_date();
}

void app_set_selected_date(const char * yyyy_mm_dd)
{
    (void)yyyy_mm_dd;
    today_str(g.selected_date, sizeof(g.selected_date));
}

const char * app_sales_employee_id(void)
{
    return g.sales_employee_id;
}

void app_set_sales_employee_id(const char * id)
{
    if(!id) return;
    strncpy(g.sales_employee_id, id, sizeof(g.sales_employee_id) - 1);
    g.sales_employee_id[sizeof(g.sales_employee_id) - 1] = '\0';
}

static bool s_sales_open_for_edit;

void app_set_sales_open_for_edit(bool edit)
{
    s_sales_open_for_edit = edit;
}

bool app_take_sales_open_for_edit(void)
{
    bool v = s_sales_open_for_edit;
    s_sales_open_for_edit = false;
    return v;
}

void app_gen_id(char * out, size_t out_sz)
{
    g.id_counter++;
#if !APP_USE_LIBC_TIME
    snprintf(out, out_sz, "%lu%04lu", (unsigned long)g.id_counter, (unsigned long)(millis() & 0xFFFF));
#else
    snprintf(out, out_sz, "%lu%04lu", (unsigned long)time(NULL), (unsigned long)g.id_counter);
#endif
}

void app_pad_barcode(const char * input, char * out, size_t out_sz)
{
    char digits[32] = "";
    size_t j = 0;
    for(size_t i = 0; input[i] && j < sizeof(digits) - 1; i++) {
        if(input[i] >= '0' && input[i] <= '9') digits[j++] = input[i];
    }
    digits[j] = '\0';
    size_t len = strlen(digits);
    if(len > 13) {
        memcpy(out, digits + len - 13, 13);
        out[13] = '\0';
    }
    else {
        size_t pad = 13 - len;
        memset(out, '0', pad);
        memcpy(out + pad, digits, len + 1);
    }
}

void app_week_key(const char * yyyy_mm_dd, char * out, size_t out_sz)
{
#if !APP_USE_LIBC_TIME
    /* No mktime on ESP loopTask — use date as week key for device */
    strncpy(out, yyyy_mm_dd, out_sz - 1);
    out[out_sz - 1] = '\0';
    return;
#else
    int y, m, d;
    if(sscanf(yyyy_mm_dd, "%d-%d-%d", &y, &m, &d) != 3) {
        strncpy(out, yyyy_mm_dd, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }
    struct tm tm = {0};
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    mktime(&tm);
    int wday = tm.tm_wday;
    int diff = tm.tm_mday - wday + (wday == 0 ? -6 : 1);
    tm.tm_mday = diff;
    mktime(&tm);
    snprintf(out, out_sz, "%04d-%02d-%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
#endif
}

void app_sales_sent_key(const char * employee_id, const char * date, char * out, size_t out_sz)
{
    snprintf(out, out_sz, "%s|%s", employee_id, date);
}

app_sales_entry_t * app_find_entry(const char * employee_id, const char * date)
{
    for(int i = 0; i < g.entry_count; i++) {
        if(strcmp(g.entries[i].employee_id, employee_id) == 0 &&
           strcmp(g.entries[i].date, date) == 0) {
            return &g.entries[i];
        }
    }
    return NULL;
}

bool app_is_entry_sent(const char * employee_id, const char * date)
{
    char key[APP_SENT_KEY_LEN];
    app_sales_sent_key(employee_id, date, key, sizeof(key));
    return key_in_list(key);
}

bool app_is_date_sent(const char * date)
{
    for(int i = 0; i < g.settings.sent_date_count; i++) {
        if(strcmp(g.settings.sent_dates[i], date) == 0) return true;
    }
    return false;
}

static bool item_row_valid(const app_sales_item_t * it)
{
    if(!it || !it->barcode[0]) return false;
    if(it->quantity <= 0) return false;
    if(it->price <= 0.0f && it->line_total <= 0.0f) return false;
    return true;
}

bool app_entry_compact_items(app_sales_entry_t * entry)
{
    if(!entry) return false;
    int w = 0;
    float grand = 0.0f;
    for(int i = 0; i < entry->item_count; i++) {
        if(!item_row_valid(&entry->items[i])) continue;
        if(w != i)
            entry->items[w] = entry->items[i];
        grand += entry->items[w].line_total;
        w++;
    }
    bool changed = (w != entry->item_count);
    entry->item_count = w;
    entry->grand_total = grand;
    return changed;
}

void app_upsert_entry(const app_sales_entry_t * entry)
{
    if(!entry) return;
    char key[APP_SENT_KEY_LEN];
    app_sales_sent_key(entry->employee_id, entry->date, key, sizeof(key));
    if(key_in_list(key)) return;

    for(int i = 0; i < g.entry_count; i++) {
        if(strcmp(g.entries[i].id, entry->id) == 0) {
            g.entries[i] = *entry;
            app_entry_compact_items(&g.entries[i]);
            persist_sales();
            return;
        }
    }
    if(g.entry_count < APP_MAX_ENTRIES) {
        g.entries[g.entry_count] = *entry;
        app_entry_compact_items(&g.entries[g.entry_count]);
        g.entry_count++;
        persist_sales();
    }
}

void app_date_bounds(char * min_out, char * max_out, size_t sz)
{
    char today[APP_DATE_LEN];
    today_str(today, sizeof(today));
    strncpy(max_out, today, sz - 1);
    max_out[sz - 1] = '\0';

#if !APP_USE_LIBC_TIME
    strncpy(min_out, today, sz - 1);
    min_out[sz - 1] = '\0';
#else
    int y, m, d;
    sscanf(today, "%d-%d-%d", &y, &m, &d);
    struct tm tm = {0};
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d - 7;
    mktime(&tm);
    snprintf(min_out, sz, "%04d-%02d-%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
#endif
}

bool app_clamp_selected_date(void)
{
    const char * today = app_today_date();
    if(strcmp(g.selected_date, today) != 0) {
        strncpy(g.selected_date, today, sizeof(g.selected_date) - 1);
        g.selected_date[sizeof(g.selected_date) - 1] = '\0';
        return true;
    }
    return false;
}

void app_data_sync_selected_to_today(void)
{
    app_set_selected_date(app_today_date());
}

const char * app_employee_name(const char * employee_id)
{
    for(int i = 0; i < g.settings.employee_count; i++) {
        if(strcmp(g.settings.employees[i].id, employee_id) == 0)
            return g.settings.employees[i].name;
    }
    return "Unknown";
}

int app_entry_count(void)
{
    return g.entry_count;
}

app_sales_entry_t * app_get_entry(int index)
{
    if(index < 0 || index >= g.entry_count) return NULL;
    return &g.entries[index];
}

void app_upsert_reconciliation(const app_reconciliation_t * rec)
{
    if(!rec) return;
    for(int i = 0; i < g.recon_count; i++) {
        if(strcmp(g.reconciliation[i].employee_id, rec->employee_id) == 0) {
            g.reconciliation[i] = *rec;
            return;
        }
    }
    if(g.recon_count < APP_MAX_RECON) {
        g.reconciliation[g.recon_count++] = *rec;
    }
}

app_reconciliation_t * app_find_reconciliation(const char * employee_id, float system_total)
{
    for(int i = 0; i < g.recon_count; i++) {
        if(strcmp(g.reconciliation[i].employee_id, employee_id) == 0 &&
           fabsf(g.reconciliation[i].system_total - system_total) < 0.01f) {
            return &g.reconciliation[i];
        }
    }
    return NULL;
}

bool app_is_entry_reconciled(const app_sales_entry_t * entry)
{
    if(!entry) return false;
    app_reconciliation_t * r = app_find_reconciliation(entry->employee_id, entry->grand_total);
    return r && r->match;
}

bool app_has_unsent_sales_work(void)
{
    for(int i = 0; i < g.entry_count; i++) {
        if(g.entries[i].item_count <= 0) continue;
        char key[APP_SENT_KEY_LEN];
        app_sales_sent_key(g.entries[i].employee_id, g.entries[i].date, key, sizeof(key));
        if(!key_in_list(key)) return true;
    }
    return false;
}

static bool entry_pending_email(const app_sales_entry_t * e)
{
    if(!e || e->item_count <= 0 || !e->closed) return false;
    if(!app_is_entry_reconciled(e)) return false;
    char key[APP_SENT_KEY_LEN];
    app_sales_sent_key(e->employee_id, e->date, key, sizeof(key));
    return !key_in_list(key);
}

int app_pending_email_count(void)
{
    int n = 0;
    for(int i = 0; i < g.entry_count; i++) {
        if(entry_pending_email(&g.entries[i])) n++;
    }
    return n;
}

const app_sales_entry_t * app_pending_email_entry(int index)
{
    int n = 0;
    for(int i = 0; i < g.entry_count; i++) {
        if(!entry_pending_email(&g.entries[i])) continue;
        if(n == index) return &g.entries[i];
        n++;
    }
    return NULL;
}

static void add_sent_key(const char * key)
{
    if(key_in_list(key)) return;
    if(g.settings.sent_key_count >= APP_MAX_SENT_KEYS) return;
    strncpy(g.settings.sent_keys[g.settings.sent_key_count], key, APP_SENT_KEY_LEN - 1);
    g.settings.sent_keys[g.settings.sent_key_count][APP_SENT_KEY_LEN - 1] = '\0';
    g.settings.sent_key_count++;
}

static void remove_sent_key(const char * key)
{
    for(int i = 0; i < g.settings.sent_key_count; i++) {
        if(strcmp(g.settings.sent_keys[i], key) != 0) continue;
        for(int j = i; j < g.settings.sent_key_count - 1; j++) {
            strncpy(g.settings.sent_keys[j], g.settings.sent_keys[j + 1], APP_SENT_KEY_LEN - 1);
            g.settings.sent_keys[j][APP_SENT_KEY_LEN - 1] = '\0';
        }
        g.settings.sent_key_count--;
        return;
    }
}

static void remove_sent_date(const char * date)
{
    for(int i = 0; i < g.settings.sent_date_count; i++) {
        if(strcmp(g.settings.sent_dates[i], date) != 0) continue;
        for(int j = i; j < g.settings.sent_date_count - 1; j++) {
            strncpy(g.settings.sent_dates[j], g.settings.sent_dates[j + 1], APP_DATE_LEN - 1);
            g.settings.sent_dates[j][APP_DATE_LEN - 1] = '\0';
        }
        g.settings.sent_date_count--;
        return;
    }
}

static void unarchive_entry_by_id(const char * id)
{
    for(int i = 0; i < g.archived_count; i++) {
        if(strcmp(g.archived[i].id, id) != 0) continue;
        if(g.entry_count < APP_MAX_ENTRIES)
            g.entries[g.entry_count++] = g.archived[i];
        for(int j = i; j < g.archived_count - 1; j++)
            g.archived[j] = g.archived[j + 1];
        g.archived_count--;
        return;
    }
}

static void add_sent_date(const char * date)
{
    for(int i = 0; i < g.settings.sent_date_count; i++) {
        if(strcmp(g.settings.sent_dates[i], date) == 0) return;
    }
    if(g.settings.sent_date_count >= APP_MAX_SENT_DATES) return;
    strncpy(g.settings.sent_dates[g.settings.sent_date_count], date, APP_DATE_LEN - 1);
    g.settings.sent_dates[g.settings.sent_date_count][APP_DATE_LEN - 1] = '\0';
    g.settings.sent_date_count++;
}

static bool date_fully_emailed(const char * date)
{
    for(int i = 0; i < g.entry_count; i++) {
        if(strcmp(g.entries[i].date, date) != 0) continue;
        if(g.entries[i].item_count <= 0) continue;
        char key[APP_SENT_KEY_LEN];
        app_sales_sent_key(g.entries[i].employee_id, date, key, sizeof(key));
        if(!key_in_list(key)) return false;
    }
    return true;
}

static void archive_entry_by_id(const char * id)
{
    for(int i = 0; i < g.entry_count; i++) {
        if(strcmp(g.entries[i].id, id) != 0) continue;
        if(g.archived_count < APP_MAX_ARCHIVED) {
            g.archived[g.archived_count++] = g.entries[i];
        }
        for(int j = i; j < g.entry_count - 1; j++) {
            g.entries[j] = g.entries[j + 1];
        }
        g.entry_count--;
        return;
    }
}

int app_send_pending_email(char * msg_out, size_t msg_sz)
{
    int pending = app_pending_email_count();
    printf("[email] send start pending=%d entries=%d\n", pending, g.entry_count);

#if !APP_EMAIL_SIMULATE
    char mail_err[128];
    mail_err[0] = '\0';
    printf("[email] SMTP send...\n");
    if(!app_mail_send_invoice(&g.settings, mail_err, sizeof(mail_err))) {
        printf("[email] SMTP FAILED: %s\n", mail_err[0] ? mail_err : "unknown");
        if(msg_out && msg_sz > 0) {
            snprintf(msg_out, msg_sz, "%s", mail_err[0] ? mail_err : "Email failed");
        }
        return 0;
    }
    printf("[email] SMTP OK\n");
#endif

    char keys[APP_MAX_ENTRIES][APP_SENT_KEY_LEN];
    char dates[APP_MAX_ENTRIES][APP_DATE_LEN];
    char ids[APP_MAX_ENTRIES][APP_ID_LEN];
    int batch = 0;

    for(int i = 0; i < g.entry_count; i++) {
        if(!entry_pending_email(&g.entries[i])) continue;
        if(batch >= APP_MAX_ENTRIES) break;
        app_sales_sent_key(g.entries[i].employee_id, g.entries[i].date,
                           keys[batch], sizeof(keys[batch]));
        strncpy(dates[batch], g.entries[i].date, APP_DATE_LEN - 1);
        dates[batch][APP_DATE_LEN - 1] = '\0';
        strncpy(ids[batch], g.entries[i].id, APP_ID_LEN - 1);
        ids[batch][APP_ID_LEN - 1] = '\0';
        batch++;
    }

    if(batch == 0) {
        printf("[email] WARN: SMTP ok but batch=0 (nothing to archive)\n");
        if(msg_out && msg_sz > 0)
            snprintf(msg_out, msg_sz, "Nothing to archive after send");
        return 0;
    }
    printf("[email] archiving batch=%d\n", batch);

    char new_dates[APP_MAX_ENTRIES][APP_DATE_LEN];
    int new_date_count = 0;

    for(int i = 0; i < batch; i++)
        add_sent_key(keys[i]);

    for(int i = 0; i < batch; i++)
        archive_entry_by_id(ids[i]);

    for(int i = 0; i < batch; i++) {
        if(!date_fully_emailed(dates[i])) continue;
        bool dup = false;
        for(int j = 0; j < new_date_count; j++) {
            if(strcmp(new_dates[j], dates[i]) == 0) { dup = true; break; }
        }
        if(dup) continue;
        if(new_date_count < APP_MAX_ENTRIES) {
            strncpy(new_dates[new_date_count], dates[i], APP_DATE_LEN - 1);
            new_dates[new_date_count][APP_DATE_LEN - 1] = '\0';
            new_date_count++;
        }
    }
    for(int i = 0; i < new_date_count; i++)
        add_sent_date(new_dates[i]);

    if(!persist_sales()) {
        printf("[email] ERROR: sales.cfg save failed after send\n");
        for(int i = 0; i < new_date_count; i++)
            remove_sent_date(new_dates[i]);
        for(int i = 0; i < batch; i++) {
            remove_sent_key(keys[i]);
            unarchive_entry_by_id(ids[i]);
        }
        if(msg_out && msg_sz > 0)
            snprintf(msg_out, msg_sz, "Email sent but SD save failed — retry");
        return 0;
    }
    printf("[email] sales saved active=%d archived=%d\n", g.entry_count, g.archived_count);

    if(!app_storage_save_settings(&g.settings)) {
        printf("[email] WARN: settings save after email failed\n");
    }
    else {
        printf("[email] settings saved\n");
    }

    if(msg_out && msg_sz > 0) {
#if APP_EMAIL_SIMULATE
        snprintf(msg_out, msg_sz, "Sent %d invoice%s (saved to SD).",
                 batch, batch > 1 ? "s" : "");
#else
        snprintf(msg_out, msg_sz, "Emailed %d invoice%s — list cleared (on SD).",
                 batch, batch > 1 ? "s" : "");
#endif
    }
    printf("[email] send complete batch=%d\n", batch);
    return batch;
}

bool app_apply_settings(const app_settings_t * settings)
{
    if(!settings) return false;
    g.settings = *settings;
    /* Settings save must not rewrite sales.cfg — entries are saved on each edit. */
    return app_storage_save_settings(&g.settings);
}

bool app_employee_name_exists(const app_settings_t * settings, const char * name,
                              const char * exclude_id)
{
    if(!settings || !name || !name[0]) return false;
    for(int i = 0; i < settings->employee_count; i++) {
        if(exclude_id && strcmp(settings->employees[i].id, exclude_id) == 0) continue;
        if(name_equal_ci(settings->employees[i].name, name)) return true;
    }
    return false;
}

void app_unlock_sent_date(app_settings_t * settings, const char * date)
{
    if(!settings || !date) return;

    int w = 0;
    for(int i = 0; i < settings->sent_date_count; i++) {
        if(strcmp(settings->sent_dates[i], date) == 0) continue;
        if(w != i) strcpy(settings->sent_dates[w], settings->sent_dates[i]);
        w++;
    }
    settings->sent_date_count = w;

    char suffix[20];
    snprintf(suffix, sizeof(suffix), "|%s", date);
    size_t slen = strlen(suffix);
    w = 0;
    for(int i = 0; i < settings->sent_key_count; i++) {
        size_t klen = strlen(settings->sent_keys[i]);
        if(klen >= slen && strcmp(settings->sent_keys[i] + klen - slen, suffix) == 0) continue;
        if(w != i) strcpy(settings->sent_keys[w], settings->sent_keys[i]);
        w++;
    }
    settings->sent_key_count = w;
}

static const app_sales_entry_t * history_entry_ptr(int index)
{
    int n = 0;
    for(int i = 0; i < g.entry_count; i++) {
        if(g.entries[i].item_count <= 0) continue;
        if(n == index) return &g.entries[i];
        n++;
    }
    for(int i = 0; i < g.archived_count; i++) {
        if(g.archived[i].item_count <= 0) continue;
        if(n == index) return &g.archived[i];
        n++;
    }
    return NULL;
}

int app_history_entry_count(void)
{
    int n = 0;
    for(int i = 0; i < g.entry_count; i++)
        if(g.entries[i].item_count > 0) n++;
    for(int i = 0; i < g.archived_count; i++)
        if(g.archived[i].item_count > 0) n++;
    return n;
}

const app_sales_entry_t * app_history_get_entry(int index)
{
    return history_entry_ptr(index);
}

static int cmp_week_desc(const void * a, const void * b)
{
    const app_week_summary_t * wa = a;
    const app_week_summary_t * wb = b;
    return strcmp(wb->week_key, wa->week_key);
}

static int build_week_summaries(app_week_summary_t * out, int max_out)
{
    app_week_summary_t weeks[APP_MAX_WEEKS];
    int wn = 0;
    int total = app_history_entry_count();
    for(int i = 0; i < total; i++) {
        const app_sales_entry_t * e = app_history_get_entry(i);
        if(!e) continue;
        int found = -1;
        for(int w = 0; w < wn; w++) {
            if(strcmp(weeks[w].week_key, e->week_key) == 0) { found = w; break; }
        }
        if(found < 0) {
            if(wn >= APP_MAX_WEEKS) continue;
            found = wn++;
            strncpy(weeks[found].week_key, e->week_key, APP_DATE_LEN - 1);
            weeks[found].week_key[APP_DATE_LEN - 1] = '\0';
            snprintf(weeks[found].week_label, sizeof(weeks[found].week_label),
                     "Week of %s", e->week_key);
            weeks[found].total = 0;
            weeks[found].entry_count = 0;
        }
        weeks[found].total += e->grand_total;
        weeks[found].entry_count++;
    }
    qsort(weeks, (size_t)wn, sizeof(app_week_summary_t), cmp_week_desc);
    if(out && max_out > 0) {
        int copy = wn < max_out ? wn : max_out;
        memcpy(out, weeks, sizeof(app_week_summary_t) * (size_t)copy);
    }
    return wn;
}

int app_week_summary_count(void)
{
    return build_week_summaries(NULL, 0);
}

void app_week_summary_get(int index, app_week_summary_t * out)
{
    if(!out) return;
    app_week_summary_t weeks[APP_MAX_WEEKS];
    int wn = build_week_summaries(weeks, APP_MAX_WEEKS);
    if(index >= 0 && index < wn) *out = weeks[index];
    else memset(out, 0, sizeof(*out));
}

int app_week_entry_count(const char * week_key)
{
    if(!week_key) return 0;
    int n = 0;
    int total = app_history_entry_count();
    for(int i = 0; i < total; i++) {
        const app_sales_entry_t * e = app_history_get_entry(i);
        if(e && strcmp(e->week_key, week_key) == 0) n++;
    }
    return n;
}

const app_sales_entry_t * app_week_entry_at(const char * week_key, int index)
{
    if(!week_key) return NULL;
    int n = 0;
    int total = app_history_entry_count();
    for(int i = 0; i < total; i++) {
        const app_sales_entry_t * e = app_history_get_entry(i);
        if(!e || strcmp(e->week_key, week_key) != 0) continue;
        if(n == index) return e;
        n++;
    }
    return NULL;
}

static int build_invoice_dates(char dates[][APP_DATE_LEN], int max_dates)
{
    int dn = 0;
    int total = app_history_entry_count();
    for(int i = 0; i < total; i++) {
        const app_sales_entry_t * e = app_history_get_entry(i);
        if(!e) continue;
        bool dup = false;
        for(int d = 0; d < dn; d++) {
            if(strcmp(dates[d], e->date) == 0) { dup = true; break; }
        }
        if(!dup && dn < max_dates) {
            strncpy(dates[dn], e->date, APP_DATE_LEN - 1);
            dates[dn++][APP_DATE_LEN - 1] = '\0';
        }
    }
    for(int i = 0; i < dn; i++) {
        for(int j = i + 1; j < dn; j++) {
            if(strcmp(dates[j], dates[i]) > 0) {
                char tmp[APP_DATE_LEN];
                strcpy(tmp, dates[i]);
                strcpy(dates[i], dates[j]);
                strcpy(dates[j], tmp);
            }
        }
    }
    return dn;
}

#define APP_INVOICE_DATE_MAX  24

int app_invoice_date_count(void)
{
    char dates[APP_INVOICE_DATE_MAX][APP_DATE_LEN];
    return build_invoice_dates(dates, APP_INVOICE_DATE_MAX);
}

const char * app_invoice_date_at(int index)
{
    static char dates[APP_INVOICE_DATE_MAX][APP_DATE_LEN];
    static int dn;
    dn = build_invoice_dates(dates, APP_INVOICE_DATE_MAX);
    if(index < 0 || index >= dn) return NULL;
    return dates[index];
}

void app_invoice_date_status_label(const char * date, char * out, size_t out_sz)
{
    if(!out || out_sz == 0) return;
    bool all_sent = true;
    bool any_ready = false;
    int total = app_history_entry_count();
    for(int i = 0; i < total; i++) {
        const app_sales_entry_t * e = app_history_get_entry(i);
        if(!e || strcmp(e->date, date) != 0) continue;
        char key[APP_SENT_KEY_LEN];
        app_sales_sent_key(e->employee_id, e->date, key, sizeof(key));
        if(!key_in_list(key) && !app_is_date_sent(date)) all_sent = false;
        if(e->closed && app_is_entry_reconciled(e)) any_ready = true;
    }
    if(all_sent) strncpy(out, "SENT", out_sz - 1);
    else if(any_ready) strncpy(out, "PARTIAL", out_sz - 1);
    else strncpy(out, "IN PROGRESS", out_sz - 1);
    out[out_sz - 1] = '\0';
}

app_inv_badge_t app_entry_invoice_badge(const app_sales_entry_t * entry)
{
    if(!entry || entry->item_count == 0) return APP_INV_BADGE_NONE;
    char key[APP_SENT_KEY_LEN];
    app_sales_sent_key(entry->employee_id, entry->date, key, sizeof(key));
    if(key_in_list(key) || app_is_date_sent(entry->date)) return APP_INV_BADGE_SENT;
    if(!entry->closed) return APP_INV_BADGE_OPEN;
    if(!app_is_entry_reconciled(entry)) return APP_INV_BADGE_AWAIT_RECON;
    return APP_INV_BADGE_PENDING;
}

void app_clear_week(const char * week_key)
{
    if(!week_key) return;
    for(int i = g.entry_count - 1; i >= 0; i--) {
        if(strcmp(g.entries[i].week_key, week_key) == 0) {
            for(int j = i; j < g.entry_count - 1; j++) g.entries[j] = g.entries[j + 1];
            g.entry_count--;
        }
    }
    for(int i = g.archived_count - 1; i >= 0; i--) {
        if(strcmp(g.archived[i].week_key, week_key) == 0) {
            for(int j = i; j < g.archived_count - 1; j++) g.archived[j] = g.archived[j + 1];
            g.archived_count--;
        }
    }
    bool emp_still[APP_MAX_EMPLOYEES];
    for(int e = 0; e < g.settings.employee_count; e++) {
        emp_still[e] = false;
        const char * eid = g.settings.employees[e].id;
        for(int i = 0; i < g.entry_count; i++) {
            if(strcmp(g.entries[i].employee_id, eid) == 0) { emp_still[e] = true; break; }
        }
        if(emp_still[e]) continue;
        for(int i = 0; i < g.archived_count; i++) {
            if(strcmp(g.archived[i].employee_id, eid) == 0) { emp_still[e] = true; break; }
        }
    }
    int rw = 0;
    for(int i = 0; i < g.recon_count; i++) {
        bool keep = false;
        for(int e = 0; e < g.settings.employee_count; e++) {
            if(!emp_still[e]) continue;
            if(strcmp(g.reconciliation[i].employee_id, g.settings.employees[e].id) == 0) {
                keep = true;
                break;
            }
        }
        if(keep) {
            if(rw != i) g.reconciliation[rw] = g.reconciliation[i];
            rw++;
        }
    }
    g.recon_count = rw;
    persist_sales();
}

bool app_is_monday_today(void)
{
#if !APP_USE_LIBC_TIME
    return false;
#else
    time_t t = time(NULL);
    struct tm * tm = localtime(&t);
    return tm && tm->tm_wday == 1;
#endif
}
