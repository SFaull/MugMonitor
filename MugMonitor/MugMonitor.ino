/*  Known issues:
 *  - No hysterisis implemented - there is temperature value which will sit on the edge of the threshold and cause
 *    cycling through all states.
 *  - In the "running state", colour always seems to go to red first and then drop back to blue (when the object is cold)
 *  
 *  Feature requests:
 *  - While in standby, collect ambient temperature data and dynamicallt set off_threshold based on temperature history
 *  - Some sort of "optimum drinking temperature" animation or alarm
 */

/* Includes */
#include <TinyWireM.h> 
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MLX90614.h>
#include "config.h"

/* Class instances */
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

/* Custom types */
typedef enum {
    kStandby = 0,
    kStartup = 1,
    kRunning = 2,
    kShutdown = 3
} state_t;

typedef enum {
  kCold = 0,
  kCool,
  kWarm,
  kHot,
  kScolding
} temperature_t;

/* Global Variables */
const uint16_t thresholds[] = { TEMP_MIN , TEMP_OPTIMAL_LOWER, TEMP_OPTIMAL_MID, TEMP_OPTIMAL_UPPER};
const int thresholdCount = sizeof(thresholds)/sizeof(uint16_t);
unsigned long ledTimer;
unsigned long animationTimer;
uint8_t currentColour[3];
state_t currentState;
uint16_t temp_ambient = 0;
uint16_t temp_object = 0;
uint8_t ledIndex = 0;
uint8_t startupCycles = 0;


/* Initialisation */
void setup() 
{
  // initialise the sensor and LEDs
  mlx.begin();  
  pixel.begin();
  
  currentState = kStandby;
}

/* Main code entry point */
void loop() 
{
#ifdef CAL_MODE
  while(1)
  {
    pixel.show(); // This sends the updated pixel color to the hardware.
    temp_object = (uint16_t)(mlx.readObjectTempC()*10.0);
    uint8_t tens = temp_object/100;
    uint8_t ones = (temp_object%100)/10;
    uint8_t decimal = ((temp_object%100)%10);
    for (uint8_t i=0; i<NUM_LEDS; i++)
    {
      uint8_t r = 0, g = 0, b = 0;

      if(i==tens-1)        r = BRIGHTNESS;
      if(i==ones-1)        g = BRIGHTNESS;
      if(i==decimal-1)     b = BRIGHTNESS;
      
      pixel.setPixelColor(i, pixel.Color(r,g,b));
    }  

    //delay(500);
  }
#endif
  
  pixel.show(); // This sends the updated pixel color to the hardware.
  switch(currentState)
  {
      case kStandby:
          currentState = do_Standby();
          break;
      case kStartup:
          currentState = do_Startup();
          break;
      case kRunning:
          currentState = do_Running();
          break;
      case kShutdown:
          currentState = do_Shutdown();
          break;
      default:
          currentState = kStandby;
          break;
  }
}


state_t transition_A(void)
{
    // TODO: intiate startup animation
    SetColourAll(0,0,0);
    setTimer(&animationTimer); // reset timer
    ledIndex=0;
    startupCycles=0;
    return kStartup;
}
state_t transition_B(void)
{
    SetColourAll(0,0,0);
    return kRunning;
}

state_t transition_C(void)
{
    setTimer(&ledTimer); // reset timer
    return kShutdown;
}

state_t transition_D(void)
{
    SetColourAll(0,0,0);
    return kStandby;
}

state_t do_Standby(void)
{
  updateReadings();
  if (isObjectPresent())
    return transition_A();
  return kStandby;
}

state_t do_Startup(void)
{
  // at the end of each animation cycle see, whetehr the object is still there ( exit if not)
  if(ledIndex >= NUM_LEDS)
  {
    ledIndex = 0;
    startupCycles++;
    
     updateReadings();
      if (!isObjectPresent())
        return transition_D();
  }

  // Once all animations complete
  if (startupCycles >= 3)
        return transition_B();

  // do animation
  if(timerExpired(animationTimer, SWEEP_TIMEOUT))
  {
    setTimer(&animationTimer); // reset timer
    SetColourAll(0,0,0);
    SetColour(ledIndex++,BRIGHTNESS,BRIGHTNESS,BRIGHTNESS);
  }
  
  return kStartup;
}

state_t do_Running(void)
{
  // take temperature readings
  updateReadings();
  
  // if object gone, shutdown
  if (isObjectRemoved())
  {
    return transition_C();
  }

  // update the LED colours based on temp
  if(timerExpired(ledTimer, LED_UPDATE_TIMEOUT))
  {
    uint8_t r = 0, g = 0, b = 0;
    setTimer(&ledTimer); // reset timer

    temperature_t tempState = getTemperatureState();

    switch(tempState)
    {
      case kCold:       /* do nothing */        break;
      case kCool:       b = BRIGHTNESS;         break;
      case kWarm:       flash_green_process();  return kRunning;
      case kHot:        g = BRIGHTNESS;         break;
      case kScolding:   r = BRIGHTNESS;         break;
    }
  
    SetColourTargetAll(r,g,b);
  }
  
  return kRunning;
}

state_t do_Shutdown(void)
{
  if(timerExpired(ledTimer, LED_UPDATE_TIMEOUT))
  {
    setTimer(&ledTimer); // reset timer
    SetColourTargetAll(0,0,0);
  }

  if(currentColour[0] == 0 && currentColour[1] == 0 && currentColour[2] == 0)
   return transition_D();

  return kShutdown;
}

void flash_green_process(void)
{
  static bool targetMet = false;
  static uint8_t brightness = BRIGHTNESS;
  
  targetMet = SetColourTargetAll(0,brightness, 0);

  if(targetMet)
  {
    targetMet = false;
    
    if (brightness == BRIGHTNESS)
      brightness = 0;
    else if (brightness == 0)
      brightness = BRIGHTNESS;
  }
}

void updateReadings(void)
{
  // read the sensor
  temp_ambient = (uint16_t)(mlx.readAmbientTempC()*10.0);
  temp_object = (uint16_t)(mlx.readObjectTempC()*10.0);
}

/* object is assumed present if there is a large enough positive delta between object temp and ambient temp */
bool isObjectPresent(void)
{
  uint16_t temp_delta = temp_object - temp_ambient;
  
  // check delta is large enough and that object temperature exceeds minimum temperature
  if (temp_delta > TEMP_DELTA_THRESHOLD && temp_object > TEMP_SENSE_ON)
    return true;

  return false;
}

// assumes aboject was present in the first place
bool isObjectRemoved(void)
{
  // TODO: this could be achieved a different way by looking for a rapid decrease in value
  
  uint16_t temp_delta = temp_object - temp_ambient;
  
  // check delta is large enough and that object temperature exceeds minimum temperature
  // TODO: should also be a condition in here to check that  temp_object > TEMP_MIN
  if (/*temp_delta < threshold && */temp_object < TEMP_SENSE_OFF)
    return true;

  return false;
}

/* pass this function a pointer to an unsigned long to store the start time for the timer */
void setTimer(unsigned long *startTime)
{
  *startTime = millis();  // store the current time
}

/* call this function and pass it the variable which stores the timer start time and the desired expiry time 
   returns true fi timer has expired */
bool timerExpired(unsigned long startTime, unsigned long expiryTime)
{
  if( (millis() - startTime) >= expiryTime )
    return true;
  return false;
}

void SetColour(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    pixel.setPixelColor(index, pixel.Color(r,g,b));  
}

void SetColourAll(uint8_t r, uint8_t g, uint8_t b)
{  
  currentColour[0] = r;
  currentColour[1] = g;
  currentColour[2] = b;
  
  for (uint8_t i=0; i<NUM_LEDS; i++)
    pixel.setPixelColor(i, pixel.Color(r,g,b));  
}

bool SetColourTargetAll(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t temp[3] = {r,g,b};
  bool targetMet = true;

  for (uint8_t i= 0; i<3; i++)
  {
    if(currentColour[i]<temp[i])
      currentColour[i]++;
    else if (currentColour[i]>temp[i])
      currentColour[i]--;

    targetMet &= (currentColour[i]==temp[i]);
  }
  
  for (uint8_t i=0; i<NUM_LEDS; i++)
    pixel.setPixelColor(i, pixel.Color(currentColour[0],currentColour[1],currentColour[2]));

  return targetMet;
}

temperature_t getTemperatureState()
{
  static int objectState = 0;

  while (objectState < thresholdCount && temp_object >= thresholds[objectState]) {
    ++objectState;
  }

  while (objectState > 0 && temp_object < thresholds[objectState-1] - HYSTERESIS) {
    --objectState;
  }

  return (temperature_t)objectState;
}


/* Compile time config value checks */

#if (TEMP_OPTIMAL_UPPER < TEMP_OPTIMAL_MID)
  #error "TEMP_OPTIMAL_UPPER must be greater than TEMP_OPTIMAL_MID"
#endif
#if (TEMP_OPTIMAL_MID < TEMP_OPTIMAL_LOWER)
  #error "TEMP_OPTIMAL_MID must be greater than TEMP_OPTIMAL_LOWER"
#endif
#if (TEMP_OPTIMAL_LOWER < TEMP_MIN)
  #error "TEMP_OPTIMAL_LOWER must be greater than TEMP_MIN"
#endif
