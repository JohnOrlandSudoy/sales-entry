#include "app_sd_mount.h"
#include "board_config.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

static bool s_mounted;
static bool s_mount_gave_up;
static uint32_t s_last_remount_ms;
static SPIClass s_sd_spi(HSPI);
static char s_last_err[80];
static char s_data_dir[48] = "/sdcard";

static const uint32_t SD_SPEEDS_HZ[] = {200000, 400000, 1000000, 4000000, 8000000};

const char * app_sd_last_error(void)
{
    return s_last_err;
}

static void sd_log_err(const char * msg)
{
    strncpy(s_last_err, msg ? msg : "", sizeof(s_last_err) - 1);
    s_last_err[sizeof(s_last_err) - 1] = '\0';
    Serial.printf("[SD] %s\n", s_last_err);
}

static void sd_spi_prepare(void)
{
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_SD_MISO, INPUT_PULLUP);
    pinMode(BOARD_SD_MOSI, OUTPUT);
    pinMode(BOARD_SD_SCK, OUTPUT);
    s_sd_spi.end();
    delay(20);
    s_sd_spi.begin(BOARD_SD_SCK, BOARD_SD_MISO, BOARD_SD_MOSI, BOARD_SD_CS);
    delay(80);
}

static void sd_spi_release(void)
{
    SD.end();
    s_sd_spi.end();
    digitalWrite(BOARD_SD_CS, HIGH);
    delay(20);
}

static bool sd_write_probe(void)
{
    FILE * f = fopen("/sdcard/.write_probe", "w");
    if(!f) {
        sd_log_err("write test failed (no card or read-only)");
        return false;
    }
    if(fprintf(f, "ok\n") < 0) {
        fclose(f);
        sd_log_err("write test failed (fprintf)");
        return false;
    }
    fflush(f);
    fclose(f);
    remove("/sdcard/.write_probe");
    return true;
}

static bool sd_probe_write_in_dir(const char * dir)
{
    if(dir && strcmp(dir, "/sdcard") == 0)
        return sd_write_probe();

    char path[72];
    snprintf(path, sizeof(path), "%s/wprobe.txt", dir ? dir : "/sdcard");
    FILE * f = fopen(path, "w");
    if(!f) {
        Serial.printf("[SD] write test fail %s errno=%d\n", path, errno);
        return false;
    }
    fprintf(f, "ok\n");
    fclose(f);
    remove(path);
    return true;
}

static bool sd_try_mkdir_dailysales(void)
{
    struct stat st;
    if(stat("/sdcard/DailySales", &st) == 0 && S_ISDIR(st.st_mode))
        return true;
    if(mkdir("/sdcard/DailySales", 0755) == 0)
        return true;
    if(errno == EEXIST)
        return true;
    if(!SD.exists("/DailySales"))
        SD.mkdir("/DailySales");
    return (stat("/sdcard/DailySales", &st) == 0 && S_ISDIR(st.st_mode));
}

bool app_sd_ensure_data_dir(void)
{
    if(!s_mounted)
        return false;

    if(sd_try_mkdir_dailysales() && sd_probe_write_in_dir("/sdcard/DailySales")) {
        strncpy(s_data_dir, "/sdcard/DailySales", sizeof(s_data_dir) - 1);
        s_data_dir[sizeof(s_data_dir) - 1] = '\0';
        Serial.println("[SD] data dir: /sdcard/DailySales");
        return true;
    }

    Serial.printf("[SD] DailySales unavailable (errno=%d) — using /sdcard root\n", errno);
    if(sd_probe_write_in_dir("/sdcard")) {
        strncpy(s_data_dir, "/sdcard", sizeof(s_data_dir) - 1);
        s_data_dir[sizeof(s_data_dir) - 1] = '\0';
        return true;
    }
    Serial.printf("[SD] /sdcard not writable errno=%d\n", errno);
    return false;
}

const char * app_sd_data_dir(void)
{
    return s_data_dir;
}

static bool sd_try_mount_hz(uint32_t hz)
{
    sd_spi_prepare();
    if(!SD.begin(BOARD_SD_CS, s_sd_spi, hz, "/sdcard")) {
        sd_spi_release();
        return false;
    }
    if(SD.cardType() == CARD_NONE) {
        sd_spi_release();
        return false;
    }
    return true;
}

bool app_sd_mount(void)
{
    if(s_mounted && sd_write_probe() && app_sd_ensure_data_dir()) {
        return true;
    }

    if(s_mounted) {
        sd_spi_release();
        s_mounted = false;
    }

    sd_log_err("mounting TF card (HSPI)...");

    for(size_t i = 0; i < sizeof(SD_SPEEDS_HZ) / sizeof(SD_SPEEDS_HZ[0]); i++) {
        uint32_t hz = SD_SPEEDS_HZ[i];
        Serial.printf("[SD] try SPI %lu Hz (CS=%d SCK=%d MOSI=%d MISO=%d)\n",
                      (unsigned long)hz, BOARD_SD_CS, BOARD_SD_SCK, BOARD_SD_MOSI, BOARD_SD_MISO);
        if(!sd_try_mount_hz(hz)) {
            continue;
        }
        if(!sd_write_probe()) {
            sd_spi_release();
            continue;
        }
        if(!app_sd_ensure_data_dir()) {
            Serial.println("[SD] WARN: mounted but data folder not writable");
        }

        s_mounted = true;
        s_mount_gave_up = false;
        uint64_t mb = SD.cardSize() / (1024ULL * 1024ULL);
        Serial.printf("[SD] mounted type=%u size=%lluMB @%luHz\n",
                      (unsigned)SD.cardType(), (unsigned long long)mb, (unsigned long)hz);
        sd_log_err("OK");
        return true;
    }

    s_mount_gave_up = true;
    sd_log_err("mount failed — card not responding on SPI");
    Serial.println("[SD] cmd 0x00 = no card / bad contact / wrong slot");
    Serial.println("[SD] Check: TF slot on BACK, FAT32, push card until click");
    return false;
}

const char * app_sd_user_message(void)
{
    if(s_mounted)
        return "SD card OK";
    return "SD not detected — settings in device memory";
}

bool app_sd_mount_retry_user(void)
{
    s_mount_gave_up = false;
    s_last_remount_ms = 0;
    if(s_mounted) {
        sd_spi_release();
        s_mounted = false;
    }
    delay(100);
    return app_sd_mount();
}

bool app_sd_remount(void)
{
    if(s_mounted) {
        return true;
    }
    uint32_t now = millis();
    if(s_mount_gave_up && (now - s_last_remount_ms) < 60000) {
        Serial.println("[SD] skip remount (failed recently — using NVS)");
        return false;
    }
    s_last_remount_ms = now;

    if(s_mounted) {
        sd_spi_release();
        s_mounted = false;
    }
    delay(100);
    return app_sd_mount();
}

bool app_sd_is_mounted(void)
{
    if(!s_mounted) return false;
    if(SD.cardType() == CARD_NONE) {
        s_mounted = false;
        return false;
    }
    return sd_write_probe();
}
