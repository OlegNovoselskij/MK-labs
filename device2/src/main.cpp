#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "CommunicationService.h"
#include <WebSocketsServer.h> 

#define BUTTON_PIN D2
#define LED1 D3
#define LED2 D5
#define LED3 D4

const char* apSSID = "ESP8266-DANIEL";
const char* apPassword = "123456789";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
SoftwareSerial mySerial(D7, D6, false);
CommunicationService communicationService(mySerial, 115200);

int leds[] = {LED1, LED2, LED3};

bool direction = true;
int ledIndex = 0;
unsigned long previousMillis = 0;
const int interval = 300;

void IRAM_ATTR handleButton() {
    direction = !direction;
    server.send(303);
    communicationService.send(ToogleCommand::ON);
}

void setupPins() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED1, OUTPUT);
    pinMode(LED2, OUTPUT);
    pinMode(LED3, OUTPUT);
}

void sendLEDState() {
    String message = String(digitalRead(leds[0])) + "," +
                     String(digitalRead(leds[1])) + "," +
                     String(digitalRead(leds[2]));
    webSocket.broadcastTXT(message);
}

void setupWiFi() {
    WiFi.softAP(apSSID, apPassword);
    Serial.println("Access Point Started: " + String(apSSID));
    Serial.println("IP Address: " + WiFi.softAPIP().toString());
}

void setupWebSocket() {
    webSocket.begin();
    sendLEDState();
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        if (type == WStype_TEXT) {
            String command = String((char*)payload);
            if (command == "TOGGLE") {
                direction = !direction;
            }
        }
    });
    Serial.println("WebSocket server started.");
}

void setupServer() {
    server.on("/", []() {
        server.send(200, "text/html",
          "<!DOCTYPE html>"
          "<html>"
          "<head>"
          "<title>LED Control</title>"
          "<style>"
          "body {text-align:center; font-family:Arial;}"
          "button {font-size:20px; padding:10px;}"
          ".led {width: 50px; height: 50px; display: inline-block; border-radius: 50%; margin: 10px;}"
          "</style>"
          "</head>"
          "<body>"
          "<h1>LED Control via Web</h1>"
          "<button onclick=\"stopLEDs()\">Togle leds</button>"
          "<button onclick=\"simulateButtonRemote()\">Press Remote Button</button>"
          "<div>"
          "<div id='led1' class='led' style='background-color:gray;'></div>"
          "<div id='led2' class='led' style='background-color:gray;'></div>"
          "<div id='led3' class='led' style='background-color:gray;'></div>"
          "</div>"
          "<script>"
          "var ws = new WebSocket('ws://' + location.hostname + ':81/');"
          "ws.onmessage = function(event) {"
          "  var states = event.data.split(',');"
          "  document.getElementById('led1').style.backgroundColor = states[0] == '1' ? 'green' : 'gray';"
          "  document.getElementById('led2').style.backgroundColor = states[1] == '1' ? 'blue' : 'gray';"
          "  document.getElementById('led3').style.backgroundColor = states[2] == '1' ? 'green' : 'gray';"
          "};"
          "function stopLEDs() { fetch('/toggleLEDs'); }"
          "function simulateButtonRemote() { fetch('/simulateRemote'); }"
          "</script>"
          "</body>"
          "</html>"
        );
    });

    server.on("/toggleLEDs", []() {
        direction = !direction;
        server.send(200, "text/plain", "LEDs will stop for 15 seconds.");
    });

    server.on("/simulateRemote", []() {
        communicationService.send(ToogleCommand::ON);
        server.send(200, "text/plain", "Simulated remote button press.");
    });

    server.begin();
    Serial.println("Server started.");
}

void handleButtonPress() {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        for (int pin : leds) digitalWrite(pin, LOW);
        int index = (digitalRead(BUTTON_PIN) == LOW) ^ !direction ? 2 - ledIndex : ledIndex;
        digitalWrite(leds[index], HIGH);

        ledIndex = (ledIndex + 1) % 3;

        sendLEDState();
    }
}

void setup() {
    Serial.begin(9600);
    setupPins();
    setupWiFi();
    setupWebSocket();
    setupServer();
    communicationService.init();

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButton, FALLING);
}

void loop() {
    server.handleClient();
    webSocket.loop();
    handleButtonPress();

    communicationService.onReceive([](ToogleCommand command) {
        Serial.print("Received command: ");
        Serial.println((int)command);

        if (command == ToogleCommand::ON) {
            direction = !direction;
            Serial.println("Received ON command! Resuming LEDs...");
        }
    });
}