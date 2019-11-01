// Devlopment tools
//#define CAL_MODE  // uncomment to enter calibration mode (temperature displayed as clock, format rg.b degrees)

// Hardware configuration
#define NUM_LEDS    11
#define DATA_PIN    1


// Customisations
#define LED_UPDATE_TIMEOUT          5
#define STARTUP_ANIMATION_DURATION  500 // 0.5 seconds
#define SWEEP_TIMEOUT               30
#define BRIGHTNESS                  200 // 255 is max 


/*  The following threshold define the temperature at which colour changes occur:
 *                    |
 *                    | Red
 * TEMP_OPTIMAL_UPPER--
 *                    | Green
 * TEMP_OPTIMAL_MID----
 *                    | Green (Flashing)
 * TEMP_OPTIMAL_LOWER--
 *                    | Blue
 * TEMP_MIN------------
 *                    | Off
 *                    |
 */

#define TEMP_OPTIMAL_UPPER    600 // 60.0 degC
#define TEMP_OPTIMAL_MID      500 // 56.0 degC
#define TEMP_OPTIMAL_LOWER    430 // 43.0 degC
#define TEMP_MIN              350 // 35.0 degC

// NOTE: all temperatures are scaled up by 10
#define TEMP_SENSE_ON         350 // 35.0 degC
#define TEMP_SENSE_OFF        300 // 30.0 degC
#define TEMP_DELTA_THRESHOLD  20  // 2.0 degC

#define HYSTERESIS            10



 
