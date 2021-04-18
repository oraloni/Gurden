/*


*/

#include <Wire.h>
#include <BH1750.h>
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define numChars 8
#define MAXLUX 8

char light_mode = '0';
char temp_mode = '1';

#define MAXLUX 8

BH1750 lightMeter;

// Data wire is plugged into digital pin 2 on the Arduino
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);


boolean newRead = true;

void setup(){
  Serial.begin(115200);

  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();
  // On esp8266 you can select SCL and SDA pins using Wire.begin(D4, D3);

  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);

}

void loop() {
  
  char light_mode = '0';
  char temp_mode = '1';
  
  int lux = lightMeter.readLightLevel();
  delay(500);
  sendDataToEsp(lux, light_mode);
  sensors.requestTemperatures();
  
  int temp = sensors.getTempCByIndex(0);
  delay(500);
  sendDataToEsp(temp, temp_mode);
  delay(500);
}

void sendDataToEsp(int measure, char mod)
{
  if (mod == '0'){
    String light = "<" + String(measure) + ">";
    char buff[sizeof(light) + 1];
    int i;
    for (i=0; i<sizeof(light); i++){
      buff[i] = light[i];
    }
    buff[i+1] = '\0';
  //  Serial.println(buff);
    Serial.write(buff);
  }
  else if (mod == '1'){
    String temp = "[" + String(measure) + "]";
    char buff[sizeof(temp) + 1];
    int i;
    for (i=0; i<sizeof(temp); i++){
      buff[i] = temp[i];
    }
    buff[i+1] = '\0';
  //  Serial.println(buff);
    Serial.write(buff);
  }
}
