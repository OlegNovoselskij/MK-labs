#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include "CommunicationService.h"
#include "ToogleCommand.h"
#include <WebSocketsServer.h> 

#define GREEN_LED D2
#define RED_LED D3
#define BLUE_LED D4
#define BUTTON_PIN D5

const char* ssid = "ESP8266_AP";
const char* pass = "12345678";

ESP8266WebServer server(80);
SoftwareSerial mySerial(D7, D6, false);
CommunicationService communicationService(mySerial, 115200); 
WebSocketsServer webSocket(81);


volatile bool buttonHeld = false;
volatile unsigned long buttonPressStart = 0;
unsigned long interval = 1000;
int ledState = 0;

void IRAM_ATTR handleButtonPress();
void setupHardware();
void setupWiFiServer();
void logStatus();
void updateLEDs();
void checkButton();
void processWebRequests();

void setup() {
    setupHardware();
    setupWiFiServer();
    logStatus();
    communicationService.init();
}

void loop() {
    checkButton();
    updateLEDs();
    processWebRequests();
    communicationService.onReceive([](ToogleCommand command) {
        if (command == ToogleCommand::ON) {
            buttonHeld = true;
            communicationService.send(ToogleCommand::SUCCESSFULLY_RECEIVED);
        }
    });
    webSocket.loop();
}


void IRAM_ATTR handleButtonPress() {
    buttonPressStart = millis();
}

void logStatus() {
    Serial.print("[INFO] Current Interval: ");
    Serial.print(interval);
    Serial.println(" ms");
}

void updateLEDs() {
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        ledState = (ledState + 1) % 3;
        digitalWrite(GREEN_LED, ledState == 0 ? HIGH : LOW);
        digitalWrite(RED_LED, ledState == 1 ? HIGH : LOW);
        digitalWrite(BLUE_LED, ledState == 2 ? HIGH : LOW);
        
        Serial.print("[LED] New State: ");
        Serial.println(ledState);

        String ledStr = String(ledState);
        webSocket.broadcastTXT(ledStr);
    }
}

void checkButton() {
    if (buttonPressStart > 0 && millis() - buttonPressStart >= 1000) {
        buttonHeld = true;
        buttonPressStart = 0;
    }
    if (buttonHeld) {
        buttonHeld = false;
        interval -= 200;
        if (interval <= 200) interval = 1000;
        Serial.println("[BUTTON] Hold detected. Speed changed.");
        logStatus();
    }
}

void processWebRequests() {
    server.handleClient();
}

void setupHardware() {
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);
    Serial.begin(9600);
    Serial.println("[SYSTEM] Initializing hardware...");
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_CONNECTED) {
        Serial.println("[WebSocket] Client connected.");
    }
}

void setupWiFiServer() {
    WiFi.softAP(ssid, pass);
    Serial.println("[WiFi] Access Point Started!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.softAPIP());
    server.on("/", []() {
        server.send(200, "text/html",
            R"rawliteral(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>ESP8266 LED Control</title>
                <style>
                    body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
                    button { font-size: 20px; padding: 10px 20px; margin: 10px; cursor: pointer; }
                    .led-container { display: flex; justify-content: center; margin-top: 20px; }
                    .led { width: 50px; height: 50px; margin: 10px; border-radius: 5px; background-color: gray; transition: 0.1s; }
                </style>
            </head>
            <body>
                <h1>ESP8266 LED Control</h1>
                <button onclick="sendRequest('/changeInterval')">Change LED Speed</button>
                <button onclick="sendRequest('/remote')">Trigger Partner Device</button>
                
                <div class="led-container">
                    <div id="green-led" class="led"></div>
                    <div id="red-led" class="led"></div>
                    <div id="blue-led" class="led"></div>
                </div>

                <script>
                    var ws = new WebSocket("ws://" + location.host + ":81/");

                    ws.onmessage = function(event) {
                        let state = event.data;
                        let green = document.getElementById('green-led');
                        let red = document.getElementById('red-led');
                        let blue = document.getElementById('blue-led');

                        green.style.backgroundColor = (state == "0") ? "blue" : "gray";
                        red.style.backgroundColor = (state == "1") ? "blue" : "gray";
                        blue.style.backgroundColor = (state == "2") ? "blue" : "gray";
                    };

                    function sendRequest(url) {
                        fetch(url).then(response => console.log("Request sent to: " + url));
                    }
                </script>
            </body>
            </html>
            )rawliteral"
        );
    });

    server.on("/changeInterval", []() {
        interval -= 200;
        if (interval <= 200) interval = 1000;
        Serial.println("[WEB] Button clicked. Speed changed.");
        logStatus();
        server.send(204);
    });

    server.on("/remote", []() {
        communicationService.send(ToogleCommand::ON);
        Serial.println("[WEB] Sent 'ON' command to partner.");
        server.send(204);
    });

    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("[WEB] Server & WebSocket started.");
}