/* Teensy_UTFT_I2C_1Wire_Master
// 
// Version 0.01 - 08/22/2014 - Jim Mayhugh
//
// This creates an I2C / 1-Wire master device which talks to the simple I2C / 1-Wire slave device given in the
// slave sketch.
//
//
// This example code is in the public domain.
*/

#define USE1WIRE 1 // set to 1 to use 1-wire, 0 for I2C

#if USE1WIRE == 1
#include <OneWire.h>

// 1-Wire specific function prototypes
void find1Wire(void);

OneWire  ds(19);  // a 4.7K resistor is necessary
uint8_t  oneWireAddr[8];

#else

#include <WireFast.h>

#define RCM_SRS0_WAKEUP                     0x01
#define RCM_SRS0_LVD                        0x02
#define RCM_SRS0_LOC                        0x04
#define RCM_SRS0_LOL                        0x08
#define RCM_SRS0_WDOG                       0x20
#define RCM_SRS0_PIN                        0x40
#define RCM_SRS0_POR                        0x80

#define RCM_SRS1_LOCKUP                     0x02
#define RCM_SRS1_SW                         0x04
#define RCM_SRS1_MDM_AP                     0x08
#define RCM_SRS1_SACKERR                    0x20

// Command definitions
#define WRITE 0x10
#define READ  0x20

// I2C specific Function prototypes
void print_i2c_status(void);

const uint8_t i2cSlave = 0x44; // slave addr

#endif

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


glcdCMD glcdCLR = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", 0, 0, 0, 0, 0, 0};

union glcdData
{
  glcdCMD glcdBUF;
  uint8_t glcdARRAY[sizeof(glcdCMD)];
}glcdUNION;

enum fonts { NONE, UBUNTUBOLD, BIGFONT, SMALLFONT, SEVENSEGMENTNUMFONT };


// Function prototypes
void scnClr(void);
void clearCommandBuf(void);
void sendCommand(void);
void clearDisplayStr(void);
void KickDog(void);
void buildCommand(uint32_t  flags , uint8_t font, uint8_t bGR, uint8_t bGG, uint8_t bGB,
                  uint8_t cR, uint8_t cG, uint8_t cB, uint8_t dispP, uint8_t lineP, uint16_t chrP, uint16_t degR, 
                  char *dSTR, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t rad, uint8_t bRDY);


void setup()
{
  pinMode(13,OUTPUT);       // LED

  Serial.begin(115200);

  clearCommandBuf();

#if USE1WIRE == 1
  delay(5000);
  find1Wire(); 
#else
  // Setup for Master mode, pins 18/19, external pullups, 2400kHz
  Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, I2C_RATE_100);
#endif
}

void loop()
{
#if USE1WIRE == 1
  find1Wire();
#endif
  uint8_t displayStrCnt;
  
  for(uint8_t cnt = 1; cnt < 16; cnt++)
  {
    buildCommand(setFillScrVGA, 0, cnt, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", 0, 0, 0, 0, 0, 1);
    sendCommand();
  }

    buildCommand((setfont | setInitL | clrScn), UBUNTUBOLD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", 0, 0, 0, 0, 0, 1);
    sendCommand();

  for(uint8_t cnt = 0; cnt < 15; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB), 0, 0, 0, 0, random(100, 255), random(100, 255), random(100, 255), 0, cnt, cnt, 0, "This is test 0", 0, 0, 0, 0, 0, 1);
    sendCommand();
  }

  for(uint8_t cnt = 0; cnt < 15; cnt++)
  {
    clearDisplayStr();
    displayStrCnt = sprintf(displayStr+cnt, "%s", "This is test 1");
    displayStrCnt += cnt;
    displayStr[displayStrCnt] = 0x20;
    buildCommand((setColorRGB | setPrintStr | setBackRGB), 0,
                 random(200, 255), random(200, 255), random(200, 255),
                 random(0, 128), random(0, 128), random(0, 128),
                 0, cnt, 0, 0, (char *) displayStr, 0, 0, 0, 0, 0, 1);
    sendCommand();
  }

  scnClr();
  
  for(uint8_t cnt = 0; cnt < 15 ; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB), 0,
                 0, 0, 0, random(200, 255), random(200, 255), random(200, 255), 
                 0, cnt, 19-cnt, 0, "This is test 2", 0, 0, 0, 0, 0, 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 15; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr |setBackRGB), 0,
                 random(0, 128), random(0, 128), random(0, 128),
                 random(200, 255), random(200, 255), random(200, 255),
                 0, cnt, cnt, 0, "This is test 3", 0, 0, 0, 0, 0, 1);
    sendCommand();
  }

  scnClr();

  for(uint8_t cnt = 0; cnt < 15 ; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr |setBackRGB), 0,
                 random(200, 255), random(200, 255), random(200, 255),
                 random(100, 128), random(100, 128), random(100, 128),
                 0, cnt, 19-cnt, 0, "This is test 4", 0, 0, 0, 0, 0, 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | clrScn), 0,
                 0, 0, 0, random(0, 255), random(0, 255), random(0, 255),
                 0, random(15), random(18), 0, "This is test 5", 0, 0, 0, 0, 0, 1);
    sendCommand();
  }
  
  scnClr();

  buildCommand((setColorRGB | setPrintStr | setBackRGB | setfont), 1,
               0, 0, 0, 255, 0, 0,
               0, 0, 0, 0, "UbuntuBold", 0, 0, 0, 0, 0, 1);
  sendCommand();
  buildCommand((setColorRGB | setPrintStr | setBackRGB | setfont), 2,
               0, 0, 0, 0, 255, 0,
               0, 4, 0, 0, "BigFont", 0, 0, 0, 0, 0, 1);
  sendCommand();
  buildCommand((setColorRGB | setPrintStr | setBackRGB | setfont), 3,
               0, 0, 0, 0, 0, 255,
               0, 8, 0, 0, "SmallFont", 0, 0, 0, 0, 0, 1);
  sendCommand();
  buildCommand((setColorRGB | setPrintStr | setBackRGB | setfont), 4,
               0, 0, 0, 255, 255, 255,
               0, 4, 0, 0, "0123456789", 0, 0, 0, 0, 0, 1);
  sendCommand();
  
  scnClr();

  buildCommand((setColorRGB | setPrintStr | setBackRGB | setfont), 1,
               0, 0, 0, 0, 0, 0,
               0, 0, 0, 0, "", 0, 0, 0, 0, 0, 1);
  sendCommand();
  
  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | setDrawLine), 0,
                 0, 0, 0, random(100, 255), random(100, 255), random(100, 255),
                 0, 0, 0, 0, "Test 6 - Lines", random(10, 790), random(35, 470), random(10, 790), random(3, 470), 0, 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | setDrawRect), 0,
                 0, 0, 0, random(100, 255), random(100, 255), random(100, 255),
                 0, 0, 0, 0, "Test 7 - Rectangles", random(10, 790), random(35, 470), random(10, 790), random(35, 470), 0, 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | setFillRect), 0,
                 0, 0, 0, random(100, 255), random(100, 255), random(100, 255),
                 0, 0, 0, 0, "Test 8 - Filled Rectangles", random(10, 790), random(35, 470), random(10, 790), random(35, 470), 0, 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | setDrawRRect), 0,
                 0, 0, 0, random(100, 255), random(100, 255), random(100, 255),
                 0, 0, 0, 0, "Test 9 - Rounded Rectangles", random(10, 790), random(35, 470), random(10, 790), random(35, 470), 0, 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | setDrawFRRect), 0,
                 0, 0, 0, random(100, 255), random(100, 255), random(100, 255),
                 0, 0, 0, 0, "Test 10-Filled Rounded Rectangles", random(10, 790), random(35, 470), random(10, 790), random(35, 470), 0, 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | setDrawCircle), 0,
                 0, 0, 0, random(100, 255), random(100, 255), random(100, 255),
                 0, 0, 0, 0, "Test 11-Circles", random(10, 790), random(35, 470), 0, 0, random(5, 50), 1);
    sendCommand();
  }
  
  scnClr();

  for(uint8_t cnt = 0; cnt < 50; cnt++)
  {
    buildCommand((setColorRGB | setPrintStr | setBackRGB | setDrawFillCircle), 0,
                 0, 0, 0, random(100, 255), random(100, 255), random(100, 255),
                 0, 0, 0, 0, "Test 12-Filled Circles", random(10, 790), random(35, 470), 0, 0, random(5, 50), 1);
    sendCommand();
  }
  
  delay(5000);
}

void buildCommand(uint32_t  flags , uint8_t font, uint8_t bGR, uint8_t bGG, uint8_t bGB,
                  uint8_t cR, uint8_t cG, uint8_t cB, uint8_t dispP, uint8_t lineP, uint16_t chrP, uint16_t degR, 
                  char *dSTR, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t rad, uint8_t bRDY)
{
  glcdUNION.glcdBUF.flags = flags;
  glcdUNION.glcdBUF.font = font;
  glcdUNION.glcdBUF.bGR =  bGR;
  glcdUNION.glcdBUF.bGG = bGG;
  glcdUNION.glcdBUF.bGB = bGB;
  glcdUNION.glcdBUF.cR = cR;
  glcdUNION.glcdBUF.cG = cG;
  glcdUNION.glcdBUF.cB = cB;
  glcdUNION.glcdBUF.dispP = dispP;
  glcdUNION.glcdBUF.lineP = lineP;
  glcdUNION.glcdBUF.chrP = chrP;
  glcdUNION.glcdBUF.degR = degR;
  sprintf(glcdUNION.glcdBUF.dSTR, "%s", dSTR);
  glcdUNION.glcdBUF.x1 = x1;
  glcdUNION.glcdBUF.y1 = y1;
  glcdUNION.glcdBUF.x2 = x2;
  glcdUNION.glcdBUF.y2 = y2;
  glcdUNION.glcdBUF.rad = rad;
  glcdUNION.glcdBUF.bRDY = 1;
}  
void sendCommand(void)
{
  digitalWrite(13,HIGH);              // LED on
#if USE1WIRE == 1
  uint8_t len, x, i;

  x=0;
  Serial.print(F("ROM "));
  Serial.print(x);
  Serial.print(F(" = "));
  for( i = 0; i < 8; i++)
  {
    Serial.print(F("0x"));
    if(oneWireAddr[i] < 16) Serial.print(F("0"));
    Serial.print(oneWireAddr[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
  ds.reset();
  ds.select(oneWireAddr);
  ds.write(0x0F);                     // Write to GLCD
  ds.write_bytes(glcdUNION.glcdARRAY, sizeof(glcdCMD));
  Serial.print(F("DATA = "));
  for( i = 0; i < sizeof(glcdCMD); i++)
  {
    Serial.print(F("0x"));
    if(glcdUNION.glcdARRAY[i] < 16) Serial.print(F("0"));
    Serial.print(glcdUNION.glcdARRAY[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
#else
  uint8_t len;

  //
  // i2cBuf Write - byte block to slave.
  //

  Wire.beginTransmission(i2cSlave);     // slave addr
  Wire.write(WRITE);                  // WRITE command
  Wire.write(0);                   // memory address
  for(len = 0; len < sizeof(glcdCMD); len++)       // write  block
  {
    Wire.write(glcdUNION.glcdARRAY[len]);
//    Serial.print((char) i2cBuff[len]);
  }
  Wire.endTransmission();     // blocking I2C Tx
#endif
  digitalWrite(13,LOW);               // LED off
  KickDog();
  delay(300);
  
}

void clearCommandBuf(void)
{
  for(uint8_t x = 0; x< sizeof(glcdCMD); x++)
  {
    glcdUNION.glcdARRAY[x] = 0;
  }
}

void scnClr(void)
{
  buildCommand(clrScn, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "", 0, 0, 0, 0, 0, 1);
  sendCommand();
}

void clearDisplayStr(void)
{
  uint8_t x;
  
  for(x = 0; x < displayStrSize; x++)
  {
    displayStr[x] = ' ';
  }
  displayStr[x] = 0x00;
}

#if USE1WIRE == 1
void find1Wire(void)
{
  uint8_t x, i;
  if ( !ds.search(oneWireAddr))
  {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    x = 0;
    return;
  }
  
  
  Serial.print(F("ROM "));
  Serial.print(x);
  Serial.print(F(" = "));
  for( i = 0; i < 8; i++) {
    Serial.print(F("0x"));
    if(oneWireAddr[i] < 16) Serial.print(F("0"));
    Serial.print(oneWireAddr[i], HEX);
    Serial.print(F(" "));
  }
  
  x++;
  
  if (OneWire::crc8(oneWireAddr, 7) != oneWireAddr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (oneWireAddr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      break;
      
    case 0x28:
      Serial.println("  Chip = DS18B20");
      break;
      
    case 0x22:
      Serial.println("  Chip = DS1822");
      break;
      
    case 0x3B:
      Serial.println("  Chip = MAX31850");
      break;

    case 0x44:
      Serial.println("Teensy3.x One Wire Slave GLCD");
      break;
      
    case 0xAA:
      Serial.println("Teensy3.x One Wire Slave");
      break;
      
    default:
      Serial.print("Unknown family device - 0x");
      Serial.println(oneWireAddr[0], HEX);
      return;
  } 
}
#else
//
// print I2C status
//
void print_i2c_status(void)
{
  I2C_DEBUG_WAIT; // wait for Serial to clear debug msgs (only needed if using debug)
  switch(Wire.status())
  {
    case I2C_WAITING:  Serial.print("I2C waiting, no errors\n"); break;
    case I2C_ADDR_NAK: Serial.print("Slave addr not acknowledged\n"); break;
    case I2C_DATA_NAK: Serial.print("Slave data not acknowledged\n"); break;
    case I2C_ARB_LOST: Serial.print("Bus Error: Arbitration Lost\n"); break;
    default:           Serial.print("I2C busy\n"); break;
  }
}
#endif
void KickDog(void)
{
  Serial.println("Kicking the dog!");
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  noInterrupts();
  WDOG_REFRESH = 0xA602;
  WDOG_REFRESH = 0xB480;
  interrupts();
}


#ifdef __cplusplus
extern "C" {
#endif
  void startup_early_hook() {
    WDOG_TOVALL = (10000); // The next 2 lines sets the time-out value. This is the value that the watchdog timer compare itself to.
    WDOG_TOVALH = 0;
    WDOG_STCTRLH = (WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN | WDOG_STCTRLH_WAITEN | WDOG_STCTRLH_STOPEN); // Enable WDG
    //WDOG_PRESC = 0; // prescaler 
  }
#ifdef __cplusplus
}
#endif
