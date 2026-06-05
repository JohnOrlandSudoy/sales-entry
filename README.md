# Sales Entry — JC3248 (Guition 320×480)

Daily sales entry system para sa **Guition JC3248W535C** (ESP32-S3, 320×480 portrait). May Sales, Dashboard, History, Email invoice (CSV), cash reconciliation, at microSD backup.

May kasamang **web demo** (`src/`) at optional **email server** (`server/`) — ang pang-araw-araw na gamit ay ang firmware sa `platformio_jc3248/`.

---

## Kailangan mo bago mag-upload

| Item | Notes |
|------|--------|
| **Board** | Guition JC3248W535C (ESP32-S3 N16R8, 320×480) |
| **USB cable** | Data cable — hindi yung charge lang |
| **microSD** | Optional pero recommended; **FAT32**, 8GB+ OK |
| **WiFi** | Para sa oras (NTP) at pag-send ng invoice email |
| **PC** | Windows 10/11 (guide na ito Windows ang base) |

### Software — i-download at i-install

1. **[Visual Studio Code](https://code.visualstudio.com/)** — free  
2. Sa VS Code Extensions, hanapin at i-install ang **PlatformIO IDE**  
3. **USB driver** para sa board (kung walang lumalabas na COM port):
   - CP210x o CH340 — depende sa USB chip ng board  
   - Hanapin sa Device Manager kung may **Ports (COM & LPT)** pag nakasaksak ang USB  

Hindi kailangan ng Arduino IDE kung PlatformIO na ang gamit mo.

---

## Step-by-step: unang upload (PlatformIO)

Sundin mo to nang sunod-sunod para hindi mag-error.

### 1. Kunin ang project

```bash
git clone <url-ng-repo-mo>
cd salesentry
```

O kung ZIP: i-extract, tapos buksan sa VS Code ang folder **`salesentry`**.

### 2. Buksan ang tamang folder sa PlatformIO

- **File → Open Folder** → piliin ang **`platformio_jc3248`** (hindi lang `salesentry` root kung first time ka sa PIO)  
- Sa ibaba ng VS Code, dapat may **PlatformIO icon** (alien head)  
- Sa status bar (bottom), environment: **`jc3248w535c`** — yan ang default, huwag palitan maliban kung sinabi ng dev  

### 3. Hintayin matapos ang first build

- PlatformIO → **Build** (checkmark icon), o terminal:

```bash
cd platformio_jc3248
pio run -e jc3248w535c
```

- **Unang build matagal** (5–15 min) — nagda-download ng libraries. Normal yan.  
- Dapat matapos ng **SUCCESS** — kung may error, basahin ang [Troubleshooting](#troubleshooting) sa baba.

### 4. Ihanda ang board bago mag-upload

1. **Isara muna ang Serial Monitor** kung bukas (Stop / basurahan icon) — kung hindi, “COM port locked” ang madalas na error.  
2. I-saksak ang USB sa **port sa likod** ng board (TF slot area).  
3. Kung first upload o hindi kumukuha:
   - **Hold BOOT** (huwag bitawan pa)  
   - Click **Upload** sa PlatformIO  
   - **Bitawan BOOT** kapag nakita mo na sa log: `Writing at 0x...`

```bash
pio run -e jc3248w535c -t upload
```

### 5. Serial Monitor (optional — para makita kung OK ang boot)

```bash
pio device monitor -b 115200
```

Dapat may lines tulad ng `[SD]`, `[data]`, `[UI]`. I-stop ang monitor bago ulit mag-upload.

### 6. Pagkatapos mag-upload — gamitin ang device

| Bagay | Default / paalala |
|--------|-------------------|
| **System lock** | PIN **`1234`** — lalabas bago ang main UI |
| **Master PIN** | **`9999`** — Settings at Email (promo head) |
| **microSD** | Auto-gawa ng folder **`DailySales`** — hindi mo kailangan gawin manually sa PC |
| **Email** | Punan sa **Settings** (sender, recipient, Gmail App Password) tapos **SAVE** |

---

## microSD — bagong card

- Format: **FAT32**  
- I-insert sa TF slot (likod ng board), reboot  
- Ang app ang gagawa ng `DailySales` folder at `settings.cfg` / `sales.cfg`  
- Kung gusto mong dalhin ang lumang data: kopyahin ang buong **`DailySales`** folder mula sa lumang SD papunta sa bago  

Kung walang SD, may backup sa device memory (NVS) pero mas safe may SD para sa sales history.

---

## Email / invoice (sa device)

1. Sa **Settings**: sender email, recipient (promo head), **Gmail App Password** (16 character — hindi yung normal password)  
2. I-save  
3. Sa **Dash**: i-close ang sales, gawin ang **Cash** reconciliation  
4. **Email** icon → review → send  
5. CSV attachment may **TotQty**, **SysTotal**, **Manual** per salesman  

Gmail App Password: [Google Account → App passwords](https://myaccount.google.com/apppasswords)

**Huwag** ilagay ang tunay na password sa `app_defaults.h` kung ipu-push sa GitHub. Gamitin ang Settings sa device, o local copy ng `app_defaults.h` (tingnan `app_defaults.h.example`).

---

## Web app + email server (optional — PC lang)

Para sa browser demo at email via PC server (hindi kailangan para gumana ang JC3248):

```bash
# Sa root ng salesentry
npm install
cd server && npm install && cd ..
cp server/.env.example server/.env
# Edit server/.env — lagay SMTP details
npm run dev:all
```

Buksan ang URL na ibibigay ng Vite (kadalasan `http://localhost:5173`).

---

## Project folders (saglit lang)

```
salesentry/
├── platformio_jc3248/    ← FIRMWARE — dito mag-upload sa JC3248
├── src/                  ← Web UI (React)
├── server/               ← Optional SMTP helper
└── arduino_lcd_test/     ← Lumang display tests (hindi pang-production)
```

---

## Troubleshooting

### `Could not open COMxx` / `COM PORT LOCKED`

- Isara **lahat** ng Serial Monitor (PlatformIO, Arduino IDE, ibang terminal)  
- Unplug USB 5–10 sec, plug ulit  
- Upload ulit with **BOOT** held hanggang `Writing at...`  
- Huwag i-hardcode `upload_port` sa `platformio.ini` — hayaan auto-detect  

Tingnan din: `platformio_jc3248/UPLOAD-FIX.md`

### Build fail — `dram0_0_seg` overflow

- Gamitin environment **`jc3248w535c`** lang (hindi `jc3248_sample` maliban sa test)  
- `pio run -t clean` tapos build ulit  

### Build fail — GFX / `Arduino_ESP32LCD8.cpp`

- Minsan kailangan i-rename sa `.pio/libdeps/.../Arduino_ESP32LCD8.cpp` → `.bak`  
- Tapos `pio run` ulit (nakalagay din sa lumang `platformio_jc3248/README.md`)

### Black screen pagkatapos upload

- Serial monitor: hanapin `PSRAM` — dapat may PSRAM detected  
- Check backlight; subukan power cycle  
- Re-upload gamit BOOT procedure  

### Mali ang oras

- Kailangan naka-connect sa **WiFi** para sa NTP  
- Sa Settings, i-check ang WiFi SSID/password  

### Hindi nagse-send ang email

- Punan sender, recipient, SMTP app password sa Settings → **SAVE**  
- WiFi connected  
- Lahat ng sales **CLOSED** + cash reconciliation OK bago mag-send  

---

## Safe push sa GitHub (para sa dev / owner)

**Huwag i-commit:**

- `server/.env` — SMTP password  
- Tunay na Gmail App Password sa `app_defaults.h`  
- `node_modules/`, `.pio/`, `dist/`  

Naka-set na sa `.gitignore`. Kung may local secrets ka na, check:

```bash
git status
```

Dapat walang `.env` o `.pio` sa listahan bago `git push`.

---

## Mga command na madalas gamitin

```bash
cd platformio_jc3248

pio run -e jc3248w535c              # build lang
pio run -e jc3248w535c -t upload      # upload sa board
pio device monitor -b 115200          # serial log
pio run -t clean                      # linis build kung may weird error
```

---

## Suporta

Kung may error pa rin pagkatapos sundin ang steps, kunin ang **Serial Monitor log** (copy from boot hanggang error) at screenshot ng exact error sa PlatformIO — mas mabilis ma-troubleshoot.

---

*Board: Guition JC3248W535C · Firmware env: `jc3248w535c` · Portrait 320×480*
