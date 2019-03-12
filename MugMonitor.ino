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

#define NUM_LEDS 12
#define DATA_PIN 1
#define LED_UPDATE_TIMEOUT 5
#define STARTUP_ANIMATION_DURATION 500 // 0.5 seconds
#define SWEEP_TIMEOUT 30

#define BRIGHTNESS 255

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
// Create an instance of the sensor class
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

// all temperatures are scaled up by 10
uint16_t temp_ambient = 0;
uint16_t temp_object = 0;
uint16_t temp_max = 700; // 70.0 deg C
uint16_t temp_min = 400; // 40.0 deg C


void setup() 
{
  // initialise sensor
  mlx.begin();  
  // initialise LED strip
  pixel.begin(); // This initializes the NeoPixel library.
  
  currentState = kStandby;
}

void loop() 
{
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
uint8_t ledIndex = 0;
uint8_t startupCycles = 0;

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
    // do nothing
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
  //Serial.println("Standby");
    
  updateReadings();
  if (isObjectPresent())
    return transition_A();
  return kStandby;
}

state_t do_Startup(void)
{
  //Serial.println("Startup");

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
  //Serial.println("Running");
  // take temperature readings
  updateReadings();
  
  // if object gone, shutdown
  if (!isObjectPresent())
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
  
  //Serial.println("Object present!");
  // update the LED colours based on temp
  if(timerExpired(ledTimer, LED_UPDATE_TIMEOUT))
  {
    setTimer(&ledTimer); // reset timer

    uint8_t r,b;
    
    if (temp_object < temp_min)
      temp_object=temp_min;

    if (temp_object > temp_max)
      temp_object=temp_max;
    
    r = map(temp_object, temp_min, temp_max, 0, BRIGHTNESS);
    b = map(temp_object, temp_min, temp_max, BRIGHTNESS, 0);
  
    SetColourTargetAll(r,0,b);
  }


  return kRunning;
}


state_t do_Reminder(void)
{
  static bool targetMet = false;
  static uint8_t brightness = BRIGHTNESS;
  //Serial.println("Reminder");

  if(timerExpired(animationTimerA, 5))
  {
    setTimer(&animationTimerA); // reset timer
    // TODO: animate a green pulse to indicate good drinking temperature
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
  //Serial.println("Shutdown");
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
  temp_ambient = (uint16_t)(mlx.readAmbientTempC()*10);
  temp_object = (uint16_t)(mlx.readObjectTempC()*10);
}

/* object is assumed present if there is a large enough positive delta between object temp and ambient temp */
bool isObjectPresent(void)
{
  const uint8_t off_threshold = 240;  // assumed to be no cup presetent below this temperature
  const uint8_t threshold = 20;
  uint16_t temp_delta = temp_object - temp_ambient;
  
  // check delta is large enough and that object temperature exceeds minimum temperature
  // TODO: should also be a condition in here to check that  temp_object > temp_min
  if (temp_delta > threshold && temp_object > off_threshold)
    return true;

  return false;
}

// assumes aboject was present in the first place
bool isObjectRemoved(void)
{
  const uint8_t off_threshold = 210;  // assumed to be no cup presetent below this temperature
  const uint8_t threshold = 20;
  uint16_t temp_delta = temp_object - temp_ambient;
  
  // check delta is large enough and that object temperature exceeds minimum temperature
  // TODO: should also be a condition in here to check that  temp_object > temp_min
  if (temp_delta < threshold && temp_object < off_threshold)
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
