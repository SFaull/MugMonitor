/*  Known issues:
 *  - No hysterisis implemented - there is temperature value which will sit on the edge of the threshold and cause
 *    cycling through all states.
 *  - In the "running state", colour always seems to go to red first and then drop back to blue (when the object is cold)
 *  
 */


#include <TinyWireM.h> 
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MLX90614.h>

#define NUM_LEDS 12
#define DATA_PIN 1
#define LED_UPDATE_TIMEOUT 10
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
    kShutdown = 3
} state_t;

state_t currentState;

double temp_ambient = 0;
double temp_object = 0;
double temp_max = 70.0; //deg C
double temp_min = 50.0; //deg C
double cold2hold_threshold = 50.0;  // not considered drinkable below this temp.
double off_threshold = 21.0;  // assumed to be no cup presetent below this temperature

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
  //Serial.println("Object present!");
  // update the LED colours based on temp
  if(timerExpired(ledTimer, LED_UPDATE_TIMEOUT))
  {
    setTimer(&ledTimer); // reset timer

    int r,g,b;
    g = 0;
    
    if (temp_object < temp_min)
      temp_object=temp_min;

    if (temp_object > temp_max)
      temp_object=temp_max;
    
    r = map(temp_object, temp_min, temp_max, 0, BRIGHTNESS);
    b = map(temp_object, temp_min, temp_max, BRIGHTNESS, 0);
  
    SetColourTargetAll(r,g,b);
  }


  return kRunning;
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
  temp_ambient = mlx.readAmbientTempC();
  temp_object = mlx.readObjectTempC();
}

/* object is assumed present if there is a large enough positive delta between object temp and ambient temp */
bool isObjectPresent(void)
{
  const double threshold = 2.0;
  double temp_delta = temp_object - temp_ambient;
  // check delta is large enough and that object temperature exceeds minimum temperature
  // TODO: should also be a condition in here to check that  temp_object > temp_min
  if (temp_delta > threshold && temp_object > off_threshold)
    return true;

  return false;
}

/* pass this function a pointer to an unsigned long to store the start time for the timer */
void setTimer(unsigned long *startTime)
{
  *startTime = millis();;  // store the current time
}

/* call this function and pass it the variable which stores the timer start time and the desired expiry time 
   returns true fi timer has expired */
bool timerExpired(unsigned long startTime, unsigned long expiryTime)
{
  if ( (millis() - startTime) >= expiryTime )
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
  
  for (int i=0; i<NUM_LEDS; i++)
    pixel.setPixelColor(i, pixel.Color(r,g,b));  
}

void SetColourTargetAll(uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t temp[3] = {r,g,b};

  for (int i= 0; i<3; i++)
  {
    if(currentColour[i]<temp[i])
      currentColour[i]++;
    else if (currentColour[i]>temp[i])
      currentColour[i]--;
  }
  
  for (int i=0; i<NUM_LEDS; i++)
    pixel.setPixelColor(i, pixel.Color(currentColour[0],currentColour[1],currentColour[2]));  
}
