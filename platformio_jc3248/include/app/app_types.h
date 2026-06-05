#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define APP_MAX_EMPLOYEES   8
#define APP_MAX_ITEMS       64
#ifndef APP_MAX_ENTRIES
#define APP_MAX_ENTRIES     40
#endif
#ifndef APP_MAX_ARCHIVED
#define APP_MAX_ARCHIVED    20
#endif
#define APP_MAX_WEEKS       24
#ifndef APP_MAX_SENT_KEYS
#define APP_MAX_SENT_KEYS   128
#endif
#ifndef APP_MAX_SENT_DATES
#define APP_MAX_SENT_DATES  32
#endif
#define APP_BARCODE_LEN     16
#define APP_ID_LEN          24
#define APP_NAME_LEN        32
#define APP_DATE_LEN        12
#define APP_SENT_KEY_LEN    48
#define APP_WIFI_SSID_LEN   33
#define APP_WIFI_PASS_LEN   64
#define APP_SMTP_PASS_LEN   64
#define APP_WIFI_MAX_SCAN   16

typedef struct {
    char id[APP_ID_LEN];
    char barcode[APP_BARCODE_LEN];
    float price;
    float discount;
    int quantity;
    float line_total;
} app_sales_item_t;

typedef struct {
    char id[APP_ID_LEN];
    char employee_id[APP_ID_LEN];
    char employee_name[APP_NAME_LEN];
    app_sales_item_t items[APP_MAX_ITEMS];
    int item_count;
    float grand_total;
    float manual_total;
    bool has_manual_total;
    bool closed;
    char date[APP_DATE_LEN];
    char week_key[APP_DATE_LEN];
} app_sales_entry_t;

typedef struct {
    char id[APP_ID_LEN];
    char name[APP_NAME_LEN];
} app_employee_t;

typedef struct {
    char employee_id[APP_ID_LEN];
    float system_total;
    float actual_cash;
    bool match;
} app_reconciliation_t;

#define APP_MAX_RECON  16

typedef struct {
    char system_pin[8];
    char master_pin[8];
    app_employee_t employees[APP_MAX_EMPLOYEES];
    int employee_count;
    char sent_dates[APP_MAX_SENT_DATES][APP_DATE_LEN];
    int sent_date_count;
    char sent_keys[APP_MAX_SENT_KEYS][APP_SENT_KEY_LEN];
    int sent_key_count;
    char promo_head_name[APP_NAME_LEN];
    char sender_email[64];
    char recipient_email[64];
    char smtp_app_password[APP_SMTP_PASS_LEN];
    char wifi_ssid[APP_WIFI_SSID_LEN];
    char wifi_password[APP_WIFI_PASS_LEN];
} app_settings_t;

typedef struct {
    char ssid[APP_WIFI_SSID_LEN];
    int8_t rssi;
    bool secure;
} app_wifi_network_t;

typedef enum {
    APP_SCREEN_SALES = 0,
    APP_SCREEN_DASH,
    APP_SCREEN_HISTORY,
    APP_SCREEN_SETTINGS,
} app_screen_t;

typedef enum {
    APP_INV_BADGE_NONE = 0,
    APP_INV_BADGE_SENT,
    APP_INV_BADGE_OPEN,
    APP_INV_BADGE_AWAIT_RECON,
    APP_INV_BADGE_PENDING,
} app_inv_badge_t;

typedef struct {
    char week_key[APP_DATE_LEN];
    char week_label[40];
    float total;
    int entry_count;
} app_week_summary_t;

#endif
