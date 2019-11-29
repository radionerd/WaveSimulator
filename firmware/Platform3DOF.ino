  /*
   * Ships Motion Simulator
   *
   * Copyright (C)2019 richard.jones.1952@gmail.com
   *
   * This program is free software: you can redistribute it and/or modify
   * it under the terms of the GNU General Public License as published by
   * the Free Software Foundation, either version 3 of the License, or
   * (at your option) any later version.
   *
   * This program is distributed in the hope that it will be useful,
   * but WITHOUT ANY WARRANTY; without even the implied warranty of
   * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   * GNU General Public License for more details.
   *
   * You should have received a copy of the GNU General Public License
   * along with this program.  If not, see <https://www.gnu.org/licenses/>.
   *
   * This file contains embedded software to drive a 3 degrees of freedom motion simulator
   * The firmware was compiled and tested using Arduino v1.8.9 hosted on Ubuntu 18.04
   * The hardware is an Ardiuno Nano, pin out adapter, 3 servos and a control panel with 6 10k pots and 3 switches.
   * The similator may also be controlled over the serial USB link at 115200 baud using a VT100 terminal emulator. 
   * 
   */
#include <Servo.h>



const    int                NUM_SERVOS=3;
static float wave_period   [NUM_SERVOS] = { 9,10,11}; // seconds
static float wave_amplitude[NUM_SERVOS] = { 33.33,33.33,33.33}; // percent
static float wave_angle    [NUM_SERVOS]; // radians
static   int servo_position[NUM_SERVOS]; // degrees 0-180
Servo        servo         [NUM_SERVOS]; // create servo objects to control each servo
static unsigned task_number, max_ms, max_task; 
static bool manual_state, spare_state ;

// the setup function runs once when you press reset or power the board
void setup() {
  // Assign CPU i/o pins
  pinMode(LED_BUILTIN, OUTPUT); // initialize digital pin D13 LED_BUILTIN as an output.
  pinMode(12, INPUT_PULLUP ); // Control panel Run/Stop
  servo[0].attach(11); // servo output D11
  servo[1].attach(10); // servo output D10
  servo[2].attach( 9); // servo output D9
    // initialize serial communication
  Serial.begin(115200); // Baud rate
  char buffer[]={ 27,'[','2','J',0}; // VT100 Clear screen character sequence
  Serial.print(buffer); // Clear terminal emulator screen
}

// Flash the LED to show what a state we are in
void LEDUpdate()
{
  int led;
  int milliseconds = millis();
  int brightness = milliseconds &2047;
  if ( PanelEnabled() ) {
    // gentle fade led brightness, 2.048 second fade, 16ms pwm
    if ( brightness > 1023 ) brightness = 2047 - brightness;  // range 0-1023-0
    brightness = brightness >> 6; // now 0-15
    int pwm = milliseconds & 15;
    led = brightness <= pwm;
  } else {
    // hard flash led when panel disabled
    led = brightness > 1023;  
  }
  if ( led )
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level) 
  else
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
}



// the loop function runs over and over again forever
void loop() {
  unsigned long entry_time = millis(); // Get number of milliseconds since reset
  LEDUpdate();  
  switch (task_number) {
  // LoopTimer();   // Log loop time characteristics
    case 0: case 1: case 2: case 3 : case 4: case 5: case 6: case 7:
      PanelInput();  // Read the pots and switches on the panel and update if enabled
      break;
    case 8:
      SerialOutput();// Display an activity screen to a VT100 terminal emulator over USB
      break;
    case 9:
      SerialInput(); // Action serial commands sent over the USB connection, panel must turned off
      break;
    case 10:
      Motion();      // Update servo positions depending on run/manual mode
      break;
    default:
      task_number = -1;
  }
//  char buffer[80];
//  sprintf(buffer,"Return from task %d",task_number);
//  Serial.println(buffer);

  unsigned long elapsed_time = millis()-entry_time;
  if ( elapsed_time > max_ms ) {
    max_task = task_number;
    max_ms = elapsed_time;
  }
  task_number++;
}

bool PanelEnabled(void)
{
  return !digitalRead( 12 );
}

// Read potentiometer and switch inputs if enabled by D12
void PanelInput()
{
  int index;
  int channel = task_number & 7;
  if ( PanelEnabled() ) { // optional disable panel input for USB only control
    int val = analogRead( channel + A0 ); // Read one of 8 potentiometer inputs
    switch ( channel ) // Map analogue inputs to function
    {
    case 0: // Auto/Manual
      manual_state = val > 512;
      break;
    case 1: // spare
      spare_state = val > 512;
      break;
    case 2: case 3: case 4: // Slider pots
      index = 4 - channel;
      if ( manual_state ) {
        val = map(val, 0, 1023, 0, 180); // scale 0-180 degrees
        servo_position[index] = val;
      } else {
        float fval = val;
        fval = fval/10.23; // scale 0-100%
        wave_amplitude[index] = fval;
      }
      break;
    case 5: case 6: case 7: // Rotary pots
      index = 7 - channel;
      wave_period[index] = (float)val/50.0; // 0 to 20.47 seconds
      if ( wave_period[index] < 0.1 ) wave_period[index] = 0.1;
    }
  }
}

// Action serial commands sent over the USB connection, panel must turned off
// Build up a command line until \n received, allow bs/del for corrections
const   int INPUT_BUFFER_SIZE=81;
static char input_buffer[INPUT_BUFFER_SIZE];

void SerialInput(void)
{
  const char BELL=7;
  const char BACKSPACE=8;
  const char DELETE=0x7f;
  
  if (Serial.available() > 0) {
    char c = Serial.read();  //gets one byte from serial buffer
    unsigned len = strlen(input_buffer);
    switch ( c ) {
      case BACKSPACE :
      case DELETE :
        if ( len ) {
          input_buffer[ --len ] = 0;
          Serial.print("\010"); // backspace
        }
      break;
      case '\n' :
      case '\r' :
        DecodeCommand(input_buffer);
        input_buffer[0]=0; // clear input buffer
      break;
      default:
        if ( c >= ' ' ) {
          if ( len < ( sizeof(input_buffer)-2) ) {
            input_buffer[ len++ ] = c;
            input_buffer[ len ] = 0;
            Serial.print(c);
          } else {
            Serial.print(BELL);  
          }
        }
    }
  } 
}

// Display parameters on serial VT100 via USB at 1 second intervals
void SerialOutput()
{
  int i;
  char buffer[256];
  char home[] = {0x1b,'[','H',0}; // VT100 home string
  static unsigned long next_time;

  unsigned long now = millis();
  if ( next_time==0) {
    next_time = now+2000; // startup display
    Serial.print( F("Motion Simulator\r\n"));
    Serial.print( F("Compiled: "));
    Serial.print( F(__DATE__));
    Serial.print( F(", "));
    Serial.print( F(__TIME__));
    Serial.print( F(", GCC "));
    Serial.print( F(__VERSION__));
    Serial.print(F( "\r\nArduino IDE version: "));
    Serial.println( ARDUINO, DEC);
  }
  if ( now < next_time ) return ; // Print 1000ms intervals
  next_time = now + 1000;
  Serial.print(home);
  sprintf(buffer,"\r\n\n\n\n\t\t  Roll   Pitch   Heave");
  Serial.println(buffer);
  sprintf(buffer,"Period(Seconds)");
  Serial.print(buffer);
  buffer[0]='\t';
  for ( i = 0 ; i < NUM_SERVOS;i++) {
    dtostrf( wave_period[i] , 6, 2, buffer+1 );
    Serial.print(buffer);
  }
  Serial.println();
  sprintf(buffer,"Amplitude %%");
  Serial.print(buffer);
  buffer[0]='\t';
  for ( i = 0 ; i < NUM_SERVOS;i++) {
    dtostrf( wave_amplitude[i] , 6, 2, buffer+1 );
    Serial.print(buffer);
  }
//  Serial.println();
//  Serial.print(F("Wave Angle"));
//  buffer[0]='\t';
//  for ( i = 0 ; i < NUM_SERVOS;i++) {
//    dtostrf( wave_angle[i] , 6, 2, buffer+1 );
//    Serial.print(buffer);
//  }
  sprintf(buffer,"\r\nServo Degrees");
  Serial.print(buffer);
  for ( i = 0 ; i < NUM_SERVOS;i++) {
    sprintf(buffer,"\t%6d",servo_position[i]);
    Serial.print(buffer);
  }
  Serial.println();
  sprintf(buffer,"Max_ms[%d]=%2d\r\nPanel=%d\r\nManual=%d\r\nSpare=%d\r\n", max_task,max_ms,PanelEnabled(),manual_state,spare_state);
  Serial.print(buffer);
  Serial.print("Enter command:");
  Serial.print(input_buffer);
  Serial.print("\033[K"); // Clear to end of line
}

void DecodeCommand (char *buf ) {
  if ( PanelEnabled() ) {
    Serial.println("\n\rSwitch off control panel");
  }
  
  char cmd = toupper(buf[0]);
  int i=0;
  int var_idx=0;
  switch ( cmd )
  {
    case 'P' :
    case 'A' :
    case 'S' :
      while ( buf[i] && ( var_idx < NUM_SERVOS ) ) {
        i += strcspn( &buf[i] , ".0123456789" ); // locate number
        if ( buf[i] ) {
          if ( cmd == 'P' ) wave_period   [var_idx++] = atof( &buf[i] );
          if ( cmd == 'A' ) wave_amplitude[var_idx++] = atof( &buf[i] );
          if ( cmd == 'S' ) servo_position[var_idx++] = atof( &buf[i] );
        }
        i += strcspn( &buf[i] , " " ); // pass over previous number       
      }      
    break;
    case 'M' : // Manual = 0/1
      i = strcspn( buf , "01" );
      if ( buf[i] != 0 )
        manual_state = buf[i] & 1;
      break;
  }
}
//static float wave_period   [NUM_SERVOS]; // seconds
//static float wave_amplitude[NUM_SERVOS]; // percent
//static float wave_angle    [NUM_SERVOS]; // radians
//static   int servo_position[NUM_SERVOS]; // degrees 0-180

// Update servo positions @ 40Hz i.e. every 25ms
void Motion()
{
  static unsigned long next_time;
  const float sample_freq = 40.0; // 40.0Hz
  const int sample_rate_ms = 1000/sample_freq;
  const float pi = 3.1415926;
  float wave_height [NUM_SERVOS];
  int i;

  unsigned long now = millis();
  // if ( next_time==0) next_time = now; // set startup condition
  if ( now < next_time ) return ; // 
  next_time = now + sample_rate_ms; // Update servos at 25 ms intervals for precise rate control in auto
  if ( ! manual_state ) {
    // Auto mode
    // update waves for roll, pitch and heave
    // Assumptions:
    // 1. Wave motions are sinusoidal
    // 2. Wave in different directions add linearly
    // 3. The servo angular displacements translate linearly through the linkeage to the platform (they don't)
    // 4. To avoid limiting, the amplitude for each servo should be set no more than 33%
    // Nevertheless these assumptions are a handy first approximation
    for ( i = 0 ; i < NUM_SERVOS ; i++ ) {
      if ( wave_period[i] < 0.1 ) wave_period[i] = 0.1; // prevent divide by zero errors
      wave_angle[i] += (2*pi)/(wave_period[i]*sample_freq);
      if ( wave_angle[i] > 2*pi ) wave_angle[i] -= 2*pi;
      wave_height[i] = sin( wave_angle[i] ) * wave_amplitude[i] / 100.0; // range at 100% +/-1
    }
    const int roll=0, pitch=1,heave=2;
    const int left=0,centre=1,right=2;
    // map waves onto servo positions:
    //  Servo0  Servo2:
    //   Left    Right
    //     Servo1
    //     Centre
    servo_position[left]   = ((+wave_height[roll]+wave_height[pitch]+wave_height[heave])*90.0)+90;
    servo_position[centre] = ((                  -wave_height[pitch]+wave_height[heave])*90.0)+90;
    servo_position[right]  = ((-wave_height[roll]+wave_height[pitch]+wave_height[heave])*90.0)+90;
  }
  // Update servo positions
  for ( i = 0 ; i < NUM_SERVOS ; i++ ) {
    //if ( servo_position[i] <  0 ) servo_position[i] =   0;
    //if ( servo_position[i] >180 ) servo_position[i] = 180;   
    servo[i].write(servo_position[i]);
  }
}
