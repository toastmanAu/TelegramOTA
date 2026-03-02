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
 * Author: Wyltek Industries / toastmanAu
 * License: MIT
 */

#ifndef TELEGRAM_OTA_H
#define TELEGRAM_OTA_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <functional>

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

using TOTAFlashCallback   = std::function<void(bool success, const String& msg)>;
using TOTACommandCallback = std::function<bool(const String& cmd, const String& args)>;
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
     *                   Get yours: message @userinfobot
     */
    TelegramOTA(const String& botToken, const String& chatId);

    // ── Setup ─────────────────────────────────────────────────────────────────

    /**
     * Call once in setup(). Sends startup message to your chat if connected.
     * @param appVersion  Optional version string reported by /version command
     *                    e.g. "1.2.0" or __DATE__ " " __TIME__
     */
    void begin(const String& appVersion = "");

    /**
     * Call in loop(). Polls Telegram, handles commands, triggers OTA if needed.
     * Non-blocking between polls — safe to call every loop iteration.
     */
    void handle();

    // ── Callbacks ─────────────────────────────────────────────────────────────

    /** Called after a flash attempt (success or failure) */
    void onFlash(TOTAFlashCallback cb)      { _flashCb = cb; }

    /** Called during flash with bytes written + total size */
    void onProgress(TOTAProgressCallback cb) { _progressCb = cb; }

    /**
     * Register a custom command handler. Return true if you handled it,
     * false to let TelegramOTA process it normally.
     * Example: ota.onCommand([](const String& cmd, const String& args) {
     *     if (cmd == "/sensor") { sendReading(); return true; }
     *     return false;
     * });
     */
    void onCommand(TOTACommandCallback cb)  { _commandCb = cb; }

    // ── Config ────────────────────────────────────────────────────────────────

    /** Allow additional chat IDs (e.g. a group) */
    void addAuthorisedChat(const String& chatId);

    /** Skip TLS cert verification (easier setup, less secure) */
    void setInsecure(bool insecure = true)  { _insecure = insecure; }

    /** Set custom root CA cert for Telegram API */
    void setCACert(const char* cert)        { _caCert = cert; }

    /** Set device name shown in status messages */
    void setDeviceName(const String& name)  { _deviceName = name; }

    // ── Log buffer (for /log command) ─────────────────────────────────────────

    /**
     * Append a line to the in-memory log ring buffer.
     * Call this wherever you'd normally Serial.println().
     * Accessible via /log command from Telegram.
     */
    void log(const String& line);

    // ── Manual triggers ───────────────────────────────────────────────────────

    /** Send a message to the authorised chat */
    bool sendMessage(const String& text);

    /** Trigger OTA from a URL directly (e.g. from your own CI) */
    TOTAResult flashFromUrl(const String& url, size_t expectedSize = 0);

private:
    // ── Telegram API ──────────────────────────────────────────────────────────

    String  _botToken;
    String  _chatId;
    String  _appVersion;
    String  _deviceName;
    bool    _insecure    = true;        // default insecure for ease of use
    const char* _caCert  = nullptr;

    int64_t _lastUpdateId = 0;
    uint32_t _lastPollMs  = 0;

    std::vector<String> _authorisedChats;
    String  _logBuffer[TOTA_LOG_BUFFER_LINES];
    int     _logHead = 0;
    int     _logCount = 0;

    TOTAFlashCallback    _flashCb    = nullptr;
    TOTAProgressCallback _progressCb = nullptr;
    TOTACommandCallback  _commandCb  = nullptr;

    // ── Internal methods ──────────────────────────────────────────────────────

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

    WiFiClientSecure _client;
};

#endif // TELEGRAM_OTA_H
