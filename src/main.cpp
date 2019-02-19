/*
 * CO2 TEMPERATURE HUMIDITY PRESSURE INDOOR METER
 */
#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <HTU21D.h>
#include <Adafruit_BMP280.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define DEBUG_SERIAL Serial
#define SENSOR_SERIAL swSer
#define I2C_SDA 5 // D1
#define I2C_SCL 4 // D2

byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
byte abcon[9] = {0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};
byte abcoff[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};
byte response[7];

/* 
* Turn on/off MH-Z19B auto baseline correction function
* if true  - abc is off
* if false - abc is on
*/
bool setAbcOff = true;

char auth[] = "9a6028a485d5463ab994c013f57ed129";
char ssidMobile[] = "texas";
char passMobile[] = "vertigo118";
char ssidHome[] = "ESP";
char passHome[] = "Blackat45";

BlynkTimer timer;
HTU21D myHTU21D(HTU21D_RES_RH12_TEMP14);
Adafruit_BMP280 bme;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE);
SoftwareSerial swSer(13, 15, false, 256); // GPIO15 (TX) and GPIO13 (RX)
WidgetTerminal terminal(V5);

float t = 99;
int p = -1;
int h = 99;
int co2 = -1;

char loader[4]{'.'};

void readCarbonDioxide()
{

        bool header_found = false;

        SENSOR_SERIAL.write(cmd, 9);
        memset(response, 0, 7);

        // Looking for packet start
        while (SENSOR_SERIAL.available() && (!header_found))
        {
                if (SENSOR_SERIAL.read() == 0xff)
                {
                        if (SENSOR_SERIAL.read() == 0x86)
                                header_found = true;
                }
        }

        if (header_found)
        {
                SENSOR_SERIAL.readBytes(response, 7);

                byte crc = 0x86;
                for (int i = 0; i < 6; i++)
                {
                        crc += response[i];
                }
                crc = 0xff - crc;
                crc++;

                if (!(response[6] == crc))
                {
                        BLYNK_LOG4("MH-Z19B CRC error: ", crc, " / ", response[6]);
                        co2 = -2;
                }
                else
                {
                        unsigned int responseHigh = (unsigned int)response[0];
                        unsigned int responseLow = (unsigned int)response[1];
                        unsigned int ppm = (256 * responseHigh) + responseLow;
                        co2 = ppm;
                        BLYNK_LOG("CO2: %d ppm", co2);
                        BLYNK_LOG("MH-Z19B ABC: %d", setAbcOff);
                }
                if (setAbcOff)
                {
                        SENSOR_SERIAL.write(abcoff, 9);
                        setAbcOff = false;
                        co2 = -4;
                        BLYNK_LOG("MH-Z19B ABC turn off");
                }
        }
        else
        {
                BLYNK_LOG("MH-Z19B Header not found");
                co2 = -3;
        }
}

void getSensors()
{
        // TEMP
        float tf = myHTU21D.readTemperature();
        tf = floor(tf * 10);
        t = tf / 10;
        BLYNK_LOG("Temperature: %.1f C", t);
        // HUMIDITY
        float hf = myHTU21D.readCompensatedHumidity();
        h = round(hf);
        BLYNK_LOG("Humidity: %d %%", h);
        // PRESSURE
        // BLYNK_LOG("Pressure: %d mmHg", p);
        // CO2
        readCarbonDioxide();
        //BLYNK_LOG("CO2: %d ppm", co2);

        // Send to server
        Blynk.virtualWrite(V1, t);
        Blynk.virtualWrite(V2, h);
        Blynk.virtualWrite(V3, p);
        Blynk.virtualWrite(V4, co2);
}

void drawLoading()
{
        long unsigned int count{(millis() / 500) % 4};
        memset(loader, '.', count);
        memset(&loader[count], 0, 1);
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_inb19_mf);
        byte x = (128 - u8g2.getStrWidth(loader)) / 2;
        byte y = 16 + u8g2.getAscent() - u8g2.getDescent();
        u8g2.drawStr(x, y, loader);
        u8g2.sendBuffer();
}

void drawSensors()
{

        u8g2.clearBuffer();
        // Frames
        //u8g2.drawFrame(0,0,128,16);
        //u8g2.drawFrame(0,16,128,48);
        //u8g2.drawRFrame(0,16,128,48,2);
        //u8g2.drawHLine(78,40,50);
        //u8g2.drawVLine(78, 16, 48);

        // Temperature
        u8g2.setFont(u8g2_font_logisoso16_tf);
        u8g2.setCursor(82, 40);
        u8g2.printf("%.1f", t);
        u8g2.setFont(u8g2_font_ncenB08_tf);
        u8g2.printf("%cC", 176);

        // Humidity
        u8g2.setFont(u8g2_font_logisoso16_tf);
        u8g2.setCursor(82, 62);
        u8g2.printf("%d", h);
        u8g2.setFont(u8g2_font_ncenB08_tf);
        u8g2.printf("%%");

        // CO2
        u8g2.setFont(u8g2_font_ncenB08_tf);
        u8g2.setCursor(2, 30);
        u8g2.printf("CO2 ppm");
        u8g2.setFont(u8g2_font_logisoso26_tf);
        u8g2.setCursor(0, 62);
        u8g2.printf("%d", co2);

        // Connection
        u8g2.setFont(u8g2_font_ncenB08_tf);
        u8g2.setCursor(2, 14);

        if (Blynk.connected())
        {
                u8g2.print("Online");
        }
        else
        {
                u8g2.print("Offline");
        }

        u8g2.print(" [");
        u8g2.print(WiFi.localIP());
        u8g2.print("]");

        u8g2.sendBuffer();
}

void draw()
{

        if (co2 > 0 && setAbcOff == false)
        {
                drawSensors();
        }
        else
        {
                drawLoading();
        }

        //drawSensors();
}

void drawBoot(String msg = "Loading...")
{
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_9x18_mf);
        byte x = (128 - u8g2.getStrWidth(msg.c_str())) / 2;
        byte y = 32 + u8g2.getAscent() / 2;
        u8g2.drawStr(x, y, msg.c_str());
        u8g2.sendBuffer();
}

void myConnectWiFi(const char *ssid, const char *pass)
{
        BLYNK_LOG2(BLYNK_F("WiFi connecting to "), ssid);
        WiFi.mode(WIFI_STA);
        if (WiFi.status() != WL_CONNECTED)
        {
                if (pass && strlen(pass))
                {
                        WiFi.begin(ssid, pass);
                }
                else
                {
                        WiFi.begin(ssid);
                }
        }
}

void myCheckWIfi()
{
        static bool check = true;
        if (WiFi.status() != WL_CONNECTED)
        {
                if (check)
                {
                        check = !check;
                        myConnectWiFi(ssidMobile, passMobile);
                }
                else
                {
                        check = !check;
                        myConnectWiFi(ssidHome, passHome);
                }
        }
        else if (WiFi.status() == WL_CONNECTED)
        {
                if (check)
                {
                        BLYNK_LOG2(BLYNK_F("WiFi connected to "), ssidHome);
                }
                else
                {
                        BLYNK_LOG2(BLYNK_F("WiFi connected to "), ssidMobile);
                }
                
                ArduinoOTA.begin();
                //BLYNK_LOG("Ready");
                IPAddress myip = WiFi.localIP();
                BLYNK_LOG_IP("IP: ", myip);
        }
}

BLYNK_WRITE(V5)
{

        if (String("ip") == param.asStr())
        {
                //terminal.print("ip: ");
                terminal.println(WiFi.localIP());
        }
        else if (String("cl") == param.asStr())
        {
                terminal.clear();
        }
        else if (String("se") == param.asStr())
        {
                //terminal.clear();
                //terminal.println("Sensors-->");
                terminal.printf("Temp: %.1fC \n", t);
                terminal.printf("RH: %d%% \n", h);
                terminal.printf("co2: %dppm \n", co2);
        }
        else
        {
                //terminal.clear();
                terminal.print("Command '");
                terminal.write(param.getBuffer(), param.getLength());
                terminal.println("' not found!");
        }

        terminal.flush();
}

void setup()
{

        Serial.begin(9600);
        SENSOR_SERIAL.begin(9600);
        Wire.begin(I2C_SDA, I2C_SCL);
        u8g2.begin();
        terminal.clear();

        drawBoot();

        if (!myHTU21D.begin(I2C_SDA, I2C_SCL))
        {
                BLYNK_LOG("Could not find a valid HTU21D sensor, check wiring!");
        }
        else
        {
                BLYNK_LOG("HTU21D sensor OK!");
        }
        if (!bme.begin())
        {
                BLYNK_LOG("Could not find a valid BMP280 sensor, check wiring!");
        }
        else
        {
                BLYNK_LOG("BMP280 sensor OK!");
        }

        //WiFi connection
        myConnectWiFi(ssidHome, passHome);
        Blynk.config(auth);
        Blynk.connect(10);

        // OTA
        // Port defaults to 8266
        // ArduinoOTA.setPort(8266);

        // Hostname defaults to esp8266-[ChipID]
        // ArduinoOTA.setHostname("myesp8266");

        // No authentication by default
        // ArduinoOTA.setPassword("admin");

        // Password can be set with it's md5 value as well
        // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
        // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

        ArduinoOTA.onStart([]() {
                String type;
                if (ArduinoOTA.getCommand() == U_FLASH)
                {
                        type = "sketch";
                }
                else
                { // U_SPIFFS
                        type = "filesystem";
                }

                // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
                BLYNK_LOG2("Start updating ", type);
        });
        ArduinoOTA.onEnd([]() {
                BLYNK_LOG("\nEnd");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
                BLYNK_LOG("Progress: %u%%\r", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t error) {
                BLYNK_LOG("Error[%u]: ", error);
                if (error == OTA_AUTH_ERROR)
                {
                        BLYNK_LOG("Auth Failed");
                }
                else if (error == OTA_BEGIN_ERROR)
                {
                        BLYNK_LOG("Begin Failed");
                }
                else if (error == OTA_CONNECT_ERROR)
                {
                        BLYNK_LOG("Connect Failed");
                }
                else if (error == OTA_RECEIVE_ERROR)
                {
                        BLYNK_LOG("Receive Failed");
                }
                else if (error == OTA_END_ERROR)
                {
                        BLYNK_LOG("End Failed");
                }
        });
        ArduinoOTA.begin();
        //BLYNK_LOG("Ready");
        IPAddress myip = WiFi.localIP();
        BLYNK_LOG_IP("IP: ", myip);

        timer.setInterval(10000L, getSensors);
        timer.setInterval(30000L, myCheckWIfi);

} //setup

void loop()
{
        ArduinoOTA.handle();
        Blynk.run();
        timer.run();
        draw();
}
