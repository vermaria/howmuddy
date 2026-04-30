#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h> // Include this for "nicknames"

const char* ssid = "MIT";
const char* password = "p$QfHpUP1d";

// We will find this IP dynamically using the name "table"
// IPAddress tableIP; 
IPAddress tableIP(10, 29, 197, 36);
unsigned int localPort = 2390;
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Chair connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nChair Node Connected!");

  // Look for the Table Node's nickname "table"
  Serial.print("Targeting Table at: ");
  Serial.println(tableIP);
  
  // while (tableIP.toString() == "0.0.0.0") {
  //   tableIP = MDNS.queryHost("table");
  //   delay(1000);
  //   Serial.print("?");
  // }
  Serial.print("Found Table at: ");
  Serial.println(tableIP);
}

void loop() {
  int fsrValue = analogRead(A0);
  
  udp.beginPacket(tableIP, localPort);
  udp.printf("%d", fsrValue);
  udp.endPacket();

  Serial.printf("Sent Force Value: %d to %s\n", fsrValue, tableIP.toString().c_str());
  delay(100); 
}
