/* Teensy_UTFT_I2C_1Wire_Slave
// 
// Version 0.04 - 01/10/2015 - Jim Mayhugh
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

const char* versionStrNumber = "V-0.04";
const char* versionStrDate   = "01/10/2015";

#define USE1WIRE   1 // set to 1 to use 1-wire, 0 for I2C
#define USECPLD    1 // set to 1 if using CTE70CPLD unit with paging and backlight control
#define USEVGAONLY 1 // if set to 1, use VGA Values ONLY, otherwise use VGA or RGB

#define FALSE 0

// Should restart Teensy 3, will also disconnect USB during restart

// From http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0337e/Cihcbadd.html
// Search for "0xE000ED0C"
// Original question http://forum.pjrc.com/threads/24304-_reboot_Teensyduino%28%29-vs-_restart_Teensyduino%28%29?p=35981#post35981

#define RESTART_ADDR       0xE000ED0C
#define READ_RESTART()     (*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val) ((*(volatile uint32_t *)RESTART_ADDR) = (val))

#include <UTFT.h>
#include <TeensyNetGLCD.h>

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
extern uint8_t ExtraSevenSegNumFont[];

const uint8_t ledPin = 13;


#if USECPLD == 1
const uint8_t oneWireSlave = 0x45; // slave 1-wire ID for CPLD display
UTFT    myGLCD(CTE70CPLD,23,22, 3, 4);     // 7" CTE display with Paged Display
#else
const uint8_t oneWireSlave = 0x44; // slave 1-wire ID for non-CPLD display
UTFT    myGLCD(CTE70,23,22, 3, 4);     // 7" CTE display
#endif

#if USE1WIRE == 1 // set for 1-Wire

//function prototypes specific to 1-Wire

void glcd(void);
void blinking(void);

int ledState = LOW;             // ledState used to set the LED
long previousMillis = 0;        // will store last time LED was updated
long interval = 250;            // interval at which to blink (milliseconds)

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


int x = 0, y = 0;
uint32_t setDebug = 0x00000008;
const uint32_t debugALL   = 0xFFFFFFFF;
const uint32_t debug1Wire = 0x00000001;
const uint32_t debugI2C   = 0x00000002;
const uint32_t debugGLCD  = 0x00000004;
const uint32_t debugCPLD  = 0x00000008;

uint8_t fontHMult, fontVMult;


void setup()
{
  uint8_t x;
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
  for( x = 0; x < 6; x++ )
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
  Wire1.begin(I2C_SLAVE, 0x44, I2C_PINS_29_30, I2C_PULLUP_EXT, I2C_RATE_2400);
  // register events
  Wire1.onReceive(receiveEvent);
#endif
  
  myGLCD.InitLCD(LANDSCAPE);  // default mode
#if USECPLD == 1
  for(x = 0; x < 8; x++)
  {
    myGLCD.setWritePage(x);
    myGLCD.clrScr();
    myGLCD.setDisplayPage(x);
  }
    myGLCD.setWritePage(0);
    myGLCD.clrScr();
    myGLCD.setDisplayPage(0);
  myGLCD.lcdOn(); // turn on LCD if CTE70CPLD
#endif

  myGLCD.clrScr();
  myGLCD.setFont(UbuntuBold);
  fontVMult = myGLCD.getFontYsize();
  myGLCD.setColor(getVGAcolor(WHITE));
  #if USE1WIRE == 1
  myGLCD.print("1-Wire Display Ready", 0, 0, 0);
  #else
  myGLCD.print("I2C Display Ready", 0, 0, 0);
  #endif
  myGLCD.print(versionStrNumber, 0, (1 * fontVMult), 0);
  myGLCD.print(versionStrDate, 0, (2 * fontVMult), 0);
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

 //   blinking();
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

#if USEVGAONLY == 0
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

#endif
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

      case setFont:
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
          case 5:
          {
            myGLCD.setFont(ExtraSevenSegNumFont);
            if(setDebug)
              Serial.println(F("Set Font to ExtraSevenSegNumFont"));
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
#if USEVGAONLY == 1
        myGLCD.print((char *) glcdUNION.glcdBUF.dSTR, (glcdUNION.glcdBUF.chrP * fontHMult), (glcdUNION.glcdBUF.lineP * fontVMult));
#else
        myGLCD.print((char *) glcdUNION.glcdBUF.dSTR, (glcdUNION.glcdBUF.chrP * fontHMult), (glcdUNION.glcdBUF.lineP * fontVMult), glcdUNION.glcdBUF.degR);
#endif
        if(setDebug)
        {
          Serial.print(F("Character string ** "));
          Serial.print((char *) glcdUNION.glcdBUF.dSTR);
          Serial.print(F(" ** printed at line ")); 
          Serial.print(glcdUNION.glcdBUF.chrP, DEC);
          Serial.print(F(", character position ")); 
          Serial.print(glcdUNION.glcdBUF.lineP, DEC);
          Serial.print(F(", rotated ")); 
#if USEVGAONLY == 0
          Serial.print(glcdUNION.glcdBUF.degR, DEC);
#endif
          Serial.println(F(" degrees"));
        }
        break;
      }

      case setPrintStrXY:
      {
#if USEVGAONLY == 1
        myGLCD.print((char *) glcdUNION.glcdBUF.dSTR, glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1);
#else
        myGLCD.print((char *) glcdUNION.glcdBUF.dSTR, glcdUNION.glcdBUF.x1, glcdUNION.glcdBUF.y1, glcdUNION.glcdBUF.degR);
#endif
        if(setDebug)
        {
          Serial.print(F("Character string ** "));
          Serial.print((char *) glcdUNION.glcdBUF.dSTR);
          Serial.print(F(" ** printed at ")); 
          Serial.print(glcdUNION.glcdBUF.x1, DEC);
          Serial.print(F(", ")); 
          Serial.print(glcdUNION.glcdBUF.y1, DEC);
          Serial.print(F(", rotated ")); 
#if USEVGAONLY == 0
          Serial.print(glcdUNION.glcdBUF.degR, DEC);
#endif
          Serial.println(F(" degrees"));
        }
        break;
      }

      case setPageWrite:
      {
        myGLCD.setWritePage(glcdUNION.glcdBUF.dispP);
        if(setDebug & debugCPLD)
        {
          Serial.print(F("setWritePage() page  "));
          Serial.print(glcdUNION.glcdBUF.dispP);
          Serial.println(F(" selected"));
        }
        break;
      }

      case setPageDisplay:
      {
        myGLCD.setDisplayPage(glcdUNION.glcdBUF.dispP);
        if(setDebug & debugCPLD)
        {
          Serial.print(F("setDisplayPage() page  "));
          Serial.print(glcdUNION.glcdBUF.dispP);
          Serial.println(F(" selected"));
        }
        break;
      }

      case setResetDisplay:
      {
        softReset();
        break;
      }

      default:
      {
        break;
      }
    }
  }  
  KickDog();
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

void softReset(void)
{
  // 0000101111110100000000000000100
  // Assert [2]SYSRESETREQ
  WRITE_RESTART(0x5FA0004);
}  

void KickDog(void)
{
  noInterrupts();
  WDOG_REFRESH = 0xA602;
  WDOG_REFRESH = 0xB480;
  interrupts();
}

#ifdef __cplusplus
extern "C" {
#endif
  void startup_early_hook() {
    WDOG_TOVALL = 0x93E0; // The next 2 lines sets the time-out value. (5 Minutes) This is the value that the watchdog timer compare itself to.
    WDOG_TOVALH = 0x0004;
    WDOG_STCTRLH = (WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN | WDOG_STCTRLH_WAITEN | WDOG_STCTRLH_STOPEN); // Enable WDG
    //WDOG_PRESC = 0; // prescaler 
  }
#ifdef __cplusplus
}
#endif

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

