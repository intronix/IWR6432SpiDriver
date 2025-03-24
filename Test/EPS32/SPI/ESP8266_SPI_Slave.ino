
#include <Arduino.h>

// SPI pins for ESP8266
#define MOSI_PIN 13 // D7
#define MISO_PIN 12 // D6
#define SCK_PIN 14  // D5
#define CS_PIN 15   // D8

volatile uint8_t counter = 0;
volatile bool transfer_complete = false;
volatile uint8_t received_byte = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nESP8266 SPI Slave");

    // Configure pins
    pinMode(MOSI_PIN, INPUT);
    pinMode(MISO_PIN, OUTPUT);
    pinMode(SCK_PIN, INPUT);
    pinMode(CS_PIN, INPUT_PULLUP); // Use internal pullup

    digitalWrite(MISO_PIN, LOW);

    // Attach interrupt for CS pin
    attachInterrupt(digitalPinToInterrupt(CS_PIN), handleCS, FALLING);

    Serial.println("Pin Configuration:");
    Serial.printf("- MOSI: GPIO%d (D7)\n", MOSI_PIN);
    Serial.printf("- MISO: GPIO%d (D6)\n", MISO_PIN);
    Serial.printf("- SCK:  GPIO%d (D5)\n", SCK_PIN);
    Serial.printf("- CS:   GPIO%d (D8)\n", CS_PIN);
}

ICACHE_RAM_ATTR void handleCS()
{
    // Wait for CS to stabilize
    delayMicroseconds(1);

    if (digitalRead(CS_PIN) == LOW)
    {
        uint8_t rx_byte = 0;

        // Read 8 bits
        for (int i = 0; i < 8; i++)
        {
            while (digitalRead(SCK_PIN) == LOW)
                ; // Wait for clock rising edge
            rx_byte = (rx_byte << 1) | digitalRead(MOSI_PIN);
            while (digitalRead(SCK_PIN) == HIGH)
                ; // Wait for clock falling edge
        }

        received_byte = rx_byte;

        // If received command is 0x04, prepare response
        if (rx_byte == 0x04)
        {
            uint8_t response = counter++;

            // Send 8 bits
            for (int i = 7; i >= 0; i--)
            {
                digitalWrite(MISO_PIN, (response >> i) & 0x01);
                while (digitalRead(SCK_PIN) == LOW)
                    ; // Wait for clock rising edge
                while (digitalRead(SCK_PIN) == HIGH)
                    ; // Wait for clock falling edge
            }

            transfer_complete = true;
        }
    }
}

void loop()
{
    if (transfer_complete)
    {
        Serial.printf("Received command: 0x%02X, Responded with: 0x%02X\n",
                      received_byte, counter - 1);
        transfer_complete = false;
    }
    yield(); // Allow ESP8266 background tasks to run
    delay(1);
}
