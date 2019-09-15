# ESP32-Weather-Station
ESP32 based weather station with indoor/outdor readings, DHT22, BME280, MH-Z19B CO2 sensors and big LCD!

This is a personal weather station based on ESP32 NodeMCU module (should work on ESP8266 too). 
Outdoor data is collected from DHT22 sensor. Indoor data is collected from BME280 and MH-Z19B sensors. Data is displayed on 2004 LCD module and also sent over WiFi to home server where InfluxDB and Grafana is hosted.

<img src="/images/Finished_product.jpg" alt="Finished weather station" width="650"/>

## Parts used for this project

ESP32 NodeMCU developement board<br/>
<img src="/images/ESP32.jpg" alt="ESP32" width="250"/>

BME280 temperature, humidity, atmospheric pressure sensor<br/>
<img src="/images/BME280.jpg" alt="BME280" width="250"/>

DHT22 temperature and humidity  sensor<br/>
<img src="/images/DHT22.jpg" alt="DHT22" width="250"/>

MH-Z19B CO2 sensor<br/>
<img src="/images/MH-Z19B.jpg" alt="MH-Z19B" width="250"/>

2004 LCD module with 20 chars * 4 rows<br/>
<img src="/images/2004-LCD.jpg" alt="2004 LCD" width="250"/>

I2C adapter for 2004 LCD module<br/>
<img src="/images/I2C-Adapter-LCD.jpg" alt="I2C adapter LCD" width="250"/>
