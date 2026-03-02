/*
 * TelegramOTA — BasicOTA example
 *
 * Flash your ESP32 remotely via Telegram:
 *   1. Get your bot token from @BotFather
 *   2. Get your chat ID from @userinfobot
 *   3. Flash this sketch via USB (first time only)
 *   4. Build your next sketch, export .bin (Sketch → Export Compiled Binary)
 *   5. Send /flash to your bot with the .bin attached
 *   6. Done — you'll never need USB again
 */

#include <WiFi.h>
#include <TelegramOTA.h>

const char* WIFI_SSID  = "your-wifi-ssid";
const char* WIFI_PASS  = "your-wifi-password";
const char* BOT_TOKEN  = "123456:ABC-your-bot-token";
const char* CHAT_ID    = "123456789";   // your Telegram user ID

// Version string — update this in each firmware so you can confirm
// the new version is running after a flash
#define FIRMWARE_VERSION "1.0.0"

TelegramOTA ota(BOT_TOKEN, CHAT_ID);

void setup() {
    Serial.begin(115200);

    // Connect WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected: " + WiFi.localIP().toString());

    // Start TelegramOTA
    ota.setDeviceName("MyESP32");
    ota.begin(FIRMWARE_VERSION);

    // Optional: callback after flash
    ota.onFlash([](bool success, const String& msg) {
        Serial.println("Flash result: " + msg);
    });

    // Optional: progress during download
    ota.onProgress([](size_t written, size_t total) {
        Serial.printf("OTA: %u / %u bytes\n", written, total);
    });

    // Optional: log buffer — replace your Serial.printlns with ota.log()
    // so /log command shows recent output
    ota.log("Setup complete");
}

void loop() {
    ota.handle();   // polls Telegram, handles commands

    // Your normal sketch code here
    // Replace Serial.println with ota.log() for remote visibility:
    // ota.log("Sensor reading: " + String(analogRead(34)));

    delay(10);
}
