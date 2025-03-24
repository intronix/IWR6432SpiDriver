
#include <Arduino.h>

// SPI pins for ESP8266
#define MOSI_PIN 13 // D7
#define MISO_PIN 12 // D6
#define SCK_PIN 14  // D5
#define CS_PIN 15   // D8

// Commands
#define CMD_REQUEST_TRANSFER 0x01
#define CMD_TRANSFER_DATA 0x02
#define CMD_CHECK_STATUS 0x03

// Status codes
#define STATUS_READY 0x10
#define STATUS_BUSY 0x11
#define STATUS_ERROR 0x12

#define BUFFER_SIZE 4096 // 1KB buffer

volatile bool transfer_complete = false;
volatile uint8_t received_byte = 0;
volatile uint8_t current_status = STATUS_READY;
volatile uint16_t data_index = 0;
uint8_t dataBuffer[BUFFER_SIZE];
uint16_t receivedCRC = 0;

// CRC16 calculation function
uint16_t calculateCRC16(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

// Function to send response byte
void sendResponse(uint8_t response)
{
    for (int i = 7; i >= 0; i--)
    {
        digitalWrite(MISO_PIN, (response >> i) & 0x01);
        while (digitalRead(SCK_PIN) == LOW)
            ; // Wait for clock rising edge
        while (digitalRead(SCK_PIN) == HIGH)
            ; // Wait for clock falling edge
    }
}

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

        switch (rx_byte)
        {
        case CMD_REQUEST_TRANSFER:
            current_status = STATUS_READY;
            data_index = 0;
            sendResponse(STATUS_READY);
            break;

        case CMD_CHECK_STATUS:
            sendResponse(current_status);
            break;

        case CMD_TRANSFER_DATA:
            if (data_index < BUFFER_SIZE)
            {
                // Read data byte
                rx_byte = 0;
                for (int i = 0; i < 8; i++)
                {
                    while (digitalRead(SCK_PIN) == LOW)
                        ; // Wait for clock rising edge
                    rx_byte = (rx_byte << 1) | digitalRead(MOSI_PIN);
                    while (digitalRead(SCK_PIN) == HIGH)
                        ; // Wait for clock falling edge
                }
                dataBuffer[data_index++] = rx_byte;

                if (data_index == BUFFER_SIZE)
                {
                    // Read CRC (2 bytes)
                    uint16_t receivedCRC = 0;
                    for (int byte = 0; byte < 2; byte++)
                    {
                        rx_byte = 0;
                        for (int i = 0; i < 8; i++)
                        {
                            while (digitalRead(SCK_PIN) == LOW)
                                ; // Wait for clock rising edge
                            rx_byte = (rx_byte << 1) | digitalRead(MOSI_PIN);
                            while (digitalRead(SCK_PIN) == HIGH)
                                ; // Wait for clock falling edge
                        }
                        receivedCRC = (receivedCRC << 8) | rx_byte;
                    }

                    // Verify CRC
                    uint16_t calculatedCRC = calculateCRC16(dataBuffer, BUFFER_SIZE);
                    current_status = (receivedCRC == calculatedCRC) ? STATUS_READY : STATUS_ERROR;
                    transfer_complete = true;
                }
            }
            break;
        }
    }
}

void loop()
{
    if (transfer_complete)
    {
        Serial.printf("Transfer complete. Status: 0x%02X\n", current_status);
        if (current_status == STATUS_READY)
        {
            Serial.println("Data transfer successful!");
            Serial.printf("Received %d bytes\n", data_index);
            Serial.printf("CRC: 0x%04X\n", calculateCRC16(dataBuffer, BUFFER_SIZE));

            // Print first few bytes for verification
            Serial.println("First 16 bytes received:");
            for (int i = 0; i < 16; i++)
            {
                Serial.printf("0x%02X ", dataBuffer[i]);
                if ((i + 1) % 8 == 0)
                    Serial.println();
            }
        }
        else
        {
            Serial.println("Data transfer failed - CRC mismatch!");
        }
        transfer_complete = false;
    }
    yield(); // Allow ESP8266 background tasks to run
    delay(1);
}
