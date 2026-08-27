#include "sd_card.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>

namespace {
  bool mounted = false;
  char typeStr[16] = "None";
  float sizeGB = 0.0f;

  // Store up to 20 filenames from root directory
  static const int MAX_FILES = 20;
  char fileNames[MAX_FILES][32];
  int numFiles = 0;

  void scanRootFiles() {
    numFiles = 0;
    File root = SD.open("/");
    if (!root || !root.isDirectory()) return;

    while (numFiles < MAX_FILES) {
      File entry = root.openNextFile();
      if (!entry) break;
      // Skip hidden files and directories
      const char *name = entry.name();
      if (name[0] != '.') {
        strncpy(fileNames[numFiles], name, 31);
        fileNames[numFiles][31] = '\0';
        numFiles++;
      }
      entry.close();
    }
    root.close();
  }

  bool mount() {
    // Use the shared SPI bus (already initialized by main sketch)
    if (!SD.begin(SD_CS, SPI)) {
      mounted = false;
      strcpy(typeStr, "None");
      sizeGB = 0.0f;
      numFiles = 0;
      return false;
    }

    mounted = true;

    // Identify card type
    uint8_t ct = SD.cardType();
    switch (ct) {
      case CARD_MMC:  strcpy(typeStr, "MMC");  break;
      case CARD_SD:   strcpy(typeStr, "SD");   break;
      case CARD_SDHC: strcpy(typeStr, "SDHC"); break;
      default:        strcpy(typeStr, "Unknown"); break;
    }

    // Card size in GB
    uint64_t bytes = SD.cardSize();
    sizeGB = (float)(bytes / (1024ULL * 1024ULL)) / 1024.0f;

    // List root directory
    scanRootFiles();

    // ----------------------------------------------------
    // SD Card Write Integrity Self-Test
    // ----------------------------------------------------
    Serial.println("Running SD Card Write Integrity Test...");
    const char* testPath = "/test_integrity.dat";
    if (SD.exists(testPath)) {
      SD.remove(testPath);
    }
    
    File testFile = SD.open(testPath, FILE_WRITE);
    if (!testFile) {
      Serial.println("  [FAIL] Could not open test file for writing.");
    } else {
      const char* payload = "BMO Gameboy SD Integrity Payload";
      size_t payloadLen = strlen(payload);
      size_t written = testFile.write((const uint8_t*)payload, payloadLen);
      testFile.close();
      
      if (written != payloadLen) {
        Serial.println("  [FAIL] Partial write occurred.");
      } else {
        File readFile = SD.open(testPath, FILE_READ);
        if (!readFile) {
          Serial.println("  [FAIL] Could not open test file for reading.");
        } else {
          char readBuffer[64] = {0};
          size_t bytesRead = readFile.read((uint8_t*)readBuffer, payloadLen);
          readFile.close();
          
          if (bytesRead != payloadLen || strncmp(payload, readBuffer, payloadLen) != 0) {
            Serial.println("  [FAIL] Read-back data did not match written payload.");
          } else {
            Serial.println("  [PASS] Write/Read integrity verified successfully!");
          }
        }
      }
      SD.remove(testPath);
    }
    // ----------------------------------------------------

    return true;
  }
}

bool SDCard::begin() {
  return mount();
}

bool SDCard::rescan() {
  SD.end();
  delay(100);
  return mount();
}

bool SDCard::isMounted() {
  return mounted;
}

const char *SDCard::cardType() {
  return typeStr;
}

float SDCard::cardSizeGB() {
  return sizeGB;
}

int SDCard::fileCount() {
  return numFiles;
}

const char *SDCard::fileName(int index) {
  if (index < 0 || index >= numFiles) return "";
  return fileNames[index];
}
