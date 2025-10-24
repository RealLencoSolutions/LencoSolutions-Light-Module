#include <SPI.h>
#include "mcp2515.h"

#define ESC_CAN_ID 107
#define BALANCE_BUDDY_CAN_ID 36

typedef enum {
  CAN_PACKET_PROCESS_SHORT_BUFFER = 8,
  CAN_PACKET_FILL_RX_BUFFER = 5,
  CAN_PACKET_PROCESS_RX_BUFFER = 7
} CAN_PACKET_ID;

class ESC {
private:
  MCP2515 mcp2515;

  struct can_frame responses[10];
  int responsesLength = 0;

  uint8_t readBuffer[50];
  uint8_t readBufferLength = 0;
  uint8_t readBufferInfo[8];
  uint8_t readBufferInfoLength = 0;

public:
  // Only the fields we need
  double dutyCycle;
  double erpm;
  double voltage;

  ESC() : mcp2515(10) {}

  void setup() {
    SPI.begin();
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();

    dutyCycle = 0;
    erpm = 0;
    voltage = 0;
  }

  // Polls for new data (non-blocking)
  void loop() {
    getRealtimeData();
  }

  void getRealtimeData() {
    struct can_frame canMsg;
    canMsg.can_id = (uint32_t(0x8000) << 16) +
                    (uint16_t(CAN_PACKET_PROCESS_SHORT_BUFFER) << 8) +
                    ESC_CAN_ID;
    canMsg.can_dlc = 0x07;
    canMsg.data[0] = BALANCE_BUDDY_CAN_ID;
    canMsg.data[1] = 0x00;
    canMsg.data[2] = 0x32;  // command id for ERPM/Duty/Voltage
    canMsg.data[3] = 0x00;
    canMsg.data[4] = 0x00;
    canMsg.data[5] = B10000001;
    canMsg.data[6] = B11000011;
    mcp2515.sendMessage(&canMsg);

    batchRead();
    parseRealtimeData();
  }

  bool parseRealtimeData() {
    if (readBufferLength != 0x12 || readBuffer[0] != 0x32) return false;

    dutyCycle = (((int16_t)readBuffer[9] << 8) + readBuffer[10]) / 1000.0;
    erpm = (((int32_t)readBuffer[11] << 24) + ((int32_t)readBuffer[12] << 16) +
            ((int32_t)readBuffer[13] << 8) + readBuffer[14]);
    voltage = (((int16_t)readBuffer[15] << 8) + readBuffer[16]) / 10.0;

    return true;
  }

private:
  void batchRead() {
    readBufferLength = 0;
    readBufferInfoLength = 0;
    bool received[50] = {false};
    int expectedLength = -1;

    for (int i = 0; i < 32; i++) {
      struct can_frame frame;
      if (mcp2515.readMessage(&frame) != MCP2515::ERROR_OK) break;

      if (frame.can_id == 0x80000000 + ((uint16_t)CAN_PACKET_FILL_RX_BUFFER << 8) + BALANCE_BUDDY_CAN_ID) {
        const uint8_t base = frame.data[0];
        for (int j = 1; j < frame.can_dlc; j++) {
          int idx = base + (j - 1);
          if (idx < 50 && !received[idx]) {
            received[idx] = true;
            readBuffer[idx] = frame.data[j];
            readBufferLength++;
          }
        }
      } else if (frame.can_id == 0x80000000 + ((uint16_t)CAN_PACKET_PROCESS_RX_BUFFER << 8) + BALANCE_BUDDY_CAN_ID) {
        for (int j = 0; j < frame.can_dlc; j++) readBufferInfo[j] = frame.data[j];
        readBufferInfoLength = frame.can_dlc;
        if (frame.can_dlc >= 4) expectedLength = ((uint16_t)readBufferInfo[2] << 8) + readBufferInfo[3];
      }

      if (expectedLength >= 0 && readBufferLength >= expectedLength) break;
    }

    // validate
    if (readBufferInfoLength >= 4) {
      uint16_t supposedLength = ((uint16_t)readBufferInfo[2] << 8) + readBufferInfo[3];
      if (readBufferLength != supposedLength) {
        readBufferLength = 0;
        readBufferInfoLength = 0;
      }
    } else {
      readBufferLength = 0;
      readBufferInfoLength = 0;
    }
  }
};
