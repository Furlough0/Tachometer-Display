//
// This code is a sketch for an Arduino Uno to act as a Tachometer.
// It was tested on a Sieg SX2P milling machine with the 'extra' header data.
// I believe it will work on a number of other mills and lathes but is not tested.
// This version uses an I2C LCD display board as an output.
// It uses the serial hardware capability of the Arduino to receive the rpm count from the
// mill.

//--------------------------------------
//  Author: Martyn Holliday - 7/10/2019  -->
//--------------------------------------
// Release Version V-1.0
//--------------------------------------

//--------------------------------------
// My thanks to:
//--------------------------------------
// Jennifer Edwards for her code and the impetus for my version.
//--------------------------------------
// Jeffery Nelson of "macpod.net" for his original analysis of the encoded data.
//--------------------------------------
//
//--------------------------------------
// I2C Board Wiring Instructions
//
// LCD Board:
// I2C display "SCL" to arduino board pin A5
// I2C display "SDA" to arduino board pin A4
// I2C display "VCC" to arduino board pin 5V
// I2C display "GND" to arduino board pin GND
//
//--------------------------------------
// SPI interface Wiring Instructions
//    SPI Master__________________________SPI Slave
//
//    Mill                                Tachometer
//    NC       --------------------------->(10) SS
//    DATA ( 3)--------------------------->(11) MOSI
//    NC       <---------------------------(12) MISO
//    CLK  ( 2)--------------------------->(13) SCK
//
//    or simulator                        Tachometer
//    NC       --------------------------->(10) SS
//    DATA ( 6)--------------------------->(11) MOSI
//    NC       <---------------------------(12) MISO
//    CLK  ( 5)--------------------------->(13) SCK
//--------------------------------------------------

// Pinout diagram below -view is looking at socket on milling machine
// Note that the pin numbering is for the plugs that I used.
// This is NOT the same plug as described elsewhere.
// The pin signals however obviously are the same.

//  Socket pin layout:
//      X ---------> the notch in the socket
//   7     2
//
//  6   1   3
//
//    5   4
//
//  1 - Brown  LCDCS
//  2 - Red    LCDCL
//  3 - Orange LCDDI
//  4 - Green  GND
//  5 - Blue   +5V
//  6 - Pink   GND (not used)
//  7 - Grey   +5V (Not used)

// The Frame signal from the mill is NOT used.

// This software has three modes of operation
// 1. to present mill speed as an rpm count on the display.
// 2. to present the header data available now on new mills in hex.
// 3. to present the clock pulse count as a diagnostic aid and the last data byte in hex.
//
// These modes are controlled by a jumper on pin (0) PIN_IN_MODE connected to gnd.
// With no jumper act as a tachometer.
// With jumper present on startup display the header data.
// With jumper present after startup show the received clock pulse count.
// ----------------------------------------------------------------------------------------
// NOTE that with this jumper in place then loading software into the Arduino is inhibited!
// ----------------------------------------------------------------------------------------

#include <SPI.h>
#include <Wire.h>                  // Needed for I2C function
#include <LiquidCrystal_PCF8574.h> // I2C support
LiquidCrystal_PCF8574 lcd(0x27);   // Assign serial address to LCD

#define PIN_IN_MODE 0
#define PACKET_BITS_COUNT 104
#define TIME_OUT_COUNT 12500

inline void block_delay(unsigned long units) __attribute__((always_inline));
inline void DecodeHeader() __attribute__((always_inline));
inline void DecodeBuffer() __attribute__((always_inline));
//inline void DecodeDebug() __attribute__((always_inline));
inline void DecodeError(int eCode) __attribute__((always_inline));
inline int get_rpm() __attribute__((always_inline));

boolean Diagnostics = false;
boolean HeaderMode = false;

uint8_t special = false;
boolean inError = false;

const uint8_t  bufSize = 40;  //More than necessary to allow for accidental overflow!

volatile uint8_t  buf [bufSize];
volatile uint8_t  pos;
volatile boolean process_it;
volatile boolean InPacket ;

//-----------------------------------------------------------------------------------------------------
ISR(TIMER1_COMPA_vect)
{
  // A timer1 interrupt here means we are not receiving frequent enough clock pulses to be
  // within a packet transfer.
  // Therefore we must be between packets.

  InPacket = false; //Set packet flag false.
  process_it = true;
}

//-----------------------------------------------------------------------------------------------------
ISR (SPI_STC_vect)// SPI interrupt routine
{
  buf [pos]  = SPDR;  // grab byte from SPI Data Register

  // add to buffer if room
  if (pos < (sizeof (buf) - 1))
  {
    pos++;
  }

  TCNT1 = 0;  //Reset timer counter value to 0 which defers any timeout
  InPacket = true;

}  // end of interrupt routine SPI_STC_vect

//-----------------------------------------------------------------------------------------------------
void setup (void)
{
  Serial.begin (115200);   // debugging
  lcd.begin(16, 2);              //  - init lcd
  lcd.setBacklight(255);         //  - turn on backlight
  Wire.begin();                  //  - open two wire serial port to LCD
  Wire.beginTransmission(0x27);  //  - start communications with serial port

  pinMode(PIN_IN_MODE, INPUT);
  //Inverted because I wanted the default without jumper to be Tacho mode.
  HeaderMode = !digitalRead(PIN_IN_MODE);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Initialising");
  lcd.setCursor(0, 1);

  if (HeaderMode)
  {
        lcd.print("Serial Reader");  // Announcement
  } else
  {
        lcd.print("Tachometer");     // Announcement
  }

  lcd.setCursor(0, 0);

  // get ready for an interrupt
  pos = 0;   // buffer empty
  process_it = false;
  InPacket = false;

  //-----Timers
  TIMSK0 &= ~(1 << TOIE0); // Disable timer0
  cli();//stop interrupts
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register
  OCR1A = TIME_OUT_COUNT;
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  TCCR1B = (1 << CS12);//prescale = 256
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  //-----SPI Interface
  // turn on SPI in slave mode
  SPCR |= bit (SPE);

  // have to send on master in, *slave out* ie MISO as OUTPUT, but
  //Temporarily set as high impedance
  pinMode(MISO, INPUT);

  SPI.setDataMode (SPI_MODE2);
  SPI.setClockDivider(SPI_CLOCK_DIV32);
  // now turn on interrupts
  SPI.attachInterrupt();
  sei();//allow interrupts, possibly redundant

  inError = false;
  Serial.println("\n\t\tSTARTING");
}  // end of setup

//-----------------------------------------------------------------------------------------------------
char toHex(byte in)
{
  switch (in)
  {
    case 0:      return ('0');
    case 1:      return ('1');
    case 2:      return ('2');
    case 3:      return ('3');
    case 4:      return ('4');
    case 5:      return ('5');
    case 6:      return ('6');
    case 7:      return ('7');
    case 8:      return ('8');
    case 9:      return ('9');
    case 10:      return ('A');
    case 11:      return ('B');
    case 12:      return ('C');
    case 13:      return ('D');
    case 14:      return ('E');
    case 15:      return ('F');
    default:      return ('_');
  }
}
//-----------------------------------------------------------------------------------------------------
boolean GetStopFlag()
{
  if (buf[10] & 0x01 ==  1)
  {
    return true;
  }
  else
  {
    return false;
  }
}

//-----------------------------------------------------------------------------------------------------
void block_delay(unsigned long units)// 100000 ~= 88ms, 100 ~= 88us, 114 ~= 100us
{
  unsigned long i;
  for (i = 0; i < 1980; i++)
  {
    unsigned long j;
    for (j = 0; j < units; j++)
    { // 100000 ~= 88ms, 100 ~= 88us, 114 ~= 100us
      // a "nop" takes one clock cycle ~= 1/16us
      asm("nop"); // Stop optimizations
    }
  }
}

//-----------------------------------------------------------------------------------------------------
byte GetHex(int start)
{
  //index-----0---/----1---/----2---/----------------------------
  //     ---------/--------/--------/----------------------------
  //     --------------^----^-------/----------------------------
  //need to get 4 bits from index and index+1
  //bits are streamed highest first so after 4 bits shifting right will not produce sufficient bits.
  //extra bits have to be supplemented from index+1
  byte index = 0;
  byte atBit = 0;
  byte hex = 0;

  //So why not stick them all into an integer and then take a 4 bit slice?

  index = start / 8;
  atBit = start % 8;
  uint16_t local = (uint16_t) buf[index] * 256 + buf[index + 1];

  //
  //      Serial.print ("index= 0x");
  //      Serial.println (index, HEX);
  //      Serial.print ("atBit= 0x");
  //      Serial.println (atBit, HEX);
  //      Serial.print ("local= 0x");
  //      Serial.println (local, HEX);

  switch (atBit)
  {
    case 0:
      hex = (local >> (16 - 4) ) & 0x0F; //-----/--------|--------/-----
      break;                             //      ^^^^
    case 1:
      hex = (local >> (16 - 5) ) & 0x0F; //-----/--------|--------/-----
      break;                             //       ^^^^
    case 2:
      hex = (local >> (16 - 6) ) & 0x0F; //-----/--------|--------/-----
      break;                             //        ^^^^
    case 3:
      hex = (local >> (16 - 7) ) & 0x0F; //-----/--------|--------/-----
      break;                             //         ^^^^
    case 4:
      hex = (local >> (16 - 8) ) & 0x0F; //-----/--------|--------/-----
      break;                             //          ^^^^
    case 5:
      hex = (local >> (16 - 9) ) & 0x0F; //-----/--------|--------/-----
      break;                             //           ^^^^
    case 6:
      hex = (local >> (16 - 10) ) & 0x0F;//-----/--------|--------/-----
      break;                             //            ^^^^
    case 7:
      hex = (local >> (16 - 11) ) & 0x0F;//-----/--------|--------/-----
      break;                             //             ^^^^
    default:
      hex = 0;
      break;
  }
  return hex;
}

//-----------------------------------------------------------------------------------------------------
int get_digit_from_data(uint8_t data)
{
  uint8_t segments = (data & 0xFE);
  int ret = 0;
  //  Serial.println ();
  //  Serial.print("segments=0x");
  //  Serial.println (segments, HEX);
  switch (segments)
  {
    case 0xFA:   //0x7D:  //FA
      ret = 0;
      break;
    case 0x0A:   //0x05:  //0A
      ret = 1;
      break;
    case 0xD6:   //0x6B:  //D7
      ret = 2;
      break;
    case 0x9E:   //0x4F:  //9E
      ret = 3;
      break;
    case 0x2E:   //0x17:  //2E
      ret = 4;
      break;
    case 0xBC:    //0x5E:  //BC
      ret = 5;
      break;
    case 0xFC:    //0x7E:  //FC
      ret = 6;
      break;
    case 0x1A:    //0x0D:  //1A
      ret = 7;
      break;
    case 0xFE:    //0x7F:  //FE
      ret = 8;
      break;
    case 0xBE:    //0x5F:  //BE
      ret = 9;
      break;
    default:    //
      ret = -1;
      break;
  }
  //  Serial.print("ret=");
  //  Serial.println (ret);
  return ret;
}
//-----------------------------------------------------------------------------------------------------
int get_rpm()
{
  //  printf("get_rpm\n");
  //  return 101;
  //----------------------------------------------------------------------------------------------------
  // Returns the spindle speed or an error code as a -ve value.
  //----------------------------------------------------------------------------------------------------
  //  int temp = 0;
  int ret = 0;
  uint8_t ads = 0;
  uint8_t thou = 0;
  uint8_t hunds = 0;
  uint8_t tens = 0;

  ads = GetHex(36) * 0x10 + GetHex(40);
  if (ads != 0xA0)
  {
    return (0 - ads);
  }
  thou = get_digit_from_data(GetHex(45) * 0x10 + GetHex(49));

  ads = GetHex(53) * 0x10 + GetHex(57);
  if (ads != 0xA1)
  {
    return (0 - ads - 1000);
  }
  hunds = get_digit_from_data(GetHex(62) * 0x10 + GetHex(66));

  ads = GetHex(70) * 0x10 + GetHex(74);
  if (ads != 0xA2)
  {
    return (0 - ads - 2000);
  }
  tens = get_digit_from_data(GetHex(79) * 0x10 + GetHex(83));

  ret = thou * 1000 + hunds * 100 + tens * 10;
  return ret;
}

//-----------------------------------------------------------------------------------------------------
void lcdHex(int start)
{
  lcd.print(toHex(GetHex(start)));
}

//-----------------------------------------------------------------------------------------------------
void DecodeError(int eCode)
{
  inError = true;

//  lcd.begin(16, 2);
//
//  lcd.print("Error=");
//  lcd.print(eCode, DEC);
//  lcd.println("     ");

    lcd.setCursor(15, 1);
  lcd.println(".");

  //  Serial.print ("eCode=");
  //  Serial.println (eCode);
  //  block_delay (400);//allow time for Serial i/f to complete.
}

//
//-----------------------------------------------------------------------------------------------------
void DecodeHeader()
{
  //  Serial.println ("DecodeHeader");
  //  int rpm; // 'rpm' is displayed as hex after being resolved from the four header "frames"
  //  unsigned long count;
  if (pos * 8 >= PACKET_BITS_COUNT)
  {
    //    DecodeDebug();

    // Header Frame 0
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcdHex(0);
    //      lcd.print(" ");
    lcdHex(4);
    //      lcd.print(" ");
    lcdHex(8);
    lcd.print(".");

    // Header Frame 1
    lcdHex(12);
    //      lcd.print(" ");
    lcdHex(16);
    //      lcd.print(" ");
    lcdHex(20);
    lcd.print(".");

    // Header Frame 2
    lcdHex(24);
    //      lcd.print(" ");
    lcdHex(28);
    //      lcd.print(" ");
    lcdHex(32);
    lcd.print(" ");

    lcd.setCursor(0, 1);
    //    //      Data Frame 0
    //    lcd.print(">");
    //    lcdHex(45);
    //    lcdHex(49);
    //    //      Data Frame 1
    //    lcd.print("  ");
    //    lcdHex(62);
    //    lcdHex(66);
    //    //      Data Frame 2
    //    lcd.print("  ");
    //    lcdHex(79);
    //    lcdHex(83);
    //    //      Data Frame 3
    //    lcd.print("  ");
    //    lcdHex(96);
    //    lcdHex(100);
    //    lcd.print("<");

    lcd.setCursor(0, 1);
    lcd.print(8 * pos);
    lcd.print(" cks");

  } else
  {
    DecodeError(-1);
  }
}

//-----------------------------------------------------------------------------------------------------
void DecodeData()
{
  //  Serial.println ("DecodeData");
  int rpm = 0; // variable rpm is sent to display after being resolved from the four "frames"
  //  unsigned long count;
  //  Serial.print ("8*pos= ");
  //  Serial.println (8 * pos);
  if (8 * pos == 104) //(packet_bits_pos >= PACKET_BITS_COUNT) // && count < MAXCOUNT)
  {
    // Tacho mode
    //    DecodeDebug();

    //    lcd.setCursor(0, 0);
    rpm = get_rpm(); // Extract RPM's from the data buffer return -ve if in error

    if (rpm < 0)
    {
      DecodeError(rpm);
    }
    else if (rpm == 0)//Mill stopped, STOP flag shown independantly.
    {
//      Serial.print (rpm);
//      Serial.println (" RPM");
      lcd.begin(16, 2);
      lcd.setCursor(0, 0);
      lcd.print(rpm);
      lcd.print(" RPM");

      if (GetStopFlag())
      {
//        Serial.println(" STOPPED");
        lcd.setCursor(7, 0);
        lcd.print("STOPPED");
      }
    }
    else  //Must be positive RPM
    {
      Serial.print (rpm);
      Serial.println (" RPM");
      lcd.begin(16, 2);
      lcd.setCursor(0, 0);
      lcd.print(rpm);
      lcd.print(" rpm");
      lcd.setCursor(0, 1);
      lcd.print("               ");
    }


    if (Diagnostics == true)//Only if pin 0 is set to LOW
    {
      lcd.setCursor(0, 1);
      lcd.print(pos * 8);
      lcd.print(" cks");
      // Frame 3 (the fourth frame)

      special = GetHex(96) * 0x10 + GetHex(100);  //get4th();

//      Serial.print("special=0x");
//      Serial.println(special, HEX);
//      Serial.println(special);

      lcd.setCursor(8, 1);
      lcd.print("0x");
      lcd.print(toHex((special >> 4) & 0x0F));
      lcd.print(toHex(special & 0x0F));

      lcd.setCursor(0, 1);
      lcd.print(pos * 8);
      lcd.print(" cks");
    }
  }
  else
  {
    DecodeError(-2);
  }
}

//-----------------------------------------------------------------------------------------------------
void DecodeBuffer()
{
  if (HeaderMode)
  {
    DecodeHeader();
  } else
  {
    DecodeData();
  }
}

//-----------------------------------------------------------------------------------------------------
void loop (void)// main loop - wait for flag set in interrupt routine
{
  process_it = false;
  InPacket = false;

  pinMode(MISO, INPUT);
  //  Serial.println ("Clear buffer and reset pointer used in interrupt");
  for (uint8_t  c = 0; c < bufSize; c++)
  {
    buf[c] = 0;
  }
  //
  sei();//allow interrupts
  //If interrupts are already occuring then we must have missed the start of the
  //clocks for this packet.
  while (InPacket == true)//So wait until clocks have ended
  { // receiving interrupts
    block_delay(22);
  }

  //reset pointer used in interrupt
  pos = 0;
  while (pos == 0) //spin here until a clock has incremented the pointer pos
  {
    asm("nop");
  }

  //Received a clock so InPacket is true
  //  Serial.println ("Wait for ProcessIt flag");
  while (InPacket == true)
  { // receiving interrupts
    block_delay(1);
  }
  //  cli();//stop interrupts

  if (process_it)
  {
    DecodeBuffer();
    //    int   rpm = get_rpm();  //GetHex(36); //get_rpm();
    //    Serial.print(rpm, DEC);
    //    Serial.println(" rpm , ");
  } else
  {
    Serial.println ("--- !");  //ProcessIt received");
  }
  if (inError)
  {
    cli();//stop interrupts
    // Reset SPI, and most importantly clears the SPDR of its last value.
    SPCR &= ~(_BV(SPE));
    SPCR |= (_BV(MSTR));
    SPCR &= ~(_BV(MSTR));
    SPCR |= (_BV(SPE));
    block_delay (2000);
    inError = false;
  }

  Diagnostics = !digitalRead(PIN_IN_MODE);
  delay (200);
}  // end of loop
//END OF SKETCH
