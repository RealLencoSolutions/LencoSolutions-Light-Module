#include <SPI.h>
#include "mcp2515.h"

#define ESC_CAN_ID 107
#define BALANCE_BUDDY_CAN_ID 36

typedef enum {
  CAN_PACKET_SET_DUTY = 0,
  CAN_PACKET_SET_CURRENT,
  CAN_PACKET_SET_CURRENT_BRAKE,
  CAN_PACKET_SET_RPM,
  CAN_PACKET_SET_POS,
  CAN_PACKET_FILL_RX_BUFFER,
  CAN_PACKET_FILL_RX_BUFFER_LONG,
  CAN_PACKET_PROCESS_RX_BUFFER,
  CAN_PACKET_PROCESS_SHORT_BUFFER,
  CAN_PACKET_STATUS,
  CAN_PACKET_SET_CURRENT_REL,
  CAN_PACKET_SET_CURRENT_BRAKE_REL,
  CAN_PACKET_SET_CURRENT_HANDBRAKE,
  CAN_PACKET_SET_CURRENT_HANDBRAKE_REL,
  CAN_PACKET_STATUS_2,
  CAN_PACKET_STATUS_3,
  CAN_PACKET_STATUS_4,
  CAN_PACKET_PING,
  CAN_PACKET_PONG,
  CAN_PACKET_DETECT_APPLY_ALL_FOC,
  CAN_PACKET_DETECT_APPLY_ALL_FOC_RES,
  CAN_PACKET_CONF_CURRENT_LIMITS,
  CAN_PACKET_CONF_STORE_CURRENT_LIMITS,
  CAN_PACKET_CONF_CURRENT_LIMITS_IN,
  CAN_PACKET_CONF_STORE_CURRENT_LIMITS_IN,
  CAN_PACKET_CONF_FOC_ERPMS,
  CAN_PACKET_CONF_STORE_FOC_ERPMS,
  CAN_PACKET_STATUS_5,
  CAN_PACKET_POLL_TS5700N8501_STATUS,
  CAN_PACKET_CONF_BATTERY_CUT,
  CAN_PACKET_CONF_STORE_BATTERY_CUT,
  CAN_PACKET_SHUTDOWN,
  CAN_PACKET_IO_BOARD_ADC_1_TO_4,
  CAN_PACKET_IO_BOARD_ADC_5_TO_8,
  CAN_PACKET_IO_BOARD_ADC_9_TO_12,
  CAN_PACKET_IO_BOARD_DIGITAL_IN,
  CAN_PACKET_IO_BOARD_SET_OUTPUT_DIGITAL,
  CAN_PACKET_IO_BOARD_SET_OUTPUT_PWM,
  CAN_PACKET_BMS_V_TOT,
  CAN_PACKET_BMS_I,
  CAN_PACKET_BMS_AH_WH,
  CAN_PACKET_BMS_V_CELL,
  CAN_PACKET_BMS_BAL,
  CAN_PACKET_BMS_TEMPS,
  CAN_PACKET_BMS_HUM,
  CAN_PACKET_BMS_SOC_SOH_TEMP_STAT
} CAN_PACKET_ID;

class ESC {
private:
  MCP2515 mcp2515;

  struct can_frame responses[10];
  int respnsesLength = 0;

  uint8_t readBuffer[50];
  uint8_t readBufferLength = 0;
  uint8_t readBufferInfo[8];
  uint8_t readBufferInfoLength = 0;

public:
  // RT Data vars
  double dutyCycle;
  double erpm;
  double voltage;

  // Balance vars
  // Only fields used by the rest of the project
  uint16_t balanceState;
  double adc1;
  double adc2;

  // Rate limiting for balance polling to reduce CAN traffic
  uint8_t balancePollCounter = 0;
  static const uint8_t balancePollEvery = 4; // poll balance every 4 loops

  ESC() : mcp2515(10) {}
  void setup() {
    SPI.begin();

    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();

    // For testing
    getBalance();
  }

  // New method for polling realtime data (called only when needed)
  bool pollRealtimeData() {
    getRealtimeData();
    return parseRealtimeData();
  }
  
  // New method for non-blocking message listening
  void listenForMessages() {
    // Just check for any incoming messages without sending requests
    struct can_frame frame;
    if (mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
      // Handle any incoming messages if needed
      // For now, we just consume them to keep the buffer clear
    }
  }
  
  // Keep the old loop method for backward compatibility if needed
  void loop() {
    getRealtimeData();
    if (balancePollCounter == 0) {
      getBalance();
    }
    balancePollCounter = (balancePollCounter + 1) % balancePollEvery;
  }
  void ping() {
    // 80001101 1 1
    // 80001201 1 1
    //  8000 dunno what this is
    //  11/12 the can command id
    //  01 the id of the esc we are talking to
    //  1 length
    //  1 id
    struct can_frame canMsg1;
    canMsg1.can_id = (uint32_t(0x8000) << 16) +
                     (uint16_t(CAN_PACKET_PING) << 8) + ESC_CAN_ID;
    canMsg1.can_dlc = 0x01;
    canMsg1.data[0] = BALANCE_BUDDY_CAN_ID;
    mcp2515.sendMessage(&canMsg1);
  }

  void pong() {
    struct can_frame canMsg1;
    canMsg1.can_id = (uint32_t(0x8000) << 16) +
                     (uint16_t(CAN_PACKET_PONG) << 8) + ESC_CAN_ID;
    canMsg1.can_dlc = 0x01;
    canMsg1.data[0] = BALANCE_BUDDY_CAN_ID;
    mcp2515.sendMessage(&canMsg1);
  }

  void getRealtimeData() {
    // Send request for realtime data
    struct can_frame canMsg1;
    canMsg1.can_id = (uint32_t(0x8000) << 16) +
                     (uint16_t(CAN_PACKET_PROCESS_SHORT_BUFFER) << 8) +
                     ESC_CAN_ID;
    canMsg1.can_dlc = 0x07;
    canMsg1.data[0] = BALANCE_BUDDY_CAN_ID;
    canMsg1.data[1] = 0x00;
    canMsg1.data[2] = 0x32;
    // mask
    canMsg1.data[3] = 0x00;
    canMsg1.data[4] = 0x00;
    canMsg1.data[5] = B10000001;
    canMsg1.data[6] = B11000011;
    mcp2515.sendMessage(&canMsg1);

    // Read the response
    batchRead();
  }

  bool parseRealtimeData() {
    if (readBufferLength != 0x12 || readBuffer[0] != 0x32) {
      return false;
    }
    // Skip tempMosfet parsing since it's not used
    dutyCycle =
        (((int16_t)readBuffer[9] << 8) + ((int16_t)readBuffer[10])) / 1000.0;
    erpm = (((int32_t)readBuffer[11] << 24) + ((int32_t)readBuffer[12] << 16) +
            ((int32_t)readBuffer[13] << 8) + ((int32_t)readBuffer[14]));
    voltage =
        (((int16_t)readBuffer[15] << 8) + ((int16_t)readBuffer[16])) / 10.0;

    return true;
  }

  void getBalance() {

    // 80000803 3 2 0 0
    struct can_frame canMsg1;
    canMsg1.can_id = (uint32_t(0x8000) << 16) +
                     (uint16_t(CAN_PACKET_PROCESS_SHORT_BUFFER) << 8) +
                     ESC_CAN_ID;
    canMsg1.can_dlc = 0x03;
    canMsg1.data[0] = BALANCE_BUDDY_CAN_ID;
    canMsg1.data[1] = 0x00;
    canMsg1.data[2] = 0x4F;
    mcp2515.sendMessage(&canMsg1);

    batchRead();
    parseBalance();
  }

  void parseBalance() {
    if (readBufferLength != 0x25 || readBuffer[0] != 0x4F) {
      return;
    }
    balanceState = ((uint16_t)readBuffer[25] << 8) + ((uint16_t)readBuffer[26]);
    adc1 = (((int32_t)readBuffer[29] << 24) + ((int32_t)readBuffer[30] << 16) +
            ((int32_t)readBuffer[31] << 8) + ((int32_t)readBuffer[32])) /
           1000000.0;
    adc2 = (((int32_t)readBuffer[33] << 24) + ((int32_t)readBuffer[34] << 16) +
            ((int32_t)readBuffer[35] << 8) + ((int32_t)readBuffer[36])) /
           1000000.0;
    //    Serial.print("PID Output: ");
    //    Serial.print(pidOutput);
    //    Serial.println();
    //    Serial.print("Pitch: ");
    //    Serial.print(pitch);
    //    Serial.println();
    //    Serial.print("Roll: ");
    //    Serial.print(roll);
    //    Serial.println();
  }

  void printFrame(struct can_frame *frame) {
    Serial.print(frame->can_id, HEX); // print ID
    Serial.print(" ");
    Serial.print(frame->can_dlc, HEX); // print DLC
    Serial.print(" ");
    for (int i = 0; i < frame->can_dlc; i++) { // print the data
      Serial.print(frame->data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  void batchRead() {
    // Reset lengths; avoid costly full buffer clears
    readBufferLength = 0;
    readBufferInfoLength = 0;

    // Track which data bytes have been received to allow early exit
    bool received[50];
    for (int i = 0; i < 50; i++) received[i] = false;

    // Unknown at start; becomes known when PROCESS_RX_BUFFER arrives
    int expectedLength = -1;

    // Read frames with timeout to prevent blocking
    // Reduced iteration limit for faster response
    for (int iter = 0; iter < 32; iter++) {
      struct can_frame frame;
      if (mcp2515.readMessage(&frame) != MCP2515::ERROR_OK) {
        // No more messages pending right now
        break;
      }

      if (frame.can_id ==
          0x80000000 + ((uint16_t)CAN_PACKET_FILL_RX_BUFFER << 8) +
              BALANCE_BUDDY_CAN_ID) {
        // frame.data[0] is the offset; following bytes are payload
        const uint8_t base = frame.data[0];
        for (int j = 1; j < frame.can_dlc; j++) {
          const int idx = base + (j - 1);
          if (idx >= 0 && idx < 50) {
            if (!received[idx]) {
              received[idx] = true;
              readBufferLength++;
            }
            readBuffer[idx] = frame.data[j];
          }
        }
      } else if (frame.can_id ==
                 0x80000000 + ((uint16_t)CAN_PACKET_PROCESS_RX_BUFFER << 8) +
                     BALANCE_BUDDY_CAN_ID) {
        // Header with command and total length
        const int copy = frame.can_dlc < 8 ? frame.can_dlc : 8;
        for (int j = 0; j < copy; j++) {
          readBufferInfo[j] = frame.data[j];
        }
        readBufferInfoLength = copy;
        if (copy >= 4) {
          expectedLength = ((uint16_t)readBufferInfo[2] << 8) + readBufferInfo[3];
        }
      }

      // Early exit when we know how many bytes we expect and we have them
      if (expectedLength >= 0 && readBufferLength >= expectedLength) {
        break;
      }
    }

    // Validate length; if mismatch, discard
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
