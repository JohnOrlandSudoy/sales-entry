#ifndef APP_DATA_H
#define APP_DATA_H

#include <stddef.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_data_init(void);

/** Re-load settings + sales from SD (call after late mount if boot load missed card). */
void app_data_reload_from_storage(void);

app_settings_t * app_settings(void);

/** Live calendar date from RTC/NTP (use for sales, dash, email). */
const char * app_today_date(void);
/** Same as app_today_date() — legacy name. */
const char * app_selected_date(void);
void app_set_selected_date(const char * yyyy_mm_dd);

const char * app_sales_employee_id(void);
void app_set_sales_employee_id(const char * id);
/** Set before opening Sales from Dashboard Edit (auto-fill first item). */
void app_set_sales_open_for_edit(bool edit);
bool app_take_sales_open_for_edit(void);

app_sales_entry_t * app_find_entry(const char * employee_id, const char * date);
bool app_is_entry_sent(const char * employee_id, const char * date);
bool app_is_date_sent(const char * date);

void app_upsert_entry(const app_sales_entry_t * entry);
/** Drop blank/zero items and recompute grand_total. Returns true if item_count changed. */
bool app_entry_compact_items(app_sales_entry_t * entry);

void app_gen_id(char * out, size_t out_sz);
void app_pad_barcode(const char * input, char * out, size_t out_sz);
void app_week_key(const char * yyyy_mm_dd, char * out, size_t out_sz);
void app_sales_sent_key(const char * employee_id, const char * date, char * out, size_t out_sz);

void app_date_bounds(char * min_out, char * max_out, size_t sz);
bool app_clamp_selected_date(void);
/** Set sales date to today (boot / after NTP). */
void app_data_sync_selected_to_today(void);

const char * app_employee_name(const char * employee_id);

int app_entry_count(void);
app_sales_entry_t * app_get_entry(int index);

void app_upsert_reconciliation(const app_reconciliation_t * rec);
app_reconciliation_t * app_find_reconciliation(const char * employee_id, float system_total);

bool app_is_entry_reconciled(const app_sales_entry_t * entry);
bool app_has_unsent_sales_work(void);
int app_pending_email_count(void);
const app_sales_entry_t * app_pending_email_entry(int index);

/** Mark pending CLOSED+reconciled entries as sent; returns number of salesman keys sent. */
int app_send_pending_email(char * msg_out, size_t msg_sz);

bool app_apply_settings(const app_settings_t * settings);

bool app_employee_name_exists(const app_settings_t * settings, const char * name,
                              const char * exclude_id);

/** Remove locked date and all sent keys for that date (draft or live settings). */
void app_unlock_sent_date(app_settings_t * settings, const char * date);

/* History: active + archived (emailed) entries */
int app_history_entry_count(void);
const app_sales_entry_t * app_history_get_entry(int index);

int app_week_summary_count(void);
void app_week_summary_get(int index, app_week_summary_t * out);
const app_sales_entry_t * app_week_entry_at(const char * week_key, int index);
int app_week_entry_count(const char * week_key);

int app_invoice_date_count(void);
const char * app_invoice_date_at(int index);
void app_invoice_date_status_label(const char * date, char * out, size_t out_sz);

app_inv_badge_t app_entry_invoice_badge(const app_sales_entry_t * entry);
void app_clear_week(const char * week_key);
bool app_is_monday_today(void);

#ifdef __cplusplus
}
#endif

#endif
