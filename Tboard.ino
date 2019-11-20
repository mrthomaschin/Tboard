/*************************/
/********LIBRARIES********/
/*************************/
#include <crc.h>
#include <VescUart.h>
#include <datatypes.h>
#include <buffer.h>
#include <ArduinoNunchuk.h>

/*************************/
/********VARIABLES********/
/*************************/
VescUart VESCUART;
ArduinoNunchuk nunchuk = ArduinoNunchuk();
int throttle = 0; //Controls speed. Low = 33, Mid = 129, High = 230
float current = 0.0;
bool freeSwitch = 0;
String command;

/*************************/
/********FUNCTIONS********/
/*************************/

//Converts analog signal from nunchuck
void JoystickADC()
{
  nunchuk.update();
  throttle = nunchuk.analogY;
}

//Converts analog signal from deck sensors
void SensorADC()
{
  
}

/*************************/
/******STATE MACHINE******/
/*************************/

enum States_Free_Switch {SWITCH_INIT, SWITCH_ON, SWITCH_OFF, SWITCH_SETON, SWITCH_SETOFF} state1;
void checkFree()
{
  //Transitions
  switch(state1)
  {
    case SWITCH_INIT:
      state1 = SWITCH_OFF;
      
      break;
      
    case SWITCH_OFF:
      if(nunchuk.zButton == 0)
        state1 = SWITCH_OFF;
      else
        state1 = SWITCH_SETON;
      
      break;

    case SWITCH_ON:
      if(nunchuk.zButton == 0)
        state1 = SWITCH_ON;
      else
        state1 = SWITCH_SETOFF;
      
      break;

    case SWITCH_SETON:
      if(nunchuk.zButton == 1)
        state1 = SWITCH_SETON;
      else
        state1 = SWITCH_ON;
      
      break;

    case SWITCH_SETOFF:
      if(nunchuk.zButton == 1)
        state1 = SWITCH_SETOFF;
      else
        state1 = SWITCH_OFF;
      
      break;
  }
  
  //Actions
  switch(state1)
  {
     case SWITCH_INIT:
      break;
      
    case SWITCH_OFF:
      freeSwitch = 0;
      
      break;

    case SWITCH_ON:
      freeSwitch = 1;
      
      break;

    case SWITCH_SETON:
      break;

    case SWITCH_SETOFF:
      break;
  }
}

enum States {INIT, LOCK, FREE, NEUTRAL, ACCEL, DECEL} state;
void runBoard() 
{
//  Init: Initial setup
//  Lock: Board is controlled by sensors and remote. Both feet are not on deck, board slows down until stopped.
//  Free: board rolls freely
//  Netural: Both feet are on deck, joystick in 0 position. Board rolls freely
//  Accel: Board accelerates depending on position of joystick
//  Decel: Board decelerates depending on position of joystick

  
  //Transitions
  switch(state)
  {
    case INIT:
      if(freeSwitch == 0) 
        state = LOCK;
      else
        state = FREE;
        
      Serial.println("Init");
      state = LOCK; 
      break;

  
    case LOCK:
      if(freeSwitch == 1) //If FREE switch activated, state FREE
        state = FREE;
//      
//      if(/*both feet*/ == 1) //If both feet are on, board is neutral. Otherwise, lock board
//        state = NEUTRAL;
//      else
//        state = LOCK;

      state = NEUTRAL;
        
      break;


    case FREE:
      if(freeSwitch == 0) //If FREE switch deactivated, state LOCK. Otherwise, stay FREE
        state = LOCK;
      else
        state = FREE;
        
      break;

      
    case NEUTRAL:
      if(freeSwitch == 1) //If FREE switch activated, state FREE
         state = FREE;
      else
        if(throttle >= 131) //If joystick moves up, accelerate (+- 4 threshold)
          state = ACCEL;
        else
          if(throttle <= 123) //If joystick moves down, decelerate
            state = DECEL;
          else
            if(throttle > 123 && throttle < 131)
                state = NEUTRAL;

//   
//      if(/*both feet*/ == 0) //If both feet are off, lock board
//        state = LOCK;
      break;


    case ACCEL:
      if(freeSwitch == 1) //If FREE switch activated, state FREE
        state = FREE;
//
//      if(/*both feet*/ == 0) //If both feet are off, lock board
//        state = LOCK;
//
      if(throttle < 131) //If joystick moves down
      {
        if(VESCUART.data.rpm == 0) //Reverse
          state = NEUTRAL;   

        VESCUART.setCurrent(-2.0); //Slow down
      }
      else
        state = ACCEL;
         
      break;

    case DECEL:
      if(freeSwitch == 1) //If FREE switch activated, state FREE
        state = FREE;
//
//      if(/*both feet*/ == 0) //If both feet are off, lock board
//        state = LOCK;
//
      if(throttle > 123) //If joystick moves up, accelerate
      {
        if(VESCUART.data.rpm == 0) //Accel
          state = NEUTRAL;

        VESCUART.setCurrent(2.0); //Slow down
      }
      else  
        state = DECEL;
        
      break;
  }


  //Actions
  switch(state)
  {
    case INIT:
      break;

    case LOCK:
      break;

    case FREE:
      Serial.println("Free: ");
      JoystickADC();
      
      break;

    case NEUTRAL:
      Serial.print("Neutral: ");
      Serial.print(VESCUART.nunchuck.valueY); 
      Serial.print("   Current: ");
      Serial.println(current);  
      current = 0.0; 
      VESCUART.setCurrent(current);

      JoystickADC();
      
      break;

    case ACCEL:
      JoystickADC();
      VESCUART.nunchuck.valueY = throttle;
      Serial.print("Accel: ");
      Serial.print(VESCUART.nunchuck.valueY); 
      Serial.print("   Current: ");
      Serial.println(current);  
      current = (VESCUART.nunchuck.valueY - 130) * 0.34;
      VESCUART.setCurrent(current);
      
      break;

    case DECEL:
      JoystickADC();
      VESCUART.nunchuck.valueY = throttle;        
      Serial.print("Decel:  ");
      Serial.print(VESCUART.nunchuck.valueY); 
      Serial.print("   Current: ");
      Serial.println(current);
      current = -(abs(VESCUART.nunchuck.valueY - 130) * 0.34); //Hard stop, reverse

        
      VESCUART.setCurrent(current);
      
      break;
  }
}

/*************************/
/***********MAIN**********/
/*************************/

void setup() 
{
  //Initialize input
  Serial.begin(115200);
  VESCUART.setSerialPort(&Serial);
  nunchuk.init();
  //Initialize output

}

void loop() 
{
  runBoard();
  checkFree();
}
