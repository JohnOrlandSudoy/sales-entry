"""Patch GFX for PlatformIO + disable unused databus drivers."""
Import("env")
from pathlib import Path

DISABLE = (
    "Arduino_ESP32LCD8.cpp",
    "Arduino_ESP32LCD16.cpp",
    "Arduino_ESP32RGBPanel.cpp",
    "Arduino_ESP32DSIPanel.cpp",
)

PATCHES = (
    ('#include "esp32-hal-periman.h"\n', '/* PIO: periman */\n'),
    ('#include "esp_private/periph_ctrl.h"\n', '/* PIO: periph_ctrl */\n'),
)

# ESP Mail Client defaults to LittleFS on ESP32 2.x; core build may lack LittleFS.h
MAIL_FS_PATCH = (
    "#include <LittleFS.h>\n#define ESP_MAIL_DEFAULT_FLASH_FS LittleFS",
    "#include <SPIFFS.h>\n#define ESP_MAIL_DEFAULT_FLASH_FS SPIFFS",
)

def patch_esp_mail_fs(libdeps: Path) -> None:
    for fs_h in libdeps.rglob("ESP_Mail_FS.h"):
        text = fs_h.read_text(encoding="utf-8")
        orig = text
        for old, new in (MAIL_FS_PATCH,):
            text = text.replace(old, new)
        if "ESP_MAIL_DISABLE_ONBOARD_WIFI" not in text:
            marker = "#pragma once"
            if marker in text:
                text = text.replace(
                    marker,
                    marker + "\n\n#ifndef ESP_MAIL_DISABLE_ONBOARD_WIFI\n"
                    "#define ESP_MAIL_DISABLE_ONBOARD_WIFI\n#endif",
                    1,
                )
            else:
                text = (
                    "#ifndef ESP_MAIL_DISABLE_ONBOARD_WIFI\n"
                    "#define ESP_MAIL_DISABLE_ONBOARD_WIFI\n#endif\n\n"
                    + text
                )
        if text != orig:
            fs_h.write_text(text, encoding="utf-8")
            print(f"[fix_deps] Patched {fs_h.name} (SPIFFS + external WiFi)")


def patch_gfx_headers(libdeps: Path) -> None:
    for spi_h in libdeps.rglob("Arduino_ESP32SPI.h"):
        text = spi_h.read_text(encoding="utf-8")
        orig = text
        for old, new in PATCHES:
            text = text.replace(old, new)
        if text != orig:
            spi_h.write_text(text, encoding="utf-8")
            print(f"[fix_deps] Patched {spi_h.name}")


libdeps_dir = Path(env.subst("$PROJECT_LIBDEPS_DIR"))
if libdeps_dir.is_dir():
    patch_gfx_headers(libdeps_dir)
    patch_esp_mail_fs(libdeps_dir)
    for name in DISABLE:
        for src in libdeps_dir.rglob(name):
            bak = src.with_suffix(".cpp.pio_bak")
            if not bak.exists() and src.exists():
                src.rename(bak)
                print(f"[fix_deps] Disabled {name}")
