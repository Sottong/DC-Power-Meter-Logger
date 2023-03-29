#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include "AiEsp32RotaryEncoder.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>


void RunSetup();
void run_once_onStart();
void start();
void ReadAccu();
void set_variable();
void ReadAccu();
void LcdPrint();
void UpdateLogger();

#define ROTARY_ENCODER_A_PIN 12
#define ROTARY_ENCODER_B_PIN 14
#define ROTARY_ENCODER_BUTTON_PIN 13
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4
#define RELAY_PIN 16

const char* ssid = "ElisaCom_Kos";
const char* password = "kosElisa12345";
const char* mqtt_server = "industrial.api.ubidots.com";
const char* port = "1883";
const char* token = "BBFF-rqPDXAflwgWtPnBoladP9ZovvTKHyd";


unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(100)

int value = 0;

//SSD1306
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_INA219 ina219;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);


bool setting_menu = true;
int menu = 0;

unsigned int startTime = 0;

unsigned long previousMillis = 0;
unsigned long last_time = 0;
unsigned long current_time = 0;

unsigned long previousMillisUpdate;
unsigned long last_timeUpdate;

int interval_logger = 3; //in minute

float voltage = 0.0;
int current = 0;
int power = 0;
float Wh = 0.0;

unsigned int Samples = 0;
float voltageSamples = 0.0;
float currentSamples = 0.0;


void IRAM_ATTR readEncoderISR()
{
	rotaryEncoder.readEncoder_ISR();
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),token, NULL)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  rotaryEncoder.begin();
	rotaryEncoder.setup(readEncoderISR);

  StaticJsonDocument<200> doc;

  bool circleValues = false;
	rotaryEncoder.setBoundaries(0, 1000, circleValues);
  rotaryEncoder.setAcceleration(250); 
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // display.display();
  // delay(2000);
  display.clearDisplay();
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 20);
  display.println(F("SMART DC LOAD LOGGER"));
  display.setCursor(18, 40);
  display.println(F("By : Mr. Sottong"));
  display.display();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  delay(1000);
  display.clearDisplay();
  delay(1000);
  display.setCursor(20, 30);
  display.println(F("INPUT PARAMETER"));
  display.display();

  

}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  if(setting_menu){
    if(menu < 2){
      RunSetup();
    }
    else{
      setting_menu = false;
      run_once_onStart();
    }
  }
  else{
    start();
  }
}

void RunSetup(){
  
  Serial.println(menu);
  if(rotaryEncoder.isEncoderButtonClicked()){
    Serial.println("tombol ditekan");
    delay(100);
    menu++;
  }

  if(menu == 1){
    display.clearDisplay();
    display.setCursor(10, 10);
    display.println(F("SET INTERVAL UPLINK"));  
    display.setCursor(10, 20); 
    display.println(F("0"));  
    display.setCursor(30, 20); 
    display.println("menit"); 
    display.display();
    set_variable();
  }
}

void run_once_onStart(){
  Serial1.println("DC Load mulai mengukur");
  setting_menu = false;

  if (! ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(21, 20);
  display.println(F("MULAI MENGUKUR"));
  display.display();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  delay(200);
  startTime = millis();
}

void set_variable() {

  // float lim_chrg = limit_discharge;
  // float lst_lim_charg;
  // int lst_log_interval;

  bool editVar = true;

  int currentState = 0;
  int lastState = 0;

  int interval = 0;

  while (editVar) {
    
    
    if (rotaryEncoder.encoderChanged()) {
      //interval 1 3 5 10 20 30 60
      if(interval == 7)interval = 0;
      Serial.println("rotary change");
      Serial.println(rotaryEncoder.readEncoder());
      
      currentState = rotaryEncoder.readEncoder();
      if(currentState > lastState){
        interval++;
        lastState = currentState;
      } 
      else if(currentState < lastState){
        if(interval > 1) interval--;  
        lastState = currentState;
      }
      
      
      
      switch (interval)
      {
        case 1:
          interval_logger = 1;
          break;

        case 2:
          interval_logger = 3;
          break;

        case 3:
          interval_logger = 5;
          break;
        
        case 4:
          interval_logger = 10;
          break;

        case 5:
          interval_logger = 20;
          break;

        case 6:
          interval_logger = 30;
          break;

        case 7:
          interval_logger = 60;
          break;
        
        // case 8:
        //   interval= 0;
        //   break;
      }

      display.clearDisplay();
      display.setCursor(10, 10);
      display.println(F("SET INTERVAL UPLINK"));  
      display.setCursor(10, 20); 
      display.println(interval_logger);  
      display.setCursor(30, 20); 
      display.println("menit"); 
      display.display();
    }
    delay(1);
    if (rotaryEncoder.isEncoderButtonClicked()){

      editVar = !editVar;
      display.clearDisplay();
      display.setCursor(10, 10);
      display.println(F("log_interval saved!"));  
      display.display();

    }
  }
  delay(2000);
  menu++;
}


void start(){
  unsigned long currentMillis = millis();
    if (currentMillis - previousMillisUpdate >= 1000) {
      // Serial.println((currentMillis - previousMillis) / 1000);
      previousMillisUpdate = currentMillis;
        ReadAccu();
        LcdPrint();
    }
    if ((currentMillis - previousMillis) / 60000 >= interval_logger) {
      previousMillis = currentMillis;
        UpdateLogger();
        // DisplayRuntime();
        // delay(3000);
        // lcd.clear();
    }
}

void meanReadAccu(float sampleVolt, float sampleCurrent){
  Samples++;
  voltageSamples += sampleVolt;
  currentSamples += sampleCurrent;
}

void getMeanReadAccu(){
  voltage = voltageSamples / Samples;
  current = int(currentSamples / Samples);
  voltageSamples = 0;
  currentSamples = 0;
  Samples = 0;
}

void ReadAccu() {
  last_time = current_time;
  current_time = millis();
  current = int(ina219.getCurrent_mA());  // in Ampere
  voltage = ina219.getBusVoltage_V();
  power = ina219.getPower_mW();
  meanReadAccu(voltage, current);
  Wh = Wh + (voltage * (float(current) / 1000)) * ((current_time - last_time) / 3600000.0);
}

void UpdateLogger() {
  getMeanReadAccu();

  //creat payload dan upload to ubidots using MQTT
  //create JSON
  /*
    {
      "tegangan" : 5.00,
      "arus" : 1.00,
      "power" : 1.00,
      "kapasitas" : 1.00
    }
  */
StaticJsonDocument<64> doc;

doc["tegangan"] = voltage;
doc["arus"] = current;
doc["power"] = power;
doc["kapasitas"] = Wh;

String output;


serializeJson(doc, output);

Serial.println(output);

char charArray[output.length() + 1];

output.toCharArray(charArray, output.length() + 1);


  // Serial.println(msg);
// client.publish("/v1.6/devices/dc-load-logger/tegangan", charArray);
client.publish("/v1.6/devices/dc-load-logger", charArray);

  // delay(10);
  // snprintf (msg, MSG_BUFFER_SIZE, "%d", current);
  // client.publish("/v1.6/devices/dc-load-logger/arus", msg);
  // Serial.println(msg);

  // delay(10);
  // snprintf (msg, MSG_BUFFER_SIZE, "%f", Wh);
  // client.publish("/v1.6/devices/dc-load-logger/power", msg);
  // Serial.println(msg);
  
  // delay(10);
  // float power = voltage * (float(current) / 1000);
  // snprintf (msg, MSG_BUFFER_SIZE, "%f", power);
  // client.publish("/v1.6/devices/dc-load-logger/kapasitas", msg);
  // Serial.println(msg);
  
  Serial.println("done send");

}

void LcdPrint() {

  display.clearDisplay();

  display.setCursor(0, 0);
  display.println(voltage);  
  display.setCursor(30, 0);
  display.println(F("V"));  

  display.setCursor(0, 10);
  display.println(current);  
  display.setCursor(30, 10);
  display.println(F("mA"));  

  display.setCursor(0, 20);
  display.println(voltage * (float(current) / 1000));  
  display.setCursor(30, 20);
  display.println(F("W"));  

  display.setCursor(0, 30);
  display.println(Wh);  
  display.setCursor(30, 30);
  display.println(F("WH"));  



  display.display();

}