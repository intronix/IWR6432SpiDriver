/*
 * ESP8266 NodeMCU - SPI Master
 * Communicates with another ESP8266 acting as SPI slave
 */

#include <SPI.h>

// SPI pins for ESP8266 NodeMCU
#define MOSI_PIN D7  // GPIO13
#define MISO_PIN D6  // GPIO12
#define SCK_PIN  D5  // GPIO14
#define CS_PIN   D8  // GPIO15

// Handshake pins
#define MASTER_READY_PIN D4  // Used to tell slave master is ready
#define SLAVE_READY_PIN  D3  // Used to check if slave is ready

// Communication settings
#define BUFFER_SIZE 32
#define SPI_CLOCK 1000000  // 1MHz

// Data buffers
uint8_t txBuffer[BUFFER_SIZE];
uint8_t rxBuffer[BUFFER_SIZE];

// Communication statistics
uint32_t totalTransfers = 0;
uint32_t lastTransferTime = 0;
const uint32_t TRANSFER_INTERVAL = 1000;  // Transfer every 1 second

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nESP8266 SPI Master Initialized");
  
  // Configure SPI pins
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);  // CS is active LOW
  
  // Configure handshake pins
  pinMode(MASTER_READY_PIN, OUTPUT);
  digitalWrite(MASTER_READY_PIN, LOW);  // Not ready initially
  
  pinMode(SLAVE_READY_PIN, INPUT);
  
  // Initialize the TX buffer with a pattern
  for (int i = 0; i < BUFFER_SIZE; i++) {
    txBuffer[i] = 0x50 + i;  // Pattern for master
  }
  
  // Initialize SPI in master mode
  SPI.begin();
  SPI.setFrequency(SPI_CLOCK);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  
  Serial.println("Configuration:");
  Serial.printf("- Clock: %d Hz\n", SPI_CLOCK);
  Serial.println("- Mode: 0 (POL0/PHA0)");
  Serial.println("- Buffer size: " + String(BUFFER_SIZE) + " bytes");
  Serial.println("Ready to transfer data to slave...");
}

void loop() {
  uint32_t currentTime = millis();
  
  // Check if it's time to initiate a transfer
  if (currentTime - lastTransferTime >= TRANSFER_INTERVAL) {
    // Signal that master is ready to send
    digitalWrite(MASTER_READY_PIN, HIGH);
    
    // Wait for slave to be ready
    Serial.println("Waiting for slave to be ready...");
    uint32_t timeout = millis() + 1000;  // 1 second timeout
    bool slaveReady = false;
    
    while (millis() < timeout) {
      if (digitalRead(SLAVE_READY_PIN) == HIGH) {
        slaveReady = true;
        break;
      }
      delay(1);
    }
    
    if (slaveReady) {
      // Perform SPI transaction
      Serial.print("Sending: ");
      for (int i = 0; i < min(8, BUFFER_SIZE); i++) {
        Serial.printf("0x%02X ", txBuffer[i]);
      }
      if (BUFFER_SIZE > 8) Serial.print("...");
      Serial.println();
      
      // Start transaction
      digitalWrite(CS_PIN, LOW);
      delayMicroseconds(10);  // Short delay to ensure CS is stable
      
      // Transfer data
      for (int i = 0; i < BUFFER_SIZE; i++) {
        rxBuffer[i] = SPI.transfer(txBuffer[i]);
      }
      
      // End transaction
      delayMicroseconds(10);  // Short delay to ensure all data is transferred
      digitalWrite(CS_PIN, HIGH);
      
      // Print received data
      Serial.print("Received: ");
      for (int i = 0; i < min(8, BUFFER_SIZE); i++) {
        Serial.printf("0x%02X ", rxBuffer[i]);
      }
      if (BUFFER_SIZE > 8) Serial.print("...");
      Serial.println();
      
      // Update TX data for next transfer
      for (int i = 0; i < BUFFER_SIZE; i++) {
        txBuffer[i] = (txBuffer[i] + 1) % 256;
      }
      
      totalTransfers++;
      Serial.printf("Transfer #%u completed\n\n", totalTransfers);
    } else {
      Serial.println("Timeout waiting for slave!");
    }
    
    // Reset master ready signal
    digitalWrite(MASTER_READY_PIN, LOW);
    lastTransferTime = currentTime;
  }
  
  // Small delay to prevent CPU hogging
  delay(10);
}