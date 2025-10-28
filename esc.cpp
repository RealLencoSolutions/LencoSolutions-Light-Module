#include <Arduino.h>
#include <SPI.h>
#include "mcp2515.h"

#define ESC_CAN_ID 107
#define NODE_CAN_ID 36 // Your device's CAN ID

// Relevant CAN command IDs
typedef enum {
  CAN_PACKET_PROCESS_SHORT_BUFFER = 8,
  CAN_PACKET_FILL_RX_BUFFER = 5,
  CAN_PACKET_PROCESS_RX_BUFFER = 7
} CAN_PACKET_ID;

class ESC {
  private:
    MCP2515 mcp2515;
    struct can_frame rxFrame;
    uint8_t rxData[50];
    uint8_t rxLen = 0;

  public:
    // Realtime vars
    int32_t erpm = 0;
    double voltage = 0.0;
    double dutyCycle = 0.0;

    ESC() : mcp2515(10) {} // CS pin for MCP2515

    void setup() {
      SPI.begin();
      mcp2515.reset();
      mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
      mcp2515.setNormalMode();
    }

    // Called periodically (e.g. every 100ms)
    bool getRealtimeData() {
      sendRealtimeRequest();
      bool newData = readRealtimeResponse();
      return newData;
    }

    // Optional: passive listening for any messages (not required for realtime)
    void listenForMessages() {
      // Non-blocking read — only parses known message types
      if (mcp2515.readMessage(&rxFrame) == MCP2515::ERROR_OK) {
        uint32_t id = rxFrame.can_id;
        if (id == (0x80000000 + ((uint16_t)CAN_PACKET_FILL_RX_BUFFER << 8) + NODE_CAN_ID)) {
          if (rxFrame.data[0] + rxFrame.can_dlc - 1 < sizeof(rxData)) {
            memcpy(&rxData[rxFrame.data[0]], &rxFrame.data[1], rxFrame.can_dlc - 1);
            rxLen += rxFrame.can_dlc - 1;
          }
        } else if (id == (0x80000000 + ((uint16_t)CAN_PACKET_PROCESS_RX_BUFFER << 8) + NODE_CAN_ID)) {
          if (rxLen >= 17 && rxData[0] == 0x32) {
            parseRealtimeData();
          }
          rxLen = 0;
        }
      }
    }

  private:
    void sendRealtimeRequest() {
      struct can_frame msg;
      msg.can_id  = (uint32_t(0x8000) << 16) | (uint16_t(CAN_PACKET_PROCESS_SHORT_BUFFER) << 8) | ESC_CAN_ID;
      msg.can_dlc = 7;
      msg.data[0] = NODE_CAN_ID;
      msg.data[1] = 0x00;
      msg.data[2] = 0x32; // Realtime data command
      msg.data[3] = 0x00;
      msg.data[4] = 0x00;
      msg.data[5] = B10000001;
      msg.data[6] = B11000011;
      mcp2515.sendMessage(&msg);
    }

    bool readRealtimeResponse() {
      bool dataReady = false;

      // Small loop to catch frames for this transaction
      unsigned long startTime = millis();
      while (millis() - startTime < 5) { // very short window
        if (mcp2515.readMessage(&rxFrame) == MCP2515::ERROR_OK) {
          uint32_t id = rxFrame.can_id;

          if (id == (0x80000000 + ((uint16_t)CAN_PACKET_FILL_RX_BUFFER << 8) + NODE_CAN_ID)) {
            if (rxFrame.data[0] + rxFrame.can_dlc - 1 < sizeof(rxData)) {
              memcpy(&rxData[rxFrame.data[0]], &rxFrame.data[1], rxFrame.can_dlc - 1);
              rxLen += rxFrame.can_dlc - 1;
            }
          } else if (id == (0x80000000 + ((uint16_t)CAN_PACKET_PROCESS_RX_BUFFER << 8) + NODE_CAN_ID)) {
            if (rxLen >= 17 && rxData[0] == 0x32) {
              parseRealtimeData();
              dataReady = true;
            }
            rxLen = 0;
          }
        }
      }

      return dataReady;
    }

    void parseRealtimeData() {
      dutyCycle = ((int16_t(rxData[9]) << 8) | int16_t(rxData[10])) / 1000.0;
      erpm      = ((int32_t(rxData[11]) << 24) | (int32_t(rxData[12]) << 16) |
                   (int32_t(rxData[13]) << 8)  | (int32_t(rxData[14])));
      voltage   = ((int16_t(rxData[15]) << 8) | int16_t(rxData[16])) / 10.0;
    }
};

