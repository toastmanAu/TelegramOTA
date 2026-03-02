/*
 * CellularOTA — TelegramOTA over cellular modem (TinyGSM)
 *
 * Tested with: LilyGO T-SIM7080G-S3
 * Works with any TinyGSM-supported modem (A7670, SIM800, SIM7600, etc)
 *
 * To enable GSM transport, add to platformio.ini:
 *   build_flags = -D TOTA_USE_GSM
 *
 * NOTE: This uses SIM credit for data. Make sure your SIM has a data plan
 * before enabling. WiFi (BasicOTA example) is free and faster for development.
 */

// ── Modem type — set BEFORE TelegramOTA include ───────────────────────────────
#define TINY_GSM_MODEM_SIM7672    // T-SIM7080G-S3 / T-SIM7670G-S3
// #define TINY_GSM_MODEM_A7670   // T-A7670
// #define TINY_GSM_MODEM_SIM800  // SIM800L modules

#define TOTA_USE_GSM
#include <TelegramOTA.h>
#include <TinyGsmClient.h>

#define BOT_TOKEN   "YOUR_BOT_TOKEN"
#define CHAT_ID     "YOUR_CHAT_ID"
#define NETWORK_APN "live.vodafone.com"  // Your carrier APN

// T-SIM7080G-S3 modem pins (from utilities.h LILYGO_T_SIM767XG_S3)
#define MODEM_TX     11
#define MODEM_RX     10
#define MODEM_PWRKEY 18
#define MODEM_RESET  17

TinyGsm     modem(Serial1);
TelegramOTA ota(BOT_TOKEN, CHAT_ID);

bool startModem() {
    Serial.println("Starting modem...");
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);  delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH); delay(1000);
    digitalWrite(MODEM_PWRKEY, LOW);  delay(5000);

    Serial1.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);

    if (!modem.testAT(10000)) { Serial.println("Modem not responding"); return false; }
    if (!modem.waitForNetwork(60000)) { Serial.println("Network timeout"); return false; }
    if (!modem.gprsConnect(NETWORK_APN)) { Serial.println("GPRS failed"); return false; }

    Serial.println("Modem ready. IP: " + modem.localIP().toString());
    return true;
}

void setup() {
    Serial.begin(115200);
    if (!startModem()) { while (1) delay(1000); }
    ota.setDeviceName("T-SIM7080G");
    ota.begin("1.0.0", &modem);
}

void loop() {
    ota.handle();
    delay(5);
}
