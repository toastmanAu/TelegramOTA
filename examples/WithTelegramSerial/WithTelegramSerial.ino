/*
 * TelegramOTA + TelegramSerial — Full remote dev loop
 *
 * Combines both libraries for the complete experience:
 *   - Serial output → your Telegram chat (TelegramSerial)
 *   - Remote flash  → /flash + .bin attachment (TelegramOTA)
 *   - Remote reboot → /reboot
 *   - Status check  → /status
 *   - Log dump      → /log
 *
 * Both libraries share the same WiFiClientSecure connection pool
 * and poll intervals — no doubling up on API calls.
 *
 * Workflow:
 *   Code → PlatformIO build → copy .bin → send to Telegram → /flash
 *   Serial output streams back to you automatically
 */

#include <WiFi.h>
#include <TelegramSerial.h>
#include <TelegramOTA.h>

const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASS = "your-wifi-password";
const char* BOT_TOKEN = "123456:ABC-your-bot-token";
const char* CHAT_ID   = "123456789";

#define FIRMWARE_VERSION "1.0.0"

TelegramSerial tgSerial(BOT_TOKEN, CHAT_ID);
TelegramOTA    ota(BOT_TOKEN, CHAT_ID);

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);

    // TelegramSerial — streams Serial output to Telegram
    tgSerial.begin();

    // TelegramOTA — handles /flash, /reboot, /status etc.
    ota.setDeviceName("MyESP32");
    ota.begin(FIRMWARE_VERSION);

    // Bridge ota.log() → TelegramSerial so everything goes to one place
    // (optional — remove if you want them separate)
    // ota.onProgress([](size_t w, size_t t) {
    //     Serial.printf("[OTA] %u/%u\n", w, t);
    // });

    Serial.println("Ready — v" FIRMWARE_VERSION);
}

void loop() {
    tgSerial.handle();
    ota.handle();

    // Your code here — Serial.println goes to Telegram automatically
    delay(10);
}
