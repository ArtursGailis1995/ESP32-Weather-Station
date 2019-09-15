#include <WiFi.h>
//DHT sensor library for ESP
#include <DHTesp.h>
//Software Serial library for ESP (v5.0.4)
#include <SoftwareSerial.h>
//CO2 sensor library
#include "MHZ19.h"
//Adafruit BME280 sensor library
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
//LCD I2C library for 20x4 character LCD
#include "LiquidCrystal_I2C.h"

#define RX_PIN 25 //RX pin for CO2 sensor
#define TX_PIN 27 //TX pin for CO2 sensor

const char* ssid = ""; //WiFi SSID, ex. myRouter-WiFiName
const char* password = ""; //WiFi password, ex. mySecureRouterPassword123
const char* influxdb_host = ""; //InfluxDB server IP address, ex. 192.168.1.10 (make sure it is configured as STATIC)
const int influxdb_port = 8086; //InfluxDB server port, ex. 8086 (default)
const char* influxdb_database = ""; //InfluxDB database name, ex. "arduino" (must be created first)

//Define custom characters for LCD
byte thermometer[8] = { B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110 };
byte hygrometer[8] = { B00100, B00100, B01010, B01010, B10001, B10001, B10001, B01110 };

//Definde variables for storing of data
float bme280_t; //BME280 temperature
float bme280_p; //BME280 pressure
float bme280_h; //BME280 humidity
float dht_temp; //DHT22 temperature
float dht_hum; //DHT22 humidity
int co2ppm = 0; //CO2 concentration
int preheatSec = 10; //CO2 sensor preheat time, needs to be 180 secs (from datasheet)

//Define timer and debug values
unsigned long prevTime = 0;
const long interval = 30000;
unsigned long prevTime_sense = 0;
const long interval_sense = 5000;
const int updRate = 2500;
bool data_to_show = false;
int errorCount = 0;
const bool debug = 1;

//Define sensors and LCD
LiquidCrystal_I2C lcd(0x3F, 20, 4); //Address, chars, rows
DHTesp dht; //DHT22 sensor
Adafruit_BME280 bme; //BME280 sensor
MHZ19 myMHZ19; //CO2 sensor
SoftwareSerial co2Serial(RX_PIN, TX_PIN); //Used for CO2 sensor reading data

void setup() {
  if (debug) {
    Serial.begin(115200);
    Serial.println("Setup: begin!");
  }

  //Initialize LCD screen
  lcd.begin();
  lcd.createChar(1, thermometer);
  lcd.createChar(2, hygrometer);
  lcd.setCursor(0, 0);
  lcd.print("Notiek ielade..."); //Display loading message
  delay(250);

  //Connect to WiFi network, use station mode
  WiFi.enableSTA(true);
  connectWiFi();

  //Check for DHT22 sensor on PIN 17
  dht.setup(17, DHTesp::DHT22);

  //Check for BME280 sensor (addresses in library files, 0x77 or 0x76)
  bool status = bme.begin();

  if ((!status) && debug) {
    Serial.println("Neizdevas noteikt derigu BME280 sensoru!"); //Error if sensor not found/valid
    isError();
  }

  //Read and store initial values from sensors
  bme280_t = bme.readTemperature() - 1.36; //A little offset for my environement
  bme280_p = bme.readPressure() * 0.00750061683; //Convert pressure to mmHg
  bme280_h = bme.readHumidity();
  dht_hum = dht.getHumidity();
  dht_temp = dht.getTemperature();

  //Init CO2 sensor and disable auto calibration
  co2Serial.begin(9600);
  myMHZ19.begin(co2Serial);
  myMHZ19.autoCalibration(false);

  if (debug) {
    Serial.println("Setup: OK!");
  }
}

void loop() {
  unsigned long currTime = millis();

  //Preheat CO2 sensor, check time every 10 seconds
  if (preheatSec > 0) {
    if (debug) {
      Serial.println("Preheating CO2 sensor: " + String(preheatSec) + " sec. remaining...");
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sagatavo CO2 sensoru"); //Preparing CO2 sensor
    lcd.setCursor(0, 1);
    lcd.print("~ 3 minutes..."); //About 3 minutes remaining
    
    preheatSec -= 10;
    delay(10000);
  }
  //If CO2 sensor is ready, go on
  else {
    //Read sensor data if interval_sense has passed
    if (currTime - prevTime_sense > interval_sense) {
      prevTime_sense = currTime;
      bme280_t = bme.readTemperature() - 1.36;
      bme280_p = bme.readPressure() * 0.00750061683;
      bme280_h = bme.readHumidity();
      dht_hum = dht.getHumidity();
      dht_temp = dht.getTemperature();
      co2ppm = myMHZ19.getCO2();
    }
    
    //Print data to console if debug mode is active
    if (debug) {
      printConsole();
    }
    
    //Output sensor data to LCD
    printLCD();

    //If interval has passed data must be sent to InfluxDB host over WiFi
    if (currTime - prevTime > interval) {
      prevTime = currTime;

      WiFiClient client;

      //Send the data only if CO2 values have stabilized
      if (co2ppm >= 350 && co2ppm <= 5000) {
        //And if connection to DB is successful
        if (client.connect(influxdb_host, influxdb_port)) {
          if (debug) {
            Serial.println("TCP savienojums ar InfluxDB serveri: OK!"); //TCP connection debug string
          }

          //Print to LCD about connection proccess
          lcd.clear();
          lcd.print("Savienojas ar"); //Connecting to
          lcd.setCursor(0, 1);
          lcd.print("InfluxDB...");

          //Build the string (query) for my personal needs and send to InfluxDB
          String vaicajums = "sensori,use=indoor t_bmp=" + String(bme280_t) + ",p_bmp=" + String(bme280_p) + ",t_dht=" + String(dht_temp) + ",h_dht=" + String(bme280_h) + ",h_out_dht=" + String(dht_hum) + ",co2_ppm=" + String(co2ppm);

          String http = String();
          http += "/write?db=arduino";

          //Print Request URL if in debug mode
          if (debug) {
            Serial.print("Veido pieprasijumu URL: "); //Requesting URL:
            Serial.println(http);
          }

          //Send POST request to HTTP client
          client.print(String("POST ") +
                       http + " HTTP/1.1\r\n" +
                       "User-Agent: ESP32/0.1\r\n" +
                       "Host: 192.168.1.10\r\n" + //change the hostname here
                       "Accept: */*\r\n" +
                       "Content-Length: " + String(vaicajums.length()) + "\r\n" +
                       "Content-Type: application/x-www-form-urlencoded\r\n" +
                       "Connection: close\r\n\r\n" + vaicajums + "\r\n");

          unsigned long timeout = millis();

          //Check for some client timeouts
          while (client.available() == 0) {
            if (millis() - timeout > 2500) {
              if (debug) {
                Serial.println(">>> Iestajies klienta taimauts (InfluxDB)!"); //Client timeout!
              }

              client.stop();
              return;
            }
          }

          //Reset errorCount if succesfuly sent data
          errorCount = 0;

          lcd.setCursor(0, 3);
          lcd.print("Dati nosutiti!"); //Data has been sent
          delay(500);

          //Print returned HTTP data to console if in debug mode
          if (debug) {
            Serial.println("Dati nosutiti uz InfluxDB: "); //Data has been sent to InfluxDB

            while (client.available()) {
              String line = client.readStringUntil('\r');
              Serial.print(line);
            }
          }
        }
        else {
          if (debug) {
            Serial.println("TCP savienojums ar InfluxDB: ERROR!"); //Failed to connect to InfuxDB
          }

          lcd.clear();
          lcd.print("Nevar izveidot"); //Error connecting to InfluxDB
          lcd.setCursor(0, 1);
          lcd.print("savienojumu ar");
          lcd.setCursor(0, 2);
          lcd.print("InfluxDB!");
          delay(500);
          isError();
        }
      }
      else {
        //Increase errorCount if something goes wrong with the readings/sending data
        Serial.println("Possibly invalid data has been detected - not uploading to target!");

        errorCount++;

        //Reboot if error count reaches 10
        if (errorCount >= 10) {
          isError();
        }
      }
    }
  }
}

//Function to inform about error and restart ESP32 device
void isError() {
  if (debug) {
    Serial.println("Notikusi kluda, iekarta tiks restarteta!"); //Error, rebooting...
  }

  lcd.clear();
  lcd.print("--------------------");
  lcd.setCursor(0, 1);
  lcd.print("---Notikusi kluda---"); //There is an error
  lcd.setCursor(0, 2);
  lcd.print("------Restarte------"); //Rebooting
  lcd.setCursor(0, 3);
  lcd.print("--------------------");

  delay(500);

  ESP.restart();
}

//Function used for connecting to WiFi
void connectWiFi() {
  lcd.clear();
  lcd.print("Veido savienojumu"); //Connecting
  lcd.setCursor(0, 1);
  lcd.print("ar WiFi tiklu.."); //to WiFi
  lcd.setCursor(0, 2);
  lcd.print("--------------------");
  lcd.setCursor(0, 3);
  lcd.print(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(750);
    if (debug) {
      Serial.println("Veido savienojumu ar WiFi tiklu.."); //Connecting to WiFi
    }
  }

  if (debug) {
    Serial.println("Savienojums ar WiFi tiklu izveidots!"); //WiFi connected
  }

  lcd.clear();
  lcd.print("WiFi savientos:"); //WiFi connected
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  lcd.setCursor(0, 2);
  lcd.print(WiFi.localIP());
  lcd.setCursor(0, 3);
  lcd.print(WiFi.macAddress());

  //Print IP and MAC to console if in debug mode
  if (debug) {
    Serial.print("ESP32 IP adrese: "); //ESP32 IP address
    Serial.println(WiFi.localIP());
    Serial.print("ESP32 MAC adrese: "); //ESP32 MAC address
    Serial.println(WiFi.macAddress());
  }

  delay(250);
}

//Function to print sensor data to console
void printConsole() {
  Serial.print("Temperatura istaba: "); //Indoor temperature C
  Serial.print(bme280_t);
  Serial.println("C");

  Serial.print("Gaisa mitrums istaba: "); //Indoor humidity %
  Serial.print(bme280_h);
  Serial.println("%");

  Serial.print("Atm. spiediens istaba: "); //Atmospheric pressure mmHg
  Serial.print(bme280_p);
  Serial.println("mmHg");

  Serial.print("Temperatura ara: "); //Outdoor temperature C
  Serial.print(dht_temp);
  Serial.println("C");

  Serial.print("Gaisa mitrums ara: "); //Outdoor humidity %
  Serial.print(dht_hum);
  Serial.println("%");

  Serial.print("CO2 koncentracija: "); //Indor CO2 ppm
  Serial.print(co2ppm);
  Serial.println("ppm");
}

//Function for printing data to LCD
void printLCD() {
  data_to_show = false;
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("  ISTABA     ARA"); //INDOOR OUTDOOR

  lcd.setCursor(0, 1);
  lcd.write(byte(1));
  lcd.setCursor(2, 1);
  lcd.print(bme280_t);
  lcd.print(char(223));
  lcd.print("C");

  lcd.setCursor(11, 1);
  lcd.write(byte(1));
  lcd.setCursor(13, 1);
  lcd.print(dht_temp);
  lcd.print(char(223));
  lcd.print("C");

  lcd.setCursor(0, 2);
  lcd.write(byte(2));
  lcd.setCursor(2, 2);
  lcd.print(bme280_h);
  lcd.print("%");

  lcd.setCursor(11, 2);
  lcd.write(byte(2));
  lcd.setCursor(13, 2);
  lcd.print(dht_hum);
  lcd.print("%");

  //If CO2 data is valid then alternate between showing CO2 data and air pressure
  if (data_to_show == false && co2ppm > 0) {
    lcd.setCursor(0, 3);
    lcd.print("CO2 konc.: "); //CO2 concentration
    lcd.print(co2ppm);
    lcd.print("ppm");
    data_to_show = true;
  }
  else {
    lcd.setCursor(0, 3);
    lcd.print("Atm. sp.: "); //Atmospheric pressure
    lcd.print(bme280_p);
    lcd.print("mmHg");
  }

  delay(updRate);
}
