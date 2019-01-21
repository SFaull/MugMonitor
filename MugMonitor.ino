#include <Wire.h>
#include "FastLED.h"
#include "LedController.h"
#include <Adafruit_MLX90614.h>

#define NUM_LEDS 1
#define DATA_PIN 3
#define BUZZER_PIN 8
#define LED_UPDATE_TIMEOUT 20

// Define the array of leds
CRGB leds[NUM_LEDS];
// Create an instance of the led controller class
LEDController ledController(leds);
// Create an instance of the sensor class
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

unsigned long runTime;
unsigned long ledTimer;

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
double temp_min = 10.0; //deg C
double cold2hold_threshold = 50.0;  // not considered drinkable below this temp.
double off_threshold = 21.0;  // assumed to be no cup presetent below this temperature

void setup() 
{
  // initialise sensor
  mlx.begin();  
  Serial.begin(115200);
  // initialise LED strip
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  // ensure LEDS are off
  ledController.setColour(0,0,0);
  ledController.setColourTarget(0,0,0);
  
  currentState = kStandby;
}

void loop() 
{
  ledController.run();
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
    //ledController.setColourTarget(255,255,255); // TODO: change to some sort of sweeping animation to the temperature mapped colour
    Serial.println("Transition A");
    return kStartup;
}
state_t transition_B(void)
{
    // do nothing
    Serial.println("Transition B");
    return kRunning;
}

state_t transition_C(void)
{
  // TODO: intiate shutdown animation
  ledController.setColourTarget(0,0,0);
  Serial.println("Transition C");
    return kShutdown;
}

state_t transition_D(void)
{
    // do nothing
    Serial.println("Transition D");
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
  // animation process
  return transition_B();
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
  updateLEDs();
  return kRunning;
}

state_t do_Shutdown(void)
{
  //Serial.println("Shutdown");
   return transition_D();
}

void updateReadings(void)
{
  // read the sensor
  temp_ambient = mlx.readAmbientTempC();
  temp_object = mlx.readObjectTempC();
    // print debug info
  //Serial.print("Ambient: "); Serial.println(temp_ambient);
  //Serial.print("Object: "); Serial.println(temp_object);
}

void updateLEDs(void)
{
  // TODO update these values based on temperature
  //(temp_object - temp_min)/(temp_max - temp_min) * 256

            /* Periodically update the LEDs */
      //if(timerExpired(ledTimer, LED_UPDATE_TIMEOUT))
      //{
        setTimer(&ledTimer); // reset timer
        int r = map(temp_object, temp_min, temp_max, 0, 256);
        int g = 0;
        int b = map(temp_object, temp_min, temp_max, 256, 0);
      
        ledController.setColourTarget(r,g,b);
      //}
  

}

/* object is assumed present if there is a large enough positive delta between object temp and ambient temp */
bool isObjectPresent(void)
{
  const double threshold = 2.0;
  double temp_delta = temp_object - temp_ambient;
  //Serial.println(temp_delta);

  // check delta is large enough and that object temperature exceeds minimum temperature
  if (temp_delta > threshold && temp_object > off_threshold)
    return true;

  return false;
}

void soundAlarm(void)
{
    /*Tone needs 2 arguments, but can take three
    1) Pin#
    2) Frequency - this is in hertz (cycles per second) which determines the pitch of the noise made
    3) Duration - how long teh tone plays
  */
  tone(BUZZER_PIN, 1000, 500);
}

/* pass this function a pointer to an unsigned long to store the start time for the timer */
void setTimer(unsigned long *startTime)
{
  runTime = millis();    // get time running in ms
  *startTime = runTime;  // store the current time
}

/* call this function and pass it the variable which stores the timer start time and the desired expiry time 
   returns true fi timer has expired */
bool timerExpired(unsigned long startTime, unsigned long expiryTime)
{
  runTime = millis(); // get time running in ms
  if ( (runTime - startTime) >= expiryTime )
    return true;
  else
    return false;
}
