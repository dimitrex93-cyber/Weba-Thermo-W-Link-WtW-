#ifndef ESPNOW_BRIDGE_H
#define ESPNOW_BRIDGE_H

#include <Arduino.h>
#include "config.h"
#include "types.h"

#if ENABLE_ESPNOW_LINK
#include <ESP8266WiFi.h>
extern "C" {
  #include <espnow.h>
}
#endif

enum EspNowCommandType : uint8_t {
  ESPNOW_CMD_NONE = 0,
  ESPNOW_CMD_START_HEATER = 1,
  ESPNOW_CMD_STOP_HEATER = 2,
  ESPNOW_CMD_REQUEST_STATUS = 3
};

#pragma pack(push, 1)
struct EspNowCommandPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t command;
  uint32_t unixTimeHint;
};

struct EspNowStatusPacket {
  uint8_t magic;
  uint8_t version;
  uint8_t heatingActive;
  float innenTemperatur;
  float batterieSpannung;
  float batterieLeistung;
  uint16_t restzeitS;
  uint32_t uptimeS;
};
#pragma pack(pop)

class EspNowBridge {
public:
  bool initialize() {
#if ENABLE_ESPNOW_LINK
    instance_ = this;
    pendingCommand_ = ESPNOW_CMD_NONE;
    nextStatusDueS_ = 0;
    statusDirty_ = true;

    WiFi.mode(WIFI_STA);
    wifi_set_channel(ESPNOW_WIFI_CHANNEL);

    if (esp_now_init() != 0) {
      #if ENABLE_SERIAL_DEBUG
      Serial.println("! ESP-NOW init failed");
      #endif
      initialized_ = false;
      return false;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(&EspNowBridge::onDataRecvStatic);

    uint8_t peerMac[6] = {
      ESPNOW_REMOTE_PEER_MAC_0,
      ESPNOW_REMOTE_PEER_MAC_1,
      ESPNOW_REMOTE_PEER_MAC_2,
      ESPNOW_REMOTE_PEER_MAC_3,
      ESPNOW_REMOTE_PEER_MAC_4,
      ESPNOW_REMOTE_PEER_MAC_5
    };

    if (esp_now_add_peer(peerMac, ESP_NOW_ROLE_COMBO, ESPNOW_WIFI_CHANNEL, nullptr, 0) != 0) {
      #if ENABLE_SERIAL_DEBUG
      Serial.println("! ESP-NOW add peer failed");
      #endif
      initialized_ = false;
      return false;
    }

    memcpy(peerMac_, peerMac, sizeof(peerMac_));
    initialized_ = true;

    #if ENABLE_SERIAL_DEBUG
    Serial.println(">> ESP-NOW link initialized");
    #endif
    return true;
#else
    return false;
#endif
  }

  bool isEnabled() const {
#if ENABLE_ESPNOW_LINK
    return initialized_;
#else
    return false;
#endif
  }

  void updateStatusSnapshot(const RuntimeState& state,
                            bool heatingActive,
                            uint32_t restzeitS,
                            uint32_t uptimeS) {
#if ENABLE_ESPNOW_LINK
    lastStatus_.magic = 0xA5;
    lastStatus_.version = 1;
    lastStatus_.heatingActive = heatingActive ? 1 : 0;
    lastStatus_.innenTemperatur = state.innenTemperatur;
    lastStatus_.batterieSpannung = state.batterieSpannung;
    lastStatus_.batterieLeistung = state.batterieLeistung;
    lastStatus_.restzeitS = (uint16_t)((restzeitS > 0xFFFFU) ? 0xFFFFU : restzeitS);
    lastStatus_.uptimeS = uptimeS;
    statusDirty_ = true;
#else
    (void)state;
    (void)heatingActive;
    (void)restzeitS;
    (void)uptimeS;
#endif
  }

  void loop(uint32_t nowS) {
#if ENABLE_ESPNOW_LINK
    if (!initialized_) {
      return;
    }

    if ((int32_t)(nowS - nextStatusDueS_) >= 0 && statusDirty_) {
      sendStatus();
      nextStatusDueS_ = nowS + ESPNOW_STATUS_INTERVAL_S;
      statusDirty_ = false;
    }
#else
    (void)nowS;
#endif
  }

  bool popPendingCommand(EspNowCommandType& cmd) {
#if ENABLE_ESPNOW_LINK
    if (pendingCommand_ == ESPNOW_CMD_NONE) {
      return false;
    }

    cmd = pendingCommand_;
    pendingCommand_ = ESPNOW_CMD_NONE;
    return true;
#else
    (void)cmd;
    return false;
#endif
  }

  bool isLinkAlive(uint32_t nowS, uint16_t timeoutS = 20) const {
#if ENABLE_ESPNOW_LINK
    if (!initialized_ || !hasRx_) {
      return false;
    }

    return (uint32_t)(nowS - lastRxTimeS_) <= timeoutS;
#else
    (void)nowS;
    (void)timeoutS;
    return false;
#endif
  }

  void requestImmediateStatus() {
#if ENABLE_ESPNOW_LINK
    statusDirty_ = true;
    nextStatusDueS_ = 0;
#endif
  }

private:
#if ENABLE_ESPNOW_LINK
  static EspNowBridge* instance_;

  static void onDataRecvStatic(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
    if (instance_ != nullptr) {
      instance_->onDataRecv(mac, incomingData, len);
    }
  }

  void onDataRecv(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
    (void)mac;
    if (len != sizeof(EspNowCommandPacket)) {
      return;
    }

    EspNowCommandPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));

    if (pkt.magic != 0xA5 || pkt.version != 1) {
      return;
    }

    if (pkt.command >= ESPNOW_CMD_START_HEATER && pkt.command <= ESPNOW_CMD_REQUEST_STATUS) {
      pendingCommand_ = static_cast<EspNowCommandType>(pkt.command);
      lastRxTimeS_ = millis() / 1000UL;
      hasRx_ = true;
    }
  }

  void sendStatus() {
    esp_now_send(peerMac_, reinterpret_cast<uint8_t*>(&lastStatus_), sizeof(lastStatus_));
  }

  bool initialized_ = false;
  uint8_t peerMac_[6] = {0};
  EspNowStatusPacket lastStatus_ = {0};
  volatile EspNowCommandType pendingCommand_ = ESPNOW_CMD_NONE;
  uint32_t nextStatusDueS_ = 0;
  bool statusDirty_ = true;
  uint32_t lastRxTimeS_ = 0;
  bool hasRx_ = false;
#endif
};

#if ENABLE_ESPNOW_LINK
EspNowBridge* EspNowBridge::instance_ = nullptr;
#endif

#endif
