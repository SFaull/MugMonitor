/*  Known issues:
 *  - No hysterisis implemented - there is temperature value which will sit on the edge of the threshold and cause
 *    cycling through all states.
 *  - In the "running state", colour always seems to go to red first and then drop back to blue (when the object is cold)
 *  
 *  Feature requests:
 *  - While in standby, collect ambient temperature data and dynamicallt set off_threshold based on temperature history
 *  - Some sort of "optimum drinking temperature" animation or alarm
 */


#include <TinyWireM.h> 
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MLX90614.h>

#define NUM_LEDS 13 // actually only 12 LEDs, one fake one to account for gap by connector
#define DATA_PIN 1
#define LED_UPDATE_TIMEOUT 5
#define STARTUP_ANIMATION_DURATION 500 // 0.5 seconds
#define SWEEP_TIMEOUT 30

// configuration options (all temperatures are scaled up by 10)
#define BRIGHTNESS            200 // 255 is max 
#define TEMP_SENSE_ON         350 // 35.0 degC
#define TEMP_SENSE_OFF        300 // 30.0 degC
#define TEMP_DELTA_THRESHOLD  20  // 2.0 degC
#define TEMP_MAX              500 // 70.0 degC
#define TEMP_MIN              350 // 45.0 degC

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

unsigned long ledTimer;
unsigned long animationTimerA;

uint8_t currentColour[3];

typedef enum {
    kStandby = 0,
    kStartup = 1,
    kRunning = 2,
    kReminder = 3,
    kShutdown = 4
} state_t;

state_t currentState;

uint16_t temp_ambient = 0;
uint16_t temp_object = 0;

uint8_t ledIndex = 0;
uint8_t startupCycles = 0;

void setup() 
{
  mlx.begin();  
  pixel.begin(); // This initializes the NeoPixel library.
  
  currentState = kStandby;
}

void loop() 
{
if(0)
{
  while(1)
  {
    pixel.show(); // This sends the updated pixel color to the hardware.
    temp_object = (uint16_t)(mlx.readObjectTempC()*10.0);
    temp_object/=100;
    for (uint8_t i=0; i<NUM_LEDS; i++)
    {
      if(i<temp_object)
        pixel.setPixelColor(i, pixel.Color(100,100,100));
      else
        pixel.setPixelColor(i, pixel.Color(0,0,0));
    }  
  }
}
  
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
      case kReminder:
          currentState = do_Reminder();
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
    setTimer(&animationTimerA); // reset timer
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

state_t transition_E(void)
{
  // start animation
  setTimer(&animationTimerA); // reset timer
  return kReminder;
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
  if(timerExpired(animationTimerA, SWEEP_TIMEOUT))
  {
    setTimer(&animationTimerA); // reset timer
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
    //Serial.println("No object present!");
    return transition_C();
  }

  // if optimum temperature, do a little animation
  if(temp_object > 50 && temp_object < 60)
  {
    // TODO: needs some sort of timer on this, dont want it going off all the time
    return transition_E();
  }
  
  // update the LED colours based on temp
  if(timerExpired(ledTimer, LED_UPDATE_TIMEOUT))
  {
    setTimer(&ledTimer); // reset timer

    uint8_t r,b;
    
    if (temp_object < TEMP_MIN)
      temp_object=TEMP_MIN;

    if (temp_object > TEMP_MAX)
      temp_object=TEMP_MAX;
    
    r = map(temp_object, TEMP_MIN, TEMP_MAX, 0, BRIGHTNESS);
    b = map(temp_object, TEMP_MIN, TEMP_MAX, BRIGHTNESS, 0);
  
    SetColourTargetAll(r,0,b);
  }


  return kRunning;
}


state_t do_Reminder(void)
{
  static bool targetMet = false;
  static uint8_t brightness = BRIGHTNESS;
  
  if(timerExpired(animationTimerA, 5))
  {
    setTimer(&animationTimerA); // reset timer
    targetMet = SetColourTargetAll(0,brightness, 0);
  }

  if(targetMet)
  {
    targetMet = false;
    
    if (brightness == BRIGHTNESS)
      brightness = 0;
    else if (brightness == 0)
    {
      brightness = BRIGHTNESS;
      return transition_B();
    }
  }

  return kReminder;
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
