#pragma once
/*
 * TelegramOTA.h — Remote OTA flashing for ESP32 via Telegram
 *
 * Flash your ESP32 from anywhere in the world:
 *   1. Send /flash to your bot with a .bin file attached
 *   2. Device downloads, verifies, flashes, reboots
 *   3. New firmware reports back: "✅ Running v1.2"
 *
 * Pairs naturally with TelegramSerial for full remote dev loop.
 *
 * Transport options (set in platformio.ini build_flags or before #include):
 *   Default:           WiFi (WiFiClientSecure) — no extra dependencies
 *   -D TOTA_USE_GSM    Cellular via TinyGSM — call begin() after modem is up
 *
 * Author: Wyltek Industries / toastmanAu
 * License: MIT
 */

#ifndef TELEGRAM_OTA_H
#define TELEGRAM_OTA_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <functional>

// ── Transport selection ───────────────────────────────────────────────────────
//
// Default: WiFi (WiFiClientSecure). No changes needed for normal ESP32 use.
//
// For cellular (TinyGSM boards — T-SIM7080G-S3, T-A7670, etc):
//   Add to platformio.ini build_flags: -D TOTA_USE_GSM
//   Also define your modem type BEFORE this header:
//     #define TINY_GSM_MODEM_SIM7672   (or A7670, SIM800, etc)
//     #include <TinyGsmClient.h>
//   Then pass your TinyGsm modem to begin():
//     ota.begin("1.0.0", &modem);
//
// Note: GSM mode does NOT initialise the modem — call startModem() or
// your own modem init first. TelegramOTA just borrows the connection.

#ifdef TOTA_USE_GSM
  #ifndef TINY_GSM_MODEM_SIM7672
    #warning "TOTA_USE_GSM defined but no modem type set. Add e.g. #define TINY_GSM_MODEM_SIM7672 before including TelegramOTA.h"
  #endif
  #include <TinyGsmClient.h>
  #define TOTA_CLIENT_TYPE TinyGsmClientSecure
#else
  #include <WiFiClientSecure.h>
  #define TOTA_CLIENT_TYPE WiFiClientSecure
#endif

// ── Config defaults (override before #include or in platformio.ini) ───────────

#ifndef TOTA_POLL_INTERVAL_MS
#define TOTA_POLL_INTERVAL_MS   5000    // how often to poll Telegram (ms)
#endif

#ifndef TOTA_HTTPS_TIMEOUT_MS
#define TOTA_HTTPS_TIMEOUT_MS   15000   // HTTPS connection timeout
#endif

#ifndef TOTA_CHUNK_SIZE
#define TOTA_CHUNK_SIZE         1024    // bytes per OTA write chunk
#endif

#ifndef TOTA_LOG_BUFFER_LINES
#define TOTA_LOG_BUFFER_LINES   50      // lines kept in /log ring buffer
#endif

#ifndef TOTA_MAX_FILE_SIZE
#define TOTA_MAX_FILE_SIZE      (2 * 1024 * 1024)  // 2MB sanity limit
#endif

// ── Callback types ────────────────────────────────────────────────────────────

using TOTAFlashCallback    = std::function<void(bool success, const String& msg)>;
using TOTACommandCallback  = std::function<bool(const String& cmd, const String& args)>;
using TOTAProgressCallback = std::function<void(size_t written, size_t total)>;

// ── Flash result ──────────────────────────────────────────────────────────────

enum class TOTAResult {
    OK,
    ERR_NOT_AUTHORISED,
    ERR_NO_FILE,
    ERR_DOWNLOAD_FAILED,
    ERR_TOO_LARGE,
    ERR_WRITE_FAILED,
    ERR_VERIFY_FAILED,
};

// ── Main class ────────────────────────────────────────────────────────────────

class TelegramOTA {
public:
    // ── Constructor ───────────────────────────────────────────────────────────

    /**
     * @param botToken   Telegram bot token (from @BotFather)
     * @param chatId     Authorised chat ID — only this chat can trigger OTA
     */
    TelegramOTA(const String& botToken, const String& chatId);

    // ── Setup ─────────────────────────────────────────────────────────────────

    /**
     * Call once in setup() after WiFi (or modem) is connected.
     * @param appVersion  Optional version string e.g. "1.2.0"
     * @param modem       GSM mode only: pointer to your TinyGsm modem instance
     */
#ifdef TOTA_USE_GSM
    void begin(const String& appVersion = "", TinyGsm* modem = nullptr);
#else
    void begin(const String& appVersion = "");
#endif

    /**
     * Call in loop(). Polls Telegram, handles commands, triggers OTA.
     * Non-blocking between polls — safe to call every loop iteration.
     */
    void handle();

    // ── Callbacks ─────────────────────────────────────────────────────────────

    void onFlash(TOTAFlashCallback cb)       { _flashCb = cb; }
    void onProgress(TOTAProgressCallback cb) { _progressCb = cb; }

    /**
     * Register a custom command handler. Return true if you handled it,
     * false to let TelegramOTA process it normally.
     */
    void onCommand(TOTACommandCallback cb)   { _commandCb = cb; }

    // ── Config ────────────────────────────────────────────────────────────────

    void addAuthorisedChat(const String& chatId);
    void setInsecure(bool insecure = true)   { _insecure = insecure; }
    void setCACert(const char* cert)         { _caCert = cert; }
    void setDeviceName(const String& name)   { _deviceName = name; }

    // ── Log buffer ────────────────────────────────────────────────────────────

    void log(const String& line);

    // ── Manual triggers ───────────────────────────────────────────────────────

    bool sendMessage(const String& text);
    TOTAResult flashFromUrl(const String& url, size_t expectedSize = 0);

private:
    String   _botToken;
    String   _chatId;
    String   _appVersion;
    String   _deviceName;
    bool     _insecure   = true;
    const char* _caCert  = nullptr;
    int64_t  _lastUpdateId = 0;
    uint32_t _lastPollMs   = 0;

    std::vector<String> _authorisedChats;
    String  _logBuffer[TOTA_LOG_BUFFER_LINES];
    int     _logHead  = 0;
    int     _logCount = 0;

    TOTAFlashCallback    _flashCb    = nullptr;
    TOTAProgressCallback _progressCb = nullptr;
    TOTACommandCallback  _commandCb  = nullptr;

#ifdef TOTA_USE_GSM
    TinyGsm*             _modem = nullptr;
    TinyGsmClientSecure  _client;
#else
    WiFiClientSecure     _client;
#endif

    void        _poll();
    void        _handleUpdate(const String& json);
    void        _handleMessage(const String& chatId, const String& text,
                               const String& fileId, size_t fileSize);
    void        _handleCommand(const String& chatId, const String& cmd,
                               const String& args);
    TOTAResult  _flashFromFileId(const String& fileId, size_t fileSize);
    String      _getFileUrl(const String& fileId);
    String      _apiGet(const String& endpoint);
    bool        _isAuthorised(const String& chatId);
    String      _buildStatusMessage();
    String      _dumpLog();
};

#endif // TELEGRAM_OTA_H
