/* Teensy_UTFT_I2C_1Wire_Slave
// 
// Version 0.01 - 08/22/2014 - Jim Mayhugh
//
// This program requires the UTFT library from
// web: http://www.henningkarlsen.com/electronics
// and the WireFast library from Brian (nox771 at gmail.com)
// All applicable licenses apply.
//
// This program prints a character display in Landscape Mode
// using I2C or 1-Wire
// This example code is in the public domain.
*/

#define USE1WIRE 1 // set to 1 to use 1-wire, 0 for I2C

#include <UTFT.h>
#if USE1WIRE == 1
#include <OneWireSlave.h>
#include <t3mac.h>
#else
#include <WireFast.h>
#endif
// items used by both selections

// Function prototypes
void clearCommandBuf(void);
void printCommand(void);
uint16_t getVGAcolor(uint8_t VGAcolor);


// Declare which fonts we will be using
extern uint8_t SmallFont[];
extern uint8_t BigFont[];
extern uint8_t UbuntuBold[];
extern uint8_t SevenSegNumFont[];

const uint8_t ledPin = 13;

const uint8_t  displayStrSize = 33; 
char displayStr[displayStrSize+1];

typedef struct
{
  uint32_t  flags;                   // various flags to control setup
  uint8_t   font;                    // Font to use - 0 - No Change, 1 - UbuntuBold, 2 - BigFont, 3 - Small Font, 4 - SevenSegNumFont
  uint8_t   bGR;                     // Background RED value or VGA_Color
  uint8_t   bGG;                     // Background GREEN value
  uint8_t   bGB;                     // Background BLUE value
  uint8_t   cR;                      // Character/foreground RED or VGA_Color value for draw*, fill* and print commands 
  uint8_t   cG;                      // Character/foreground GREEN value for draw*, fill* and print commands
  uint8_t   cB;                      // Character/foreground BLUE value for draw*, fill* and print commands
  uint8_t   dispP;                   // Display Page 0 - 7 (CPLD only)
  uint8_t   lineP;                   // Line Position 0 - 14
  uint8_t   chrP;                    // Character Position 0 - 32 or  0xFC 0=LEFT 0xFD=RIGHT 0xFE=CENTER
  uint16_t  degR;                    // Degress of Rotation 0-359 Default = 0
  char      dSTR[displayStrSize+1];  // Character string to print
  uint16_t   x1;                     // rectangle x start or x center of circle
  uint16_t   y1;                     // rectangle y start or y center of circle
  uint16_t   x2;                     // rectangle x end
  uint16_t   y2;                     // rectangle y end
  uint16_t   rad;                    // radius of circle
  uint8_t   bRDY;                    // Buffer ready to process = 1
}glcdCMD;

const uint32_t setInitL           = 0x00000001;               // initialize screen (Landscape)
const uint32_t setInitP           = (setInitL          << 1); // 0x00000001 //initialize screen (Portrait)
const uint32_t clrScn             = (setInitP          << 1); // 0x00000002;  // clear scrn flag
const uint32_t setFillScrRGB      = (clrScn            << 1); // 0x00000004;  // set fillSCR() with RGB values
const uint32_t setFillScrVGA      = (setFillScrRGB     << 1); // 0x00000008;  // set fillSCR() with VGA coloors
const uint32_t setBackRGB         = (setFillScrVGA     << 1); // 0x00000010;  // set setBackColor() with RGB values
const uint32_t setBackVGA         = (setBackRGB        << 1); // 0x00000020;  // set setBackColor() with VGA values
const uint32_t setColorRGB        = (setBackVGA        << 1); // 0x00000040;  // set setColor() with RGB values
const uint32_t setColorVGA        = (setColorRGB       << 1); // 0x00000080;  // set setColor() with VGA values
const uint32_t setfont            = (setColorVGA       << 1); // 0x00000200;  // set font
const uint32_t setDispPage        = (setfont           << 1); // 0x00000400;  // set display Page (CPLD only)
const uint32_t setWrPage          = (setDispPage       << 1); // 0x00000800;  // set Page to Write Page (CPLD only)
const uint32_t setDrawPixel       = (setWrPage         << 1); // 0x00001000;  // set drawPixel
const uint32_t setDrawLine        = (setDrawPixel      << 1); // 0x00002000;  // set drawLine()
const uint32_t setDrawRect        = (setDrawLine       << 1); // 0x00004000;  // set drawRect()
const uint32_t setFillRect        = (setDrawRect       << 1); // 0x00008000;  // set fillRect();
const uint32_t setDrawRRect       = (setFillRect       << 1); // 0x00010000;  // set drawRoundRec()
const uint32_t setDrawFRRect      = (setDrawRRect      << 1); // 0x00020000;  // set drawFillRoundRec()
const uint32_t setDrawCircle      = (setDrawFRRect     << 1); // 0x00040000;  // set drawCircle()
const uint32_t setDrawFillCircle  = (setDrawCircle     << 1); // 0x00080000;  // set fillCircle()
const uint32_t setPrintStr        = (setDrawFillCircle << 1); // 0x00080000;  // set fillCircle()


glcdCMD glcdCLR = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", 0, 0, 0, 0, 0, 0};

union glcdData
{
  glcdCMD glcdBUF;
  uint8_t glcdARRAY[sizeof(glcdCMD)];
}glcdUNION;

#if USE1WIRE == 1 // set for 1-Wire

//function prototypes specific to 1-Wire

void glcd(void);
void blinking(void);

int ledState = LOW;             // ledState used to set the LED
long previousMillis = 0;        // will store last time LED was updated
long interval = 250;            // interval at which to blink (milliseconds)
const uint8_t oneWireSlave = 0x44; // slave addr

//               {Family ,         <---, ----, ----, ID--, ----, --->,  CRC} 
uint8_t rom[8] = {oneWireSlave,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00};

const uint8_t dsslavepin = 29;

OneWireSlave ds(dsslavepin);
#else // set for I2C
// function prototypes specific to I2C
void receiveEvent(size_t len);
void requestEvent(void);
void print_i2c_status(void);

const uint8_t i2cSlave = 0x44; // slave addr
size_t addr;

// I2C Definitions
// Command definitions
#define WRITE 0x10
#define READ  0x20
#endif
// uint8_t i2cBuff[i2cBuffSize]; // command string sent to GLCD Slave

enum vgaColor {BLACK, WHITE, RED, GREEN, BLUE, SILVER, GRAY, MAROON, YELLOW, OLIVE, LIME, AQUA, TEAL, NAVY, FUCHSIA, PURPLE};


// Set the pins to the correct ones for your development shield
// ------------------------------------------------------------
// Teensy 3.x TFT Test Board                   : <display model>,23,22, 3, 4
//
// Remember to change the model parameter to suit your display module!
// UTFT    myGLCD(ITDB50,23,22, 3, 4); // 5" SEEEDStudio display
UTFT    myGLCD(CTE70,23,22, 3, 4);     // 7" CTE display




int x = 0, y = 0;
uint32_t setDebug = 0x00000001;
const uint32_t debugALL  = 0xFFFFFFFF;
const uint32_t debug1Wire = 0x00000001;
const uint32_t debugI2C   = 0x00000002;
const uint32_t debugGLCD  = 0x00000004;

uint8_t fontHMult, fontVMult;


void setup()
{
  if(setDebug)
  {
    Serial.begin(115200);
    delay(2000);
    Serial.println(F("Starting Serial Debug"));
  }

  pinMode(ledPin,OUTPUT); // LED

  clearCommandBuf();

#if USE1WIRE == 1
  // set the 1-wire address:
  read_mac();
  for( uint8_t x = 0; x < 6; x++ )
  {
    rom[x+1] = mac[x];
  }
  rom[7] = ds.crc8(rom, 7);
  attachInterrupt(dsslaveassignedint, slave, CHANGE);
  pinMode(ledPin, OUTPUT);
  ds.init(rom);
  ds.setPower(EXTERNAL);
  if(setDebug & debugGLCD)
  {
    Serial.println(F("Setting attach0F to glcd"));
  }
  ds.attach0Fh(glcd);
  if(setDebug & debugGLCD)
  {
    Serial.println(F("Exiting attach0F"));
  }
#else
  // Setup for Slave mode, address 0x44, pins 29/30, external pullups, 2400kHz
  Wire1.begin(I2C_SLAVE, 0x44, I2C_PINS_29_30, I2C_PULLUP_EXT, I2C_RATE_400);
  // register events
  Wire1.onReceive(receiveEvent);
#endif
  
  myGLCD.InitLCD(LANDSCAPE);  // default mode

  myGLCD.clrScr();
}

void loop()
{
#if USE1WIRE == 1
  if(setDebug & debug1Wire)
  {

    Serial.print(F("1-Wire Address = "));
    for(uint8_t x = 0; x < sizeof(rom); x++)
    {
      Serial.print(F("0x"));
      if(rom[x] < 0x10)
      {
        Serial.print(F("0"));
      }
      Serial.print(rom[x], HEX);
      Serial.print(F(" "));
    }
    Serial.println();

    Serial.print(F("1-Wire Command Buffer = "));
    for(uint8_t x = 0; x < sizeof(glcdCMD); x++)
    {
      Serial.print(F("0x"));
      if(glcdUNION.glcdARRAY[x] < 0x10)
      {
        Serial.print(F("0"));
      }
      Serial.print(glcdUNION.glcdARRAY[x], HEX);
      Serial.print(F(" "));
    }
    Serial.println();

    blinking();
  }
#else
  if(glcdUNION.glcdBUF.bRDY)
  {
    digitalWrite(ledPin,HIGH); // turn LED on at start of I2C requests
    if(setDebug & debugI2C)
    {
      printCommand();
    }
    parseCommand();
    clearCommandBuf();
    digitalWrite(ledPin,LOW); // turn LED off upon completion
  }
#endif
}

//
// handle Rx Event (incoming request/data)
//
#if USE1WIRE == 1

void slave()
{
  ds.MasterResetPulseDetection();
}

void glcd() 
{
  noInterrupts();
  if(setDebug & debugGLCD)
  {
    Serial.println(F("Entering glcd"));
  }
  ds.recvData(glcdUNION.glcdARRAY, sizeof(glcdCMD));

  if(setDebug)
  {
    Serial.print(F("1-Wire Command Buffer = "));
    for(uint8_t x = 0; x < sizeof(glcdCMD); x++)
    {
      Serial.print(F("0x"));
      if(glcdUNION.glcdARRAY[x] < 0x10)
      {
        Serial.print(F("0"));
      }
      Serial.print(glcdUNION.glcdARRAY[x], HEX);
      Serial.print(F(" "));
    }
    Serial.println();
  }

  parseCommand();
  clearCommandBuf();
  interrupts();
}

void blinking()
{
  unsigned long currentMillis = millis(); 
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
  }
}

#else
void receiveEvent(size_t len)
{
  if(Wire1.available())
  {
    // grab command
    switch(Wire1.readByte())
    {
      case WRITE:
      {
        addr = Wire1.readByte();                // grab addr
        while(Wire1.available())
        if(addr < sizeof(glcdCMD))                 // drop data beyond mem boundary
          glcdUNION.glcdARRAY[addr++] = Wire1.readByte(); // copy data to mem
        else
          Wire1.readByte();               // drop data if mem full
        break;
      }

      case READ:
      {
        addr = Wire1.readByte();                // grab addr
        break;
      }
    }
  }
}
//
// print I2C status
//
void print_i2c_status(void)
{
  int status = Wire1.status();
  if(setDebug)
  {
    Serial.print(F("Wire1.status() = "));
    Serial.print(status);
    switch(status)
    {
      case I2C_WAITING:  Serial.print(F(" - I2C waiting, no errors\n")); break;
      case I2C_ADDR_NAK: Serial.print(F(" - Slave addr not acknowledged\n")); break;
      case I2C_DATA_NAK: Serial.print(F(" - Slave data not acknowledged\n")); break;
      case I2C_ARB_LOST: Serial.print(F(" - Bus Error: Arbitration Lost\n")); break;
      default:           Serial.print(F(" - I2C busy\n")); break;
    }
  }
}
#endif

void parseCommand(void)
{
  
  for(uint8_t x = 1; x < 32; x++)
  {
    uint32_t y = 0x00000001;
    y = y << x;
    switch(glcdUNION.glcdBUF.flags & y)
    {
      case clrScn: // clear screen
      {
        myGLCD.clrScr();
        break;
      }

      case setFillScrRGB: // set screen color with RGB Values
      {
        myGLCD.fillScr(glcdUNION.glcdBUF.bGR, glcdUNION.glcdBUF.bGG, glcdUNION.glcdBUF.bGB);
        if(setDebug)
        {
          Serial.print(F("Screen Fill RGB Color set to "));
          Serial.print(glcdUNION.glcdBUF.bGR);
          Serial.print(F(", ")); 
          Serial.print(glcdUNION.glcdBUF.bGG);
          Serial.print(F(", ")); 
          Serial.print(glcdUNION.glcdBUF.bGB);
          Serial.println();
        } 
        break;
      }

      case setFillScrVGA: // set screen color with VGA_Color Value
      {
        myGLCD.fillScr(getVGAcolor(glcdUNION.glcdBUF.bGR));
        if(setDebug)
        {
          Serial.print(F("glcdUNION.glcdBUF.bGR = "));
          Serial.print(glcdUNION.glcdBUF.bGR);
          Serial.print(F(" - Screen Fill VGA Color set to 0x"));
          Serial.println(getVGAcolor(glcdUNION.glcdBUF.bGR), HEX);
        } 
        break;
      }

      case setBackRGB:
      {
        myGLCD.setBackColor(glcdUNION.glcdBUF.bGR, glcdUNION.glcdBUF.bGG, glcdUNION.glcdBUF.bGB);
        if(setDebug)
        {
          Serial.print(F("Background RGB Color set to "));
          Serial.print(glcdUNION.glcdBUF.bGR);
          Serial.print(F(", ")); 
          Serial.print(glcdUNION.glcdBUF.bGG);
          Serial.print(F(", ")); 
          Serial.print(glcdUNION.glcdBUF.bGB);
          Serial.println();
        } 
        break;
      }

      case setBackVGA:
      {
        myGLCD.setBackColor(getVGAcolor(glcdUNION.glcdBUF.bGR));
        if(setDebug)
        {
          Serial.print(F("Background UGA Color set to 0x"));
          Serial.println(getVGAcolor(glcdUNION.glcdBUF.bGR), HEX);
        } 
        break;
      }

      case setColorRGB:
      {
        myGLCD.setColor(glcdUNION.glcdBUF.cR, glcdUNION.glcdBUF.cG, glcdUNION.glcdBUF.cB);
        if(setDebug & debugGLCD)
        {
          Serial.print(F("Character RGB Color set to "));
          Serial.print(glcdUNION.glcdBUF.cR);
          Serial.print(F(", ")); 
          Serial.print(glcdUNION.glcdBUF.cG);
          Serial.print(F(", ")); 
          Serial.print(glcdUNION.glcdBUF.cB);
          Serial.println();
        } 
        break;
      }

      case setColorVGA:
      {
        myGLCD.setColor(getVGAcolor(glcdUNION.glcdBUF.cR));
        if(setDebug)
        {
          Serial.print(F("Character VGA Color set to 0x"));
          Serial.print(getVGAcolor(glcdUNION.glcdBUF.cR), HEX);
        } 
        break;
      }

      case setfont:
      {
        switch(glcdUNION.glcdBUF.font)
        {
          case 1:
          {
            myGLCD.setFont(UbuntuBold);
            if(setDebug)
              Serial.println(F("Set Font to UbuntuBold"));
            break;  
          }
          case 2:
          {
            myGLCD.setFont(BigFont);
            if(setDebug)
              Serial.println(F("Set Font to BigFont"));
            break;  
          }
          case 3:
          {
            myGLCD.setFont(SmallFont);
            if(setDebug)
              Serial.println(F("Set Font to SmallFont"));
            break;  
          }
          case 4:
          {
            myGLCD.setFont(SevenSegNumFont);
            if(setDebug)
              Serial.println(F("Set Font to SevenSegNumFont"));
            break;  
          }
          case 0:
          default:
          {
            break; // do nothing
          }
        }
      
        fontHMult = myGLCD.getFontXsize();
        fontVMult = myGLCD.getFontYsize();
        if(setDebug)
        {
          Serial.print(F("Font Size is "));
          Serial.print(fontHMult);
          Serial.print(F(" x "));
          Serial.print(fontVMult);
          Serial.println(F(" pixels"));
        }
        break;
      }

      case setInitL:
      {
        myGLCD.InitLCD(LANDSCAPE);  // default mode
        if(setDebug)
        {
          Serial.println(F("LANDSCAPE Mode Set "));
        }
        break;
      }

      case setInitP:
      {
        myGLCD.InitLCD(PORTRAIT);  // default mode
        if(setDebug)
        {
          Serial.println(F("PORTRAIT Mode Set "));
        }
        break;
      }

      case setDispPage:
      {
        myGLCD.setDisplayPage(glcdUNION.glcdBUF.dispP);
        if(setDebug)
        {
          Serial.print(F("Display Page set to "));
          Serial.println(glcdUNION.glcdBUF.dispP);
        }
        break;
      }

      case setWrPage:
      {
        myGLCD.setWritePage(glcdUNION.glcdBUF.dispP);
        if(setDebug)
        {
          Serial.print(F("Write Page set to "));
          Serial.println(glcdUNION.glcdBUF.dispP);
        }
        break;
      }

      case setDrawPixel:
      {
        myGLCD.drawPixel(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1);
        if(setDebug)
        {
          Serial.print(F("Pixel drawn at location "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.println(glcdUNION.glcdBUF.y1);
        }
        break;
      }

      case setDrawLine:
      {
        myGLCD.drawLine(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.x2, glcdUNION.glcdBUF.y2);
        if(setDebug)
        {
          Serial.print(F("Line Drawn From "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.print(glcdUNION.glcdBUF.y1);
          Serial.print(F(", to "));
          Serial.print(glcdUNION.glcdBUF.x2);
          Serial.print(F(", "));
          Serial.println(glcdUNION.glcdBUF.y2);
        }
        break;
      }

      case setDrawRect:
      {
        myGLCD.drawRect(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.x2, glcdUNION.glcdBUF.y2);
        if(setDebug)
        {
          Serial.print(F("Rectangle Drawn From "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.print(glcdUNION.glcdBUF.y1);
          Serial.print(F(", to "));
          Serial.print(glcdUNION.glcdBUF.x2);
          Serial.print(F(", "));
          Serial.println(glcdUNION.glcdBUF.y2);
        }
        break;
      }

      case setDrawRRect:
      {
        myGLCD.drawRoundRect(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.x2, glcdUNION.glcdBUF.y2);
        if(setDebug)
        {
          Serial.print(F("Rounded Rectangle Drawn From "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.print(glcdUNION.glcdBUF.y1);
          Serial.print(F(", to "));
          Serial.print(glcdUNION.glcdBUF.x2);
          Serial.print(F(", "));
          Serial.println(glcdUNION.glcdBUF.y2);
        }
        break;
      }

      case setFillRect:
      {
        myGLCD.fillRect(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.x2, glcdUNION.glcdBUF.y2);
        if(setDebug)
        {
          Serial.print(F("Filled Rectangle Drawn From "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.print(glcdUNION.glcdBUF.y1);
          Serial.print(F(", to "));
          Serial.print(glcdUNION.glcdBUF.x2);
          Serial.print(F(", "));
          Serial.println(glcdUNION.glcdBUF.y2);
        }
        break;
      }

      case setDrawFRRect:
      {
        myGLCD.fillRoundRect(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.x2, glcdUNION.glcdBUF.y2);
        if(setDebug)
        {
          Serial.print(F("Filled Rounded Rectangle Drawn From "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.print(glcdUNION.glcdBUF.y1);
          Serial.print(F(", to "));
          Serial.print(glcdUNION.glcdBUF.x2);
          Serial.print(F(", "));
          Serial.println(glcdUNION.glcdBUF.y2);
        }
        break;
      }

      case setDrawCircle:
      {
        myGLCD.drawCircle(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.rad);
        if(setDebug)
        {
          Serial.print(F("Circle Drawn with Center at "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.print(glcdUNION.glcdBUF.y1);
          Serial.print(F(", with a radius of "));
          Serial.println(glcdUNION.glcdBUF.rad);
        }
        break;
      }

      case setDrawFillCircle:
      {
        myGLCD.fillCircle(glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.rad);
        if(setDebug)
        {
          Serial.print(F("Filled Circle Drawn with Center at "));
          Serial.print(glcdUNION.glcdBUF.x1);
          Serial.print(F(", "));
          Serial.print(glcdUNION.glcdBUF.y1);
          Serial.print(F(", with a radius of "));
          Serial.println(glcdUNION.glcdBUF.rad);
        }
        break;
      }

      case setPrintStr:
      {
        myGLCD.print((char *) glcdUNION.glcdBUF.dSTR, (glcdUNION.glcdBUF.chrP * fontHMult), (glcdUNION.glcdBUF.lineP * fontVMult), glcdUNION.glcdBUF.degR);
        if(setDebug)
        {
          Serial.print(F("Character string ** "));
          Serial.print((char *) glcdUNION.glcdBUF.dSTR);
          Serial.print(F(" ** printed at line ")); 
          Serial.print(glcdUNION.glcdBUF.chrP, DEC);
          Serial.print(F(", character position ")); 
          Serial.print(glcdUNION.glcdBUF.lineP, DEC);
          Serial.print(F(", rotated ")); 
          Serial.print(glcdUNION.glcdBUF.degR, DEC);
          Serial.println(F(" degrees"));
        }
        break;
      }

      default:
      {
        break;
      }
    }
  }  
}

void clearCommandBuf(void)
{
    glcdUNION.glcdBUF = glcdCLR;
}

uint16_t getVGAcolor(uint8_t VGAcolor)
{
  uint16_t color;
  
  switch(VGAcolor)
  {
    case BLACK:
    {
      color = VGA_BLACK;
      break;
    }
    
    case WHITE:
    {
      color = VGA_WHITE;
      break;
    }
    
    case RED:
    {
      color = VGA_RED;
      break;
    }
    
    case GREEN:
    {
      color = VGA_GREEN;
      break;
    }
    
    case BLUE:
    {
      color = VGA_BLUE;
      break;
    }
    
    case SILVER:
    {
      color = VGA_SILVER;
      break;
    }
    
    case GRAY:
    {
      color = VGA_GRAY;
      break;
    }
    
    case MAROON:
    {
      color = VGA_MAROON;
      break;
    }
    
    case YELLOW:
    {
      color = VGA_YELLOW;
      break;
    }
    
    case OLIVE:
    {
      color = VGA_OLIVE;
      break;
    }
    
    case LIME:
    {
      color = VGA_LIME;
      break;
    }
    
    case AQUA:
    {
      color = VGA_AQUA;
      break;
    }
    
    case TEAL:
    {
      color = VGA_TEAL;
      break;
    }
    
    case NAVY:
    {
      color = VGA_NAVY;
      break;
    }
    
    case FUCHSIA:
    {
      color = VGA_FUCHSIA;
      break;
    }

    case PURPLE:
    {
      color = VGA_PURPLE;
      break;
    }

     default:
    {
      color = VGA_BLACK;
      break;
    }
  }
  return(color);
}

void printCommand(void)
{
/*
  Serial.print(F("Command = "));
  Serial.print(glcdUNION.glcdBUF.flags);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.font);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.setBG);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.bGR);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.bGG);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.bGB);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.setC);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.cR);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.cG);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.cB);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.lineP);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.chrP);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.degR);
  Serial.print(F(", "));
  Serial.print(glcdUNION.glcdBUF.dSTR);
  Serial.println();
*/
}

