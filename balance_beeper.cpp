#include "beeper.cpp"

#define BEEPER_PIN 4

#define PLAY_STARTUP true
#define DUTY_CYCLE_ALERT 0.80 // 0 to disable
#define LOW_VOLTAGE 51.3 // 0 to disable
#define LOW_VOLTAGE_INTERVAL 5 * 1000 // every 30 seconds

class BalanceBeeper {
  private:
    Beeper beeper;

    long lastLowVoltageMillis = 0;
  public:
    BalanceBeeper() :
      beeper(BEEPER_PIN){
    }
    void setup(){ 
      beeper.setup();
      if(PLAY_STARTUP){
        beeper.queueThreeShort();
      }
    }

    void loop(double dutyCycle, double erpm, double voltage){
      beeper.loop();

      // Non latching beeps for Duty Cycle
      if(fabsf(dutyCycle) > DUTY_CYCLE_ALERT && DUTY_CYCLE_ALERT > 0){
        beeper.queueShortSingle();
      }

      // Low voltage, time based repeat
      if(voltage < LOW_VOLTAGE && LOW_VOLTAGE > 0 && lastLowVoltageMillis + LOW_VOLTAGE_INTERVAL < millis()){
        beeper.queueSad();
        lastLowVoltageMillis = millis();
      }
    }

};