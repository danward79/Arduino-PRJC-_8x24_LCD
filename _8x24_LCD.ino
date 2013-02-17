/* 3x3 Large Number display for 24x8 LCD with Serial backpack 
   Details of display available here:
   http://www.pjrc.com/store/mp3_display.html
   http://www.pjrc.com/tech/mp3/lcd_protocol.html
   http://www.pjrc.com/tech/mp3/lcd_fonts.html
   http://www.pjrc.com/store/tman_lcd.html        
   http://arduino.cc/forum/index.php/topic,22490.15.html
   http://www.kinzers.com/don/LCD_Panel/
   http://home.arcor.de/dreckes
   */

/* A big thank you for the example found at the address below, 
   it helped me write this program

Large number example 
May 24, 2009
Fof HIFIDUINO
www.hifiduino.blogspot.com                                          */

/* Custom characters created using the following website:
   http://www.btinternet.com/~e2one/lcd/graphics_character_definer.htm  */


//---------------------------------------------------
// Setup
//--------------------------------------------------

#define MYNODE 3            	// Node ID
#define FREQ RF12_433MHZ     	// Frequency
#define GROUP 212            	// Network group 
#define CTTX 2					// Current Monitor TX node ID
#define BASE 1					// Base node ID
#define RXPIN 5					// LCD Serial Pin RX
#define TXPIN 6					// LCD Serial Pin TX
#define CLOCKUPDATE 5000		// Clock Update Period (ms)
#define POWERUPDATE 200			// Power Update Period (ms)
#define POWERUPDATETIMEOUT 25000// Power Update Timeout Period (ms)
#define LCDREFRESH 60000		// General LCD Refrsh Period (ms)
#define MAINSV 240				// Mains Voltage for Calculation of Power... Unit I get a value measured by the base station.

//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------

typedef struct { double current; int battery; } PayloadTX;
PayloadTX emontx; 

#include <JeeLib.h>	     		// https://github.com/jcw/jeelib
#include <SoftwareSerial.h>		// Soft Serial for LCD comms
#include <RTClib.h>
#include <Wire.h>

SoftwareSerial Serial1(RXPIN, TXPIN);	//Software Serial Instance
RTC_Millis RTC;							//RTC Instance

// The routine to create the custom characters in the LCD
void Font10()    //Used to save custom segments in LCD RAM, which make up the large number font
{
  Serial1.write(0x5C);//,BYTE);
  Serial1.write("D01*????????");    //Font 10, character 0
  Serial1.write(0x5C);//
  Serial1.write("D02*?????   ");    //Font 10, character 1
  Serial1.write(0x5C);//
  Serial1.write("D03*   ?????");    //Font 10, character 2
  Serial1.write(0x5C);//
  Serial1.write("D04*        ");    //Font 10, character 3
  Serial1.write(0x5C);//
  Serial1.write("D05*??????  ");    //Font 10, character 4
  Serial1.write(0x5C);//
  Serial1.write("D06* ?????  ");    //Font 10, character 5
  Serial1.write(0x5C);//
  Serial1.write("D07* ???????");    //Font 10, character 6
}


// Array index into parts of big numbers. Numbers consist of 9 custom characters in 3 lines
//            0      1      2      3      4      5      6      7      8      9    
char bn1[]={1,2,1, 3,1,4, 2,2,1, 2,2,1, 1,4,1, 1,2,2, 1,2,2, 2,2,1, 1,2,1, 1,2,1};
char bn2[]={1,4,1, 4,1,4, 7,6,5, 6,6,1, 5,6,1, 5,6,7, 1,6,7, 4,4,1, 1,6,1, 5,6,1};
char bn3[]={1,3,1, 3,1,3, 1,3,3, 3,3,1, 4,4,1, 3,3,1, 1,3,1, 4,4,1, 1,3,1, 3,3,1};

long lastTimeUpdate = -5000;		// Force update on first cycle
long lastPowerUpdate = 0;
int lastPower = 0;					// Last Power stats update
int runningAverage = 0;
double todaysKwh = -0.00000000012;				// Total kWh usage since midnight	
bool initScreen = false;
bool newData = false;
int lastMin = 61;
int lastHour = 25;
long last = 0;//debugging

void LCD_init()
/* Sets the LCD display to a known initialized state */
{
	Serial1.write(0x5C);//
	Serial1.write(0x40);//  //Clear Display and Set Font
	Serial1.write(0x20);//  //Font 0
	Serial1.write(0x30);//

	Serial1.write(0x5C);//
	Serial1.write(0x41);//  //Mode
	Serial1.write(0x31);//  //wrap on, scroll off

	Serial1.write(0x5C);//
	Serial1.write(0x5B);//  //Display incomming characters   
}

void setup()
{
	Serial.begin(57600); 
	Serial1.begin(9600);
	
	LCD_init();         // Init the display
	Font10();           // Create the custom characters 
	
	rf12_set_cs(10); 	//emonTx, emonGLCD, NanodeRF, JeeNode
	rf12_initialize(MYNODE, FREQ, GROUP);
	
	// following line sets the RTC to the date & time this sketch was compiled
    RTC.begin(DateTime(__DATE__, __TIME__));
	
	Serial.println("Setup");
}


void loop()
{
  
	if (rf12_recvDone())
	{      
		if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
		{
			int node_id = (rf12_hdr & 0x1F);
			if (node_id == CTTX)     		
			{
				emontx = *(PayloadTX*) rf12_data;                              // get emontx data 
				
				Serial.print("I: "); Serial.print(emontx.current);     // Add power reading
				Serial.print(", B: "); Serial.print(emontx.battery);    // Add emontx battery voltage reading
				Serial.print(", mS: "); Serial.println(millis()-last);
				last = millis();
				newData = true;
				
			}
			if (node_id ==BASE)
			{
				Serial.println("T: "); 
			}
        }
    } 
	
	//Faster Update Cycle for Power
	if (millis() - lastPowerUpdate > POWERUPDATE) 
	{
		char str[10];
		todaysKwh += (emontx.current * MAINSV) * 0.2 / 3600000;
		
		if (newData)
		{
			if (todaysKwh<10.0) dtostrf(todaysKwh,0,1,str); else itoa((int)todaysKwh,str,10);
			runningAverage = runningAverage + ((emontx.current * MAINSV) - runningAverage)*0.50;
			updatePower(runningAverage, str);
		}
		lastPowerUpdate = millis();
		newData = false;
	}
	
	//Slower Update Cycle for Clock, etc.
	if (millis() - lastTimeUpdate > CLOCKUPDATE)
	{
		DateTime now = RTC.now();
		int min = now.minute();
		int hour = now.hour();
		
		if ((min != lastMin) | (hour != lastHour))
		{
			updateTime(hour, min);
			lastMin = min;
			lastHour = hour;
		}
		
		if ((hour == 0) & (min == 0))
		{
			todaysKwh = 0;
		}
		
		lastTimeUpdate = millis();
	}
	
	//Very Slow General Screen Refresh
	if (!initScreen)
	{
		setFont(1);
		positionCursor(3, 1); 
		Serial1.write("Power Now");
		positionCursor(1, 6); 
		Serial1.write("Today:");
		positionCursor(5, 7); 
		Serial1.print("kWh");
		
		//Top Row Weather
		positionCursor(18, 1); 
		Serial1.write("Weather");
		positionCursor(17, 2); 
		Serial1.write("Out 12.7");
		positionCursor(17, 3); 
		Serial1.write("Hum 77%");
		positionCursor(17, 4); 
		Serial1.write("Pr 1002");
		Serial1.write(0x93);
		
		drawDividers();
		initScreen = true;
	}
}


void drawDividers()
{
	//Divider Line Horz
	positionCursor(1, 5); 
	for (int i = 1; i <= 7; i++)
	{
		Serial1.write(0x82);  
	}
	Serial1.write(0x8b); 
	
	for (int i = 1; i <= 7; i++)
	{
		Serial1.write(0x82);  
	}
	Serial1.write(0x8A); 	
	
	for (int i = 1; i < 9; i++)
	{
		Serial1.write(0x82);  
	}
	
	//Draw Line Vert Bottom
	positionCursor(8, 6);
	for (int i = 6; i <= 8; i++)
	{
		positionCursor(8, i);
		Serial1.write(0x81); 
	}

	//Draw Line Vert Top
	for (int i = 1; i <= 4; i++)
	{
		positionCursor(16, i);
		Serial1.write(0x81); 
	}

}

void updatePowerTimeOut()
{
	clearLastDigit(1, 2);
	clearLastDigit(5, 2);
	clearLastDigit(9, 2);
	clearLastDigit(13, 2);
}

void updatePower(int power, char* kwh)
{	
	if ((power < 1000) & (power < lastPower))
	{	
		clearLastDigit(13, 2);
	}
		
	setFont(1);
	positionCursor(1, 7); 
	Serial1.print(kwh);
	
	printNumber(power, 1, 2, 0);
	lastPower = power;
}

void clearLastDigit (int column, int row)
{
	setFont(1);
	for (int y = column; y < column + 3; y++)
	{
		for (int x = row; x < row + 3; x++)
		{
			positionCursor(y, x);
			Serial1.write(0x120);
		}
	}
}

void updateTime(int hour, int min)
{
	printNumber(hour, 10, 6, 1);	  
	printNumber(min, 18, 6, 1);
}

void toggleLed()
{
	//Serial1.write(0x5c);
	//Serial1.write(0x4a);
	//Serial1.write(0x04);
	//Serial1.write(0x01);
}

void clearLCD()
{
	Serial1.write(0x5C);//
	Serial1.write(0x40);//  //Clear Display and Set Font
}

void setFont(int fontNumber)
{
  Serial1.write(0x5C);//
  Serial1.write(0x43);//  //Set Font
  Serial1.write(0x20 + fontNumber);//  //Font 1
}

void positionCursor(int column, int row)
{
  Serial1.write(0x5C);//
  Serial1.write(0x42);//
  Serial1.write(byte (column + 31));//Column
  Serial1.write(byte (row + 31));//Row  
}

void printNumber(int number, int column, int row, bool zero)
{
  int digit0;  // To represent the ones
  int digit1;  // To represent the tens
  int digit2;  // To represent the hundreds
  int digit3;  // To represent the thousands
  int digit4;  // To represent the ten-thousands
  
  column -=1;
  row -= 1;
  
  setFont(10);
  
	if (number < 10)
	{
		if (zero)
		{
			printOneNumber(0, column+32, row+32);
			printOneNumber(number, column+36, row+32);
		}
		else
		{
			printOneNumber(number, column+32, row+32);
		}
	}
	if (number > 9 && number < 100)
	{
		digit0 = number % 10;
		printOneNumber(digit0, column+36, row+32);
		digit1 = number / 10;
		printOneNumber(digit1, column+32, row+32);
	} 
	if (number > 99 && number < 1000)
	{
		digit0 = number % 10;
		printOneNumber(digit0, column+40, row+32);
		digit1 = (number / 10) % 10;
		printOneNumber(digit1, column+36, row+32);
		digit2 = number / 100;
		printOneNumber(digit2, column+32, row+32);
	}
	if (number > 999 && number < 10000)
	{
		digit0 = number % 10;
		printOneNumber(digit0, column+44, row+32);
		digit1 = (number / 10) % 10;
		printOneNumber(digit1, column+40, row+32);
		digit2 = (number / 100) % 10;
		printOneNumber(digit2, column+36, row+32);
		digit3 = number / 1000;
		printOneNumber(digit3, column+32, row+32);
	}
	if (number > 9999 && number < 100000)
	{
		digit0 = number % 10;
		printOneNumber(digit0, column+48, row+32);
		digit1 = (number / 10) % 10;
		printOneNumber(digit1, column+44, row+32);
		digit2 = (number / 100) % 10;
		printOneNumber(digit2, column+40, row+32);
		digit3 = (number / 1000) % 10;
		printOneNumber(digit3, column+36, row+32);
		digit4 = number / 10000;
		printOneNumber(digit4, column+32, row+32);
  }
}

void printOneNumber(int digit, byte Column, byte Row)
{
  int i;
  setFont(10);
   
  // Line 1 of the one digit number
  Serial1.write(0x5C);//
  Serial1.write(0x42);//
  Serial1.write(Column);//  //Column
  Serial1.write(Row);//  //Row
  
  for(i=0;i<3;i++)
  {
    Serial1.write(0x5C);//
    Serial1.write(bn1[digit*3+i] + 32);//
  }

  // Line 2 of the one-digit number
  Serial1.write(0x5C);//
  Serial1.write(0x42);//
  Serial1.write(Column);//  //Column
  Serial1.write(Row+1);//  //Row
     for(i=0;i<3;i++)
  {
    Serial1.write(0x5C);//
    Serial1.write(bn2[digit*3+i] + 32);//
  }
  
  // Line 3 of the one-digit number
  Serial1.write(0x5C);//
  Serial1.write(0x42);//
  Serial1.write(Column);//  //Column
  Serial1.write(Row+2);//  //Row
  
  for(i=0;i<3;i++)
  {
    Serial1.write(0x5C);//
    Serial1.write(bn3[digit*3+i] + 32);//
  }
}
