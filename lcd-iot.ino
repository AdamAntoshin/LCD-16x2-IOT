#define SERIAL_BAUD_RATE 115200
#define UPDATE_DELAY 3000 //TIME BETWEEN UPDATES
#define WIFI_TIMEOUT 5000 //WIFI CONNECTION TIMEOUT
#define CLIENT_TIMEOUT 5000 //CLIENT CONNECTION TIMEOUT
#define TIMER_NUM 3 
//TIMER INDEXES
#define UPDATE_TIMER 0
#define WIFI_TIMEOUT_TIMER 1
#define CLIENT_TIMEOUT_TIMER 2

//IO PINS DEFINITIONS
#define BAT_LED_PIN 33
#define BAT_ANALOG_PIN 32

//LOW BATTERY DETECTION PARAMETERS
#define SUPPLY_VOLTAGE_THRESHOLD 7.0
#define VOLTAGE_DIVIDER_RATIO 3.0
#define MCU_MAX_VOLTAGE 3.3
#define MCU_ADC_BIT_RES 12

#include <WiFi.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

//WiFi AP name and password
const char* ssid     = "***";
const char* password = "***";

//HTTP server with data
const char* host = "***.azurewebsites.net";

unsigned long timers[TIMER_NUM]; //array of timers the program uses
unsigned long analog_thres; //analog value for low battery
String ln0, ln1; //text to be displayed on both rows of LCD

//initialize all timers
void init_timers() {
  int i;
  for (i = 0; i < TIMER_NUM; i++) 
    timers[i] = millis();
  }

//update a timer
void start_timer(byte t_id) {
  timers[t_id] = millis();
  }

//return elapsed time since last timer update
unsigned long elapsed_timer(byte t_id) {
  return millis() - timers[t_id];
  }

boolean connect_to_wifi() {
    //We start by connecting to a WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    //if wifi connection timeout, stop and return 0 (false)
    start_timer(WIFI_TIMEOUT_TIMER);
    while (WiFi.status() != WL_CONNECTED) {
        if (elapsed_timer(WIFI_TIMEOUT_TIMER) >= WIFI_TIMEOUT) {
          Serial.println("WIFI CONNECTION FAILED");
          //WiFi.end();
          return 0;
          }
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    //return 1 (true) to indicate successful connection
    return 1;
  }

//update text to be displayed
void update_data() {
    //if wifi disconnected, try to reconnect
    if (WiFi.status() != WL_CONNECTED)
      while(!connect_to_wifi());
      
    Serial.print("connecting to ");
    Serial.println(host);

    //Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");
        return;
    }
    
    String url = "/data.txt"; //text file containing data

    Serial.print("Requesting URL: ");
    Serial.println(url);

    //This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");

    //if client timeout, exit
    start_timer(CLIENT_TIMEOUT_TIMER);
    while (client.available() == 0) {
        if (elapsed_timer(CLIENT_TIMEOUT_TIMER) >= CLIENT_TIMEOUT) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }
    
    //read response. if a line contains the special header char ($), start reading data and trim whitespace
    while(client.available()) {
        String line = client.readStringUntil('\n');
        if (line.indexOf('$') != -1) {
          ln0 = line.substring(1);
          ln0.trim();
          ln1 = client.readStringUntil('\n');
          ln1.trim();
          }
        //Serial.print(line);
    }
    Serial.print("LINE 0 = '"); Serial.print(ln0); Serial.println("'");
    Serial.print("LINE 1 = '"); Serial.print(ln1); Serial.println("'");
    Serial.println();
    Serial.println("closing connection");
  }

//print text to lcd
void update_lcd() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(ln0);
  lcd.setCursor(0,1);
  lcd.print(ln1);
  }

//return analog value threshold for low batter detection
unsigned int calc_thres(float min_voltage, float volt_div, float mcu_voltage, byte adc_res) {
  float min_input_voltage = min_voltage / volt_div;
  float min_input_ratio = min_input_voltage / mcu_voltage;
  unsigned int max_analog_val = pow(2, adc_res) - 1;
  unsigned int threshold = min_input_ratio * max_analog_val;
  return threshold;
  }

void init_IO() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(10);
    
    pinMode(BAT_LED_PIN, OUTPUT);
    pinMode(BAT_ANALOG_PIN, INPUT);
    digitalWrite(BAT_LED_PIN, LOW);
    
    lcd.begin();
    lcd.backlight();
    //lcd.noBacklight();
    lcd.print("Initializing...");
  }

//update low battery detection
void update_bat_led() {
  unsigned int analog_val = analogRead(BAT_ANALOG_PIN);
  if (analog_val < analog_thres) digitalWrite(BAT_LED_PIN, HIGH);
  else digitalWrite(BAT_LED_PIN, LOW);
  }

void setup() {
    init_IO();
    analog_thres = calc_thres(SUPPLY_VOLTAGE_THRESHOLD, VOLTAGE_DIVIDER_RATIO, MCU_MAX_VOLTAGE, MCU_ADC_BIT_RES);
    //try to connect to wifi until successful
    while(!connect_to_wifi());
    start_timer(UPDATE_TIMER);
}

void loop() { 
    //start updating data after a certain time since last update
    if (elapsed_timer(UPDATE_TIMER) >= UPDATE_DELAY) { 
      update_data();
      update_lcd();
      start_timer(UPDATE_TIMER);
    }

    update_bat_led();
}
