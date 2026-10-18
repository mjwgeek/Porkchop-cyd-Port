#pragma once
// Hosyond CYD40 - manual, confirmed updates from the owner's fork ONLY.
// Reuse existing dual OTA slots where safe. Otherwise save the verified
// image to SD for the user's existing launcher; never rewrite partitions.
#ifdef HOSYOND_CYD40
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>
#include <SD.h>

#ifndef PORKCHOP_FW_BUILD
#define PORKCHOP_FW_BUILD 0
#endif

namespace PorkchopOta {
#ifdef PORKCHOP_TEST_CHANNEL
// TEST firmware checks the isolated release, not the live Pages feed.
static constexpr const char* kManifest =
    "https://github.com/mjwgeek/Porkchop-cyd-Port/releases/download/hosyond-cyd40-display-ota-test/ota.json";
static constexpr const char* kFirmware =
    "https://github.com/mjwgeek/Porkchop-cyd-Port/releases/download/hosyond-cyd40-display-ota-test/Porkchop-Hosyond-4.0-Launcher.bin";
#else
static constexpr const char* kManifest = "https://mjwgeek.github.io/Porkchop-cyd-Port/ota.json";
static constexpr const char* kFirmware = "https://mjwgeek.github.io/Porkchop-cyd-Port/flash/firmware.bin";
#endif
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
inline bool isOtaSlot(const esp_partition_t* p) {
  return p && p->type == ESP_PARTITION_TYPE_APP &&
      (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
       p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1);
}
inline bool compatible() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
  if (!isOtaSlot(running) || !isOtaSlot(target) ||
      running->address == target->address ||
      target->size < 1600000) return false;

  // An SD-card launcher or a different firmware may occupy the other OTA
  // slot. NEVER clobber that app: fall back to SD download instead.
  esp_app_desc_t currentApp = {};
  esp_app_desc_t targetApp = {};
  if (esp_ota_get_partition_description(running, &currentApp) != ESP_OK)
    return false;
  if (esp_ota_get_partition_description(target, &targetApp) == ESP_OK)
    return strncmp(currentApp.project_name, targetApp.project_name,
                   sizeof(currentApp.project_name)) == 0;
  // A completely erased slot is safe. Unknown/non-erased app images are not.
  uint32_t firstWord = 0;
  if (esp_partition_read(target, 0, &firstWord, sizeof(firstWord)) != ESP_OK)
    return false;
  return firstWord == UINT32_MAX;
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
  if (!compatible() && !sdAvailable) {
    status("NO FREE OTA SLOT OR SD CARD");
    return false;
  }
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
            snprintf(state,sizeof(state),"r%lu AVAILABLE - %s",
                     (unsigned long)build,
                     compatible() ? "TAP INSTALL OTA" : "TAP SAVE TO SD");
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
// For SD launchers with a single app slot (or a protected launcher app in
// the other slot), download a verified application BIN rather than requiring
// a risky partition-table rewrite or overwriting the launcher.
inline bool saveUpdateOnSd() {
  if (!sdAvailable || latestSize < 1024) {
    status("SD CARD REQUIRED TO SAVE UPDATE");
    return false;
  }
  const char* tempName="/Porkchop-Hosyond-OTA.part";
  const char* finalName="/Porkchop-Hosyond-4.0-Launcher.bin";
  if (SD.totalBytes() - SD.usedBytes() < latestSize + 32768) {
    status("NOT ENOUGH FREE SD SPACE");
    return false;
  }
  SD.remove(tempName);
  File output=SD.open(tempName, FILE_WRITE);
  if (!output) { status("SD FILE CREATE FAILED"); return false; }
  paint("OTA: SAVING NEW FIRMWARE TO SD ...", 0);
  WiFiClientSecure tls;
  tls.setInsecure(); // Test-only: must harden HTTPS validation later.
  tls.setTimeout(18000);
  HTTPClient http;
  http.setTimeout(18000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  bool success=false;
  if (http.begin(tls,kFirmware)) {
    const int code=http.GET();
    const int contentBytes=http.getSize();
    if (code==HTTP_CODE_OK && contentBytes>0 &&
        (size_t)contentBytes==latestSize) {
      const int written=http.writeToStream(&output);
      output.flush();
      if (written>0 && (size_t)written==latestSize) success=true;
      else Serial.printf("[OTA] SD transfer incomplete: wrote=%d expected=%u\n",
                         written,(unsigned)latestSize);
    } else Serial.printf("[OTA] SD HTTP=%d bytes=%d expected=%u\n",
                        code,contentBytes,(unsigned)latestSize);
    http.end();
  }
  output.close();
  if (!success) {
    SD.remove(tempName);
    status("SD UPDATE DOWNLOAD FAILED");
    return false;
  }

  File verify=SD.open(tempName,FILE_READ);
  if (!verify) {
    SD.remove(tempName);
    status("SD UPDATE VERIFY OPEN FAILED");
    return false;
  }
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx,0);
  uint8_t block[2048];
  size_t hashed=0;
  while (verify.available() && hashed<latestSize) {
    const int n=verify.read(block,sizeof(block));
    if(n<=0) break;
    mbedtls_sha256_update(&ctx,block,n);
    hashed+=(size_t)n;
  }
  verify.close();
  uint8_t digest[32];
  mbedtls_sha256_finish(&ctx,digest);
  mbedtls_sha256_free(&ctx);
  char actual[65];
  for (int i=0;i<32;i++) snprintf(actual+i*2,3,"%02x",digest[i]);
  actual[64]=0;
  if (hashed!=latestSize || strcmp(actual,latestSha)!=0) {
    SD.remove(tempName);
    status("SD UPDATE HASH MISMATCH");
    return false;
  }
  // Never remove the last known-good download unless its replacement has
  // already passed the SHA-256 check.
  if (SD.exists(finalName)) SD.remove(finalName);
  if (!SD.rename(tempName,finalName)) {
    status("SD UPDATE RENAME FAILED");
    return false;
  }
  Serial.printf("[OTA] Verified SD BIN saved: %s (%u bytes)\n",
                finalName,(unsigned)latestSize);
  status("SAVED TO SD - SELECT BIN IN LAUNCHER");
  paint(state,100);
  return true;
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
  if (!compatible()) {
    bool saved = saveUpdateOnSd();
    available = false;
    disconnect();
    busy=false;
    return saved;
  }
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
