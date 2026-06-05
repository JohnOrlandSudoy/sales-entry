#include "app_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
  #include "app_sd_mount.h"
  #include "app_storage_backup.h"
  #include <sys/stat.h>
  #define APP_SETTINGS_FILE_DEFAULT  "/sdcard/DailySales/settings.cfg"
#else
  #define APP_STORAGE_DIR        "data"
  #define APP_SETTINGS_FILE      APP_STORAGE_DIR "/settings.cfg"
  #define APP_SETTINGS_FALLBACK  "../data/settings.cfg"
  #include <errno.h>
  #ifdef _WIN32
    #include <direct.h>
  #else
    #include <sys/stat.h>
  #endif
#endif

static char s_active_path[96];
#ifdef ESP_PLATFORM
static bool s_last_settings_on_sd;
static char s_last_err[80];
#endif

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

#ifndef ESP_PLATFORM
static bool mkdir_one(const char * dir)
{
#ifdef _WIN32
    return _mkdir(dir) == 0 || errno == EEXIST;
#else
    return mkdir(dir, 0755) == 0 || errno == EEXIST;
#endif
}

static bool ensure_storage_dir(void)
{
    bool ok = mkdir_one(APP_STORAGE_DIR);
#ifndef ESP_PLATFORM
    mkdir_one("../data");
#endif
    return ok;
}
#endif

#ifdef ESP_PLATFORM
static void storage_build_paths(char * settings, size_t settings_sz,
                                char * sales, size_t sales_sz)
{
    const char * dir = app_sd_data_dir();
    if(settings && settings_sz > 0)
        snprintf(settings, settings_sz, "%s/settings.cfg", dir);
    if(sales && sales_sz > 0)
        snprintf(sales, sales_sz, "%s/sales.cfg", dir);
}

static bool ensure_storage_dir(void)
{
    if(!app_sd_is_mounted()) {
        if(!app_sd_mount_retry_user() && !app_sd_remount()) {
            snprintf(s_last_err, sizeof(s_last_err), "%s", app_sd_last_error());
            printf("[storage] SD not mounted\n");
            return false;
        }
    }
    if(!app_sd_ensure_data_dir()) {
        /* Card mounted but subfolder probe failed — still try /sdcard paths */
        if(app_sd_is_mounted()) {
            printf("[storage] using SD path %s (limited)\n", app_sd_data_dir());
            return true;
        }
        snprintf(s_last_err, sizeof(s_last_err), "SD folder not writable");
        printf("[storage] failed to ensure %s\n", app_sd_data_dir());
        return false;
    }
    return true;
}

bool app_storage_last_settings_on_sd(void)
{
    return s_last_settings_on_sd;
}

const char * app_storage_last_error(void)
{
    return s_last_err[0] ? s_last_err : "unknown";
}
#endif

const char * app_storage_settings_path(void)
{
    if(s_active_path[0])
        return s_active_path;
#ifdef ESP_PLATFORM
    static char s_default_settings[96];
    storage_build_paths(s_default_settings, sizeof(s_default_settings), NULL, 0);
    return s_default_settings;
#else
    return APP_SETTINGS_FILE;
#endif
}

static bool write_settings_body(FILE * f, const app_settings_t * settings)
{
    if(!f || !settings) return false;
    fprintf(f, "v1\n");
    fprintf(f, "system_pin=%s\n", settings->system_pin);
    fprintf(f, "master_pin=%s\n", settings->master_pin);
    fprintf(f, "promo_head=%s\n", settings->promo_head_name);
    fprintf(f, "sender_email=%s\n", settings->sender_email);
    fprintf(f, "recipient_email=%s\n", settings->recipient_email);
    fprintf(f, "smtp_app_password=%s\n", settings->smtp_app_password);
    fprintf(f, "wifi_ssid=%s\n", settings->wifi_ssid);
    fprintf(f, "wifi_password=%s\n", settings->wifi_password);
    fprintf(f, "@employees\n");
    for(int i = 0; i < settings->employee_count; i++) {
        fprintf(f, "%s|%s\n", settings->employees[i].id, settings->employees[i].name);
    }
    fprintf(f, "@sent_dates\n");
    for(int i = 0; i < settings->sent_date_count; i++) {
        fprintf(f, "%s\n", settings->sent_dates[i]);
    }
    fprintf(f, "@sent_keys\n");
    for(int i = 0; i < settings->sent_key_count; i++) {
        fprintf(f, "%s\n", settings->sent_keys[i]);
    }
    return fflush(f) == 0;
}

/** Write temp file then rename — avoids blank sales/settings after power loss mid-save. */
static bool atomic_commit_file(const char * final_path, const char * tmp_path,
                               bool (*writer)(FILE * f, void * ctx), void * ctx)
{
    FILE * f = fopen(tmp_path, "w");
    if(!f) {
        printf("[storage] tmp open failed: %s\n", tmp_path);
        return false;
    }
    if(!writer(f, ctx)) {
        fclose(f);
        remove(tmp_path);
        return false;
    }
    fclose(f);
    remove(final_path);
    if(rename(tmp_path, final_path) != 0) {
        printf("[storage] rename failed %s -> %s\n", tmp_path, final_path);
        remove(tmp_path);
        return false;
    }
    return true;
}

static bool settings_file_writer(FILE * f, void * ctx)
{
    return write_settings_body(f, (const app_settings_t *)ctx);
}

bool app_storage_save_settings(const app_settings_t * settings)
{
    if(!settings) return false;

#ifdef ESP_PLATFORM
    s_last_settings_on_sd = false;
    s_last_err[0] = '\0';
#endif

#ifndef ESP_PLATFORM
    ensure_storage_dir();
#endif

    char settings_path[96];
    storage_build_paths(settings_path, sizeof(settings_path), NULL, 0);
    const char * final_path = s_active_path[0] ? s_active_path : settings_path;
    char tmp_path[128];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", final_path);

#ifdef ESP_PLATFORM
    bool sd_ready = ensure_storage_dir();
#else
    bool sd_ready = true;
#endif

    if(sd_ready && atomic_commit_file(final_path, tmp_path, settings_file_writer, (void *)settings)) {
#ifdef ESP_PLATFORM
        s_last_settings_on_sd = true;
        (void)app_storage_backup_save_settings(settings);
#endif
        return true;
    }

#ifndef ESP_PLATFORM
    if(strcmp(final_path, APP_SETTINGS_FILE) == 0) {
        if(atomic_commit_file(APP_SETTINGS_FALLBACK, APP_SETTINGS_FALLBACK ".tmp",
                             settings_file_writer, (void *)settings)) {
            strncpy(s_active_path, APP_SETTINGS_FALLBACK, sizeof(s_active_path) - 1);
            s_active_path[sizeof(s_active_path) - 1] = '\0';
            return true;
        }
    }
#endif

#ifdef ESP_PLATFORM
    printf("[storage] SD settings save failed, trying NVS backup\n");
    if(app_storage_backup_save_settings(settings)) {
        snprintf(s_last_err, sizeof(s_last_err), "SD unavailable — saved to device");
        return true;
    }
    snprintf(s_last_err, sizeof(s_last_err), "%s", app_sd_last_error());
#endif
    return false;
}

static bool parse_settings_file(FILE * f, app_settings_t * out)
{

    app_settings_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    strcpy(tmp.system_pin, "1234");
    strcpy(tmp.master_pin, "9999");
    strcpy(tmp.promo_head_name, "Ms. Helen");

    enum { SEC_NONE, SEC_EMP, SEC_DATES, SEC_KEYS } section = SEC_NONE;
    char line[128];

    while(fgets(line, sizeof(line), f)) {
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
            strncpy(tmp.sent_dates[tmp.sent_date_count], line, APP_DATE_LEN - 1);
            tmp.sent_dates[tmp.sent_date_count][APP_DATE_LEN - 1] = '\0';
            tmp.sent_date_count++;
            continue;
        }
        if(section == SEC_KEYS) {
            if(tmp.sent_key_count >= APP_MAX_SENT_KEYS) continue;
            strncpy(tmp.sent_keys[tmp.sent_key_count], line, APP_SENT_KEY_LEN - 1);
            tmp.sent_keys[tmp.sent_key_count][APP_SENT_KEY_LEN - 1] = '\0';
            tmp.sent_key_count++;
            continue;
        }

        char * eq = strchr(line, '=');
        if(!eq) continue;
        *eq = '\0';
        const char * val = eq + 1;
        if(strcmp(line, "system_pin") == 0) {
            strncpy(tmp.system_pin, val, sizeof(tmp.system_pin) - 1);
            tmp.system_pin[sizeof(tmp.system_pin) - 1] = '\0';
        }
        else if(strcmp(line, "master_pin") == 0) {
            strncpy(tmp.master_pin, val, sizeof(tmp.master_pin) - 1);
            tmp.master_pin[sizeof(tmp.master_pin) - 1] = '\0';
        }
        else if(strcmp(line, "promo_head") == 0) {
            strncpy(tmp.promo_head_name, val, APP_NAME_LEN - 1);
            tmp.promo_head_name[APP_NAME_LEN - 1] = '\0';
        }
        else if(strcmp(line, "sender_email") == 0) {
            strncpy(tmp.sender_email, val, sizeof(tmp.sender_email) - 1);
            tmp.sender_email[sizeof(tmp.sender_email) - 1] = '\0';
        }
        else if(strcmp(line, "recipient_email") == 0) {
            strncpy(tmp.recipient_email, val, sizeof(tmp.recipient_email) - 1);
            tmp.recipient_email[sizeof(tmp.recipient_email) - 1] = '\0';
        }
        else if(strcmp(line, "smtp_app_password") == 0) {
            strncpy(tmp.smtp_app_password, val, sizeof(tmp.smtp_app_password) - 1);
            tmp.smtp_app_password[sizeof(tmp.smtp_app_password) - 1] = '\0';
        }
        else if(strcmp(line, "wifi_ssid") == 0) {
            strncpy(tmp.wifi_ssid, val, sizeof(tmp.wifi_ssid) - 1);
            tmp.wifi_ssid[sizeof(tmp.wifi_ssid) - 1] = '\0';
        }
        else if(strcmp(line, "wifi_password") == 0) {
            strncpy(tmp.wifi_password, val, sizeof(tmp.wifi_password) - 1);
            tmp.wifi_password[sizeof(tmp.wifi_password) - 1] = '\0';
        }
    }

    if(tmp.employee_count == 0) return false;

    *out = tmp;
    return true;
}

bool app_storage_load_settings(app_settings_t * out)
{
    if(!out) return false;

    char path_a[96], path_b[96], path_c[96];
#ifdef ESP_PLATFORM
    storage_build_paths(path_a, sizeof(path_a), NULL, 0);
    snprintf(path_b, sizeof(path_b), "/sdcard/DailySales/settings.cfg");
    snprintf(path_c, sizeof(path_c), "/sdcard/settings.cfg");
    const char * paths[] = { path_a, path_b, path_c, NULL };
#else
    const char * paths[] = { APP_SETTINGS_FILE, APP_SETTINGS_FALLBACK, NULL };
#endif

    for(int i = 0; paths[i]; i++) {
        FILE * f = fopen(paths[i], "r");
        if(!f) continue;
        strncpy(s_active_path, paths[i], sizeof(s_active_path) - 1);
        s_active_path[sizeof(s_active_path) - 1] = '\0';
        bool ok = parse_settings_file(f, out);
        fclose(f);
        if(ok) return true;
    }
#ifdef ESP_PLATFORM
    if(app_storage_backup_load_settings(out)) {
        printf("[storage] loaded settings from NVS backup\n");
        return true;
    }
#endif
    return false;
}

#ifndef ESP_PLATFORM
  #define APP_SALES_FILE  APP_STORAGE_DIR "/sales.cfg"
  #define APP_SALES_FALLBACK "../data/sales.cfg"
#endif

static char s_sales_path[96];

static void write_entry(FILE * f, const char * tag, const app_sales_entry_t * e)
{
    fprintf(f, "%s\n", tag);
    fprintf(f, "id=%s\n", e->id);
    fprintf(f, "emp_id=%s\n", e->employee_id);
    fprintf(f, "emp_name=%s\n", e->employee_name);
    fprintf(f, "date=%s\n", e->date);
    fprintf(f, "week=%s\n", e->week_key);
    fprintf(f, "closed=%d\n", e->closed ? 1 : 0);
    fprintf(f, "grand=%.2f\n", e->grand_total);
    fprintf(f, "manual=%.2f\n", e->has_manual_total ? e->manual_total : 0.0f);
    fprintf(f, "items=%d\n", e->item_count);
    for(int i = 0; i < e->item_count; i++) {
        const app_sales_item_t * it = &e->items[i];
        fprintf(f, "item=%s|%.2f|%.2f|%d|%.2f\n",
                it->barcode, it->price, it->discount, it->quantity, it->line_total);
    }
}

typedef struct {
    const app_sales_entry_t * active;
    int active_count;
    const app_sales_entry_t * archived;
    int archived_count;
} sales_write_ctx_t;

static bool sales_file_writer(FILE * f, void * ctx)
{
    sales_write_ctx_t * s = (sales_write_ctx_t *)ctx;
    if(!f || !s) return false;
    fprintf(f, "v1\n");
    for(int i = 0; i < s->active_count; i++)
        write_entry(f, "@active", &s->active[i]);
    for(int i = 0; i < s->archived_count; i++)
        write_entry(f, "@archive", &s->archived[i]);
    return fflush(f) == 0;
}

bool app_storage_save_sales(const app_sales_entry_t * active, int active_count,
                            const app_sales_entry_t * archived, int archived_count)
{
#ifdef ESP_PLATFORM
    bool sd_ready = ensure_storage_dir();
#else
    ensure_storage_dir();
    bool sd_ready = true;
#endif

    char sales_path[96];
    storage_build_paths(NULL, 0, sales_path, sizeof(sales_path));
    const char * final_path = s_sales_path[0] ? s_sales_path : sales_path;
    char tmp_path[128];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", final_path);

    sales_write_ctx_t ctx = { active, active_count, archived, archived_count };
    if(sd_ready && atomic_commit_file(final_path, tmp_path, sales_file_writer, &ctx)) {
        printf("[storage] sales saved %s (active=%d archived=%d)\n",
               final_path, active_count, archived_count);
        return true;
    }

#ifndef ESP_PLATFORM
    if(APP_SALES_FALLBACK) {
        sales_write_ctx_t ctx2 = ctx;
        if(atomic_commit_file(APP_SALES_FALLBACK, APP_SALES_FALLBACK ".tmp",
                              sales_file_writer, &ctx2)) {
            strncpy(s_sales_path, APP_SALES_FALLBACK, sizeof(s_sales_path) - 1);
            s_sales_path[sizeof(s_sales_path) - 1] = '\0';
            return true;
        }
    }
#endif
    printf("[storage] sales save failed: %s\n", final_path);
    return false;
}

static bool parse_entry_line(app_sales_entry_t * e, const char * key, const char * val)
{
    if(strcmp(key, "id") == 0) strncpy(e->id, val, APP_ID_LEN - 1);
    else if(strcmp(key, "emp_id") == 0) strncpy(e->employee_id, val, APP_ID_LEN - 1);
    else if(strcmp(key, "emp_name") == 0) strncpy(e->employee_name, val, APP_NAME_LEN - 1);
    else if(strcmp(key, "date") == 0) strncpy(e->date, val, APP_DATE_LEN - 1);
    else if(strcmp(key, "week") == 0) strncpy(e->week_key, val, APP_DATE_LEN - 1);
    else if(strcmp(key, "closed") == 0) e->closed = (atoi(val) != 0);
    else if(strcmp(key, "grand") == 0) e->grand_total = (float)atof(val);
    else if(strcmp(key, "manual") == 0) { e->manual_total = (float)atof(val); e->has_manual_total = true; }
    /* items=N is legacy header only — real count comes from item= lines below */
    else if(strcmp(key, "items") == 0) e->item_count = 0;
    else if(strcmp(key, "item") == 0) {
        if(e->item_count < APP_MAX_ITEMS) {
            app_sales_item_t * it = &e->items[e->item_count++];
            memset(it, 0, sizeof(*it));
            sscanf(val, "%[^|]|%f|%f|%d|%f",
                   it->barcode, &it->price, &it->discount, &it->quantity, &it->line_total);
        }
    }
    else return false;
    return true;
}

bool app_storage_load_sales(app_sales_entry_t * active, int * active_count,
                            app_sales_entry_t * archived, int * archived_count)
{
    if(!active || !active_count || !archived || !archived_count) return false;
    *active_count = 0;
    *archived_count = 0;

    char sales_a[96], sales_b[96], sales_c[96];
#ifdef ESP_PLATFORM
    storage_build_paths(NULL, 0, sales_a, sizeof(sales_a));
    snprintf(sales_b, sizeof(sales_b), "/sdcard/DailySales/sales.cfg");
    snprintf(sales_c, sizeof(sales_c), "/sdcard/sales.cfg");
    const char * paths[] = { sales_a, sales_b, sales_c, NULL };
#else
    const char * paths[] = { APP_SALES_FILE, APP_SALES_FALLBACK, NULL };
#endif
    FILE * f = NULL;
    for(int i = 0; paths[i]; i++) {
        f = fopen(paths[i], "r");
        if(f) {
            strncpy(s_sales_path, paths[i], sizeof(s_sales_path) - 1);
            s_sales_path[sizeof(s_sales_path) - 1] = '\0';
            break;
        }
    }
    if(!f) return false;

    app_sales_entry_t cur;
    memset(&cur, 0, sizeof(cur));
    bool in_entry = false;
    bool is_archive = false;
    char line[160];

    while(fgets(line, sizeof(line), f)) {
        trim(line);
        if(line[0] == '\0') continue;
        if(strcmp(line, "@active") == 0 || strcmp(line, "@archive") == 0) {
            if(in_entry) {
                if(is_archive && *archived_count < APP_MAX_ARCHIVED)
                    archived[(*archived_count)++] = cur;
                else if(!is_archive && *active_count < APP_MAX_ENTRIES)
                    active[(*active_count)++] = cur;
            }
            memset(&cur, 0, sizeof(cur));
            in_entry = true;
            is_archive = (strcmp(line, "@archive") == 0);
            continue;
        }
        char * eq = strchr(line, '=');
        if(!eq || !in_entry) continue;
        *eq = '\0';
        parse_entry_line(&cur, line, eq + 1);
    }
    if(in_entry) {
        if(is_archive && *archived_count < APP_MAX_ARCHIVED)
            archived[(*archived_count)++] = cur;
        else if(!is_archive && *active_count < APP_MAX_ENTRIES)
            active[(*active_count)++] = cur;
    }
    fclose(f);
    return (*active_count > 0 || *archived_count > 0);
}

bool app_storage_save_all(const app_settings_t * settings,
                          const app_sales_entry_t * active, int active_count,
                          const app_sales_entry_t * archived, int archived_count)
{
    bool ok1 = settings ? app_storage_save_settings(settings) : true;
    bool ok2 = app_storage_save_sales(active, active_count, archived, archived_count);
    return ok1 && ok2;
}

#ifndef ESP_PLATFORM
bool app_storage_last_settings_on_sd(void)
{
    return true;
}

const char * app_storage_last_error(void)
{
    return "unknown";
}
#endif
