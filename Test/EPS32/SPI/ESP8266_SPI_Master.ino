#include <SPI.h>

// HSPI Pins for ESP8266
#define MOSI 13 // D7
#define MISO 12 // D6
#define SCK 14  // D5
#define CS 15   // D8

// Commands
#define CMD_REQUEST_TRANSFER 0x01
#define CMD_TRANSFER_DATA 0x02
#define CMD_CHECK_STATUS 0x03

// Status codes
#define STATUS_READY 0x10
#define STATUS_BUSY 0x11
#define STATUS_ERROR 0x12

#define BUFFER_SIZE 4096 // 1KB buffer

uint8_t dataBuffer[BUFFER_SIZE];
uint16_t crc = 0;

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

// Function to poll slave status
uint8_t pollSlaveStatus()
{
    digitalWrite(CS, LOW);
    SPI.transfer(CMD_CHECK_STATUS);
    delayMicroseconds(100);
    uint8_t status = SPI.transfer(0xFF);
    digitalWrite(CS, HIGH);
    return status;
}

// Function to send data with CRC
bool sendDataWithCRC(uint8_t *data, uint16_t length)
{
    uint16_t crc = calculateCRC16(data, length);

    // Send data in chunks
    for (uint16_t i = 0; i < length; i++)
    {
        digitalWrite(CS, LOW);
        SPI.transfer(CMD_TRANSFER_DATA);
        SPI.transfer(data[i]);
        digitalWrite(CS, HIGH);
        delay(1); // Small delay between chunks
    }

    // Send CRC
    digitalWrite(CS, LOW);
    SPI.transfer((uint8_t)(crc >> 8));   // High byte
    SPI.transfer((uint8_t)(crc & 0xFF)); // Low byte
    digitalWrite(CS, HIGH);

    return true;
}

void setup()
{
    Serial.begin(115200);
    SPI.begin();
    SPI.setFrequency(100000); // Set SPI clock to 100 kHz
    SPI.setHwCs(false);
    delay(5000);
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);
}

void loop()
{
    // Generate test data
    for (uint16_t i = 0; i < BUFFER_SIZE; i++)
    {
        dataBuffer[i] = i & 0xFF;
    }

    // Request transfer
    digitalWrite(CS, LOW);
    SPI.transfer(CMD_REQUEST_TRANSFER);
    digitalWrite(CS, HIGH);

    // Poll for slave readiness
    uint8_t status;
    do
    {
        delay(100); // Wait before polling again
        status = pollSlaveStatus();
        Serial.printf("Slave status: 0x%02X\n", status);
    } while (status != STATUS_READY);

    // Send data when slave is ready
    if (status == STATUS_READY)
    {
        Serial.println("Starting 1KB data transfer...");

        if (sendDataWithCRC(dataBuffer, BUFFER_SIZE))
        {
            Serial.println("Data transfer completed");
            Serial.printf("CRC: 0x%04X\n", calculateCRC16(dataBuffer, BUFFER_SIZE));
        }
        else
        {
            Serial.println("Data transfer failed");
        }
    }

    delay(5000); // Wait before next transfer
}
