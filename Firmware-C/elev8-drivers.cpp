/**
 * This is the main elev8-drivers program file.
 */

#include <propeller.h>
#include "pins.h"
#include <simpletext.h>
#include "drivertable.h"
#include "eeprom.h"
#include "beep.h"
#include "f32.h"    
#include "led_simple.h"


#define driver_size(id) (uint32_t)(binary_##id##_dat_size)
#define use_driver(id) extern uint32_t binary_##id##_dat_start[], binary_##id##_dat_size[]
#define driver_address(id) (char *)(binary_##id##_dat_start)

//#define DO_PRINT

enum IMU_VarLabels {
    #include "QuatIMU_Vars.inc"
    IMU_VARS_SIZE                    // This entry MUST be last so we can compute the array size required
};

const char *DriverNames[NumDrivers] = {
  "F32",
  "PPM",
  "RC",
  "RemoteRX",
  "SBUS",
  "Sensors",
  "Servo32",

  "Mag_Init",
  "Mag_AddSample",
  "Mag_SetupIter",
  "Mag_CalcIter",
};


unsigned char QuatIMU_Mag_InitCalibrate[] = {
  #include "QuatIMU_Mag_InitCalibrate.inc"
};

unsigned char QuatIMU_Mag_AddCalibratePoint[] = {
  #include "QuatIMU_Mag_AddCalibratePoint.inc"
};

unsigned char QuatIMU_Mag_ComputeCalibrate_SetupIteration[] = {
  #include "QuatIMU_Mag_ComputeCalibrate_SetupIteration.inc"
};

unsigned char QuatIMU_Mag_ComputeCalibrate_IterationStep[] = {
  #include "QuatIMU_Mag_ComputeCalibrate_IterationStep.inc"
};

// Rounds a size up to the nearest eeprom page length
int PageRound( int v ) {return v; /*(v+63) & ~63;*/}

int LEDValue;
const int LED_COUNT = 1;

//LED Brightness values - AND with color values to dim them
const int LED_Full    = 0xffffff;
const int LED_Half    = 0x7f7f7f;
const int LED_Quarter = 0x3f3f3f;
const int LED_Eighth  = 0x1f1f1f;
const int LED_Dim     = 0x0f0f0f;


//LED Color values
const int LED_Red   = 0x00FF00;
const int LED_Green = 0xFF0000;
const int LED_Blue  = 0x0000FF;
const int LED_White = 0xFFFFFF;
const int LED_Yellow = LED_Red | LED_Green;
const int LED_Violet = LED_Red | LED_Blue;
const int LED_Cyan =   LED_Blue | LED_Green;

const int LED_DimCyan = (LED_Blue | LED_Green) & LED_Half;
const int LED_DimWhite = LED_White & LED_Half;


int main(void)
{
  LEDValue = LED_Red & LED_Quarter;
  LED_Start( PIN_LED, (int)&LEDValue, LED_COUNT );


  // set up the table of information about the drivers
  memset( &drivers, 0, sizeof(drivers) );

  // The use_cog_driver(DriverName) macro basically does this:

    // extern uint32_t binary_DriverName_dat_start;
    // extern uint32_t binary_DriverName_dat_end;

  // Those symbols are generated by the linker when linking in the SPIN driver objects
  // We use them here to construct a table of the drivers we're pushing into the upper eeprom

  use_driver(f32_driver);
  use_driver(rc_driver_ppm);
  #if defined( __PINS_V3_H__ )
  use_driver(rc_driver_v3);
  #endif
  #if defined( __PINS_V2_H__ )
  use_driver(rc_driver_v2);
  #endif
  use_driver(remote_rx_driver);
  use_driver(sbus_driver);
  use_driver(sensors_driver);
  use_driver(servo32_highres_driver);

  drivers.Table[DRV_F32].Size =      driver_size(f32_driver);
  drivers.Table[DRV_PPM].Size =      driver_size(rc_driver_ppm);
  #if defined( __PINS_V2_H__ )
  drivers.Table[DRV_RC].Size =       driver_size(rc_driver_v2);
  #endif
  #if defined( __PINS_V3_H__ )
  drivers.Table[DRV_RC].Size =       driver_size(rc_driver_v3);
  #endif
  drivers.Table[DRV_RemoteRX].Size = driver_size(remote_rx_driver);
  drivers.Table[DRV_SBus].Size =     driver_size(sbus_driver);
  drivers.Table[DRV_Sensors].Size =  driver_size(sensors_driver);
  drivers.Table[DRV_Servo32].Size =  driver_size(servo32_highres_driver);

  drivers.Table[DRV_Mag_Init].Size =      sizeof(QuatIMU_Mag_InitCalibrate);
  drivers.Table[DRV_Mag_AddSample].Size = sizeof(QuatIMU_Mag_AddCalibratePoint);
  drivers.Table[DRV_Mag_SetupIter].Size = sizeof(QuatIMU_Mag_ComputeCalibrate_SetupIteration);
  drivers.Table[DRV_Mag_CalcIter].Size =  sizeof(QuatIMU_Mag_ComputeCalibrate_IterationStep);

  char * DriverAddr[NumDrivers];

  DriverAddr[DRV_F32] =      driver_address(f32_driver);
  DriverAddr[DRV_PPM] =      driver_address(rc_driver_ppm);
  #if defined( __PINS_V2_H__ )
  DriverAddr[DRV_RC] =       driver_address(rc_driver_v2);
  #endif
  #if defined( __PINS_V3_H__ )
  DriverAddr[DRV_RC] =       driver_address(rc_driver_v3);
  #endif
  DriverAddr[DRV_RemoteRX] = driver_address(remote_rx_driver);
  DriverAddr[DRV_SBus] =     driver_address(sbus_driver);
  DriverAddr[DRV_Sensors] =  driver_address(sensors_driver);
  DriverAddr[DRV_Servo32] =  driver_address(servo32_highres_driver);

  DriverAddr[DRV_Mag_Init] =      (char *)QuatIMU_Mag_InitCalibrate;
  DriverAddr[DRV_Mag_AddSample] = (char *)QuatIMU_Mag_AddCalibratePoint;
  DriverAddr[DRV_Mag_SetupIter] = (char *)QuatIMU_Mag_ComputeCalibrate_SetupIteration;
  DriverAddr[DRV_Mag_CalcIter] =  (char *)QuatIMU_Mag_ComputeCalibrate_IterationStep;


  unsigned int TableAddr = DriverTableStart;   // 0x8000 is the start of eeprom, but we have prefs there, so push it up 4kb
  unsigned int StartAddr = TableAddr + PageRound(sizeof(DRIVERS));

  drivers.Table[0].Offset = StartAddr;
  for( int i=1; i<NumDrivers; i++ ) {
    drivers.Table[i].Offset = drivers.Table[i-1].Offset + PageRound(drivers.Table[i-1].Size);
  }

  drivers.MagicNumber = DriverTableMagic;
  drivers.Count = NumDrivers;
  drivers.Version = DriverTableVersion;
  drivers.MaxSize = 0;

  #ifdef DO_PRINT
  printi( "Driver layout:\r" );
  char name[16];
  #endif

  int total = 0;
  for( int i=0; i<NumDrivers; i++ ) {
    #ifdef DO_PRINT
    memset(name, 32, sizeof(name));
    name[15] = 0;
    memcpy(name, DriverNames[i], strlen(DriverNames[i]) );

    printi( "%s:  0x%4x  %4d bytes\r", name, drivers.Table[i].Offset, drivers.Table[i].Size );
    #endif
    total += drivers.Table[i].Size;
    if( drivers.MaxSize < drivers.Table[i].Size ) {
      drivers.MaxSize = drivers.Table[i].Size;
    }
  }

  #ifdef DO_PRINT
  printi("---------------------------------\r");
  printi("Total:                    %4d bytes\n", total);
  printi("Uploading to EEPROM...\r");
  #endif

  // Upload the header
  EEPROM::FromRam(&drivers, ((char *)&drivers) + sizeof(drivers)-1, TableAddr);

  #ifdef DO_PRINT
  printi("...Header uploaded\r");
  #endif

  for( int i=0; i<NumDrivers; i++ )
  {
    char * start = DriverAddr[i];
    char * end = start + drivers.Table[i].Size-1;

    EEPROM::FromRam(start, end, drivers.Table[i].Offset );

    #ifdef DO_PRINT
    printi("...");
    printi(DriverNames[i]);
    printi(" uploaded\r");
    #endif

    Beep();
    LEDValue ^= LED_Green & LED_Quarter;  // Toggle the LED from red to yellow while uploading drivers
  }    

  DIRA |= (1<<PIN_BUZZER_1) | (1<<PIN_BUZZER_2);      //Enable buzzer pins
  OUTA &= ~((1<<PIN_BUZZER_1) | (1<<PIN_BUZZER_2));   //Set the pins low

  #ifdef DO_PRINT
  printi( "\nDone!" );
  #endif

  LEDValue = LED_Green & LED_Quarter;
  Beep3();

  while(true) {
      waitcnt(CNT + 80000000);
  }    

  return 0;
}
