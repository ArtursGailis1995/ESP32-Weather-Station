# ESP32-Weather-Station
ESP32 based weather station with indoor/outdor readings, DHT22, BME280, MH-Z19B CO2 sensors and big LCD!

This is a personal weather station based on ESP32 NodeMCU module (should work on ESP8266 too). 
Outdoor data is collected from DHT22 sensor. Indoor data is collected from BME280 and MH-Z19B sensors. Data is displayed on 2004 LCD module and also sent over WiFi to home server where InfluxDB and Grafana is hosted.

## Parts used for this project

ESP32 NodeMCU developement board
<img src="/images/ESP32.jpg" alt="ESP32s" width="250"/>
