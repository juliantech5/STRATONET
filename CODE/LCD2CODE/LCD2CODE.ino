#include <RotaryEncoder.h>

#define SX1278ATTACHED

#ifdef SX1278ATTACHED
#include <RadioLib.h>
#endif

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h> // Hardware-specific library
#include <Fonts/FreeSans9pt7b.h>

#define _sclk 13
#define _miso 12
#define _mosi 11
#define _cs 10
#define _dc 9
#define _rst 8
Adafruit_ILI9341 tft = Adafruit_ILI9341(_cs, _dc, _rst);

#ifdef SX1278ATTACHED
SX1278 lora = new Module(7, 2, 3);
#endif

RotaryEncoder encoder(A1, A2);

#define BLACK   ILI9341_BLACK
#define BLUE    0x001F
#define RED     ILI9341_RED
#define GREEN   ILI9341_GREEN
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   ILI9341_WHITE

uint16_t FONT_HEIGHT = 18;
uint16_t SCREEN_TOOLBAR_HEIGHT = 32;

float CARRIER_FREQUENCY = 434.0f;

class console
{
public:
  
  void RenderToolbar()
  {
    int ox, oy;
    ox = 0;
    oy = tft.height() - SCREEN_TOOLBAR_HEIGHT;
    
    tft.fillRect(ox, oy, tft.width(), SCREEN_TOOLBAR_HEIGHT, BLACK);
    
    tft.setTextColor(WHITE);

    tft.setCursor(10, tft.height() - 8);
    int16_t  x1, y1;
    uint16_t w, h;
    tft.getTextBounds(F("STRATONET"), 10, tft.height() - 8, &x1, &y1, &w, &h);
    tft.print("STRATONET");
    

    String freqText = String(F("OPERATING @ ")) + String(CARRIER_FREQUENCY);
    tft.getTextBounds(freqText, tft.width() / 4, tft.height() - 8, &x1, &y1, &w, &h);
    tft.setCursor(tft.width() - w - 10, tft.height() - 8);
    if (!IsRunning)
    {
      tft.setTextColor(RED);
    }
    else
    {
      tft.setTextColor(WHITE);
    }
    tft.print(freqText);

    tft.setTextColor(WHITE);
  }

  void Render()
  {
    // starting at the most recent index.
    int mostRecentIndex = _MessageListCurrentIndex;
    
    // loop through the lower _MessageListCount.
    for (int i = 1; i < _MessageListCount; i++)
    {
       int previousMessageIndex = mostRecentIndex - i;

       if (previousMessageIndex < 0)
       {
          previousMessageIndex = _MessageListCount - i;
       }

       String thisMessage = _Messages[previousMessageIndex];

        int y = tft.height() - SCREEN_TOOLBAR_HEIGHT;
        y -= 4;
  
    
        int numberOfPixelsInMessage = thisMessage.length() * 12;
        int numberOfYLines = 1 + floor(numberOfPixelsInMessage / tft.width());
        y -= ( FONT_HEIGHT * numberOfYLines ) * i;
   
        int x = 2;
  
        // render the most recent on at the bottom
        tft.setCursor(x, y);
        tft.setTextColor(BLACK);
        tft.setTextSize(1);
        tft.print(String("> ") + thisMessage);
    }

    RenderToolbar();
  }
  
  void AddMessage(String pMessage)
  {
      for (byte i = 0; i < 22; i++)
      {
        _Messages[_MessageListCurrentIndex][i] = pMessage.charAt(i);
      }
      _Messages[_MessageListCurrentIndex][23] = '\0';
      
      _MessageListCurrentIndex++;
      
      if (_MessageListCurrentIndex >= _MessageListCount) // wrap around.
      {
        _MessageListCurrentIndex = 0;
      }
  }

  void Pause()
  {
      IsRunning = false;
  }
  
  void Play()
  {
     IsRunning = true;
  }

  void Error()
  {
    IsRunning = false;
    IsError = true;
  }

  void AssignMemory()
  {
    for (byte listIndex = 0; listIndex < 4; listIndex++)
    {
      for (byte charIndex = 0; charIndex < 23; charIndex++)
      {
        _Messages[listIndex][charIndex] = '1';
      }
    }
  }
  
  bool IsRunning = true;
  bool IsError = false;
private:
  char _Messages[5][40];
  
  byte _MessageListCurrentIndex = 3; // where we start rendering from at the bottom.
  
  byte _MessageListCount = 2; // how many messages we render upwards.
};

console c;

void setup()
{
    Serial.begin(115200);
    tft.begin();

    tft.setFont(&FreeSans9pt7b);
    
    tft.setRotation(3);
    
    tft.fillScreen(BLACK);
  
    tft.setCursor(0, 0);
    Serial.println("Running");

    c.AssignMemory();

    
#ifdef SX1278ATTACHED
     int state = lora.begin(434.0, 125, 11, 8, 0x13, 10);
    state = lora.setFrequency(CARRIER_FREQUENCY);
    if (state == ERR_NONE)
    {
      Serial.println(F("success!"));
    }
    else
    {
      Serial.print(F("failed, code "));
      Serial.println(state);
      //while (true);
    }

    lora.setDio0Action(setFlag);

    lora.startReceive();
 #endif

  RedrawConsole();
}



volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;

void setFlag(void)
{
  if (!enableInterrupt)
  {
    return;
  }
  enableInterrupt = false;
  
  receivedFlag = true;
}




void RedrawConsole()
{
  tft.fillScreen(WHITE);
  c.Render();
}

unsigned long timestampLastTick = millis();
boolean carrierFrequencyChanged = false;

int pos = 0;
void loop()
{
  encoder.tick();
  int newPos = encoder.getPosition();
  if (pos != newPos)
  {
    Serial.println("Moved");
    carrierFrequencyChanged = true;
    
    CARRIER_FREQUENCY = 434.0f + (newPos * 0.1f);

    timestampLastTick = millis();
   
    pos = newPos;
  }

  if (carrierFrequencyChanged)
  {
      unsigned long timeSinceEncoderTick = millis() - timestampLastTick;
      if (timeSinceEncoderTick >= 250)
      {
        //int state = lora.setFrequency(CARRIER_FREQUENCY);
        Serial.print(F("Updated radio settings, status code "));
        //Serial.println(state);
      
        RedrawConsole();

        carrierFrequencyChanged = false;
      }   
  }




  if (receivedFlag)
  {
    Serial.println("Recieved packet...");
    
    // process data.
    receivedFlag = false;
    
    if (c.IsRunning)
    {
      #ifdef SX1278ATTACHED
        String str;
        int state = lora.readData(str);
    
        if (state == ERR_NONE)
        {
        
          // packet was successfully received
          Serial.println(F("success!"));
      
          // print the data of the packet
          Serial.print("[SX1278] Data:\t\t");
          Serial.println(str);
      
          // print the RSSI (Received Signal Strength Indicator)
          // of the last received packet
          Serial.print("[SX1278] RSSI:\t\t");
          Serial.print(lora.getRSSI());
          Serial.println(" dBm");
      
          // print the SNR (Signal-to-Noise Ratio)
          // of the last received packet
          Serial.print("[SX1278] SNR:\t\t");
          Serial.print(lora.getSNR());
          Serial.println(" dBm");
      
          // print frequency error
          // of the last received packet
          Serial.print("Frequency error:\t");
          Serial.print(lora.getFrequencyError());
          Serial.println(" Hz");
  #endif
          //c.AddMessage("Test");
          

       c.AddMessage(str);
       RedrawConsole();
  
  #ifdef SX1278ATTACHED
        }
        else if (state == ERR_RX_TIMEOUT)
        {
          Serial.println(F("timeout!"));
        }
        else if (state == ERR_CRC_MISMATCH)
        {
          // packet was received, but is malformed
          Serial.println(F("CRC error!"));
      } 
  #endif
  
    }
    else
    {
      // do nothing
      Serial.println("Console not running.");
      delay(1000);
    }

    enableInterrupt = true;

    Serial.println("Done");
  }
}
