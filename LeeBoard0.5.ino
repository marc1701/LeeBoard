/*
  LeeBoard 0.5
  Marc Ciufo Green 30/05/2016

  Code for a MIDI pedal board for use with Apple MainStage
  MIDI CC and MIDI Note modes for patch/FX changes & sound triggering
  Includes provision for reading six footswitch buttons, MIDI transmission and LED feedback
  LED feedback can be set to momentary or latching (CC)
  MainStage always prefers momentary implementation of MIDI CC messages themselves
*/

#define PEDAL 0
#define PEDALSWITCH 15
#define DATA 2
#define CLOCK 3
#define LATCH 4
#define MIDINUMBER 81 // lowest MIDI CC / Note number sent out by footswitch
#define BUTTONSTART 5
#define MODEBUTTON 11 // pin where the mode switch button is connected

byte redLeds = 254; // 1111111(0) - all LEDs off - this way we only use one side of the 595
byte blueLeds = 126; // 011111(0)
int i;

bool buttonState[6];
bool oldButtonState[6];
long int buttonPressedTimes[6]; // stores last time buttons were pressed

bool modeButtonCurrent; // latest mode button state
bool modeButtonPrev; // old mode button state
long int modeButtonPressedTime; // stores last time button was pressed

int debounceDelay = 250;

bool noteMode = 0; // initialise to CC mode
int midiMessageType = 0xB0; // initialise to CC messages

bool ledLatch[6] = {1, 1, 1, 0, 0, 0}; // 1 = latching, 0 = momentary
// MainStage always prefers momentary MIDI messages

bool ledState[6];// = {0, 0, 0, 0, 0, 0}; // store LED states for latching

int pedalValue = 0;
int midiVal;
int lastMidiVal;

void setup()
{
  pinMode(DATA, OUTPUT);
  pinMode(LATCH, OUTPUT);
  pinMode(CLOCK, OUTPUT);

  Serial.begin(31250);
}

void loop()
{
  exprPedal(); // read expression pedal and send MIDI if it's moved 
  shiftWrite(); // write current state of LED to shift registers
  readModeButton(); // check the mode button

  for (i = 0; i < 6; i++) // loop through all buttons
  {
    buttonState[i] = digitalRead(i + BUTTONSTART);
    if (buttonState[i] != oldButtonState[i] && millis() - debounceDelay > buttonPressedTimes[i])
    {
      buttonPressedTimes[i] = millis();
      if (buttonState[i])
      {
//        buttonPressedTimes[i] = millis();

        if (noteMode)
        {
          midiSend(midiMessageType, MIDINUMBER + i, 127); // send note on
          flipLedState('r', i); // flip LED state variable (HIGH)

          while (buttonState[i])
          {
            shiftWrite(); // write LED values to circuit
            buttonState[i] = digitalRead(i + BUTTONSTART); // hold up the program as long as button is pressed
          }

          midiSend(midiMessageType, MIDINUMBER + i, 0); // send note off
          flipLedState('r', i); // flip LED state variable (LOW)
        }

        else // CC mode
        {
          switch (ledLatch[i])
          {
            case 0: // switch set to momentary

              midiSend(midiMessageType, MIDINUMBER + i, 127); // send CC HIGH
              flipLedState('b', i); // flip LED state variable (HIGH)

              while (buttonState[i] == HIGH)
              {
                shiftWrite();
                buttonState[i] = digitalRead(i + BUTTONSTART);
              }

              midiSend(midiMessageType, MIDINUMBER + i, 0); // send CC LOW
              flipLedState('b', i); // flip LED state variable (LOW)
              break;


            case 1: // switch set to latch

              midiSend(midiMessageType, MIDINUMBER + i, 127); // send CC HIGH

              ledState[i] = !ledState[i]; // flip LED state
              flipLedState('b', i); // flip LED state variable (HIGH OR LOW)

              while (buttonState[i] == HIGH)
              {
                shiftWrite(); // write LED values to circuit
                buttonState[i] = digitalRead(i + BUTTONSTART); // hold up the program as long as button is pressed
              }

              midiSend(midiMessageType, MIDINUMBER + i, 0); // send CC LOW
              break;


            default:
              break;
          }
          oldButtonState[i] = buttonState[i]; // latest button readings become last button readings
        }
      }
    }
  }
}

void flipLedState(char ledColour, byte ledNum)
{
  if (ledColour == 'r')
  {
    redLeds = redLeds xor 1 << ledNum + 1; // xor of 1 flips bit at given position
  }
  else if (ledColour == 'b')
  {
    blueLeds = blueLeds xor 1 << ledNum + 1;
  }
}

void shiftWrite()
{
  digitalWrite(LATCH, HIGH);
  shiftOut(DATA, CLOCK, MSBFIRST, blueLeds);
  shiftOut(DATA, CLOCK, MSBFIRST, redLeds);
  digitalWrite(LATCH, LOW);
}

// MIDI sending function
void midiSend(int message, int num, int val)
{
  Serial.write(message); Serial.write(num); Serial.write(val);
  delay(1);
}

void readModeButton()
{
  modeButtonCurrent = digitalRead(MODEBUTTON); // read mode button

  if (modeButtonCurrent != modeButtonPrev && millis() - debounceDelay > modeButtonPressedTime)
  {
    modeButtonPressedTime = millis();
    if (modeButtonCurrent == HIGH)
    {
      noteMode = !noteMode; // if the button is pressed, flip noteMode state

      if (noteMode)
      {
        midiMessageType = 0x91; // note on channel 2
      }
      else
      {
        midiMessageType = 0xB0; // CC channel 1
      }

      flashLeds(); // run flashy LED mode change feedback routine
    }
    modeButtonPrev = modeButtonCurrent;
  }
}

void flashLeds()
{
  flipLedState('b', 6);
  flipLedState('r', 6);
  shiftWrite();
  if (noteMode)
  {
    for (i = 5; i >= 0; i--) // loop through all LEDs
    {
      if (ledState[i] == HIGH) // if the current CC LED is latched to HIGH
      {
        flipLedState('b', i); // switch it off only when the wave gets to it (looks neater)
        shiftWrite();
      }
      flipLedState('r', i); // flash on all LEDs in quick succession
      shiftWrite();
      delay(50);
    }

    for (i = 5; i >= 0; i--)
    {
      flipLedState('r', i); // if we've switched to note mode, write all LOW
      shiftWrite();
      delay(50);
    }
  }
  else
  {
    for (i = 5; i >= 0; i--)
    {
      flipLedState('b', i); // flash on all LEDs in quick succession
      shiftWrite();
      delay(50);
    }
    for (i = 5; i >= 0; i--)
    {
      if (!ledState[i]) // if LED in position i is not on from previous CC use
      {
        flipLedState('b', i); // switch off
        shiftWrite();
        delay(50);
      }
    }
  }
}

void exprPedal()
{
  if (digitalRead(PEDALSWITCH))
  {
    pedalValue = analogRead(PEDAL); // read expression pedal
    midiVal = map(pedalValue, 10, 1010, 0, 127); // map reading to MIDI

    if (midiVal != lastMidiVal) // if pedal has moved
    {
      midiSend(0xB0, 11, midiVal); // send the MIDI data
    }

    lastMidiVal = midiVal; // remember last pedal position
  }
}


