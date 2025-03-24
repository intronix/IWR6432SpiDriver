#include <SPI.h>

// HSPI Pins for ESP8266
#define MOSI 13 // D7
#define MISO 12 // D6
#define SCK 14  // D5
#define CS 15   // D8

void setup()
{
    Serial.begin(115200);
    SPI.begin();
    SPI.setFrequency(100000); // Set SPI clock to 100 kHz
    SPI.setHwCs(false);
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);
}

void loop()
{
    byte response;

    // Begin transmission
    digitalWrite(CS, LOW);

    // Send command byte
    SPI.transfer(0x04);

    // Small delay to allow slave to process
    delayMicroseconds(100);

    // Get response
    response = SPI.transfer(0xFF); // Send dummy byte to receive response

    // End transmission
    digitalWrite(CS, HIGH);

    Serial.print("Sent: 0x04, Received: 0x");
    Serial.println(response, HEX);

    delay(1000);
}