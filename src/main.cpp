#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <TFT_eSPI.h> 
#include "bitmaps.h"
#include "esp_now.h"

// Stuff for debug environment (will enable or disable Serial usage
// depending on BUILD_TYPE set on the diferent platformio.ini envs)
#define SERIAL_PRINTER Serial
#ifndef BUILD_TYPE
  #define BUILD_TYPE 0
#endif

#if BUILD_TYPE==1
  #define DEBUG_PRINTF(...) { SERIAL_PRINTER.printf(__VA_ARGS__); }
  #define INFO_PRINTF(...) { SERIAL_PRINTER.printf(__VA_ARGS__); }
  #define DELAY_MULTIPLIER 500
#else
  #define DEBUG_PRINTF(...) {}
  #define DELAY_MULTIPLIER 1000
  #if BUILD_TYPE==0
    #define INFO_PRINTF(...) {}
  #else
    #define INFO_PRINTF(...) { SERIAL_PRINTER.printf(__VA_ARGS__); }
  #endif
#endif

#define CONFIG_ESPNOW_ENABLE_LONG_RANGE
uint8_t ackBuffer[2] = {0, INT8_MAX};
const uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t currentTestId = 0;
uint16_t lastPacketNumber;
uint16_t totalPacketsReceived;
uint16_t messyPacketsReceived;
uint16_t currentTestTotalPackets;

TFT_eSPI    display = TFT_eSPI();

TaskHandle_t currentDisplayTask = NULL;
QueueHandle_t i2cMutex = xSemaphoreCreateMutex();

/* -------------------------- Function declaration -------------------------- */

void initEspNow();
void wifiWatchdogTask(void * params);

void displayCurrentTestResults(bool ended);
void displayInitializingTask(void * params);
void displayNTPUpdatingTask(void * params);
void displayBleDevices(void * params);
void displayTestResultsTask(void * params);
void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int dataLen);

/* -------------------------- Application -------------------------- */

void setup() {
  // Serial port for debugging purposes
  #if BUILD_TYPE != 0
  SERIAL_PRINTER.begin(115200);
  DEBUG_PRINTF("\n\n######### Running in dev mode! #########\n");
  delay(1000);
  #endif

  display.init();
  display.setRotation(2);
  display.fillScreen(ST7735_BLACK);
  display.setTextFont(2);

  xTaskCreate(displayInitializingTask, "display init", 1000, NULL, 1, &currentDisplayTask);

  initEspNow();
  vTaskDelete(NULL);
}

void loop() {
  DEBUG_PRINTF("THIS SHOULD NOT HAPPEN");
}

/* -------------------------- Function definitions -------------------------- */

void initEspNow() {
  DEBUG_PRINTF("\n\nWIFI connecting to: %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
#ifdef CONFIG_ESPNOW_ENABLE_LONG_RANGE
  WiFi.enableLongRange(true);
#endif

  esp_err_t initResult = esp_now_init();
  if (initResult == ESP_OK) {
    DEBUG_PRINTF("\nEsp-Now initialized. Host address: %s\n", WiFi.macAddress().c_str());
    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    display.setTextColor(ST7735_WHITE, ST7735_BLACK); display.drawString("Waiting for", 32, 16, 2);display.drawString("senders", 46, 32, 2);
    xSemaphoreGive(i2cMutex);
  }
  else
  {
    DEBUG_PRINTF("ESPNow Init failed: %s\n", esp_err_to_name(initResult));
    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    display.setTextColor(ST7735_RED, ST7735_BLACK); display.drawString("Error initializing ESP-Now", 0, 16, 2);display.drawString(esp_err_to_name(initResult), 24, 32, 2);
    vTaskDelete(currentDisplayTask);
    xSemaphoreGive(i2cMutex);
    return;
  }
  
  //esp_now_set(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onDataReceived);

  // // this will broadcast a message to everyone in range
  // esp_now_peer_info_t peerInfo = {};
  // memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  // if (!esp_now_is_peer_exist(broadcastAddress))
  // {
  //   result = esp_now_add_peer(&peerInfo);
  //   if (result != ESP_OK)
  //   {
  //     Serial.printf("Failed to add broadcast peer: %s\n", esp_err_to_name(result));
  //     return false;
  //   }
  // }
}

void wifiWatchdogTask(void * params) {
  while(true) {
    if(WiFi.status() == WL_CONNECTED) {
      vTaskDelay(10000/portTICK_PERIOD_MS);
      continue;
    }

    WiFi.disconnect(true);
    initEspNow();

    if(WiFi.status() != WL_CONNECTED) vTaskDelay(20000/portTICK_PERIOD_MS);
  }
}

void displayInitializingTask(void * params) {
  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  display.setCursor(0,0); display.fillScreen(ST7735_BLACK); display.setTextSize(1); display.setTextColor(ST7735_WHITE, ST7735_BLACK); display.drawString("Initializing", 32, 16, 2);display.drawString("host", 50, 32, 2);
  xSemaphoreGive(i2cMutex);
  uint8_t frame = 0;
  while(true) {
    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    // Clear previous frame
    if(frame == 0) display.drawBitmap((display.width()-ANIMATION_WIDTH)/2, display.height()-ANIMATION_HEIGHT-5, bolas[sizeof(bolas)/sizeof(bolas[0]) -1], ANIMATION_WIDTH, ANIMATION_HEIGHT, ST7735_BLACK); 
    else display.drawBitmap((display.width()-ANIMATION_WIDTH)/2, display.height()-ANIMATION_HEIGHT-5, bolas[frame-1], ANIMATION_WIDTH, ANIMATION_HEIGHT, ST7735_BLACK);
    // Paint frame
    display.drawBitmap((display.width()-ANIMATION_WIDTH)/2, display.height()-ANIMATION_HEIGHT-5, bolas[frame], ANIMATION_WIDTH, ANIMATION_HEIGHT, ST7735_CYAN);
    xSemaphoreGive(i2cMutex);
    frame++; if(frame == sizeof(bolas)/sizeof(bolas[0])) frame = 0;
    vTaskDelay(45/portTICK_PERIOD_MS);
  }
}

void onDataReceived(const uint8_t *macAddr, const uint8_t *data, int dataLen)
{
  uint16_t command = reinterpret_cast<const uint16_t *>(data)[0];
  uint16_t packetNumber = reinterpret_cast<const uint16_t *>(data)[1];
  //DEBUG_PRINTF("EspNow received %d, %d\n", command, packetNumber);
  
  // Coordination to start test
  switch (command)
  {
  case 0: // Start new test
    if (!esp_now_is_peer_exist(macAddr))
    {
      esp_now_peer_info_t peerInfo = {};
      memcpy(&peerInfo.peer_addr, macAddr, 6);
      esp_now_add_peer(&peerInfo);
    }
    DEBUG_PRINTF("\tTest start packet incoming packets: %d\n", packetNumber);
    if(totalPacketsReceived > 0) { currentTestId++; }
    lastPacketNumber = 0;
    totalPacketsReceived = 0;
    messyPacketsReceived = 0;
    currentTestTotalPackets = packetNumber;
    if(currentTestId == 0) {
      xSemaphoreTake(i2cMutex, portMAX_DELAY);
      vTaskDelete(currentDisplayTask);
      display.fillScreen(ST7735_BLACK); display.fillRoundRect(2, 2, display.width()-4, 28, 6, 0x0050); display.setTextColor(ST7735_YELLOW); display.drawString("Ranger", 20, 5, 4);
      display.setCursor(0, 32);
      display.println("# End  ReceptRate");
      display.drawWideLine(0, display.getCursorY()+2, display.width(), display.getCursorY()+2, 2, TFT_GREENYELLOW, ST7735_BLACK);
      xSemaphoreGive(i2cMutex);
      xTaskCreate(displayTestResultsTask, "display tests", 2000, NULL, 1, &currentDisplayTask);
    }
    // Reply!!
    ackBuffer[0] = 0;
    DEBUG_PRINTF("  Sending ack %s\n", esp_err_to_name(esp_now_send(macAddr, ackBuffer, 4)));
  case 1: // Message for current test
    if (packetNumber < lastPacketNumber)
    {
      messyPacketsReceived++;
    }
    else
    {
      lastPacketNumber = packetNumber;
    }
    totalPacketsReceived++;
    //DEBUG_PRINTF("\tTest packet %d, total: %d, messy: %d\n", packetNumber, totalPacketsReceived, messyPacketsReceived);
    break;
    case 2: // End test
    // Reply!!
    DEBUG_PRINTF("\tTest end, useless payload %d\n", packetNumber);
    displayCurrentTestResults(true);
    totalPacketsReceived = 0;
    currentTestId++;
    if(currentTestId > 7) currentTestId = 0;
    ackBuffer[0] = 2;
    esp_now_send(macAddr, ackBuffer, 2);
    break;
  default:
    break;
  }

}

void displayTestResultsTask(void * params) {
  while (true) {
    if(totalPacketsReceived > 0) {
      displayCurrentTestResults(false);
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
}

void displayCurrentTestResults(bool ended) {
  xSemaphoreTake(i2cMutex, portMAX_DELAY);
  display.setCursor(0, 54+ (currentTestId*14));
  display.setTextColor(ST7735_WHITE, TFT_BLACK);
  display.printf("%d %s   %.0f  ", currentTestId, ended?"Yes":"No ", 100.00*totalPacketsReceived/(float)currentTestTotalPackets);
  xSemaphoreGive(i2cMutex);
}