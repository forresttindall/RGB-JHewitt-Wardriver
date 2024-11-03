// Forrest TIndall 2024 www.Forresttindall.com

// Modified from Joseph Hewitt 2023
//This code is for the ESP32 "Side B" of the wardriver hardware revision 3.

//Serial = PC, 115200
//Serial1 = ESP32 (side A), 115200
//Serial2 = SIM800L module, 9600

#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Update.h>
#include "mbedtls/sha256.h"
#include "mbedtls/md.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <OneWire.h>

// Constants
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define RED_PIN 2
#define GREEN_PIN 0
#define BLUE_PIN 4
#define MAX_INTENSITY 255
#define BUTTON_PIN 17
#define MAC_HISTORY_LEN 1024

// Global Variables
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
OneWire ds(22); // DS18B20 data pin is 22.
byte addr[8]; // The DS18B20 address
boolean serial_lock = false;
boolean temperature_sensor_ok = true;
boolean ota_mode = false;
String ota_hash = "";
boolean using_bw16 = false;

// New network detection flags
bool new_wifi_detected = false;
bool new_bluetooth_detected = false;
bool new_cellular_detected = false;

// MAC address history
struct mac_addr {
   unsigned char bytes[6];
};
struct mac_addr mac_history[MAC_HISTORY_LEN];
unsigned int mac_history_cursor = 0;

// BLE and WiFi variables
int ble_found = 0;
int wifi_scan_channel = 1;
BLEScan* pBLEScan;

// Global Variables
int currentMode = 0; // Declare currentMode to track the current lighting mode
int numModes = 9; // Total number of modes including the new one

// Function Prototypes
void setup_wifi();
void await_serial();
void request_temperature();
void read_temperature();
void flashColor(int color);
void buttonPressed();
void loop3();
void setup2();
void setup_id_pins();
byte read_id_pins();
void save_mac(unsigned char* mac);
boolean seen_mac(unsigned char* mac);
boolean mac_cmp(struct mac_addr addr1, struct mac_addr addr2);
void clear_mac_history();
void strobeEffect(); // Declare the strobeEffect function
void strobeEffectAlternate(); // Declare the strobeEffectAlternate function
void purpleStrobeEffect(); // Declare the purpleStrobeEffect function
void greenStrobeEffect(); // Declare the greenStrobeEffect function
void blueStrobeEffect(); // Declare the blueStrobeEffect function
void redStrobeEffect(); // Declare the redStrobeEffect function
void rgbStrobeEffect(); // Declare the rgbStrobeEffect function

// Setup function
void setup() {
    setup_wifi();
    delay(5000);
    Serial.begin(115200);
    Serial.println("Starting");
    Serial1.begin(115200, SERIAL_8N1, 27, 14);
    Serial1.println("REV3!");
    setup_id_pins();
    // Additional setup code...
}

// Setup WiFi
void setup_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

// Await serial lock
void await_serial() {
    while (serial_lock) {
        Serial.println("await");
        delay(1);
    }
}

// Flash color based on network type
void flashColor(int color) {
    switch (color) {
        case 0: // Purple for Wi-Fi
            for (int i = 0; i < 2; i++) {
                analogWrite(RED_PIN, MAX_INTENSITY);
                analogWrite(BLUE_PIN, MAX_INTENSITY);
                delay(500);
                analogWrite(RED_PIN, 0);
                analogWrite(BLUE_PIN, 0);
                delay(500);
            }
            break;
        case 1: // Blue for Bluetooth
            for (int i = 0; i < 2; i++) {
                analogWrite(BLUE_PIN, MAX_INTENSITY);
                delay(500);
                analogWrite(BLUE_PIN, 0);
                delay(500);
            }
            break;
        case 2: // Green for Cellular
            for (int i = 0; i < 2; i++) {
                analogWrite(GREEN_PIN, MAX_INTENSITY);
                delay(500);
                analogWrite(GREEN_PIN, 0);
                delay(500);
            }
            break;
    }
}

// Function to handle button press
void buttonPressed() {
    currentMode = (currentMode + 1) % numModes; // Cycle through modes
}

// Main loop for lighting effects
void loop3() {
    switch (currentMode) {
        case 0:
            strobeEffect();
            break;
        case 1:
            // Additional modes...
            break;
        case 2:
            strobeEffectAlternate();
            break;
        case 3:
            purpleStrobeEffect();
            break;
        case 4:
            greenStrobeEffect();
            break;
        case 5:
            blueStrobeEffect();
            break;
        case 6:
            redStrobeEffect();
            break;
        case 7:
            rgbStrobeEffect();
            break;
        case 8: // New mode for network indication
            if (new_wifi_detected) {
                flashColor(0); // Flash purple for new Wi-Fi
            } else if (new_bluetooth_detected) {
                flashColor(1); // Flash blue for Bluetooth
            } else if (new_cellular_detected) {
                flashColor(2); // Flash green for cellular
            }
            break;
    }
}

// Additional functions (request_temperature, read_temperature, etc.)...

// Save MAC address
void save_mac(unsigned char* mac) {
    if (mac_history_cursor >= MAC_HISTORY_LEN) {
        mac_history_cursor = 0;
    }
    struct mac_addr tmp;
    for (int x = 0; x < 6; x++) {
        tmp.bytes[x] = mac[x];
    }
    mac_history[mac_history_cursor] = tmp;
    mac_history_cursor++;
}

// Check if MAC address has been seen
boolean seen_mac(unsigned char* mac) {
    struct mac_addr tmp;
    for (int x = 0; x < 6; x++) {
        tmp.bytes[x] = mac[x];
    }
    for (int x = 0; x < MAC_HISTORY_LEN; x++) {
        if (mac_cmp(tmp, mac_history[x])) {
            return true;
        }
    }
    return false;
}

// Compare two MAC addresses
boolean mac_cmp(struct mac_addr addr1, struct mac_addr addr2) {
    for (int y = 0; y < 6; y++) {
        if (addr1.bytes[y] != addr2.bytes[y]) {
            return false;
        }
    }
    return true;
}

// Clear MAC history
void clear_mac_history() {
    struct mac_addr tmp;
    for (int x = 0; x < 6; x++) {
        tmp.bytes[x] = 0;
    }
    for (int x = 0; x < MAC_HISTORY_LEN; x++) {
        mac_history[x] = tmp;
    }
    mac_history_cursor = 0;
}

// Setup ID pins
void setup_id_pins() {
    pinMode(13, INPUT_PULLUP); // IO13 is A/B identifier pin
    pinMode(25, INPUT_PULLDOWN); // All other pins are board identifiers
    pinMode(26, INPUT_PULLDOWN);
    pinMode(32, INPUT_PULLDOWN);
    pinMode(33, INPUT_PULLDOWN);
}

// Read ID pins
byte read_id_pins() {
    byte board_id = 0;
    board_id = digitalRead(25); // shift bits to get a board ID
    board_id = (board_id << 1) + digitalRead(26);
    board_id = (board_id << 1) + digitalRead(32);
    board_id = (board_id << 1) + digitalRead(33);
    return board_id;
}

// Main loop
void loop() {
    // Call loop3 for lighting effects
    loop3();
    // Additional logic for handling network scanning and BLE...
}

// Define the strobe effect functions
void strobeEffect() {
    // Simple strobe effect: flash red
    for (int i = 0; i < 5; i++) {
        analogWrite(RED_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(RED_PIN, 0);
        delay(100);
    }
}

void strobeEffectAlternate() {
    // Alternate strobe effect: flash green and blue
    for (int i = 0; i < 5; i++) {
        analogWrite(GREEN_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(GREEN_PIN, 0);
        analogWrite(BLUE_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(BLUE_PIN, 0);
    }
}

void purpleStrobeEffect() {
    // Flash purple (red + blue)
    for (int i = 0; i < 5; i++) {
        analogWrite(RED_PIN, MAX_INTENSITY);
        analogWrite(BLUE_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(RED_PIN, 0);
        analogWrite(BLUE_PIN, 0);
        delay(100);
    }
}

void greenStrobeEffect() {
    // Flash green
    for (int i = 0; i < 5; i++) {
        analogWrite(GREEN_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(GREEN_PIN, 0);
        delay(100);
    }
}

void blueStrobeEffect() {
    // Flash blue
    for (int i = 0; i < 5; i++) {
        analogWrite(BLUE_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(BLUE_PIN, 0);
        delay(100);
    }
}

void redStrobeEffect() {
    // Flash red
    for (int i = 0; i < 5; i++) {
        analogWrite(RED_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(RED_PIN, 0);
        delay(100);
    }
}

void rgbStrobeEffect() {
    // Flash RGB in sequence
    for (int i = 0; i < 5; i++) {
        analogWrite(RED_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(RED_PIN, 0);
        analogWrite(GREEN_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(GREEN_PIN, 0);
        analogWrite(BLUE_PIN, MAX_INTENSITY);
        delay(100);
        analogWrite(BLUE_PIN, 0);
    }
}
