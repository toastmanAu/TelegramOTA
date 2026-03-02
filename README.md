# TelegramOTA

Remote OTA firmware updates for ESP32 via Telegram. Flash from anywhere — no LAN, no port forwarding, no USB after the first time.

By [Wyltek Industries](https://wyltekindustries.com) · Companion to [TelegramSerial](https://github.com/toastmanAu/TelegramSerial)

## How it works

1. Build your sketch → export `.bin`
2. Send `/flash` to your bot with the `.bin` attached
3. Device downloads, verifies, flashes, reboots
4. New firmware reports back: `✅ Running v1.1`

## Install

**Arduino Library Manager:** search `TelegramOTA`

**PlatformIO:**
```ini
lib_deps = toastmanAu/TelegramOTA
```

## Quickstart

```cpp
#include <WiFi.h>
#include <TelegramOTA.h>

TelegramOTA ota("YOUR_BOT_TOKEN", "YOUR_CHAT_ID");

void setup() {
    WiFi.begin("ssid", "pass");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    ota.begin("1.0.0");   // version string — shown after flash
}

void loop() {
    ota.handle();         // polls Telegram every 5s
}
```

## Commands

| Command | Action |
|---|---|
| `/flash` + `.bin` | Flash new firmware |
| `/rollback` | Revert to previous firmware |
| `/reboot` | Restart device |
| `/status` | Heap, uptime, RSSI, flash usage |
| `/version` | Firmware version + build date |
| `/log` | Last 50 log lines |
| `/help` | Command list |

## Remote debug loop (with TelegramSerial)

```cpp
#include <TelegramSerial.h>
#include <TelegramOTA.h>

TelegramSerial tgSerial(BOT_TOKEN, CHAT_ID);
TelegramOTA    ota(BOT_TOKEN, CHAT_ID);

void setup() {
    // ...WiFi...
    tgSerial.begin();        // Serial → Telegram
    ota.begin("1.0.0");      // OTA via Telegram
}

void loop() {
    tgSerial.handle();
    ota.handle();
    // Serial.println() now streams to your chat
}
```

Full remote dev loop: push code → build → `/flash` → read serial output — all from Telegram.

## Log buffer

Replace `Serial.println()` with `ota.log()` to keep a ring buffer accessible via `/log`:

```cpp
ota.log("Sensor: " + String(reading));
```

## Callbacks

```cpp
ota.onFlash([](bool ok, const String& msg) {
    // called after flash attempt
});

ota.onProgress([](size_t written, size_t total) {
    // called during download
});

ota.onCommand([](const String& cmd, const String& args) -> bool {
    if (cmd == "/sensor") { /* handle */ return true; }
    return false;  // let TelegramOTA handle it
});
```

## Rollback

If new firmware crashes on boot, the previous firmware is still in the inactive OTA partition. Send `/rollback` to revert (requires the device to be at least partially booting and connected to WiFi).

For automatic rollback on crash: call `esp_ota_mark_app_valid_cancel_rollback()` in your setup once you're happy the firmware is stable.

## Security

- Only the configured `CHAT_ID` can trigger OTA or commands
- Add additional authorised chats: `ota.addAuthorisedChat("987654321")`
- All Telegram traffic is HTTPS

## Configuration

Override in `platformio.ini` build_flags:

```ini
build_flags =
    -DTOTA_POLL_INTERVAL_MS=3000    ; poll every 3s (default 5s)
    -DTOTA_LOG_BUFFER_LINES=100     ; bigger log buffer
    -DTOTA_CHUNK_SIZE=2048          ; larger OTA chunks
```

## Requirements

- ESP32 (any variant)
- ArduinoJson >= 7.0.0
- WiFi connection with internet access
