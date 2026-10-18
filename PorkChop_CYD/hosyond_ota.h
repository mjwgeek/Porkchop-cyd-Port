#pragma once
// Hosyond CYD40 - manual, confirmed OTA from the owner's fork ONLY.
// Requires the custom two-slot partition CSV installed with a FULL flash.
#ifdef HOSYOND_CYD40
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

#ifndef PORKCHOP_FW_BUILD
#define PORKCHOP_FW_BUILD 0
#endif

namespace PorkchopOta {
static constexpr const char* kManifest = "https://mjwgeek.github.io/Porkchop-cyd-Port/ota.json";
static constexpr const char* kFirmware = "https://mjwgeek.github.io/Porkchop-cyd-Port/flash/firmware.bin";
static constexpr const char* kAgent = "Porkchop-Hosyond-CYD40-OTA/1.0 (mjwgeek/Porkchop-cyd-Port)";
static bool available = false;
static uint32_t latestBuild = 0;
static size_t latestSize = 0;
static char latestSha[65] = {};
static char state[76] = "TAP CHECK TO SEARCH FOR UPDATES";
static bool busy = false;
inline bool isAvailable() { return available; }
inline const char* getStatus() { return state; }

inline void status(const char* msg) {
  snprintf(state, sizeof(state), "%s", msg);
  Serial.printf("[OTA] %s\n", state);
}
inline bool compatible() {
  return esp_ota_get_next_update_partition(nullptr) != nullptr;
}
inline void paint(const char* line, uint8_t pct = 255) {
  tft.fillRect(8, TOP_BAR_H + 218, DISPLAY_W - 16, 48, colorBG());
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(colorFG(), colorBG());
  tft.drawString(line, 12, TOP_BAR_H + 223);
  if (pct <= 100) {
    tft.drawRect(12, TOP_BAR_H + 244, DISPLAY_W - 24, 8, colorFG());
    tft.fillRect(14, TOP_BAR_H + 246, (DISPLAY_W - 28) * pct / 100, 4, colorFG());
  }
}
inline bool connect() {
  if (WebUI::isActive()) { status("STOP WEB REMOTE FIRST"); return false; }
  if (!cfg.otaWifiSsid[0]) { status("SET WIFI SSID AND PASSWORD FIRST"); return false; }
  if (!compatible()) { status("FULL USB FLASH NEEDED ONCE FOR OTA"); return false; }
  // Stop 300ms channel hopping; an associated STA cannot stay connected
  // while NetworkRecon is retuning the WiFi radio.
  if (scanInProgress) { WiFi.scanDelete(); scanInProgress = false; }
  NetworkRecon::stop();
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  paint("OTA: CONNECTING TO WIFI ...");
  WiFi.begin(cfg.otaWifiSsid, cfg.otaWifiPass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-start < 15000) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    status("WIFI CONNECTION FAILED");
    WiFi.disconnect(false, false);
    NetworkRecon::start();
    return false;
  }
  Serial.printf("[OTA] connected; RSSI=%d freeHeap=%u\n",
                WiFi.RSSI(), (unsigned)ESP.getFreeHeap());
  return true;
}
inline void disconnect() {
  WiFi.disconnect(false, false);
  NetworkRecon::start();
  startScan();
}
inline bool validSha(const char* s) {
  if (!s || strlen(s)!=64) return false;
  for (int i=0;i<64;i++)
    if (!((s[i]>='0'&&s[i]<='9') || (s[i]>='a'&&s[i]<='f'))) return false;
  return true;
}
inline bool check() {
  available=false; latestBuild=0; latestSize=0; latestSha[0]=0;
  if (busy) return false;
  busy=true;
  if (!connect()) { busy=false; return false; }
  paint("OTA: CHECKING YOUR GITHUB FORK ...");
  WiFiClientSecure tls;
  // Preview compatibility with github.io; replace with CA validation or
  // signed manifests before treating this as an authenticated update channel.
  tls.setInsecure();
  tls.setTimeout(12000);
  HTTPClient http;
  http.setTimeout(12000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  bool ok=false;
  if (http.begin(tls, kManifest)) {
    http.addHeader("User-Agent",kAgent);
    int code=http.GET();
    if (code==HTTP_CODE_OK && http.getSize() < 2048) {
      JsonDocument doc;
      DeserializationError e=deserializeJson(doc,http.getStream());
      if (!e) {
        uint32_t build=doc["build"] | (uint32_t)0;
        size_t bytes=doc["bytes"] | (size_t)0;
        const char* hash=doc["sha256"] | "";
        const char* url=doc["firmware"] | "";
        const esp_partition_t* target=esp_ota_get_next_update_partition(nullptr);
        if (target && build>0 && bytes>0 && bytes<=target->size &&
            validSha(hash) && strcmp(url,kFirmware)==0) {
          latestBuild=build; latestSize=bytes;
          memcpy(latestSha,hash,65);
          if (build>PORKCHOP_FW_BUILD) {
            available=true;
            snprintf(state,sizeof(state),"r%lu AVAILABLE - TAP INSTALL",
                     (unsigned long)build);
          } else {
            snprintf(state,sizeof(state),"UP TO DATE (r%lu)",
                     (unsigned long)PORKCHOP_FW_BUILD);
          }
          ok=true;
        } else status("OTA FEED / SIZE VALIDATION FAILED");
      } else status("INVALID OTA JSON");
    } else {
      Serial.printf("[OTA] manifest HTTP=%d bytes=%d\n",code,http.getSize());
      status("UPDATE FEED UNAVAILABLE");
    }
    http.end();
  } else status("OTA HTTPS CONNECTION FAILED");
  disconnect();
  busy=false;
  Serial.printf("[OTA] installed r%lu latest r%lu: %s\n",
                (unsigned long)PORKCHOP_FW_BUILD,(unsigned long)latestBuild,state);
  return ok;
}
// Check the exact pending OTA slot contents before reboot. The staged
// firmware is rejected and boot slot restored if the digest differs.
inline bool verifyPartition(const esp_partition_t* slot, size_t length,
                            const char* expected) {
  if (!slot || !expected || length<1024 || length>slot->size) return false;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx,0);
  uint8_t buffer[4096];
  for (size_t pos=0;pos<length;pos+=sizeof(buffer)) {
    size_t n=length-pos < sizeof(buffer) ? length-pos : sizeof(buffer);
    if (esp_partition_read(slot,pos,buffer,n)!=ESP_OK) {
      mbedtls_sha256_free(&ctx); return false;
    }
    mbedtls_sha256_update(&ctx,buffer,n);
  }
  uint8_t digest[32]; mbedtls_sha256_finish(&ctx,digest);
  mbedtls_sha256_free(&ctx);
  char actual[65];
  for (int i=0;i<32;i++) snprintf(actual+2*i,3,"%02x",digest[i]);
  actual[64]=0;
  return strcmp(actual,expected)==0;
}
inline void progress(int done,int total) {
  static int prev=-1;
  if (total<=0) return;
  int pct=(int)((100LL*done)/total);
  if(pct!=prev && (pct==100 || pct/5!=prev/5)) {
    prev=pct;
    char msg[52]; snprintf(msg,sizeof(msg),"OTA INSTALLING: %d%%",pct);
    paint(msg,(uint8_t)pct);
    Serial.printf("[OTA] %d/%d (%d%%)\n",done,total,pct);
  }
}
inline bool install() {
  if (!available || busy) return false;
  busy=true;
  if (!connect()) { busy=false; return false; }
  const esp_partition_t* original=esp_ota_get_running_partition();
  const esp_partition_t* target=esp_ota_get_next_update_partition(nullptr);
  if (!target || !original || latestSize>target->size) {
    status("OTA PARTITION NOT SUITABLE");
    disconnect(); busy=false; return false;
  }
  paint("OTA: DOWNLOADING NEW FIRMWARE ...",0);
  WiFiClientSecure tls; tls.setInsecure(); tls.setTimeout(18000);
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.onProgress(progress);
  const t_httpUpdate_return code=httpUpdate.update(tls,kFirmware);
  bool success=(code==HTTP_UPDATE_OK);
  if (!success) {
    Serial.printf("[OTA] download/flash error=%d %s\n",
                  httpUpdate.getLastError(),httpUpdate.getLastErrorString().c_str());
    status("OTA DOWNLOAD / FLASH FAILED");
  } else if (!verifyPartition(target,latestSize,latestSha)) {
    Serial.println("[OTA] SHA256 MISMATCH; RESTORING BOOT SLOT");
    esp_ota_set_boot_partition(original);
    status("OTA HASH MISMATCH - KEPT OLD BUILD");
    success=false;
  }
  available=false;
  if(success) {
    status("UPDATE VERIFIED - RESTARTING");
    paint(state,100);
    delay(1200);
    ESP.restart();
  }
  disconnect(); busy=false;
  return success;
}
} // namespace PorkchopOta
#endif
