/*
 * TelegramOTA.cpp
 */

#include "TelegramOTA.h"
#include <ArduinoJson.h>

// Telegram API root CA (DigiCert Global Root G2, expires 2038)
// Update if Telegram changes their cert chain
static const char TELEGRAM_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1ng
oSFxLx38OGkS4QEET+hJrMRrLqgDwBdh8rPuRdTH5rPhLyFV5ZXBt+RIE5aaM9N
m3tSBMzfJnNQQNPzfByQDz5rJMY25/Jl7EVKRbaTnwUKWAyEuHrjbKwVzOlSJ4eO
0nBxKDTRbBIl3iSHMBGLSAWItSrHb7RWRHt3Zp0WU4nVXF9Gs7T6JLCEqF0FBGQ
thI=
-----END CERTIFICATE-----
)EOF";

// ── Constructor ───────────────────────────────────────────────────────────────

TelegramOTA::TelegramOTA(const String& botToken, const String& chatId)
    : _botToken(botToken), _chatId(chatId) {
    _authorisedChats.push_back(chatId);
}

// ── begin() ───────────────────────────────────────────────────────────────────

void TelegramOTA::begin(const String& appVersion) {
    _appVersion = appVersion;
    if (_deviceName.isEmpty()) {
        _deviceName = "ESP32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    }

    if (_insecure) {
        _client.setInsecure();
    } else if (_caCert) {
        _client.setCACert(_caCert);
    } else {
        _client.setCACert(TELEGRAM_ROOT_CA);
    }

    // Announce startup
    String msg = "🟢 *" + _deviceName + "* online";
    if (!_appVersion.isEmpty()) msg += " — v" + _appVersion;
    msg += "\n_Ready for OTA. Send /help for commands._";
    sendMessage(msg);
}

// ── handle() ─────────────────────────────────────────────────────────────────

void TelegramOTA::handle() {
    if (millis() - _lastPollMs < TOTA_POLL_INTERVAL_MS) return;
    _lastPollMs = millis();
    _poll();
}

// ── Authorisation ─────────────────────────────────────────────────────────────

void TelegramOTA::addAuthorisedChat(const String& chatId) {
    _authorisedChats.push_back(chatId);
}

bool TelegramOTA::_isAuthorised(const String& chatId) {
    for (auto& id : _authorisedChats) {
        if (id == chatId) return true;
    }
    return false;
}

// ── Log buffer ────────────────────────────────────────────────────────────────

void TelegramOTA::log(const String& line) {
    _logBuffer[_logHead] = line;
    _logHead = (_logHead + 1) % TOTA_LOG_BUFFER_LINES;
    if (_logCount < TOTA_LOG_BUFFER_LINES) _logCount++;
}

String TelegramOTA::_dumpLog() {
    if (_logCount == 0) return "_No log entries yet._";
    String out = "```\n";
    int start = (_logCount < TOTA_LOG_BUFFER_LINES)
                ? 0
                : _logHead;  // oldest entry
    for (int i = 0; i < _logCount; i++) {
        out += _logBuffer[(start + i) % TOTA_LOG_BUFFER_LINES] + "\n";
    }
    out += "```";
    // Telegram message limit 4096 chars — truncate from front if needed
    if (out.length() > 4000) out = "...(truncated)\n" + out.substring(out.length() - 3980);
    return out;
}

// ── Poll Telegram ─────────────────────────────────────────────────────────────

void TelegramOTA::_poll() {
    String endpoint = "/getUpdates?timeout=0&offset=" + String(_lastUpdateId + 1);
    String response = _apiGet(endpoint);
    if (response.isEmpty()) return;

    JsonDocument doc;
    if (deserializeJson(doc, response) != DeserializationError::Ok) return;
    if (!doc["ok"].as<bool>()) return;

    JsonArray results = doc["result"].as<JsonArray>();
    for (JsonObject update : results) {
        _lastUpdateId = update["update_id"].as<int64_t>();
        _handleUpdate(update.as<String>());
    }
}

void TelegramOTA::_handleUpdate(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    JsonObject msg = doc["message"];
    if (msg.isNull()) msg = doc["channel_post"];
    if (msg.isNull()) return;

    String fromChatId = String(msg["chat"]["id"].as<int64_t>());
    String text       = msg["text"].as<String>();
    String fileId     = "";
    size_t fileSize   = 0;

    // Check for document attachment (.bin file)
    if (!msg["document"].isNull()) {
        fileId   = msg["document"]["file_id"].as<String>();
        fileSize = msg["document"]["file_size"].as<size_t>();
        // caption may carry the command
        if (text.isEmpty()) text = msg["caption"].as<String>();
    }

    _handleMessage(fromChatId, text, fileId, fileSize);
}

void TelegramOTA::_handleMessage(const String& chatId, const String& text,
                                  const String& fileId, size_t fileSize) {
    if (!_isAuthorised(chatId)) {
        sendMessage("⛔ Unauthorised chat: " + chatId);
        return;
    }

    // Extract command and args
    String cmd  = text;
    String args = "";
    int space = text.indexOf(' ');
    if (space > 0) {
        cmd  = text.substring(0, space);
        args = text.substring(space + 1);
        args.trim();
    }
    cmd.toLowerCase();

    // Custom command handler first
    if (_commandCb && _commandCb(cmd, args)) return;

    _handleCommand(chatId, cmd, args.length() ? args : fileId);

    // /flash with attached file
    if ((cmd == "/flash" || cmd.isEmpty()) && !fileId.isEmpty()) {
        _handleCommand(chatId, "/flash", fileId);
        // Store fileSize for flash — pass via lambda capture workaround
        // (actual flash triggered inside _handleCommand)
    }
}

void TelegramOTA::_handleCommand(const String& chatId, const String& cmd,
                                  const String& args) {
    if (cmd == "/flash") {
        if (args.isEmpty()) {
            sendMessage("📎 Send a .bin file with caption /flash — or attach it and I'll flash automatically.");
            return;
        }
        sendMessage("⬇️ Downloading firmware...");
        TOTAResult result = _flashFromFileId(args, 0);

        String resultMsg;
        bool ok = (result == TOTAResult::OK);
        switch (result) {
            case TOTAResult::OK:
                resultMsg = "✅ Flash successful";
                if (!_appVersion.isEmpty()) resultMsg += " — now running v" + _appVersion;
                resultMsg += "\n_Rebooting..._";
                break;
            case TOTAResult::ERR_NO_FILE:       resultMsg = "❌ No file found — attach a .bin"; break;
            case TOTAResult::ERR_DOWNLOAD_FAILED: resultMsg = "❌ Download failed"; break;
            case TOTAResult::ERR_TOO_LARGE:     resultMsg = "❌ File too large (max 2MB)"; break;
            case TOTAResult::ERR_WRITE_FAILED:  resultMsg = "❌ OTA write failed: " + Update.errorString(); break;
            case TOTAResult::ERR_VERIFY_FAILED: resultMsg = "❌ OTA verify failed"; break;
            default:                            resultMsg = "❌ Unknown error"; break;
        }

        if (_flashCb) _flashCb(ok, resultMsg);
        sendMessage(resultMsg);

        if (ok) {
            delay(500);
            ESP.restart();
        }

    } else if (cmd == "/rollback") {
        sendMessage("↩️ Rolling back to previous firmware...");
        if (!Update.rollBack()) {
            sendMessage("❌ Rollback failed — no previous firmware in OTA partition.");
        } else {
            sendMessage("✅ Rollback applied. Rebooting...");
            delay(500);
            ESP.restart();
        }

    } else if (cmd == "/reboot") {
        sendMessage("🔄 Rebooting...");
        delay(500);
        ESP.restart();

    } else if (cmd == "/status") {
        sendMessage(_buildStatusMessage());

    } else if (cmd == "/version") {
        String v = "📦 *" + _deviceName + "*\n";
        v += _appVersion.isEmpty() ? "_No version set_" : "Version: `" + _appVersion + "`";
        v += "\nBuilt: `" __DATE__ " " __TIME__ "`";
        sendMessage(v);

    } else if (cmd == "/log") {
        sendMessage(_dumpLog());

    } else if (cmd == "/help") {
        sendMessage(
            "🤖 *TelegramOTA Commands*\n\n"
            "/flash — attach a .bin and send to flash\n"
            "/rollback — revert to previous firmware\n"
            "/reboot — restart the device\n"
            "/status — heap, uptime, WiFi RSSI\n"
            "/version — firmware version info\n"
            "/log — last " + String(TOTA_LOG_BUFFER_LINES) + " log lines"
        );
    }
    // Unknown commands are silently ignored (don't spam chat)
}

// ── Status message ────────────────────────────────────────────────────────────

String TelegramOTA::_buildStatusMessage() {
    uint32_t upSec = millis() / 1000;
    String msg = "📊 *" + _deviceName + "* status\n\n";
    msg += "⏱ Uptime: " + String(upSec / 3600) + "h " + String((upSec % 3600) / 60) + "m\n";
    msg += "💾 Free heap: " + String(ESP.getFreeHeap() / 1024) + " KB\n";
    msg += "📶 RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    msg += "🔧 Flash size: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB\n";
    msg += "🗂 Sketch size: " + String(ESP.getSketchSize() / 1024) + " KB\n";
    msg += "🆓 OTA space: " + String(ESP.getFreeSketchSpace() / 1024) + " KB\n";
    if (!_appVersion.isEmpty()) msg += "📦 Version: " + _appVersion;
    return msg;
}

// ── Flash from Telegram file ID ───────────────────────────────────────────────

TOTAResult TelegramOTA::_flashFromFileId(const String& fileId, size_t fileSize) {
    String url = _getFileUrl(fileId);
    if (url.isEmpty()) return TOTAResult::ERR_NO_FILE;
    return flashFromUrl(url, fileSize);
}

TOTAResult TelegramOTA::flashFromUrl(const String& url, size_t expectedSize) {
    HTTPClient http;
    http.setTimeout(TOTA_HTTPS_TIMEOUT_MS);

    WiFiClientSecure dlClient;
    dlClient.setInsecure();  // Telegram CDN — cert chain changes often

    if (!http.begin(dlClient, url)) return TOTAResult::ERR_DOWNLOAD_FAILED;

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        return TOTAResult::ERR_DOWNLOAD_FAILED;
    }

    size_t contentLen = http.getSize();
    if (contentLen == 0 && expectedSize > 0) contentLen = expectedSize;
    if (contentLen > TOTA_MAX_FILE_SIZE) {
        http.end();
        return TOTAResult::ERR_TOO_LARGE;
    }

    if (!Update.begin(contentLen > 0 ? contentLen : UPDATE_SIZE_UNKNOWN)) {
        http.end();
        return TOTAResult::ERR_WRITE_FAILED;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[TOTA_CHUNK_SIZE];
    size_t written = 0;

    while (http.connected() && written < contentLen) {
        size_t available = stream->available();
        if (available == 0) { delay(1); continue; }

        size_t toRead = min(available, (size_t)TOTA_CHUNK_SIZE);
        size_t read   = stream->readBytes(buf, toRead);
        size_t wr     = Update.write(buf, read);

        if (wr != read) {
            http.end();
            Update.abort();
            return TOTAResult::ERR_WRITE_FAILED;
        }
        written += wr;
        if (_progressCb) _progressCb(written, contentLen);
    }

    http.end();

    if (!Update.end(true)) return TOTAResult::ERR_VERIFY_FAILED;
    return TOTAResult::OK;
}

// ── Telegram file URL resolution ──────────────────────────────────────────────

String TelegramOTA::_getFileUrl(const String& fileId) {
    String response = _apiGet("/getFile?file_id=" + fileId);
    if (response.isEmpty()) return "";

    JsonDocument doc;
    if (deserializeJson(doc, response) != DeserializationError::Ok) return "";
    if (!doc["ok"].as<bool>()) return "";

    String filePath = doc["result"]["file_path"].as<String>();
    if (filePath.isEmpty()) return "";

    return "https://api.telegram.org/file/bot" + _botToken + "/" + filePath;
}

// ── Telegram API GET ──────────────────────────────────────────────────────────

String TelegramOTA::_apiGet(const String& endpoint) {
    HTTPClient http;
    http.setTimeout(TOTA_HTTPS_TIMEOUT_MS);

    WiFiClientSecure apiClient;
    if (_insecure) {
        apiClient.setInsecure();
    } else if (_caCert) {
        apiClient.setCACert(_caCert);
    } else {
        apiClient.setCACert(TELEGRAM_ROOT_CA);
    }

    String url = "https://api.telegram.org/bot" + _botToken + endpoint;
    if (!http.begin(apiClient, url)) return "";

    int code = http.GET();
    String body = (code == HTTP_CODE_OK) ? http.getString() : "";
    http.end();
    return body;
}

// ── Send message ──────────────────────────────────────────────────────────────

bool TelegramOTA::sendMessage(const String& text) {
    HTTPClient http;
    WiFiClientSecure msgClient;
    msgClient.setInsecure();

    String url = "https://api.telegram.org/bot" + _botToken + "/sendMessage";
    if (!http.begin(msgClient, url)) return false;

    // POST with JSON body — handles spaces, markdown, special chars correctly
    JsonDocument doc;
    doc["chat_id"]    = _chatId;
    doc["text"]       = text;
    doc["parse_mode"] = "Markdown";

    String body;
    serializeJson(doc, body);

    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();
    return (code == HTTP_CODE_OK);
}
