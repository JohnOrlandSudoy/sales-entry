# PlatformIO — JC3248 Sales Entry

**Pang-production firmware** para sa Guition JC3248W535C (320×480 portrait).

👉 **Buong guide para sa client (install, upload, SD, email, troubleshooting):**  
tingnan ang **[README sa root ng repo](../README.md)**.

## Quick commands

```bash
pio run -e jc3248w535c -t upload
pio device monitor -b 115200
```

Environment: **`jc3248w535c`** (default sa `platformio.ini`).

Upload tip: **hold BOOT** → Upload → bitawan BOOT kapag `Writing at 0x...`

Mas detalyadong upload fixes: [UPLOAD-FIX.md](UPLOAD-FIX.md)
