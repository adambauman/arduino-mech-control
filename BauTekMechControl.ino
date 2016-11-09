//**************************************************************//
//  Name    : Mech Control Interface                            //
//  Author  : Adam J. Bauman  (adam@kungfutreachery.net)        //
//  Date    : 24, December 2014                                 //
//  Version : 1.1                                               //
//  Notes   : Code to convert analog rotation input to          //
//          : key press for gaming/simulation interface.        //
//****************************************************************
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <LiquidCrystal.h>

//**************************************************************//
//  Pin Associations                                            //
//****************************************************************

// Two chained 74HC595 shift registers
// See http://www.bildr.org/2011/02/74hc595 for an awesome reference
int latchPin = 7; // RCLK (74HC5959 pin 12)
int clockPin = 8; // SRCLK (74HC595 pin 11)
int dataPin = 6;  // SER (74HC595 pin 14)

// Profile push buttons
int leftBtnPin = 5;
int rightBtnPin = 13;

// Analog input pin from the motor circuit
int rotateSensPin = 0;

// Load the LiquidCrystal library
LiquidCrystal lcd(3, 4, 9, 10, 11, 12); 

//**************************************************************//
//  Custom LCD Characters                                       //
//****************************************************************
byte symBlock = B11111111;

// A little mech
byte mechChar[8] = {
        B00000,
        B01110,
        B01110,
        B11111,
        B10101,
        B01110,
        B01010,
        B11011
};

// Reverse icon
byte equalR[8] = {
        B01110,
        B01010,
        B01100,
        B01010,
        B00000,
        B01110,
        B00000,
        B01110
};

// Delta movement icon
byte equalT[8] = {
        B01110,
        B00100,
        B00100,
        B00100,
        B00000,
        B00100,
        B01010,
        B11111
};

//**************************************************************//
//  Global Variables                                            //
//****************************************************************

// Profile tracking, change to set default profile and to add/remove entries
int currentProfile = 0;
int profileCount = 4;

// The bytes shifted out to issue keystrokes 
byte activeByteA;
byte activeByteB;

// The binary patterns that set the shift registers. These are populated during setup()
byte basicPattern[8];
byte specialPatternA[4];
byte specialPatternB[4];
byte clearPattern;

// Throttle tracking variables
int currentThrottle = 0;
boolean throttleAltered = false;
boolean isReversed = false;
boolean constantInput = false;

// Misc variables (for display, fun functions, etc.)
int throttleCounter = 5;

// Used for testing purposes
int testThrottle = 500;
  
//**************************************************************//
//  Setup()                                                     //
//****************************************************************
void setup() 
{
  // Initialize the LCD and load custom characters
  lcd.begin(16, 2);
  lcd.createChar(0, equalR);
  lcd.createChar(1, equalT);
  
  // Set pin modes and enable serial communication (if needed for debug)
  pinMode(leftBtnPin, INPUT);
  pinMode(rightBtnPin, INPUT);
  pinMode(latchPin, OUTPUT);
  
  // Comment out Serial comms unless debugging to keep things ticking over smoothly
  // Serial.begin(9600);
  
  // The core shift patterns. Since we only need one output from each shift
  // register to make a keypress we can get away with simple binary patterns
  basicPattern[0] = 1;   // 00000001
  basicPattern[1] = 2;   // 00000010
  basicPattern[2] = 4;   // 00000100
  basicPattern[3] = 8;   // 00001000
  basicPattern[4] = 16;  // 00010000
  basicPattern[5] = 32;  // 00100000
  basicPattern[6] = 64;  // 01000000
  basicPattern[7] = 128; // 10000000
  
  // Clears the output off the register. If you have pins to spare you could also connect
  // the 74HC595's OE pin (74HC595 pin 13) to the Arduino and toggle it high or low to switch output
  clearPattern = 0; // 00000000
  
  // The patterns for multiple key presses. I can't remember the details, but there was some 
  // goofy Arduino IDE reason I used integers for the control bytes rather than raw binary
  specialPatternA[0] = 33;  // 00100001 Up + Ctrl
  specialPatternB[0] = 96;  // 01100000 Up + Ctrl
  
  specialPatternA[1] = 129; // 10000001 Down + Ctrl
  specialPatternB[1] = 40;  // 00101000 Down + Ctrl
  
  specialPatternA[2] = 48;  // 00110000 Up + J
  specialPatternB[2] = 192; // 11000000 Up + J
  
  specialPatternA[3] = 80;  // 01010000 Down + J
  specialPatternB[3] = 136; // 10001000 Down + J
  
  // Flush the shift registers to make sure they're clear before we start
  activeByteA = clearPattern;
  activeByteB = clearPattern;

  // Everything set, let's run the LCD startup routine
  LCDStartupSequence();
}

//**************************************************************//
//  Loop()                                                      //
//****************************************************************
void loop() {
  int leftBtnState;
  int rightBtnState;
  int rotateSensValue;
  
  // Read inputs
  leftBtnState = digitalRead(leftBtnPin);
  rightBtnState = digitalRead(rightBtnPin);
  rotateSensValue = analogRead(rotateSensPin);
  
  // Un-comment for testing purposes only
  //rotateSensValue = testThrottle;

  // Sets the currentThrottle variable if a change has occured
  SetThrottle(rotateSensValue);
 
  // Clear the shift registers if constantInput isn't set. If you were using the
  // 74HC595's OE pin this is where you would set it high to disable output
  if (constantInput == false)
  {
    activeByteA = clearPattern;
    activeByteB = clearPattern;
    TriggerShift();
  }
 
  // Process the left profile button
  if (leftBtnState == HIGH)
  {
    if (currentProfile > 0)
    {
      currentProfile -= 1;
    } else {
      currentProfile = profileCount;
    }
    
    // Loop delay can be altered for specific profiles to increase or decrease responsiveness
    // at the expense of de-bounce on rotation readings and profile buttons pushes
    // TODO: Envoke delay only when button has been pressed
    if (currentProfile == 3 || currentProfile == 4)
      delay(150);
  }  
  
  // Process the right profile button
  if (rightBtnState == HIGH)
  {
    if (currentProfile < profileCount)
    {
      currentProfile += 1;
    } else {
      currentProfile = 0;
    }
    
    // Loop delay can be altered for specific profiles to increase or decrease responsiveness
    // at the expense of de-bounce on rotation readings and profile buttons pushes
    // TODO: Envoke delay only when button has been pressed
    if (currentProfile == 3 || currentProfile == 4)
      delay(150);
  }
  
  lcd.setCursor(14,0);
  lcd.print(" ");
  
  if (throttleCounter > 0)
  {  
    lcd.setCursor(14,0);
    lcd.write(byte(1));
    throttleCounter--;
  }
  
  // If throttle has changed switch up the key strokes being sent
  if (throttleAltered)
  {
    // Set the shift registers if the throttle has been changed
    SetKeyBytes();
        
    // Set the trigger variable back to false    
    throttleAltered = false;
    throttleCounter = 5;
  }
    
  // Display isReversed status
  if (isReversed)
  {
    lcd.setCursor(15, 0);
    lcd.write(byte(0));
  } else {
    lcd.setCursor(15, 0);
    lcd.print(" ");
  }
  
  // Display throttle status
  lcd.setCursor(0,0);
  lcd.print("t:");
  LCDThrottleText();
  
  // Display rotation sensor reading
  lcd.setCursor(7,0);
  lcd.print("r:");
  lcd.print(rotateSensValue);
  
  // Display profile name
  lcd.setCursor(0,1);
  lcd.print("p:");
  LCDProfileText(); // outputs profile name to LCD
    
  // Delay the loop, suggested to set to the computer's type repeat delay, decrease if 
  // running FPS for fast direction changes
  if (currentProfile == 3 || currentProfile == 4)
  {
    delay(50);
  } else {   
    delay(200);
  }
}

//**************************************************************//
//  TriggerShift()                                              //
//  : Begins the shift cascade                                  //
//****************************************************************
void TriggerShift()
{ 
  digitalWrite(latchPin, 0);
  shiftOut(dataPin, clockPin, activeByteB);   
  shiftOut(dataPin, clockPin, activeByteA);
  digitalWrite(latchPin, 1);
}

//**************************************************************//
//  ShiftOut()                                                  //
//  : Shifts data through the registers                         //
//****************************************************************

// From tutorial at http://arduino.cc/en/tutorial/ShiftOut
//***************************************************************//
//  Name    : shiftOutCode, Dual Binary Counters                 //
//  Author  : Carlyn Maw, Tom Igoe                               //
//  Date    : 25 Oct, 2006                                       //
//  Version : 1.0                                                //
//  Notes   : Code for using a 74HC595 Shift Register            //
//          : to count from 0 to 255                             //
//***************************************************************//
void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
  // This shifts 8 bits out MSB first, 
  //on the rising edge of the clock,
  //clock idles low

  //internal function setup
  int i=0;
  int pinState;
  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, OUTPUT);

  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(myDataPin, 0);
  digitalWrite(myClockPin, 0);

  //for each bit in the byte myDataOut
  //NOTICE THAT WE ARE COUNTING DOWN in our for loop
  //This means that %00000001 or "1" will go through such
  //that it will be pin Q0 that lights. 
  for (i=7; i>=0; i--)  {
    digitalWrite(myClockPin, 0);

    //if the value passed to myDataOut and a bitmask result 
    // true then... so if we are at i=6 and our value is
    // %11010100 it would the code compares it to %01000000 
    // and proceeds to set pinState to 1.
    if ( myDataOut & (1<<i) ) {
      pinState= 1;
    }
    else {	
      pinState= 0;
    }

    //Sets the pin to HIGH or LOW depending on pinState
    digitalWrite(myDataPin, pinState);
    //register shifts bits on upstroke of clock pin  
    digitalWrite(myClockPin, 1);
    //zero the data pin after shift to prevent bleed through
    digitalWrite(myDataPin, 0);
  }

  //stop shifting
  digitalWrite(myClockPin, 0);
}

//**************************************************************//
//  SetThrottle()                                               //
//  : Change currentThrottle based off rotateSensValue 
//****************************************************************
void SetThrottle(int rotateSensValue)
{
  int newThrottle;
  
  // Speed thresholds for throttle control
  // Set reverse values
  const int rBoost = 400;
  
  const int r2High = 430;
  const int r2Low = rBoost + 1;
  
  const int r1High = 460;
  const int r1Low = r2High + 1;
  
  // Set forward values
  const int fBoost = 620;
  
  const int f2High = fBoost - 1;
  const int f2Low = 570;
  
  const int f1High = f2Low - 1;
  const int f1Low = 540;
  
  // Set stop values
  const int restHigh = f1Low - 1; 
  const int restLow = r1High + 1;
  
  /*
  0 = Stop (508)
  1 = Reverse 1 (460)
  2 = Reverse 2 (430)
  3 = Reverse Boost (400)
  4 = Forward 1 (540)
  5 = Forward 2 (565)
  6 = Forward Boost (620)
  */
  
  if (rotateSensValue >= fBoost) 
    newThrottle = 6; // Forward Boost
  
  if (rotateSensValue <= f2High && rotateSensValue >= f2Low)
    newThrottle = 5; // Forward 2
    
  if (rotateSensValue <= f1High && rotateSensValue >= f1Low)
    newThrottle = 4; // Forward 1
    
  if (rotateSensValue <= restHigh && rotateSensValue >= restLow)
    newThrottle = 0; // Stop
    
  if (rotateSensValue <= r1High && rotateSensValue >= r1Low)
    newThrottle = 1; // Reverse 1
    
  if (rotateSensValue <= r2High && rotateSensValue >= r2Low)
    newThrottle = 2; // Reverse 2
    
  if (rotateSensValue <= rBoost)
    newThrottle = 3; // Reverse Boost 
  
  // Set global currentThrottle and flag the change
  if (newThrottle != currentThrottle)
  {
    currentThrottle = newThrottle;
    throttleAltered = true;
  }  
}

//**************************************************************//
//  SetKeyBytes()
//  : Loads the key mappings for the selected profile
//****************************************************************
void SetKeyBytes()
{
  switch (currentProfile)
  {
    case 0: // Disengaged
      activeByteA = clearPattern;
      activeByteB = clearPattern;
      TriggerShift();
      break;
    case 1: // MechWarrior
      ControlMechWarrior();
      break;
    case 2: // MetalTech
      break;
    case 3: // Shooter
      ControlShooter();
      break;
    case 4: // Hawken
      ControlHawken();
      break; 
  } 
}    

//**************************************************************//
//  ControlMechWarrior()                                        //
//****************************************************************
void ControlMechWarrior()
{
  boolean enableForward = false;
  boolean enableReverse = false;
  boolean enableConstantInput = false;
  
  int shiftIndexA;
  int shiftIndexB;
  
  switch (currentThrottle)
  {
    case 6: // J
      shiftIndexA = 4;
      shiftIndexB = 7;
      enableForward = true;
      enableConstantInput = true;
      break;
      
    case 5: // 0
      shiftIndexA = 1;
      shiftIndexB = 0;
      enableForward = true;
      break;
      
    case 4: // 6
      shiftIndexA = 0;
      shiftIndexB = 7;
      enableForward = true;
      break;
      
    case 0: // 1 (Stop)
      shiftIndexA = 1;
      shiftIndexB = 4;
      break;
    
    case 1: // 6 (Reverse)
      shiftIndexA = 0;
      shiftIndexB = 7;
      enableReverse = true;
      break;
      
    case 2: // 0 (Reverse)
      shiftIndexA = 1;
      shiftIndexB = 0;
      enableReverse = true;
      break;
      
    case 3: // J (Reverse)
      shiftIndexA = 4;
      shiftIndexB = 7;
      enableReverse = true;
      enableConstantInput = true;
      break;      
  }

  // If we're using jump jets we need to switch the controller from
  // single press to hold mode
  if (enableConstantInput)
  {
    constantInput = true;
  } else {
    constantInput = false;
  }
  
  // Trigger throttle reverse if necessary
  if (enableReverse == true && isReversed != true)
  {
    // Trigger Backspace command
    activeByteA = basicPattern[3];
    activeByteB = basicPattern[1];
    TriggerShift();
    
    // Set global reverse tracker
    isReversed = true;
    delay(200);
  }
  
  // Set the registers for the main command and shift
  activeByteA = basicPattern[shiftIndexA];
  activeByteB = basicPattern[shiftIndexB];
  TriggerShift();
}

//**************************************************************//
//  ControlShooter()                                            //
//****************************************************************
void ControlShooter()
{
  switch (currentThrottle)
  {
    case 6: // Up + H
      activeByteA = basicPattern[5]; // 5 is common
      activeByteB = basicPattern[6] + basicPattern[7];
      break;
      
    case 5: // Up
      activeByteA = basicPattern[5];
      activeByteB = basicPattern[6];
      break;
      
    case 4: // Up
      activeByteA = basicPattern[5];
      activeByteB = basicPattern[6];
      break;
    
    case 0: // null (Stop)
      activeByteA = clearPattern;
      activeByteB = clearPattern;
      break;
    
    case 1: // Down
      activeByteA = basicPattern[6];
      activeByteB = basicPattern[3];
      break;
      
    case 2: // Down
      activeByteA = basicPattern[6];
      activeByteB = basicPattern[3];
      break;
      
    case 3: // Down + H
      activeByteA = basicPattern[6] + basicPattern[5];
      activeByteB = basicPattern[3] + basicPattern[7];
      break;
  }
  
  // Set contantInput so the controller switches to hold instead of single press
  constantInput = true;
  
  // Trigger the shift
  TriggerShift();
}

//**************************************************************//
//  ControlHawken()                                             //
//****************************************************************
void ControlHawken()
{
  switch (currentThrottle)
  {
    case 6: // Up + J (forward jump jets)
      activeByteA = basicPattern[5];// + basicPattern[4];
      activeByteB = basicPattern[6] + basicPattern[2]; // + basicPattern[7];
      break;
      
    case 5: // Up + H (forward boost)
      activeByteA = basicPattern[5]; // 5 is common
      activeByteB = basicPattern[6] + basicPattern[7];
      break;
      
    case 4: // Up (normal forward)
      activeByteA = basicPattern[5];
      activeByteB = basicPattern[6];
      break;
      
    case 0: // null (stop)
      activeByteA = clearPattern;
      activeByteB = clearPattern;
      break;
      
    case 1: // Down (normal reverse)
      activeByteA = basicPattern[6];
      activeByteB = basicPattern[3];
      break;
      
    case 2: // Down
      activeByteA = basicPattern[6]; // + basicPattern[5];
      activeByteB = basicPattern[3] + basicPattern[7];
      break;
      
    case 3: // Down + H (triggers 180 flip)
      activeByteA = basicPattern[6]; // + basicPattern[5];
      activeByteB = basicPattern[3] + basicPattern[7];
      /* Old method with jump jets
      activeByteA = basicPattern[6]; // + basicPattern[4];
      activeByteB = basicPattern[3] + basicPattern[1]; //+ basicPattern[7];
      */
      break;
  }
  
  // Set contantInput so the controller switches to hold instead of single press
  constantInput = true;
  
  // Trigger the shift
  TriggerShift();  
}

//**************************************************************//
//  LCDProfileText()                                            //
//****************************************************************
void LCDProfileText()
{
  switch (currentProfile)
  {
    case 0: // Disengaged
      lcd.print("DISENGAGED    ");
      break;
    case 1: // Mechwarrior
      lcd.print("MechWarrior   ");
      break;
    case 2: // FPS
      lcd.print("MetalTech     ");
      break;
    case 3: // Shooter
      lcd.print("Shoooter      ");
      break;
    case 4: // Hawken
      lcd.print("Hawken        ");
      break;
    default:
      lcd.print("Error         ");
      break;
  }
}

//**************************************************************//
//  LCDThrottleText()                                           //
//****************************************************************
void LCDThrottleText()
{
  switch (currentThrottle)
  {
    case 0: // Stop
      lcd.print("STOP");
      break;
    case 1: // Reverse 1
      lcd.print("Rev1");
      break;
    case 2: // Reverse 2
      lcd.print("Rev2");
      break;
    case 3: // Reverse Boost
      lcd.print("RBst");
      break;
    case 4: // Forward 1
      lcd.print("Fwd1");
      break;
    case 5: // Forward 2
      lcd.print("Fwd2");
      break;
    case 6: // Forward Boost
      lcd.print("FBst");
      break;
    default:
      lcd.print("Err ");
      break;
  }
}

//**************************************************************//
//  LCDStartupSequence                                          //
//  : Every mech control interface needs a cool startup         //
//    sequence                                                  //
//****************************************************************
void LCDStartupSequence()
{
  byte kanjiA = B11000000;
  byte kanjiB = B10110011;
  byte kanjiC = B10110101;
  byte kanjiD = B10101001;
  byte kanjiE = B10111100;
  byte kanjiF = B11001110;
  byte kanjiG = B11010110;
  byte kanjiH = B10110111;
  
  // Setup character arrays
  byte blockLine[16] = { symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock, symBlock };
  byte blockKanjiA[16] = { kanjiA, kanjiB, kanjiC, kanjiD, kanjiE, kanjiF, kanjiG, kanjiH, kanjiF, kanjiD, kanjiB, kanjiA, kanjiF, kanjiH, kanjiA, kanjiC };
  byte blockKanjiB[16] = { kanjiF, kanjiE, kanjiA, kanjiH, kanjiC, kanjiD, kanjiA, kanjiD, kanjiF, kanjiG, kanjiE, kanjiH, kanjiA, kanjiB, kanjiC, kanjiB };
  
  // Fill the screen with blocks
  lcd.setCursor(0,0);
  for (int cursorPOS = 0; cursorPOS <= 15; cursorPOS++)
  {
    lcd.write(blockLine[cursorPOS]);
  }
  
  lcd.setCursor(0,1);
  for (int cursorPOS = 0; cursorPOS <= 15; cursorPOS++)
  {
    lcd.write(blockLine[cursorPOS]);
  }
  
  delay(400);
  
  // Drop kanji  
  lcd.setCursor(0,0);
  for (int cursorPOS = 0; cursorPOS <= 15; cursorPOS++)
  {
    lcd.write(blockKanjiA[cursorPOS]);
    delay(30);
  }
  
  lcd.setCursor(0,1);
  for (int cursorPOS = 0; cursorPOS <= 15; cursorPOS++)
  {
    lcd.write(blockKanjiB[cursorPOS]);
    delay(30);
  }
  
  delay(400);
  
  lcd.clear(); 
  
  lcd.print(" CONTROL SYSTEM ");
  lcd.setCursor(0,1);
  lcd.print("     ONLINE     ");
  
  delay(2000);
  
  lcd.clear();
}
