
// esp8266 side of the Rest server.
// receive sensor data from the ATmega328P.
// recieve user commands from his serial port directed to the uno. 

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// network credentials
#ifndef STASSID
#define STASSID "******"
#define STAPSK  ***********"
#endif

#define relay 4

const char *ssid = STASSID;
const char *password = STAPSK;

// For static IP address
IPAddress local_IP(10, 0, 0, 20);
IPAddress gateway(10, 0, 0, 138);
IPAddress subnet(255, 255, 0, 0);

// initializing the server instance and bind port 80
AsyncWebServer server(80);

// max characters in sensors buffers
const byte numChars = 8;
//boolean relay_flag = false;

char receivedLux[numChars];
char receivedTemp[numChars];

// new vars
const char* PARAM_INPUT_1 = "state"; 
const char* message = "Oops! wrong direction!";

int relayState = HIGH; // start state of relay output pin

///// HTML and JS ///////
// main page - sensors //
const char root_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML><html>
  <head>
    <title>Garden Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
     <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css"
          integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr"
          crossorigin="anonymous">
  <style>
    html {font-family: Ariel;
          display: inline-block;
          margin: 0px auto;
          text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    .units {font-size: 1.2rem;}
    .measure-labels{
      font-size: 1.5rem;
      vertical-align: middle;
      padding-bottom: 15px;}
    button {
      font-size: 16px;
      font-color: #F5F5F5;
      border-radius: 12px;
      border: 2px solid;
      color: white;
      padding: 12px 28px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      margin: 4px 2px;
      transition-duration: 0.4s;
      cursor: pointer;
      background-color: #67BAC2;
      }
   </style>
  </head>

  <body>
    <h2>Garden Control</h2>
    <p>
      <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
      <span class="measure-labels">Temperature</span>
      <span id="temperature">%TEMPERATURE%</span>
      <sup class="units">&deg;C</sup>
    </p>
    <p>
      <i class="far fa-sun" style="color:#00add6;"></i> 
      <span class="measure-labels">Lux</span>
      <span id="lux">%LUX%</span>
      <sup class="units">lx</sup>
    </p>
    <button>
      <a class="btn" href="/light">
        <i class="far fa-lightbulb" style="color:#FFFF49;"></i> Light</a>
    </button>
  </body>
  <script>
    setInterval(function () {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("temperature").innerHTML = this.responseText;
        }
      };
      xhttp.open("GET", "/temperature", true);
      xhttp.send();
      }, 1000) ;

    setInterval(function () {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        document.getElementById("lux").innerHTML = this.responseText;
      }
    };
    xhttp.open("GET", "/lux", true);
    xhttp.send();
    }, 500) ;
</script>
</html>)rawliteral";

// light control page
const char light_html[] PROGMEM = R"rawliteral( 
  <!DOCTYPE HTML><html> 
  <head> 
    <title>Light Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.0rem;}
    h3 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 34px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #2196F3}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    button {
      font-size: 16px;
      font-color: #F5F5F5;
      border-radius: 12px;
      border: 2px solid;
      color: white;
      padding: 12px 28px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      margin: 4px 2px;
      transition-duration: 0.4s;
      cursor: pointer;
      background-color: #67BAC2;
      }
  </style>
  </head>
  <body>
    <h2>Garden:</h2>
    <h3>Light Control</h3>
    <p>
    <i class="far fa-lightbulb"></i>
    </p>
    %BUTTONPLACEHOLDER% 
    <p></p>
    <button>
      <a class="btn" href="/">
        <i class="fas fa-long-arrow-alt-left" style="color:#FFFF49;"></i> back</a>
    </button>
  <script>function toggleCheckbox(element) {
      var xhr = new XMLHttpRequest();
      if(element.checked){xhr.open("GET", "/update1?state=0", true);}
      else {xhr.open("GET", "/update1?state=1", true);}
      xhr.send();
      } 
    setInterval (function() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) { 
          var inputChecked;
          var outputStateM;
          if(this.responseText == 1) {
            inputChecked = true;
            outputStateM = "On";
            }
          else {
            inputChecked = false;
            outputStateM = "Off";  
            };
          document.getElementById("output").checked = inputChecked;
          document.getElementById("outputState").innerHTML = outputStateM;
        }
     };
     xhttp.open("GET", "/state", true);
     xhttp.send();
    }, 100);
    </script>
   </body>   
  </html>
 )rawliteral";

//////////////////////// Uno data receive, write and validate ///////////
 
void recvWithStartEndMarker() {
  // Receiving  and parsing serial information input from Arduino 
    String serialInput;
	char firstChar;
   
    while (Serial.available() > 0) {
      // checking the first char received through serial from uno
        serialInput = Serial.readString();
		firstChar = serialInput[0];
        // if the serialInput string is formated as "<string>"
        //  input represents data from light sensor
		// if the serialInput string is formated as "[string]"
		//  input represents data from temperature sensor
		switch(firstChar) {
			// in case current serial input starts with '<'
			case '<':
				int i = 1;
				// in case current serial input ends with '>'
				while (serialInput[i] != '>'){
					receivedLux[i] = serialInput[i];
					i++;}
				receivedLux[i] = '\0';
				break;
			// in case current serial input starts with '['
			case '[':
				int i = 1;
				// in case current serial input ends with ']'
				while (serialInput[i] != ']'){
					receivedTemp[i] = serialInput[i];
					i++
				}
				receivedTemp[i] = '\0';
				break; 
			}
				
		}
}

//////////////////////// Relays Control//////////////////////////
// check for relays status and return the appropriate string for response object
// responsible for handling the slider in light control page 
String outputState(){
  // receive state value
  //  buttons += "...... <input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"> ... "
  if(digitalRead(relay)){ 
    return "checked";
  }
  else { 
    return "";
  }
  return "";
}

// button placeholder replace
String processor(const String& var) {
  Serial.println(var);
  // look for the button placeholder in html
  if(var == "BUTTONPLACEHOLDER") {
    // for the button updated html tag
    Serial.println("replacing BUTTONPLACEHOLDER");
    String buttons ="";
    String outputStateValue = outputState();
    Serial.println("in processor: " + outputStateValue);
    buttons += "<h4><span id=\"ouputState\"></span></h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

//////////////////// requests data hanlers//////////

// sensors placeholders replace
String processor2(const String& var) {
  Serial.println(var);
  // look for the button placeholder in html
  if(var == "TEMPERATURE") {
    // for the button updated html tag
    Serial.print("check for temp: ");
    Serial.println(receivedTemp);
    return receivedTemp;
    }
  else if(var == "LUX") {
    Serial.print("check for lux: ");
    Serial.println(receivedLux);
    return receivedLux;
  }
  return String();
}


//////////////Setup////////////////
void setup(void) {
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);
  
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
   // set wifi configuration
  if (!WiFi.config(local_IP, gateway, subnet)) {
     Serial.println("STA Failed to configure");
  }
  // Wait for connection
  Serial.println("Connecting to server!");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("HTTP server started");
  //////// requests /////
// root home page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", root_html, processor2);
  });
  // requests for acrimonious sensors placeholders update
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", receivedTemp);
  });
  server.on("/lux", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", receivedLux);
  });
  
  // light control page
  server.on("/light", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send_P(200, "text/html", light_html, processor);
  });
	// update light relay status
  // send a GET request to <ESP_IP>/update1?state<inputMessage>
  server.on("/update1", HTTP_GET, [] (AsyncWebServerRequest *request){
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/update1?state<inpuptMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      digitalWrite(relay, inputMessage.toInt());
      relayState = !relayState;
    }
    else {
      inputMessage = "No Message Sent";
      inputParam = "none";
  }
  
  request->send(200, "text/html", "ok");
 }); 

 server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Oops! You went the wrong direction!");
});

/// OTA ////
AsyncElegantOTA.begin(&server);

server.begin();
}

void loop(void) {
  recvWithStartEndMarker();
  recvWithStartEndMarker();
  AsyncElegantOTA.loop();
}
