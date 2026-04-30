#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

const char* ssid = "MIT";
const char* password = "p$QfHpUP1d";
const char* serverApi = "http://10.31.180.240:5000/api/report";

WiFiUDP udp;
unsigned int localPort = 2390;
char packetBuffer[255];

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTable Node Connected!");

  if (!MDNS.begin("table")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started: table.local");
    Serial.print("Local IP Address: ");
    Serial.println(WiFi.localIP());
  }

  udp.begin(localPort);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;
    
    String fsrValue = String(packetBuffer);
    int adc_raw = fsrValue.toInt();
    
    // Note: ESP32 WiFiUDP doesn't have a direct .rssi() method for packets
    // We will use the device's current RSSI as a proxy
    int currentRssi = WiFi.RSSI(); 
    bool occupied = (adc_raw > 1000); 
    
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverApi);
      http.addHeader("Content-Type", "application/json");

      // Constructing the complex JSON payload
      String jsonPayload = "{";
      jsonPayload += "\"table_id\": \"T1\",";
      jsonPayload += "\"chairs\": [";
      jsonPayload += "  {";
      jsonPayload += "    \"chair_id\": \"T1C1\",";
      jsonPayload += "    \"occupied\": " + String(occupied ? "true" : "false") + ",";
      jsonPayload += "    \"rssi\": " + String(currentRssi) + ",";
      jsonPayload += "    \"adc_raw\": " + String(adc_raw);
      jsonPayload += "  }";
      jsonPayload += "],";
      jsonPayload += "\"timestamp_ms\": " + String(millis()); 
      jsonPayload += "}";

      int httpResponseCode = http.POST(jsonPayload);
      
      if (httpResponseCode > 0) {
        Serial.printf("Sent! Response: %d\n", httpResponseCode);
      } else {
        Serial.printf("Error: %s\n", http.errorToString(httpResponseCode).c_str());
      }
      http.end();
    }
  }
} // End of loop
