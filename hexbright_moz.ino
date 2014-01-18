/*
Moz firmware. 
- there are three brightness levels and the low one is very low.
- blink mode is "mostly on but flicks off" for use as a bike headlight. Access by long press when torch is turned on
- after three seconds of being on torch will turn off at next button press (no need to cycle through all modes)
*/

#include <math.h>
#include <Wire.h>

// Settings
#define OVERTEMP                340
// Pin assignments
#define DPIN_RLED_SW            2
#define DPIN_GLED               5
#define DPIN_PWR                8
#define DPIN_DRV_MODE           9
#define DPIN_DRV_EN             10
#define APIN_TEMP               0
#define APIN_CHARGE             3
// Modes
#define MODE_OFF                0
#define MODE_LOW                1
#define MODE_MED                2
#define MODE_HIGH               3
#define MODE_BLINKING           4
#define MODE_LONGPRESS_FROM_ON  5
#define MODE_LONGPRESS_FROM_OFF 6
// button timings
#define LONG_PRESS_MS 500
#define SHORT_PRESS_MS 50
// if we've been on for a while the next mode is always OFF
#define ON_FOR_A_WHILE_MS 3000


// State
byte mode = 0;
unsigned long btnTime = 0;
boolean btnDown = false;


void setup()
{
    // We just powered on!  That means either we got plugged
    // into USB, or the user is pressing the power button.
    pinMode(DPIN_PWR,      INPUT);
    digitalWrite(DPIN_PWR, LOW);
    
    // Initialize GPIO
    pinMode(DPIN_RLED_SW,  INPUT);
    pinMode(DPIN_GLED,     OUTPUT);
    pinMode(DPIN_DRV_MODE, OUTPUT);
    pinMode(DPIN_DRV_EN,   OUTPUT);
    digitalWrite(DPIN_DRV_MODE, LOW);
    digitalWrite(DPIN_DRV_EN,   LOW);
    
    // Initialize serial busses
    Serial.begin(9600);
    Wire.begin();
    
    btnTime = millis();
    btnDown = digitalRead(DPIN_RLED_SW);
    mode = MODE_OFF;
    
    Serial.println("Powered up!");
}

void loop()
{
    static unsigned long lastTempTime;
    static unsigned long lastModeChangeTime;
    unsigned long time = millis();
    
    // Check the state of the charge controller
    int chargeState = analogRead(APIN_CHARGE);
    if (chargeState < 128)  // Low - charging
    {
        digitalWrite(DPIN_GLED, (time&0x0100)?LOW:HIGH);
    }
    else if (chargeState > 768) // High - charged
    {
        digitalWrite(DPIN_GLED, HIGH);
    }
    else // Hi-Z - shutdown
    {
        digitalWrite(DPIN_GLED, LOW);
    }
    
    // Check the temperature sensor
    if (time-lastTempTime > 1000)
    {
        lastTempTime = time;
        int temperature = analogRead(APIN_TEMP);
        Serial.print("Temp: ");
        Serial.println(temperature);
        if (temperature > OVERTEMP && mode != MODE_OFF)
        {
            Serial.println("Overheating!");
            
            for (int i = 0; i < 6; i++)
            {
                digitalWrite(DPIN_DRV_MODE, LOW);
                delay(100);
                digitalWrite(DPIN_DRV_MODE, HIGH);
                delay(100);
            }
            digitalWrite(DPIN_DRV_MODE, LOW);
            
            mode = MODE_LOW;
        }
    }
    
    // if we're in a variable-output mode, do whatever this mode does
    switch (mode)
    {
        case MODE_BLINKING:
        // this is just annoying - very bright flashes destroy night vision, but flashes too short to see by
        //    digitalWrite(DPIN_DRV_EN, (time%300)<75); // on full power for 75/300 of the time
        //    break;
        // bike light flicker-flash mode: flash flash blink blink blink blink -> and that would annoy the shit out of me while I was riding. See above.
        //    int tm = (time%700);
        //    if ((tm>=0) && (tm <50))
        //      digitalWrite(DPIN_DRV_EN, 1); // FLASH
        //    else if ((tm>=50) && (tm <100))
        //      digitalWrite(DPIN_DRV_EN, 0); // PAUSE
        //    else if ((tm>=100) && (tm <150))
        //      digitalWrite(DPIN_DRV_EN, 1); // FLASH
        //    else if ((tm>=150) && (tm <200))
        //      digitalWrite(DPIN_DRV_EN, 0); // PAUSE
        //    else if ((tm>=200) && (tm <250))
        //      analogWrite(DPIN_DRV_EN, 128);// BLINK
        //    else if ((tm>=250) && (tm <350))
        //      digitalWrite(DPIN_DRV_EN, 0); // PAUSE
        //    else if ((tm>=350) && (tm <400))
        //      analogWrite(DPIN_DRV_EN, 128);// BLINK
        //    else if ((tm>=400) && (tm <500))
        //      digitalWrite(DPIN_DRV_EN, 0); // PAUSE
        //    else if ((tm>=500) && (tm <550))
        //      analogWrite(DPIN_DRV_EN, 128);// BLINK
        //    else if ((tm>=550) && (tm <700))
        //      digitalWrite(DPIN_DRV_EN, 0); // PAUSE
        
        // invert that. On all the time except for "blinks" of off at irregular intervals. Useful, enough blinking to catch the eye.
        int tm = (time%1000)/20; // 0..50 and a "blip" is 20ms
        Serial.print("counter = ");
        Serial.println(tm);
        int pattern[] = {
            1,1,1,1, 1,1,1,1,
            1,1,0,0, 1,1,1,1,
            1,1,1,1, 1,1,1,1,
            
            1,1,1,1, 1,1,1,1,
            1,1,1,1, 1,1,1,1,
            0,1,1,1, 0,1,1,1,
            1,1,1,1, 1,1,1,1 // only need 50 but 56 is a nice round number in octal
        };
        if (pattern[tm])
            digitalWrite(DPIN_DRV_EN, 1);
        else
            analogWrite(DPIN_DRV_EN, 128);
    }
    
    // Periodically pull down the button's pin, since
    // in certain hardware revisions it can float.
    pinMode(DPIN_RLED_SW, OUTPUT);
    pinMode(DPIN_RLED_SW, INPUT);
    
    // Check for mode changes
    byte newMode = mode;
    byte newBtnDown = digitalRead(DPIN_RLED_SW);
    
    if (btnDown && !newBtnDown) {
        switch (mode)
        {
            case MODE_OFF:
                if ((time-btnTime) > LONG_PRESS_MS)
                    newMode = MODE_LONGPRESS_FROM_OFF;
                else if ((time-btnTime) > SHORT_PRESS_MS)
                    newMode = MODE_LOW;
                break;
            case MODE_LOW:
                if ((time-lastModeChangeTime) > ON_FOR_A_WHILE_MS)
                    newMode = MODE_OFF;
                else if ((time-btnTime) > LONG_PRESS_MS)
                    newMode = MODE_LONGPRESS_FROM_ON;
                else if ((time-btnTime) > SHORT_PRESS_MS)
                    newMode = MODE_MED;
                break;
            case MODE_MED:{
                if ((time-lastModeChangeTime) > ON_FOR_A_WHILE_MS)
                    newMode = MODE_OFF;
                else if ((time-btnTime) > LONG_PRESS_MS)
                    newMode = MODE_LONGPRESS_FROM_ON;
                else if ((time-btnTime) > SHORT_PRESS_MS)
                    newMode = MODE_HIGH;
                break;
            }
            case MODE_HIGH:
                if ((time-lastModeChangeTime) > ON_FOR_A_WHILE_MS)
                    newMode = MODE_OFF;
                else if ((time-btnTime) > LONG_PRESS_MS)
                    newMode = MODE_LONGPRESS_FROM_ON;
                else if ((time-btnTime) > SHORT_PRESS_MS)
                    newMode = MODE_OFF;
                break;
            case MODE_LONGPRESS_FROM_OFF:
                // This mode exists just to ignore this button release.
                newMode = MODE_HIGH;
                break;
            case MODE_LONGPRESS_FROM_ON:
                // This mode exists just to ignore this button release.
                if (btnDown && !newBtnDown)
                    newMode = MODE_BLINKING;
                break;
            case MODE_BLINKING:
                if ((time-btnTime)>SHORT_PRESS_MS) // turn off whether short or long press
                    newMode = MODE_OFF;
                break;
        } // switch
        // we're here because we've changed mode
        lastModeChangeTime = millis();
    } // if button press
    
    // Do the mode transitions
    if (newMode != mode)
    {
        switch (newMode)
        {
            case MODE_OFF:
            Serial.println("Mode = off");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, LOW);
            digitalWrite(DPIN_DRV_MODE, LOW);
            digitalWrite(DPIN_DRV_EN, LOW);
            break;
            case MODE_LOW:
            Serial.println("Mode = low");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, LOW);
            analogWrite(DPIN_DRV_EN, 16); // was 64
            break;
            case MODE_MED:
            Serial.println("Mode = medium");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, LOW);
            analogWrite(DPIN_DRV_EN, 64); // was 255
            break;
            case MODE_HIGH:
            case MODE_LONGPRESS_FROM_OFF:
            Serial.println("Mode = high");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, HIGH);
            analogWrite(DPIN_DRV_EN, 255);
            break;
            case MODE_BLINKING:
            case MODE_LONGPRESS_FROM_ON:
            Serial.println("Mode = blinking");
            pinMode(DPIN_PWR, OUTPUT);
            digitalWrite(DPIN_PWR, HIGH);
            digitalWrite(DPIN_DRV_MODE, HIGH);
            // analog write done above in the timer check
            break;
        }
        
        mode = newMode;
    }
    
    // Remember button state so we can detect transitions
    if (newBtnDown != btnDown)
    {
        btnTime = time;
        btnDown = newBtnDown;
        delay(50);
    }
}
