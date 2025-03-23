#include <SPI.h>

// SPI pins for ESP32
#define MISO_PIN 19
#define MOSI_PIN 23
#define SCK_PIN  18
#define CS_PIN   5

// Match IWR6432 configuration
#define BUFFER_SIZE 64  // Increased buffer size to hold more data per transfer
#define SPI_CLOCK   1000000

// Data buffers
uint8_t rxBuffer[BUFFER_SIZE];
uint8_t txBuffer[BUFFER_SIZE];

// SPI transaction structure for slave mode
SPIClass * vspi = NULL;
volatile bool transferComplete = false;

// Statistics tracking
uint32_t totalTransfers = 0;
uint32_t maxTransfersPerSecond = 0;
uint32_t lastSecondTransfers = 0;

// Buffer for storing transfer records
#define MAX_TRANSFERS 100  // Increased to handle more transfers per second
typedef struct {
  uint8_t rx[BUFFER_SIZE];
  uint8_t tx[BUFFER_SIZE];
  uint32_t timestamp;
  bool valid;
} TransferRecord;

TransferRecord transfers[MAX_TRANSFERS];
volatile int transferIndex = 0;
uint32_t lastUartUpdate = 0;
const uint32_t UART_UPDATE_INTERVAL = 1000; // 1 second interval

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  
  // Configure SPI slave
  vspi = new SPIClass(VSPI);
  vspi->begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  
  // Set as slave
  pinMode(MISO_PIN, OUTPUT);
  pinMode(MOSI_PIN, INPUT);
  pinMode(SCK_PIN, INPUT);
  pinMode(CS_PIN, INPUT);
  
  // Match MCSPI_FF_POL0_PHA0 from IWR6432
  vspi->setDataMode(SPI_MODE0);
  
  // Initialize the TX buffer with a pattern
  for (int i = 0; i < BUFFER_SIZE; i++) {
    txBuffer[i] = 0xAA + i;  // Different pattern to distinguish from master
  }
  
  // Initialize transfer records
  for (int i = 0; i < MAX_TRANSFERS; i++) {
    transfers[i].valid = false;
  }
  
  // Attach an interrupt to the CS pin to detect the start of transactions
  attachInterrupt(digitalPinToInterrupt(CS_PIN), csInterrupt, FALLING);
  
  Serial.println("\nESP32 SPI Slave Initialized");
  Serial.println("Configuration:");
  Serial.printf("- Clock: %d Hz\n", SPI_CLOCK);
  Serial.println("- Mode: 0 (POL0/PHA0)");
  Serial.println("- Data Size: 8 bits");
  Serial.println("- CS: Active LOW");
  Serial.println("- UART updates: Every 1 second");
  Serial.println("- Buffer size: " + String(BUFFER_SIZE) + " bytes per transfer");
  Serial.println("- Max transfers: " + String(MAX_TRANSFERS) + " per update interval");
  Serial.println("Waiting for IWR6432 master...");
  
  lastUartUpdate = millis();
}

// Interrupt handler for CS pin
void csInterrupt() {
  transferComplete = false;
}

void processTransfer() {
  // Check if CS is active (LOW)
  if (digitalRead(CS_PIN) == LOW && !transferComplete) {
    // Record start time for performance tracking
    uint32_t transferStartTime = micros();
    // Clear RX buffer
    memset(rxBuffer, 0, BUFFER_SIZE);
    
    // Wait for slave transaction to complete
    digitalWrite(MISO_PIN, LOW);  // Set MISO low before transaction
    
    // Prepare for the transaction
    vspi->beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE0));
    
    // Exchange data
    for (int i = 0; i < BUFFER_SIZE; i++) {
      rxBuffer[i] = vspi->transfer(txBuffer[i]);
    }
    
    vspi->endTransaction();
    
    // Set flag to avoid multiple reads on same CS assertion
    transferComplete = true;
    
    // Calculate transfer duration
    uint32_t transferDuration = micros() - transferStartTime;
    
    // Store the transfer in our record array
    TransferRecord* record = &transfers[transferIndex];
    
    // Copy RX and TX data
    memcpy(record->rx, rxBuffer, BUFFER_SIZE);
    memcpy(record->tx, txBuffer, BUFFER_SIZE);
    record->timestamp = millis();
    record->valid = true;
    
    // Log transfer stats every 10 transfers
    static int logCounter = 0;
    if (++logCounter % 10 == 0) {
      Serial.printf("SPI Transfer #%d: %u µs\n", logCounter, transferDuration);
    }
    
    // Move to next transfer slot
    transferIndex = (transferIndex + 1) % MAX_TRANSFERS;
    
    // Update statistics
    totalTransfers++;
    lastSecondTransfers++;
    
    // Update TX data for next transfer
    for (int i = 0; i < BUFFER_SIZE; i++) {
      txBuffer[i] = (txBuffer[i] + 1) & 0xFF;  // Increment and wrap around
    }
  }
}

void updateUart() {
  uint32_t currentTime = millis();
  
  // Check if it's time to update the UART
  if (currentTime - lastUartUpdate >= UART_UPDATE_INTERVAL) {
    // Print a header for the batch of transfers
    Serial.printf("\n==== SPI Transfers at %lu ms ====\n", currentTime);
    
    // Print all valid transfers
    int count = 0;
    for (int i = 0; i < MAX_TRANSFERS; i++) {
      if (transfers[i].valid) {
        Serial.printf("\n[%lu ms] SPI Transfer #%d:\n", transfers[i].timestamp, count+1);
        
        Serial.print("RX Data: ");
        // Only print the first 16 bytes to keep the output readable, unless there's actual data beyond
        int bytesToShow = BUFFER_SIZE;
        bool hasNonZeroLaterBytes = false;
        
        // Check if there's any non-zero data after the first 16 bytes
        for (int j = 16; j < BUFFER_SIZE; j++) {
          if (transfers[i].rx[j] != 0) {
            hasNonZeroLaterBytes = true;
            break;
          }
        }
        
        // Show first 16 bytes
        for (int j = 0; j < min(16, BUFFER_SIZE); j++) {
          Serial.printf("0x%02X ", transfers[i].rx[j]);
        }
        
        // If there are more non-zero bytes, print an ellipsis and the count
        if (hasNonZeroLaterBytes) {
          Serial.printf("... (%d more bytes)", BUFFER_SIZE - 16);
        }
        Serial.println();
        
        Serial.print("TX Data: ");
        // Same approach for TX data
        hasNonZeroLaterBytes = false;
        for (int j = 16; j < BUFFER_SIZE; j++) {
          if (transfers[i].tx[j] != 0) {
            hasNonZeroLaterBytes = true;
            break;
          }
        }
        
        for (int j = 0; j < min(16, BUFFER_SIZE); j++) {
          Serial.printf("0x%02X ", transfers[i].tx[j]);
        }
        
        if (hasNonZeroLaterBytes) {
          Serial.printf("... (%d more bytes)", BUFFER_SIZE - 16);
        }
        Serial.println();
        
        count++;
        
        // Mark as processed
        transfers[i].valid = false;
      }
    }
    
    // Update max transfers per second if needed
    if (lastSecondTransfers > maxTransfersPerSecond) {
      maxTransfersPerSecond = lastSecondTransfers;
    }
    
    Serial.printf("\n%d transfers processed in the last %d ms\n", 
                 count, UART_UPDATE_INTERVAL);
    Serial.printf("Total transfers: %u | This second: %u | Max per second: %u\n",
                 totalTransfers, lastSecondTransfers, maxTransfersPerSecond);
    
    // Reset counter for next interval
    lastSecondTransfers = 0;
    Serial.println("----------------------");
    
    // Update the last UART update time
    lastUartUpdate = currentTime;
  }
}

void loop() {
  // Process any SPI transfers
  processTransfer();
  
  // Update UART at regular intervals
  updateUart();
  
  // Small yield to prevent CPU hogging
  yield();
}