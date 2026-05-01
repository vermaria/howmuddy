#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "config.h"

// Define the packets (must match Table Gateway)
struct TableBeacon { 
  uint8_t table_id; 
  uint8_t mac[6]; 
  uint8_t channel;
};

struct OccupancyPacket {
  char chair_id[8]; 
  uint8_t table_id; 
  bool occupied;
  uint16_t adc_raw; 
  int8_t rssi; 
  uint32_t uptime_ms; 
  uint8_t battery_pct;
};

// State for localization
uint8_t targetTableMac[6];
bool foundTable = false;
int8_t lastRssi = -100;
uint8_t currentChannel = 1; // Default starting channel

// Receive beacons from the Table
void onBeaconRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // measure RSSI 
  lastRssi = info->rx_ctrl->rssi; 

  if (!foundTable) {
    memcpy(targetTableMac, info->src_addr, 6);
    foundTable = true;

    // Serial.println(currentChannel);
    // hard coding channel - might need to update depending on channel found, need to figure this out still
    // for dynamic searching, change all references to '11' to 'currentChannel' instead
    esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE); 
    // Serial.println("Radio locked to Channel 11. Searching for beacons...");
    
    // Add the table as a peer once discovered
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, targetTableMac, 6);
    peer.channel = 11; 
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed, trying again");
    ESP.restart();
  } 
  Serial.println("ESP-NOW init success!");

  esp_now_register_recv_cb(onBeaconRecv);
  Serial.println("Searching for Table Beacons...");
}

void loop() {
  if (foundTable) {
    uint16_t adcVal = analogRead(A0);
    bool occupied = (adcVal > 1000); 
    
    OccupancyPacket pkt;
    strncpy(pkt.chair_id, CHAIR_ID, 7);
    pkt.table_id = TABLE_ID_NUM; 
    pkt.occupied = occupied;
    pkt.adc_raw = adcVal;
    pkt.rssi = lastRssi; // Relative distance measure 
    pkt.uptime_ms = millis();
    pkt.battery_pct = 100; // Placeholder for battery logic 
    
    esp_now_send(targetTableMac, (uint8_t*)&pkt, sizeof(pkt)); 
    Serial.printf("[Chair] RSSI: %d | ADC: %u | Occ: %s\n", 
                  lastRssi, adcVal, occupied ? "YES" : "NO");
  } else {
    // scan through channels if table isn't found after a while
    Serial.println("Searching for table...");
    static uint32_t lastScan = 0;
    if (millis() - lastScan > 2000) {
      currentChannel = (currentChannel % 11) + 1; // Cycle 1-11
      esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
      lastScan = millis();
    }
  }
  delay(1000); // 
}
