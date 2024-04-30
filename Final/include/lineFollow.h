#include "move.h"

#include <Adafruit_MCP3008.h>

Adafruit_MCP3008 adc1;
Adafruit_MCP3008 adc2;

const unsigned int ADC_1_CS = 2;
const unsigned int ADC_2_CS = 17;

int adc1_buf[8];
int adc2_buf[8];

uint8_t lineArray[13]; 
float previousPosition = 6;

// from line-sensor ADC tutorial --------------------
void readADC() {
  for (int i = 0; i < 8; i++) {
    adc1_buf[i] = adc1.readADC(i);
    adc2_buf[i] = adc2.readADC(i);
  }
}

void digitalConvert(){
  for (int i = 0; i < 7; i++) {
    if (adc1_buf[i]>650) {
      lineArray[2*i] = 1; 
    } else {
      lineArray[2*i] = 0;
    }
    Serial.print(lineArray[2*i]); Serial.print(" ");
    // Serial.print(adc1_buf[i]); Serial.print("\t");

    if (i<6) {
      if (adc2_buf[i]>650){
        lineArray[2*i+1] = 1;
      } else {
        lineArray[2*i+1] = 0;
      }
      Serial.print(lineArray[2*i+1]); Serial.print(" ");
      // Serial.print(adc2_buf[i]); Serial.print("\t");
    }
  }
}

float getPosition(float previousPosition) {
  float pos = 0;
  uint8_t white_count = 0;
  for (int i = 0; i < 13; i++) {
    if ((lineArray[i] == 0)) {
      pos += i;
      white_count+=1;
    } 
  }

  // Serial.print("white: "); Serial.print(white_count); Serial.print("\t");
  // Serial.print("pos: "); Serial.print(pos); Serial.print("\t");
  if (white_count == 0) {
    return previousPosition;
  }
  return pos/white_count;
}
// --------------------------------------------------

// for use to detect the large, white section box
// returns 1 if section box detected, 0 otherwise
bool boxDetect() {
    readADC();
    digitalConvert();
    uint8_t white_count = 0;
    for (int i = 0; i < 13; i++) {
        if (lineArray[i] == 0) {
            white_count++;
        } 
    }

    if (white_count >= 11) {
        return 1;
    }
    else {
        return 0;
    }
}

// follow the line until a white box is hit
void followLine(const int Kp, const int Ki, const int Kd, const int MAX_PWM) {
    float error, errorTotal, control, t, tStart, tPrev, tNow, errorPrev;
    const float tInt = 100 * (1/1000000.0); // integration time in s
    float mid = 6.5; // sensor array midpoint

    errorPrev = 0;

    int lPWM, rPWM;
    lPWM = MAX_PWM;
    rPWM = MAX_PWM;

    while (!boxDetect()) {

        readADC();
        digitalConvert();
        float pos = getPosition(previousPosition);
        previousPosition = pos;

        error = pos - mid;
        errorTotal += error;

        tNow = micros() / 1000000.0;
        error = pos - mid;
        errorTotal += error;
        control = Kp * error + Ki * errorTotal * tInt + Kd * (error - errorPrev);

        lPWM = MAX_PWM - control;
        rPWM = MAX_PWM + control;

        if(lPWM < 0) {
            lPWM = 0;
        }
        if(rPWM < 0) {
            rPWM = 0;
        }
        Serial.print("----");
        Serial.print(error);
        Serial.print("----");
        Serial.print(min(lPWM, MAX_PWM));
        Serial.print("\t");
        Serial.print(min(rPWM, MAX_PWM));
        Serial.print("\n");

        if (MAX_PWM > 0) {
            M1_forward((int)min(lPWM, MAX_PWM));
            M2_forward((int)min(rPWM, MAX_PWM));
        }
        else {
            M1_backward((int)min(abs(lPWM), MAX_PWM));
            M2_backward((int)min(abs(rPWM), MAX_PWM));
        }

        errorPrev = error;
    }
    brake();
}

// move in a straight line without the aid of line following (to go backward, use negative PWM)
// PID controller https://microcontrollerslab.com/pid-controller-implementation-using-arduino/
// kp: proportional gain
// ki: integral gain
// kd: derivative gain
// mm: distance to travel in mm (set to 0 to use box detect)
// MAX_PWM: target speed for the bot (is also treated as the maximum allowable speed) 
void straight(int kp, int ki, int kd, int mm, int MAX_PWM, Encoder& enc1, Encoder& enc2) {
    const float M_PER_TICK = (PI * 0.032) / 360.0; // meters per tick
    const float REF_R = 4.3 / 100.0; // referance point in m: distance of wheel edge from center
    float sTick = (mm/1000.0) / M_PER_TICK;

    if (mm != 0) {
      mm = mm - 350;
    }

    float error, errorTotal, control, t, tStart, tPrev, tNow;
    const float tInt = 100 * (1/1000000.0); // integration time in s
    int lPWM, rPWM;
    lPWM = MAX_PWM;
    rPWM = MAX_PWM;
    tPrev = 0.0;
    error = 0.0;
    errorTotal = 0.0;

    enc1.write(0);
    enc2.write(0);
    while (true) {
        // update PID
        tNow = micros() / 1000000.0;
        error = enc1.read() + enc2.read(); // one is always negative if the motors are going in the same direction
        errorTotal += error;
        control = kp * error + ki * errorTotal * tInt + kd * (tNow - tPrev)/tInt;
        //control = kp * error + ki * errorTotal * tInt + kd * (error - errorPrev)/tInt;

        lPWM = MAX_PWM - control;
        rPWM = MAX_PWM + control;

        if (MAX_PWM > 0) {
            M1_forward((int)min(lPWM, MAX_PWM));
            M2_forward((int)min(rPWM, MAX_PWM));
        }
        else {
            M1_backward((int)min(lPWM, MAX_PWM));
            M2_backward((int)min(rPWM, MAX_PWM));
        }

        tPrev = tNow;

        // check the distance
        if (mm != 0) {
            if ((abs(enc1.read()) > sTick) || (abs(enc2.read()) > sTick)) {
                brake();
                break;
            }
        }
        else {
            if (boxDetect()) {
                brake();
                break;
            }
        }
        Serial.print(sTick);
        Serial.print("\t");
        Serial.print(enc1.read());
        Serial.print("\t");
        Serial.print(enc2.read());
        Serial.print("\t");
        Serial.print(error);
        Serial.print("\t");
        Serial.print(lPWM);
        Serial.print("\t");
        Serial.print(rPWM);
        Serial.print("\t");
        Serial.print(control);
        Serial.println();
    }
}