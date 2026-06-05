#include "app_mail.h"
#include "app_data.h"
#include "app_defaults.h"
#include "app_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)

#include "app_wifi.h"
#include <ESP_Mail_Client.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <time.h>

#define APP_MAIL_CSV_MAX  (16 * 1024)
/* ESP Mail Client requires valid time for TLS (see ESP_MAIL_CLIENT_VALID_TS). */
#define APP_MAIL_MIN_VALID_TIME  1577836800L

static SMTPSession s_smtp;
static WiFiClientSecure s_smtp_ssl;
static WiFiClient s_smtp_plain;

/** ESP Mail 3.4.x: external client + app_wifi (esp-idf). Never WiFi.mode/begin here — panics. */
static void mail_net_status_cb(void)
{
    s_smtp.setNetworkStatus(app_wifi_is_connected());
}

static void mail_net_connection_cb(void)
{
    mail_net_status_cb();
}

static void mail_setup_smtp_client(bool use_starttls, const char * host)
{
    if(use_starttls) {
        s_smtp.setClient(&s_smtp_plain);
        Serial.printf("[mail] connect %s:587 STARTTLS\n", host ? host : "?");
    }
    else {
        s_smtp_ssl.setInsecure();
        s_smtp.setClient(&s_smtp_ssl);
        Serial.printf("[mail] connect %s:465 SSL\n", host ? host : "?");
    }
    s_smtp.networkConnectionRequestCallback(mail_net_connection_cb);
    s_smtp.networkStatusRequestCallback(mail_net_status_cb);
    s_smtp.setNetworkStatus(app_wifi_is_connected());
}

static void trim_smtp_password(char * pass, size_t pass_sz)
{
    if(!pass || pass_sz == 0) return;
    char * w = pass;
    for(const char * p = pass; *p && (size_t)(w - pass) < pass_sz - 1; p++) {
        if(*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            *w++ = *p;
    }
    *w = '\0';
}

static bool mail_wait_ntp(int gmt_offset_hours, int daylight_offset_sec)
{
    if(time(nullptr) > APP_MAIL_MIN_VALID_TIME)
        return true;

    configTime(gmt_offset_hours * 3600, daylight_offset_sec, "pool.ntp.org", "time.nist.gov");
    Serial.println("[mail] waiting for NTP...");
    for(int i = 0; i < 40; i++) {
        if(time(nullptr) > APP_MAIL_MIN_VALID_TIME) {
            Serial.printf("[mail] NTP ok epoch=%lu\n", (unsigned long)time(nullptr));
            return true;
        }
        delay(250);
        esp_task_wdt_reset();
    }
    Serial.println("[mail] NTP slow — using device clock");
    return time(nullptr) > APP_MAIL_MIN_VALID_TIME;
}

static void mail_copy_error(char * err, size_t err_sz, const char * prefix, SMTPSession & smtp)
{
    if(!err || err_sz == 0) return;
    const char * why = smtp.errorReason().c_str();
    if(!why || !why[0]) why = "unknown";
    snprintf(err, err_sz, "%s: %.48s", prefix, why);
}

static void csv_escape_field(const char * in, char * out, size_t out_sz)
{
    if(!out || out_sz == 0) return;
    if(!in) { out[0] = '\0'; return; }
    bool need_quote = false;
    for(const char * p = in; *p; p++) {
        if(*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            need_quote = true;
            break;
        }
    }
    if(!need_quote) {
        strncpy(out, in, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }
    size_t j = 0;
    if(j < out_sz - 1) out[j++] = '"';
    for(const char * p = in; *p && j < out_sz - 2; p++) {
        if(*p == '"' && j < out_sz - 2) out[j++] = '"';
        out[j++] = *p;
    }
    if(j < out_sz - 1) out[j++] = '"';
    out[j] = '\0';
}

#define MAIL_INV_MAX_ROWS  96

typedef struct {
    const app_sales_entry_t * entry;
    const app_sales_item_t * item;
    bool is_last_in_sale;
    bool is_last_for_salesman;
    int salesman_total_qty;
} mail_inv_row_t;

static int mail_cmp_rows(const void * a, const void * b)
{
    const mail_inv_row_t * ra = (const mail_inv_row_t *)a;
    const mail_inv_row_t * rb = (const mail_inv_row_t *)b;
    int n = strcmp(ra->entry->employee_name, rb->entry->employee_name);
    if(n != 0) return n;
    return strcmp(ra->entry->date, rb->entry->date);
}

/** Same row order / totals as promo review (ui_email.c). */
static int mail_build_inv_rows(mail_inv_row_t * rows, int max_rows)
{
    int count = 0;
    int pending = app_pending_email_count();
    for(int p = 0; p < pending && count < max_rows; p++) {
        const app_sales_entry_t * e = app_pending_email_entry(p);
        if(!e) continue;
        for(int i = 0; i < e->item_count && count < max_rows; i++) {
            const app_sales_item_t * it = &e->items[i];
            if(!it->barcode[0] || it->quantity <= 0 ||
               (it->price <= 0.0f && it->line_total <= 0.0f))
                continue;
            mail_inv_row_t * r = &rows[count++];
            r->entry = e;
            r->item = it;
            r->is_last_in_sale = false;
            r->is_last_for_salesman = false;
            r->salesman_total_qty = 0;
        }
    }
    if(count == 0) return 0;

    qsort(rows, (size_t)count, sizeof(mail_inv_row_t), mail_cmp_rows);

    for(int i = 0; i < count; i++) {
        bool last_sale = true;
        for(int j = i + 1; j < count; j++) {
            if(rows[j].entry == rows[i].entry) {
                last_sale = false;
                break;
            }
        }
        rows[i].is_last_in_sale = last_sale;
    }

    int qty_by_name[APP_MAX_EMPLOYEES];
    const char * names[APP_MAX_EMPLOYEES];
    int name_n = 0;
    memset(qty_by_name, 0, sizeof(qty_by_name));

    for(int i = 0; i < count; i++) {
        const char * name = rows[i].entry->employee_name;
        int idx = -1;
        for(int j = 0; j < name_n; j++) {
            if(strcmp(names[j], name) == 0) { idx = j; break; }
        }
        if(idx < 0 && name_n < APP_MAX_EMPLOYEES) {
            idx = name_n;
            names[name_n++] = name;
        }
        if(idx >= 0) qty_by_name[idx] += rows[i].item->quantity;
    }

    for(int i = 0; i < count; i++) {
        const char * name = rows[i].entry->employee_name;
        int idx = -1;
        for(int j = 0; j < name_n; j++) {
            if(strcmp(names[j], name) == 0) { idx = j; break; }
        }
        if(idx >= 0) rows[i].salesman_total_qty = qty_by_name[idx];
        bool last = true;
        for(int j = i + 1; j < count; j++) {
            if(strcmp(rows[j].entry->employee_name, name) == 0) {
                last = false;
                break;
            }
        }
        rows[i].is_last_for_salesman = last;
    }
    return count;
}

static bool build_invoice_csv(const app_settings_t * settings, char * out, size_t out_sz, size_t * out_len)
{
    (void)settings;
    if(!out || out_sz < 64) return false;

    mail_inv_row_t rows[MAIL_INV_MAX_ROWS];
    int n = mail_build_inv_rows(rows, MAIL_INV_MAX_ROWS);
    if(n <= 0) return false;

    static const char hdr[] =
        "Date,Salesman,Barcode,Price,Qty,LineTotal,TotQty,SysTotal,Manual\r\n";
    int pos = snprintf(out, out_sz, "%s", hdr);
    if(pos < 0 || (size_t)pos >= out_sz) return false;

    char esc_name[APP_NAME_LEN * 2 + 4];
    char esc_bc[APP_BARCODE_LEN * 2 + 4];

    for(int i = 0; i < n; i++) {
        const mail_inv_row_t * r = &rows[i];
        const app_sales_entry_t * e = r->entry;
        const app_sales_item_t * it = r->item;

        csv_escape_field(e->employee_name, esc_name, sizeof(esc_name));
        csv_escape_field(it->barcode, esc_bc, sizeof(esc_bc));

        char tot_qty[16] = "";
        char sys_total[16] = "";
        char manual[16] = "";
        if(r->is_last_for_salesman)
            snprintf(tot_qty, sizeof(tot_qty), "%d", r->salesman_total_qty);
        if(r->is_last_in_sale) {
            snprintf(sys_total, sizeof(sys_total), "%.2f", e->grand_total);
            float man = e->has_manual_total ? e->manual_total : 0.0f;
            snprintf(manual, sizeof(manual), "%.2f", man);
        }

        int w = snprintf(out + pos, out_sz - (size_t)pos,
            "%s,%s,%s,%.2f,%d,%.2f,%s,%s,%s\r\n",
            e->date, esc_name, esc_bc,
            it->price, it->quantity, it->line_total,
            tot_qty, sys_total, manual);
        if(w < 0 || (size_t)w >= out_sz - (size_t)pos) return false;
        pos += w;
    }

    if(out_len) *out_len = (size_t)pos;
    return true;
}

static bool mail_session_config(Session_Config * cfg, const app_settings_t * settings,
                                uint16_t port, const char * smtp_host)
{
    if(!cfg || !settings || !smtp_host || !smtp_host[0]) return false;
    memset(cfg, 0, sizeof(*cfg));
    cfg->server.host_name = smtp_host;
    cfg->server.port = port;
    cfg->login.email = settings->sender_email;
    cfg->login.password = settings->smtp_app_password;
    cfg->login.user_domain = "gmail.com";
    cfg->time.ntp_server = "pool.ntp.org,time.nist.gov";
    cfg->time.gmt_offset = 8;
    cfg->time.day_light_offset = 0;
    return true;
}

static bool smtp_try_connect(Session_Config * cfg, bool use_starttls, char * err, size_t err_sz)
{
    if(!app_wifi_is_connected()) {
        snprintf(err, err_sz, "WiFi not connected");
        return false;
    }

    s_smtp.closeSession();
    s_smtp_ssl.stop();
    s_smtp_plain.stop();

    mail_setup_smtp_client(use_starttls, cfg->server.host_name.c_str());

    s_smtp.setSSLBufferSize(4096, 1024);
    s_smtp.debug(0);
    MailClient.networkReconnect(true);

    if(!s_smtp.connect(cfg)) {
        mail_copy_error(err, err_sz, use_starttls ? "SMTP587" : "SMTP465", s_smtp);
        Serial.printf("[mail] %s\n", s_smtp.errorReason().c_str());
        return false;
    }
    return true;
}

static bool esp_mail_send_smtp(const app_settings_t * settings, char * err, size_t err_sz)
{
    if(!app_wifi_is_connected()) {
        printf("[mail] ERROR: WiFi not connected\n");
        snprintf(err, err_sz, "WiFi not connected");
        return false;
    }
    printf("[mail] WiFi OK sender=%s to=%s\n",
           settings->sender_email, settings->recipient_email);
    if(!settings->sender_email[0]) {
        snprintf(err, err_sz, "Set sender email in Settings");
        return false;
    }
    if(!settings->smtp_app_password[0]) {
        snprintf(err, err_sz, "Set SMTP app password in Settings");
        return false;
    }
    if(!settings->recipient_email[0]) {
        snprintf(err, err_sz, "Set recipient email in Settings");
        return false;
    }

    char smtp_pass[APP_SMTP_PASS_LEN];
    strncpy(smtp_pass, settings->smtp_app_password, sizeof(smtp_pass) - 1);
    smtp_pass[sizeof(smtp_pass) - 1] = '\0';
    trim_smtp_password(smtp_pass, sizeof(smtp_pass));

    if(app_pending_email_count() == 0) {
        snprintf(err, err_sz, "Nothing ready to email");
        return false;
    }

    char * csv = (char *)heap_caps_malloc(APP_MAIL_CSV_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(!csv) csv = (char *)malloc(APP_MAIL_CSV_MAX);
    if(!csv) {
        snprintf(err, err_sz, "Out of memory for CSV");
        return false;
    }

    size_t csv_len = 0;
    if(!build_invoice_csv(settings, csv, APP_MAIL_CSV_MAX, &csv_len)) {
        heap_caps_free(csv);
        snprintf(err, err_sz, "No invoice rows to send");
        return false;
    }

    Serial.printf("[mail] CSV size=%u bytes\n", (unsigned)csv_len);

    char smtp_host_ip[16];
    if(!app_wifi_resolve_ipv4(APP_SMTP_HOST, smtp_host_ip, sizeof(smtp_host_ip))) {
        heap_caps_free(csv);
        snprintf(err, err_sz, "DNS failed — check hotspot internet");
        return false;
    }

    Session_Config cfg;
    app_settings_t mail_cfg = *settings;
    strncpy(mail_cfg.smtp_app_password, smtp_pass, sizeof(mail_cfg.smtp_app_password) - 1);
    mail_cfg.smtp_app_password[sizeof(mail_cfg.smtp_app_password) - 1] = '\0';

    if(!mail_wait_ntp(8, 0)) {
        heap_caps_free(csv);
        snprintf(err, err_sz, "Clock not set — connect WiFi first");
        return false;
    }
    esp_task_wdt_reset();

    bool connected = false;
    if(mail_session_config(&cfg, &mail_cfg, 587, smtp_host_ip)) {
        connected = smtp_try_connect(&cfg, true, err, err_sz);
    }
    if(!connected && mail_session_config(&cfg, &mail_cfg, 465, smtp_host_ip)) {
        char err2[128];
        connected = smtp_try_connect(&cfg, false, err2, sizeof(err2));
        if(!connected && err && err_sz > 0)
            snprintf(err, err_sz, "%.80s", err2);
    }
    if(!connected) {
        heap_caps_free(csv);
        return false;
    }
    s_smtp.debug(0);

    SMTP_Message message;
    message.sender.name = settings->promo_head_name[0] ? settings->promo_head_name : "Sales Entry";
    message.sender.email = settings->sender_email;
    message.subject = "Daily Sales Invoice (CSV)";
    message.addRecipient("Invoice", settings->recipient_email);
    message.text.content = "Daily sales invoice attached as CSV.";
    message.text.charSet = "utf-8";
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

    SMTP_Attachment att;
    att.descr.filename = "daily_sales.csv";
    att.descr.mime = "text/csv";
    att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
    att.blob.data = (uint8_t *)csv;
    att.blob.size = csv_len;
    att.descr.content_encoding = Content_Transfer_Encoding::enc_7bit;
    message.addAttachment(att);

    esp_task_wdt_reset();
    bool sent = MailClient.sendMail(&s_smtp, &message);
    esp_task_wdt_reset();
    s_smtp.sendingResult.clear();

    if(!sent) {
        snprintf(err, err_sz, "SMTP send failed");
        Serial.printf("[mail] %s\n", s_smtp.errorReason().c_str());
        heap_caps_free(csv);
        return false;
    }

    Serial.println("[mail] sent OK");
    heap_caps_free(csv);
    return true;
}

#endif /* ESP */

#if !defined(ESP_PLATFORM) && defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#define MAIL_API_HOST  "127.0.0.1"
#define MAIL_API_PORT  3001

static void json_escape(const char * in, char * out, size_t out_sz)
{
    size_t j = 0;
    if(!in) { out[0] = '\0'; return; }
    for(size_t i = 0; in[i] && j + 2 < out_sz; i++) {
        char c = in[i];
        if(c == '"' || c == '\\') {
            if(j + 2 >= out_sz) break;
            out[j++] = '\\';
        }
        out[j++] = c;
    }
    out[j] = '\0';
}

static bool http_post_invoice(const char * body, char * err, size_t err_sz)
{
    WSADATA wsa;
    SOCKET sock = INVALID_SOCKET;
    bool ok = false;

    if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        snprintf(err, err_sz, "WSAStartup failed");
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock == INVALID_SOCKET) {
        snprintf(err, err_sz, "socket failed");
        goto done;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MAIL_API_PORT);
    inet_pton(AF_INET, MAIL_API_HOST, &addr.sin_addr);

    if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        snprintf(err, err_sz, "Email server offline. Run: npm run server");
        goto done;
    }

    char req[8192];
    int body_len = (int)strlen(body);
    int hdr_len = snprintf(req, sizeof(req),
        "POST /api/send-invoice HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        MAIL_API_HOST, MAIL_API_PORT, body_len);
    if(hdr_len <= 0 || hdr_len + body_len >= (int)sizeof(req) - 1) {
        snprintf(err, err_sz, "Request too large");
        goto done;
    }
    memcpy(req + hdr_len, body, (size_t)body_len + 1);
    if(send(sock, req, hdr_len + body_len, 0) <= 0) {
        snprintf(err, err_sz, "send failed");
        goto done;
    }

    char resp[1024];
    int total = 0;
    while(total < (int)sizeof(resp) - 1) {
        int n = recv(sock, resp + total, (int)sizeof(resp) - 1 - total, 0);
        if(n <= 0) break;
        total += n;
    }
    resp[total] = '\0';
    if(strstr(resp, "\"ok\":true") || strstr(resp, "\"ok\": true"))
        ok = true;
    else
        snprintf(err, err_sz, "SMTP/API error — check server/.env");

done:
    if(sock != INVALID_SOCKET) closesocket(sock);
    WSACleanup();
    return ok;
}

static bool build_json_body(const app_settings_t * settings, char * out, size_t out_sz)
{
    char promo[APP_NAME_LEN * 2];
    char to[128];
    char from[128];
    json_escape(settings->promo_head_name, promo, sizeof(promo));
    json_escape(settings->recipient_email, to, sizeof(to));
    json_escape(settings->sender_email, from, sizeof(from));

    int n = snprintf(out, out_sz,
        "{\"to\":\"%s\",\"from\":\"%s\",\"promoHeadName\":\"%s\",\"rows\":[",
        to, from, promo);
    if(n < 0 || (size_t)n >= out_sz) return false;

    size_t pos = (size_t)n;
    int pending = app_pending_email_count();
    for(int p = 0; p < pending; p++) {
        const app_sales_entry_t * e = app_pending_email_entry(p);
        if(!e) continue;
        for(int i = 0; i < e->item_count; i++) {
            const app_sales_item_t * it = &e->items[i];
            char bc[APP_BARCODE_LEN * 2];
            char en[APP_NAME_LEN * 2];
            json_escape(it->barcode, bc, sizeof(bc));
            json_escape(e->employee_name, en, sizeof(en));
            int w = snprintf(out + pos, out_sz - pos,
                "%s{\"date\":\"%s\",\"salesman\":\"%s\",\"barcode\":\"%s\","
                "\"price\":%.2f,\"qty\":%d,\"totQty\":\"\",\"sysTotal\":\"\",\"manual\":\"\"}",
                (pos > (size_t)n) ? "," : "",
                e->date, en, bc, it->price, it->quantity);
            if(w < 0 || (size_t)w >= out_sz - pos) return false;
            pos += (size_t)w;
        }
    }

    if(pos + 2 >= out_sz) return false;
    out[pos++] = ']';
    out[pos++] = '}';
    out[pos] = '\0';
    return true;
}

#endif /* WIN32 */

extern "C" bool app_mail_send_invoice(const app_settings_t * settings, char * err, size_t err_sz)
{
    if(err && err_sz > 0) err[0] = '\0';
    if(!settings) {
        snprintf(err, err_sz, "No settings");
        return false;
    }
    if(!settings->recipient_email[0]) {
        snprintf(err, err_sz, "Set recipient email in Settings");
        return false;
    }

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
    return esp_mail_send_smtp(settings, err, err_sz);
#elif defined(_WIN32)
    static char body[16384];
    if(!build_json_body(settings, body, sizeof(body))) {
        snprintf(err, err_sz, "Invoice too large");
        return false;
    }
    return http_post_invoice(body, err, err_sz);
#else
    snprintf(err, err_sz, "Email not supported on this platform");
    return false;
#endif
}
