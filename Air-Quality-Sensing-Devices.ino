#include <SoftwareSerial.h>
#include "MHZ19.h"
#include <TM1637Display.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>

#define RX_PIN D2
#define TX_PIN D1
#define BAUDRATE 9600
#define CLK_PIN D5  
#define DIO_PIN D6 

const int httpsPort = 443;

WiFiManager wifiManager;
WiFiManagerParameter custom_host("host", "Host", "", 40);
WiFiManagerParameter custom_token("token", "Access Token", "", 80);
WiFiClientSecure client;

String host;
String accessToken;
unsigned long lastCheckTime = 0;
const int CHECK_INTERVAL = 10000;
String lastPostedStatus;
const int MAX_RETRIES = 3;

MHZ19 myMHZ19;
SoftwareSerial mySerial(RX_PIN, TX_PIN);
TM1637Display display(CLK_PIN, DIO_PIN);

void configModeCallback(WiFiManager* myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setupWiFi() {
    wifiManager.addParameter(&custom_host);
    wifiManager.addParameter(&custom_token);
    wifiManager.setAPCallback(configModeCallback);
  
    if (!wifiManager.autoConnect()) {
        Serial.println("Failed to connect and hit timeout");
    }

    host = custom_host.getValue();
    accessToken = custom_token.getValue();
    String IPaddress = WiFi.localIP().toString();
    Serial.println("Wi-Fi connected. IP Address: " + IPaddress);
}

void postToMastodon(String status) {
    int retries = 0;

    while (retries < MAX_RETRIES) {
        if (!client.connect(host.c_str(), httpsPort)) {
            Serial.println("Connection failed! Retrying...");
            retries++;
            delay(1000);
            continue;
        }

        String postData = "status=" + status;
        client.println("POST /api/v1/statuses HTTP/1.1");
        client.println("Host: " + host);
        client.println("User-Agent: D1Mini");
        client.println("Authorization: Bearer " + accessToken);
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.print("Content-Length: ");
        client.println(postData.length());
        client.println();
        client.println(postData);

        while (client.connected()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") {
                break;
            }
            Serial.println(line);
        }

        break;
    }
}

void setup() {
    Serial.begin(9600);
    mySerial.begin(BAUDRATE);
    myMHZ19.begin(mySerial);
    myMHZ19.autoCalibration(true);

    display.setBrightness(0x0f);
    display.clear();

    client.setInsecure();
    setupWiFi();
}

void loop() {
    int CO2 = myMHZ19.getCO2();
    int8_t temp = myMHZ19.getTemperature(true);

    Serial.print("CO2 (ppm): ");
    Serial.print(CO2);
    Serial.print(", Temperature (C): ");
    Serial.println(temp);

    display.showNumberDec(CO2, false);

    if (millis() - lastCheckTime >= CHECK_INTERVAL) {
        lastCheckTime = millis();
        String currentStatus = "CO2 (ppm): " + String(CO2) + ", Temperature (C): " + String(temp);
        if (currentStatus != lastPostedStatus) {
            postToMastodon(currentStatus);
            lastPostedStatus = currentStatus;
        }
    }

    delay(5000);
}
