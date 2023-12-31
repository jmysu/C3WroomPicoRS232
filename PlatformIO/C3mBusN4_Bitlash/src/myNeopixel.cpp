/**
 * @file myNeopixel.cpp
 * @author jimmy.su
 * @brief An adjustable LED Blinker w/ neopixel (IDF library)
 *        blink 100ms and wait for next interval  
 *                 
 *  Global variables:      
 *      int _msBlinkInteval: milliseconds for blinking interval
 *      uint8_t _neoR, _neoG, _neoB: 3 elements color for neopixel, use neocolorRGB(r,g,b) to update
 * 
 *
 * @version 0.1
 * @date 2022-12-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <Arduino.h>
#include "_BitlashScript.h" 

//Global variable
int8_t _cNeopixelColors=0; //-1 to stop rotate
int _msBlinkInterval=1000;   
uint8_t _neoR, _neoG, _neoB;
//Neopixel gamma table
const uint8_t _neoBrt16[16]={ 0, 1, 2,  5, 10, 16, 26, 36,
                             50,68,89,114,142,175,213,255};

Ticker tickerMyNeo;
Ticker tickerMyNeoOff;
void neopixelRGB(uint8_t r, uint8_t g, uint8_t b);


/**
 * @brief fix 400ns to 300ns for WS2813 from esp32-hal-rgb-led.c
 * 
 * @param pin 
 * @param red_val 
 * @param green_val 
 * @param blue_val 
 */
void ws2813Write(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val){
  rmt_data_t led_data[24];
  static rmt_obj_t* rmt_send = NULL;
  static bool initialized = false;
  uint8_t _pin = pin;

  if(!initialized){
    if((rmt_send = rmtInit(_pin, RMT_TX_MODE, RMT_MEM_64)) == NULL){
        log_e("WS2813 RGB LED driver initialization failed!");
        rmt_send = NULL;
        return;
    }
    rmtSetTick(rmt_send, 100);
    initialized = true;
  }

  int color[] = {green_val, red_val, blue_val};  // Color coding is in order GREEN, RED, BLUE
  int i = 0;
  for(int col=0; col<3; col++ ){
    for(int bit=0; bit<8; bit++){
      if((color[col] & (1<<(7-bit)))){
        // HIGH bit
        led_data[i].level0 = 1; // T1H
        led_data[i].duration0 = 8; // 0.8us
        led_data[i].level1 = 0; // T1L
        led_data[i].duration1 = 3; // 0.3us
      }else{
        // LOW bit
        led_data[i].level0 = 1; // T0H
        led_data[i].duration0 = 3; // 0.3us
        led_data[i].level1 = 0; // T0L
        led_data[i].duration1 = 9; // 0.8us
      }
      i++;
    }
  }
  rmtWrite(rmt_send, led_data, 24);
  delayMicroseconds(50);
}

/**
 * @brief neopixelOff()
 *          ticker callback to turnoff neopixel
 * 
 */
void neopixelOff()
{   if (_pinNeopixel)
      ws2813Write(_pinNeopixel, 0,0,0); // Black    
}
/**
 * @brief neopixelBlink
 *          ticker callback for blinking neopixel
 * 
 *          100ms ON, (_msBlinkInterval-100ms) Off
 * 
 */
void neopixelBlink()
{

    tickerMyNeo.detach();
    
    if (_cNeopixelColors<0)
        ws2813Write(_pinNeopixel, _neoR, _neoG, _neoB);
    else 
        neopixelRotateColors(_cNeopixelColors);

    if (_msBlinkInterval) {//armed another ticker
        tickerMyNeo.attach_ms(_msBlinkInterval, neopixelBlink);
        int timeOff = _msBlinkInterval>>3;
        //log_d("%d", timeOff);
        tickerMyNeoOff.once_ms(timeOff, neopixelOff); //turnOff next Interval/8
        }
    //log_d("%02X%02X%02X", _rgbNeo.R, _rgbNeo.G, _rgbNeo.B);
       
    //Make _cNeopixelColor 0~5
    //log_w("Blink %d, %d %d %d", _cNeopixelColors, _neoR, _neoG, _neoB);
    ++_cNeopixelColors;
    if (_cNeopixelColors>5)  {
      _cNeopixelColors=0;
      //Serial.println();
      }

}

/**
 * @brief neopixlRGB(r,g,b)
 * 
 * @param r 
 * @param g 
 * @param b 
 */
void neopixelRGB(uint8_t r, uint8_t g, uint8_t b)
{
    if (!_pinNeopixel) return;

    _neoR=r; _neoG=g; _neoB=b;
    ws2813Write(_pinNeopixel, r, g, b);
}

void neopixelSetupBlink(int msBlink)
{   
    if (!_pinNeopixel) return;

    _msBlinkInterval=msBlink; //update BlinkInterval
    tickerMyNeo.attach_ms(_msBlinkInterval, neopixelBlink);
    log_i("Start blink pin%d @%dms RGB:%d/%d/%d", _pinNeopixel, _msBlinkInterval, _neoR,_neoG,_neoB);
}


/*
 *  1 2 3 4 5 6 
 *  R G B C M Y
 */
const uint8_t aColor[][3]={{10,0,0},{0,8,0},{0,0,20},{0,8,20},{10,0,20},{10,8,0}};
void neopixelRotateColors(uint8_t c)
{ /*
      switch(c) {
        case 0: 
            //blinkLED1_250ms(); //do a 200ms blink
            neopixelRGB(16,0,0);
            break;
        case 1:
            neopixelRGB(0,10,0);
            break;
        case 2:
            neopixelRGB(0,0,32);
            break;
        } 
   */
  neopixelRGB(aColor[c][0], aColor[c][1], aColor[c][2]);     
  //log_w("%d:%d %d %d",c,  aColor[c][0], aColor[c][1], aColor[c][2]); 
}