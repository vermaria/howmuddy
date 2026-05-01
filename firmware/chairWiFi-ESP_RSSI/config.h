#pragma once

// ─── Table Identity ───────────────────────────────────────────────────────
#define TABLE_ID      "T1"   // unique per gateway (T1, T2, … T10) 
#define TABLE_ID_NUM  1      // Numeric ID for ESP-NOW packet filtering

// ─── WiFi Credentials ─────────────────────────────────────────────────────
// The Table Node uses these to connect to the backend; the Chair stays offline.
#define WIFI_SSID     "MIT"          
#define WIFI_PASSWORD "p$QfHpUP1d"  

// ─── Backend API ──────────────────────────────────────────────────────────
#define API_HOST    "10.31.180.240"  // Your Mac's IP address 
#define API_PORT    5000             // Flask default port 
#define API_PATH    "/api/report"    
#define UPLOAD_INTERVAL_MS  5000     // Batch-send every 5 seconds 

// ─── RSSI-Based Proximity ─────────────────────────────────────────────────
#define RSSI_SMOOTHING_N    8        // Moving average samples 
#define RSSI_CLAIM_THRESHOLD -65     // dBm threshold to "claim" a chair
#define OCCUPIED_THRESHOLD_ADC 1000  // FSR value to trigger "occupied"

// ─── ESP-NOW ──────────────────────────────────────────────────────────────
// This channel must match the channel used by the MIT WiFi router. 
// The Table Node will print this to the Serial Monitor on boot.
#define ESPNOW_CHANNEL  1            
#define MAX_CHAIRS      16           

// ─── Chair Specifics ──────────────────────────────────────────────────────
#define CHAIR_ID "T1C1"              // Unique ID for each chair
#define REPORT_INTERVAL_MS 1000      // How often the chair sends a report

// ─── Debug ────────────────────────────────────────────────────────────────
#define SERIAL_BAUD   115200      
#define DEBUG_ENABLED true        