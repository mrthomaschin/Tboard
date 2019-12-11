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
bool nunchukConnected = 0;
bool setupComplete = 0;

int throttle = 0; //Controls speed. Low = 2, Mid = 127, High = 255
float current = 0.0;

const int frontSensorPin = A0;
const int backSensorPin = A1;
int sensorValue;

int ledPin1 = 4; //Head L
int ledPin2 = 5; //H ead R
int ledPin3 = 6; //Back L
int ledPin4 = 7; //Back R

int melodyOn[] = { 587, 740, 880, 1175 }; // D, F#, A, D
int melodyOnDuration[] = { 8, 8, 8, 4 };

int melodySensor[] = { 587, 880, 587, 0 }; //D, A, D
int melodySensorDuration[] = { 8, 8, 8, 4 };

int melodyLock[] = { 392, 392, 392, 0 }; //G, G, G
int melodyLockDuration[] = { 8, 8, 8, 4 };

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
bool SensorADC()
{ 
  int frontSensorValue;
  int backSensorValue;
  bool feetOnDeck = 0;

  
  frontSensorValue = analogRead(frontSensorPin);
  backSensorValue = analogRead(backSensorPin);
  Serial.print(frontSensorValue);
  Serial.print(" ");
  Serial.println(backSensorValue);

//  frontSensorValue = 2; backSensorValue = 2; //Override lock, for testing purposes

  if(frontSensorValue < 21 || backSensorValue < 38) //Threshold for front and back sensors
    feetOnDeck = 1;
  else
    feetOnDeck = 0;
    
  return feetOnDeck;
}

void playMelody( int melodyToPlay[], int melodyDuration[] )
{
  int duration; 
  
  for(int x = 0; x < 4; x++)
  {
    duration = 1000 / melodyDuration[x];
    tone(9, melodyToPlay[x], duration);

    int pauseBetweenNotes = duration * 1.3;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(9);
  }
}

/*************************/
/******STATE MACHINE******/
/*************************/

enum States_Headlight {LIGHT_INIT, LIGHT_ON, LIGHT_OFF, LIGHT_SETON, LIGHT_SETOFF} state2;
void headlight()
{
  //Transitions
  switch(state2)
  {
    case LIGHT_INIT:
      state2 = LIGHT_OFF;
      
      break;
      
    case LIGHT_OFF:
      if(nunchuk.cButton == 0)
        state2 = LIGHT_OFF;
      else
        state2 = LIGHT_SETON;
      
      break;

    case LIGHT_ON:
      if(nunchuk.cButton == 0)
        state2 = LIGHT_ON;
      else
        state2 = LIGHT_SETOFF;
      
      break;

    case LIGHT_SETON:
      if(nunchuk.cButton == 1)
        state2 = LIGHT_SETON;
      else
        state2 = LIGHT_ON;
      
      break;

    case LIGHT_SETOFF:
      if(nunchuk.cButton == 1)
        state2 = LIGHT_SETOFF;
      else
        state2 = LIGHT_OFF;
      
      break;
  }
  
  //Actions
  switch(state2)
  {
     case LIGHT_INIT:
      break;
      
    case LIGHT_OFF:
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, LOW);
      digitalWrite(ledPin3, LOW);
      digitalWrite(ledPin4, LOW);
      
      break;

    case LIGHT_ON:
      digitalWrite(ledPin1, HIGH);
      digitalWrite(ledPin2, HIGH);
      digitalWrite(ledPin3, HIGH);
      digitalWrite(ledPin4, HIGH);
      
      break;

    case LIGHT_SETON:
      break;

    case LIGHT_SETOFF:
      break;
  }
}

enum States {INIT, LOCK, NEUTRAL, ACCEL, DECEL} state;
void runBoard() 
{
//  Init: Initial setup
//  Lock: Board is controlled by sensors and remote. Both feet are not on deck, board slows down until stopped.
//  Netural: Both feet are on deck, joystick in 0 position. Board rolls freely
//  Accel: Board accelerates depending on position of joystick
//  Decel: Board decelerates depending on position of joystick

  
  //Transitions
  switch(state)
  {
    case INIT:
      if(SensorADC() == 0) 
        state = LOCK;
      else
        state = NEUTRAL;
        
      break;

  
    case LOCK:
      if(SensorADC() == 1) //If both feet are on, board is neutral. Otherwise, lock board
      {
        playMelody(melodySensor, melodySensorDuration);
        state = NEUTRAL;
      }
      else
        state = LOCK;
        
      break;
      
    case NEUTRAL:
      if(SensorADC() == 0) //If both feet are off, lock board
      {
        JoystickADC();
        if(VESCUART.data.rpm != 0)
          current = -2.0;
        else
          current = 0.0;
          
        playMelody(melodyLock, melodyLockDuration);
        state = LOCK;
      }
      else
        if(throttle >= 131) //If joystick moves up, accelerate (+- 4 threshold)
          state = ACCEL;
        else
          if(throttle <= 123) //If joystick moves down, decelerate
            state = DECEL;
          else
            if(throttle > 123 && throttle < 131)
                state = NEUTRAL;

      break;


    case ACCEL:
      if(SensorADC() == 0) //If both feet are off, lock board
      {
        playMelody(melodyLock, melodyLockDuration);
        state = LOCK;
      }
      else
        if(throttle < 131) //If joystick moves down
          state = NEUTRAL;   
        else
          state = ACCEL;
             
      break;

    case DECEL:
      if(SensorADC() == 0) //If both feet are off, lock board
      {
        playMelody(melodyLock, melodyLockDuration);
        state = LOCK;
      }
      else
        if(throttle > 123) //If joystick moves up, accelerate
          state = NEUTRAL;
        else  
          state = DECEL;
      
      break;
  }


  //Actions
  switch(state)
  {
    case INIT:
      break;

    case LOCK: //Slow down board until stopped
      JoystickADC();
      if(VESCUART.data.rpm != 0)
        current = -2.0;
      else
        current = 0.0;
      break;

    case NEUTRAL:
      current = 0.0; 
      VESCUART.setCurrent(current);

      JoystickADC();
      
      break;

    case ACCEL:
      JoystickADC();
      VESCUART.nunchuck.valueY = throttle;
      current = (VESCUART.nunchuck.valueY - 131) * 0.4; //50A max throttle/brake
      VESCUART.setCurrent(current);
      
      break;

    case DECEL:
      JoystickADC();
      VESCUART.nunchuck.valueY = throttle;        
      current = -(abs(VESCUART.nunchuck.valueY - 123) * 0.4); //Hard stop, reverse
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
  digitalWrite(A0, HIGH);
  digitalWrite(A1, HIGH);
  
  //Initialize output
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);
  pinMode(ledPin4, OUTPUT);

  delay(1000);
  playMelody(melodyOn, melodyOnDuration);

}

void loop() 
{
  nunchuk.update();
  //Serial.println(nunchuk.analogY);
  if(nunchukConnected  == 0 && setupComplete == 0) //Deals with random values when nunchuk is not connected yet
  {
    if(nunchuk.analogY == 255 ) //255: default value when nunchuck connects
    {
      nunchukConnected = 1;
    }
    else
    {
      nunchukConnected = 0;
    }
  }
  else
    if(nunchukConnected  == 1 && setupComplete == 0) //Reset nunchuck init
    {
        nunchuk.init();
        setupComplete = 1;
    }
    else
    {
        runBoard();
        headlight(); 
    }
}
