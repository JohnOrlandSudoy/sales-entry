#include "app_wifi.h"
#include "app_data.h"
#include "app_storage.h"
#include "app_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/netdb.h"
#include "lwip/ip_addr.h"
#include "arpa/inet.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_CONNECT_RETRY_MAX 4
static EventGroupHandle_t s_wifi_eg;
static bool s_esp_inited;
static bool s_wifi_handlers;
static int s_connect_retries_left;
static bool s_waiting_connect;
static bool s_wifi_init_attempted;
static bool s_wifi_init_failed;

static bool wifi_err_ok(esp_err_t e)
{
    return (e == ESP_OK || e == ESP_ERR_INVALID_STATE);
}

/** Driver already up (do not call esp_wifi_init again — causes deinit err 0x3001). */
static bool wifi_stack_usable(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    return esp_wifi_get_mode(&mode) == ESP_OK;
}

static bool wifi_finish_sta_start(void)
{
    esp_err_t e = esp_wifi_set_mode(WIFI_MODE_STA);
    if(e != ESP_OK && e != ESP_ERR_WIFI_NOT_INIT)
        return false;
    esp_wifi_set_ps(WIFI_PS_NONE);
    wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    esp_wifi_set_country(&country);
    e = esp_wifi_start();
    return (e == ESP_OK || e == ESP_ERR_INVALID_STATE);
}
#endif

static bool s_connected;
static bool s_auto_reconnect;
static char s_connected_ssid[APP_WIFI_SSID_LEN];

static void copy_ssid(char * dst, size_t dst_sz, const char * src)
{
    if(!dst || dst_sz == 0) return;
    if(!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

void app_wifi_get_saved(char * ssid, size_t ssid_sz, char * pass, size_t pass_sz)
{
    app_settings_t * st = app_settings();
    if(ssid && ssid_sz > 0) copy_ssid(ssid, ssid_sz, st->wifi_ssid);
    if(pass && pass_sz > 0) {
        strncpy(pass, st->wifi_password, pass_sz - 1);
        pass[pass_sz - 1] = '\0';
    }
}

bool app_wifi_save_credentials(const char * ssid, const char * pass)
{
    /* Update live settings only — do NOT rewrite sales.cfg (app_apply_settings). */
    app_settings_t * st = app_settings();
    copy_ssid(st->wifi_ssid, sizeof(st->wifi_ssid), ssid);
    if(pass) {
        strncpy(st->wifi_password, pass, sizeof(st->wifi_password) - 1);
        st->wifi_password[sizeof(st->wifi_password) - 1] = '\0';
    }
    else {
        st->wifi_password[0] = '\0';
    }
    return app_storage_save_settings(st);
}

#if defined(ESP_PLATFORM)

/** Phone hotspots sometimes leave DNS empty — lwIP getaddrinfo needs this. */
static void wifi_ensure_dns_servers(void)
{
    esp_netif_t * netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(!netif)
        return;

    esp_netif_dns_info_t cur = {};
    if(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &cur) == ESP_OK &&
       cur.ip.type == ESP_IPADDR_TYPE_V4 && cur.ip.u_addr.ip4.addr != 0) {
        return;
    }

    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
    dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
    esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns);
    printf("[wifi] DNS set 8.8.8.8 / 1.1.1.1\n");
}

static void wifi_event_handler(void * arg, esp_event_base_t base, int32_t id, void * data)
{
    (void)arg;
    if(base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        /* Do not auto-connect here — it breaks scanning before user picks an AP. */
        printf("[wifi] STA started\n");
    }
    else if(base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t * ev = (wifi_event_sta_disconnected_t *)data;
        s_connected = false;
        s_connected_ssid[0] = '\0';
        printf("[wifi] disconnected reason=%d\n", ev ? (int)ev->reason : -1);
        if(s_waiting_connect) {
            if(s_connect_retries_left > 0) {
                s_connect_retries_left--;
                esp_wifi_connect();
            } else {
                s_waiting_connect = false;
                if(s_wifi_eg) xEventGroupSetBits(s_wifi_eg, WIFI_FAIL_BIT);
            }
        } else if(s_auto_reconnect) {
            char ssid[APP_WIFI_SSID_LEN];
            app_wifi_get_saved(ssid, sizeof(ssid), NULL, 0);
            if(ssid[0])
                esp_wifi_connect();
        }
    }
    else if(base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        s_waiting_connect = false;
        s_connect_retries_left = WIFI_CONNECT_RETRY_MAX;
        wifi_ap_record_t ap;
        if(esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
            copy_ssid(s_connected_ssid, sizeof(s_connected_ssid), (const char *)ap.ssid);
        wifi_ensure_dns_servers();
        app_time_on_wifi_connected();
        if(s_wifi_eg) xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_register_handlers(void)
{
    if(s_wifi_handlers)
        return true;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL, NULL);
    s_wifi_handlers = true;
    return true;
}

static bool nvs_init_safe(void)
{
    esp_err_t e = nvs_flash_init();
    if(e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        esp_err_t er = nvs_flash_erase();
        if(er != ESP_OK) return false;
        e = nvs_flash_init();
    }
    return (e == ESP_OK || e == ESP_ERR_INVALID_STATE);
}

static bool wifi_mark_ready_if_running(void)
{
    if(!wifi_stack_usable())
        return false;
    wifi_register_handlers();
    if(!wifi_finish_sta_start())
        return false;
    s_esp_inited = true;
    printf("[wifi] ready (existing driver)\n");
    return true;
}

static bool esp_wifi_init_once(void)
{
    if(s_esp_inited) return true;
    if(s_wifi_init_failed) return false;
    if(s_wifi_init_attempted)
        return wifi_mark_ready_if_running();

    if(wifi_mark_ready_if_running())
        return true;

    if(!nvs_init_safe()) {
        printf("[wifi] nvs init failed\n");
        return false;
    }
    if(!s_wifi_eg) {
        s_wifi_eg = xEventGroupCreate();
        if(!s_wifi_eg) {
            printf("[wifi] event group failed\n");
            return false;
        }
    }

    esp_err_t e = esp_netif_init();
    if(!wifi_err_ok(e)) {
        printf("[wifi] netif init err=%d\n", (int)e);
        return false;
    }
    e = esp_event_loop_create_default();
    if(!wifi_err_ok(e)) {
        printf("[wifi] event loop err=%d\n", (int)e);
        return false;
    }

    if(!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"))
        esp_netif_create_default_wifi_sta();

    /* Reuse driver if already started (avoids esp_wifi_init deinit loop / err 257). */
    if(wifi_stack_usable()) {
        wifi_register_handlers();
        if(wifi_finish_sta_start()) {
            s_esp_inited = true;
            printf("[wifi] reuse existing driver\n");
            return true;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_rx_buf_num = 4;
    cfg.dynamic_rx_buf_num = 16;
    cfg.rx_ba_win = 4;
    s_wifi_init_attempted = true;

    e = esp_wifi_init(&cfg);
    if(e == ESP_OK) {
        /* fresh init */
    }
    else if(e == ESP_ERR_WIFI_INIT_STATE || e == ESP_ERR_INVALID_STATE) {
        if(wifi_mark_ready_if_running())
            return true;
        printf("[wifi] init blocked err=%d (reboot once)\n", (int)e);
        s_wifi_init_failed = true;
        return false;
    }
    else {
        printf("[wifi] esp_wifi_init err=%d (reboot once)\n", (int)e);
        s_wifi_init_failed = true;
        return false;
    }

    wifi_register_handlers();
    if(!wifi_finish_sta_start()) {
        printf("[wifi] start failed\n");
        s_wifi_init_failed = true;
        return false;
    }
    s_esp_inited = true;
    printf("[wifi] init OK heap=%u\n", (unsigned)esp_get_free_heap_size());
    return true;
}

void app_wifi_boot_init(void)
{
#if defined(ESP_PLATFORM)
    (void)esp_wifi_init_once();
#endif
}

void app_wifi_prepare_ui(void)
{
#if defined(ESP_PLATFORM)
    if(!s_esp_inited && s_wifi_init_failed)
        printf("[wifi] unavailable — power-cycle device to retry\n");
#endif
}

static void wifi_prepare_scan(void)
{
    s_waiting_connect = false;
    esp_wifi_disconnect();
    esp_wifi_scan_stop();
    vTaskDelay(pdMS_TO_TICKS(250));
}
#endif

int app_wifi_scan(app_wifi_network_t * out, int max)
{
    if(!out || max <= 0) return 0;

#if defined(ESP_PLATFORM)
    if(!s_esp_inited) {
        printf("[wifi] scan skipped (not initialized)\n");
        return 0;
    }

    wifi_prepare_scan();

    uint16_t count = 0;
    esp_err_t e;

    wifi_scan_config_t sc;
    memset(&sc, 0, sizeof(sc));
    sc.show_hidden = true;
    sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    sc.scan_time.active.min = 120;
    sc.scan_time.active.max = 400;

    e = esp_wifi_scan_start(&sc, true);
    if(e != ESP_OK)
        printf("[wifi] active scan_start err=%d\n", (int)e);
    else
        e = esp_wifi_scan_get_ap_num(&count);

    if(e != ESP_OK || count == 0) {
        wifi_prepare_scan();
        sc.show_hidden = true;
        sc.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        sc.scan_time.passive = 400;
        e = esp_wifi_scan_start(&sc, true);
        if(e == ESP_OK)
            e = esp_wifi_scan_get_ap_num(&count);
        if(e != ESP_OK)
            printf("[wifi] passive scan err=%d\n", (int)e);
    }

    if(e != ESP_OK) return 0;
    if(count == 0) {
        printf("[wifi] scan: 0 APs\n");
        return 0;
    }

    uint16_t fetch = count;
    if(fetch > 32) fetch = 32;
    wifi_ap_record_t * recs = calloc(fetch, sizeof(wifi_ap_record_t));
    if(!recs) return 0;
    e = esp_wifi_scan_get_ap_records(&fetch, recs);
    if(e != ESP_OK) {
        printf("[wifi] scan_get_ap_records failed=%d\n", (int)e);
        free(recs);
        return 0;
    }

    int n = 0;
    for(uint16_t i = 0; i < fetch && n < max; i++) {
        if(recs[i].ssid[0] == '\0') continue;
        bool dup = false;
        for(int j = 0; j < n; j++) {
            if(strcmp(out[j].ssid, (const char *)recs[i].ssid) == 0) {
                dup = true;
                break;
            }
        }
        if(dup) continue;
        copy_ssid(out[n].ssid, sizeof(out[n].ssid), (const char *)recs[i].ssid);
        out[n].rssi = recs[i].rssi;
        out[n].secure = (recs[i].authmode != WIFI_AUTH_OPEN);
        n++;
    }
    free(recs);
    printf("[wifi] scan found=%d\n", n);
    return n;
#else
    static const char * demo[] = {
        "AndroidAP",
        "iPhone",
        "Redmi_Hotspot",
        "Shop_WiFi_2G",
        "PLDT_HomeFi",
    };
    int n = 0;
    char saved[APP_WIFI_SSID_LEN];
    app_wifi_get_saved(saved, sizeof(saved), NULL, 0);
    if(saved[0]) {
        copy_ssid(out[n].ssid, sizeof(out[n].ssid), saved);
        out[n].rssi = -42;
        out[n].secure = true;
        n++;
    }
    for(size_t i = 0; i < sizeof(demo) / sizeof(demo[0]) && n < max; i++) {
        bool dup = false;
        for(int j = 0; j < n; j++) {
            if(strcmp(out[j].ssid, demo[i]) == 0) { dup = true; break; }
        }
        if(dup) continue;
        copy_ssid(out[n].ssid, sizeof(out[n].ssid), demo[i]);
        out[n].rssi = (int8_t)(-35 - (int)i * 8);
        out[n].secure = true;
        n++;
    }
    return n;
#endif
}

bool app_wifi_connect(const char * ssid, const char * pass, char * err, size_t err_sz)
{
    if(err && err_sz > 0) err[0] = '\0';
    if(!ssid || !ssid[0]) {
        snprintf(err, err_sz, "Pick a WiFi name");
        return false;
    }

#if defined(ESP_PLATFORM)
    if(!s_esp_inited) {
        snprintf(err, err_sz, s_wifi_init_failed ? "WiFi failed — reboot device" : "WiFi not ready");
        return false;
    }

    wifi_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    if(pass) strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);

    s_waiting_connect = true;
    s_connect_retries_left = WIFI_CONNECT_RETRY_MAX;
    xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    /* Ensure stale state from previous AP does not linger. */
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_connect());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(20000));

    if(bits & WIFI_CONNECTED_BIT) {
        copy_ssid(s_connected_ssid, sizeof(s_connected_ssid), ssid);
        s_connected = true;
        s_auto_reconnect = true;
        if(!app_wifi_save_credentials(ssid, pass ? pass : ""))
            printf("[wifi] warning: WiFi OK but save failed\n");
        printf("[wifi] connected ssid=%s\n", ssid);
        return true;
    }
    s_waiting_connect = false;
    printf("[wifi] connect failed ssid=%s\n", ssid);
    snprintf(err, err_sz, "Could not connect (check pass/2.4G)");
    return false;
#else
    (void)pass;
    if(!app_wifi_save_credentials(ssid, pass ? pass : "")) {
        snprintf(err, err_sz, "Save failed");
        return false;
    }
    copy_ssid(s_connected_ssid, sizeof(s_connected_ssid), ssid);
    s_connected = true;
    return true;
#endif
}

bool app_wifi_forget(char * err, size_t err_sz)
{
    if(err && err_sz > 0) err[0] = '\0';
#if defined(ESP_PLATFORM)
    if(s_esp_inited) {
        esp_wifi_disconnect();
    }
    s_waiting_connect = false;
    s_connect_retries_left = WIFI_CONNECT_RETRY_MAX;
#endif
    s_connected = false;
    s_auto_reconnect = false;
    s_connected_ssid[0] = '\0';
    if(!app_wifi_save_credentials("", "")) {
        if(err && err_sz > 0) snprintf(err, err_sz, "Failed to clear saved WiFi");
        return false;
    }
    return true;
}

bool app_wifi_resolve_ipv4(const char * host, char * ip_out, size_t ip_sz)
{
    if(!host || !ip_out || ip_sz < 8)
        return false;
    ip_out[0] = '\0';
#if defined(ESP_PLATFORM)
    if(!app_wifi_is_connected())
        return false;

    wifi_ensure_dns_servers();

    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo * res = NULL;
    int err = getaddrinfo(host, NULL, &hints, &res);
    if(err != 0 || !res || !res->ai_addr) {
        printf("[wifi] DNS fail host=%s err=%d\n", host, err);
        if(res)
            freeaddrinfo(res);
        return false;
    }

    const struct sockaddr_in * sa = (const struct sockaddr_in *)res->ai_addr;
    const char * ip = inet_ntoa(sa->sin_addr);
    if(!ip || !ip[0]) {
        freeaddrinfo(res);
        return false;
    }
    strncpy(ip_out, ip, ip_sz - 1);
    ip_out[ip_sz - 1] = '\0';
    freeaddrinfo(res);
    printf("[wifi] DNS %s -> %s\n", host, ip_out);
    return true;
#else
    (void)host;
    strncpy(ip_out, "127.0.0.1", ip_sz - 1);
    ip_out[ip_sz - 1] = '\0';
    return true;
#endif
}

bool app_wifi_is_connected(void)
{
#if defined(ESP_PLATFORM)
    if(!s_esp_inited) return false;
    wifi_ap_record_t ap;
    if(esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        s_connected = true;
        if(!s_connected_ssid[0])
            copy_ssid(s_connected_ssid, sizeof(s_connected_ssid), (const char *)ap.ssid);
        return true;
    }
    if(s_connected) return true;
    return false;
#else
    if(s_connected) return true;
    char ssid[APP_WIFI_SSID_LEN];
    app_wifi_get_saved(ssid, sizeof(ssid), NULL, 0);
    return ssid[0] != '\0';
#endif
}

void app_wifi_status_line(char * buf, size_t buf_sz)
{
    if(!buf || buf_sz == 0) return;
#if defined(ESP_PLATFORM)
    if(!s_esp_inited) {
        if(s_wifi_init_failed)
            snprintf(buf, buf_sz, "WiFi failed — reboot device");
        else {
            char ssid[APP_WIFI_SSID_LEN];
            app_wifi_get_saved(ssid, sizeof(ssid), NULL, 0);
            if(ssid[0])
                snprintf(buf, buf_sz, "Saved: %s (tap WiFi)", ssid);
            else
                snprintf(buf, buf_sz, "Tap WiFi to connect");
        }
        return;
    }
#endif
    if(app_wifi_is_connected()) {
        const char * ssid = s_connected_ssid[0] ? s_connected_ssid : app_settings()->wifi_ssid;
        snprintf(buf, buf_sz, "Connected: %s", ssid[0] ? ssid : "?");
    }
    else {
        char ssid[APP_WIFI_SSID_LEN];
        app_wifi_get_saved(ssid, sizeof(ssid), NULL, 0);
        if(ssid[0])
            snprintf(buf, buf_sz, "Saved: %s (offline)", ssid);
        else
            snprintf(buf, buf_sz, "Not connected");
    }
}
