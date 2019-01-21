#include "LedController.h"
#include "FastLED.h"

LEDController::LEDController(CRGB *_leds)
{
  
  currentMode = kFadeAll;
  isPulse = false;
  ledTimer = 0;
  pulseTimer = 0;
  leds = _leds;
  ledCount = 1;// sizeof(leds) / sizeof(CRGB);  // this doesnt work FIXME
  setColour(0,0,0); // off
  setColourTarget(0,0,0); // off
  arrayIndex = 0;
}

void LEDController::setModeToSweep(void)
{
  currentMode = kSweep;
}

void LEDController::setModeToFade(void)
{
  currentMode = kFadeAll;;
}

void LEDController::run(void)
{
  switch(currentMode)
  {
    case kFadeAll:
          /* Periodically update the LEDs */
      if(timerExpired(ledTimer, LED_UPDATE_TIMEOUT))
      {
        setTimer(&ledTimer); // reset timer
        // display the current buffer colour
        setColour(transition[arrayIndex][0],transition[arrayIndex][1],transition[arrayIndex][2]);

        // now that it's been used, update the buffer colour to the target
        transition[arrayIndex][0] = target_colour[0];
        transition[arrayIndex][1] = target_colour[1];
        transition[arrayIndex][2] = target_colour[2];

          // increment the array index
          if (arrayIndex < STEPS - 1)
            arrayIndex++;
          else
            arrayIndex=0;
      }
  
      if(timerExpired(pulseTimer, PULSE_TIMEOUT) && isPulse)
      {
        setColourTarget(0,0,0);
        isPulse = false;
      }
    break;

    case kSweep:
      // TODO: Set led colour but 1 led at a time in a sweeping motion.
    break;

    default:  // should never get here
      currentMode = kFadeAll;
    break;
  }


}


void LEDController::setColourTarget(int r, int g, int b)
{  
  target_colour[0] = r;
  target_colour[1] = g;
  target_colour[2] = b;
  setColourTransition();
}

void LEDController::pulse(int r, int g, int b)
{
  isPulse = true;
  setTimer(&pulseTimer);
  target_colour[0] = r;
  target_colour[1] = g;
  target_colour[2] = b;
  setColourTransition();
}


void LEDController::setColourTransition(void)
{
  int counter = 0;
  int addr = arrayIndex;
  
  for(addr; addr<STEPS; addr++)  // for each element in the array
  {
    for (int i=0; i<3; i++)  // for each colour in turn
    {
      transition[addr][i] = map(counter, 0, STEPS-1, current_colour[i], target_colour[i]); // compute the proportional colour value
    }
    counter++;
    //Serial.print("Address: ");
    //Serial.println(addr);
    /*
    Serial.print(transition[addr][0]);
    Serial.print(",");
    Serial.print(transition[addr][1]);
    Serial.print(",");
    Serial.println(transition[addr][2]);
    */
  }

  addr = 0;
  
  for(addr; addr<arrayIndex; addr++)  // for each element in the array
  {
    for (int i=0; i<3; i++)  // for each colour in turn
    {
      transition[addr][i] = map(counter, 0, STEPS-1, current_colour[i], target_colour[i]); // compute the proportional colour value
    }
    counter++;
    //Serial.print("Address: ");
    //Serial.println(addr);
    /*
    Serial.print(transition[addr][0]);
    Serial.print(",");
    Serial.print(transition[addr][1]);
    Serial.print(",");
    Serial.println(transition[addr][2]);
    */
  }
}

void LEDController::sweepToColourTarget(void)
{
  // TODO: user must determine a sweep update rate and we can apply the fade to each LED individually up to the total nuber of LEDs
}

void LEDController::setColour(int r, int g, int b)
{
  current_colour[0] = r;
  current_colour[1] = g;
  current_colour[2] = b;
  applyColour(current_colour[0],current_colour[1],current_colour[2]);
}

// optional led index parameter. if omitted, whole strip will be set to colour
void LEDController::applyColour(uint8_t r, uint8_t g, uint8_t b, int ledIndex = -1)
{
  if (r < 256 && g < 256 && b < 256)
  {
    if(ledIndex < 0)
    {
      for (uint8_t i=0; i<ledCount; i++)
      {
        leds[i] = CRGB(r,g,b); 
      }
      //Serial.print("Whole strip set to ");

    }
    else
    {
      leds[ledIndex] = CRGB(r,g,b);
      //Serial.print("Index "); Serial.print(ledIndex); Serial.print(" set to "); 
    }
    /*
    Serial.print(r);
    Serial.print(",");
    Serial.print(g);
    Serial.print(",");
    Serial.println(b);
    */
    FastLED.show();
  }
  else
    Serial.println("Invalid RGB value, colour not set");
}

/* pass this function a pointer to an unsigned long to store the start time for the timer */
void LEDController::setTimer(unsigned long *startTime)
{
  *startTime = millis();  // store the current time
}

/* call this function and pass it the variable which stores the timer start time and the desired expiry time 
   returns true fi timer has expired */
bool LEDController::timerExpired(unsigned long startTime, unsigned long expiryTime)
{
  if ( ( millis() - startTime) >= expiryTime )
    return true;
  else
    return false;
}
