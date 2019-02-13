/*
 * CO2 TEMPERATURE HUMIDITY PRESSURE INDOOR METER
 */
#define BLYNK_PRINT  Serial
//#define BLYNK_DEBUG

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <HTU21D.h>
#include <Adafruit_BMP280.h>
#include <U8g2lib.h>
#include <SoftwareSerial.h>

#define DEBUG_SERIAL   Serial
#define SENSOR_SERIAL swSer
#define I2C_SDA 5 // D1
#define I2C_SCL 4 // D2 

byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
byte abcon[9] = {0xFF,0x01,0x79,0xA0,0x00,0x00,0x00,0x00,0xE6};
byte abcoff[9] = {0xFF,0x01,0x79,0x00,0x00,0x00,0x00,0x00,0x86};
byte response[7];
bool setAbcOff = true;

char auth[] = "9a6028a485d5463ab994c013f57ed129";
char ssidMobile[] = "texas";
char passMobile[] = "vertigo118";
char ssidHome[] = "ESP";
char passHome[] = "Blackat45";


BlynkTimer timer;
HTU21D  myHTU21D(HTU21D_RES_RH12_TEMP14);
Adafruit_BMP280 bme;
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE);
byte x = 0;
byte y = 0;
SoftwareSerial swSer(13, 15, false, 256); // GPIO15 (TX) and GPIO13 (RX)

float t = -100;
int p = -1;
int h = -1;
int co2 = -1;

char loader[4] {'.'};

void readCarbonDioxide() {
        
        bool header_found = false;

        SENSOR_SERIAL.write(cmd, 9);
        memset(response, 0, 7);

        // Looking for packet start
        while(SENSOR_SERIAL.available() && (!header_found)) {
                if(SENSOR_SERIAL.read() == 0xff ) {
                        if(SENSOR_SERIAL.read() == 0x86 ) header_found = true;
                }
        }

        if (header_found) {
                SENSOR_SERIAL.readBytes(response, 7);

                byte crc = 0x86;
                for (int i = 0; i < 6; i++) {
                        crc+=response[i];
                }
                crc = 0xff - crc;
                crc++;

                if ( !(response[6] == crc) ) {
                        BLYNK_LOG4("MH-Z19B CRC error: " ,crc, " / " , response[6]);
                        co2 = -2;
                } else {
                        unsigned int responseHigh = (unsigned int) response[0];
                        unsigned int responseLow = (unsigned int) response[1];
                        unsigned int ppm = (256*responseHigh) + responseLow;
                        co2 = ppm;
                        BLYNK_LOG("CO2: %d ppm", co2);
                        BLYNK_LOG("MH-Z19B ABC: %d", setAbcOff);
                }
                  if (setAbcOff) {
                        SENSOR_SERIAL.write(abcoff, 9);
                        setAbcOff = false;
                        BLYNK_LOG("MH-Z19B ABC turn off");
                  }
        } else {
                BLYNK_LOG("MH-Z19B Header not found");
                co2 = -3;
        }

}

void getSensors() {
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


void loading() {
        long unsigned int count {(millis() / 500) % 4};
        memset(loader, '.', count);
        memset(&loader[count], 0, 1);
}

void draw() {
        u8g2.clearBuffer();
        // Display status bar meauserments
        String st {"..."};
        String sh {"..."};

        const char degree {176};
        char emo[] {53};

        if (t > -100) { 
                st = String(t) + degree + "C"; 
        }

        if (h > -1) { 
                sh = String(h) + "%"; 
        }

        if (co2 < 600) { 
                emo[0] = 33; 
        } else if  (co2 < 800) { 
                emo[0] = 36; 
        } else if (co2 < 1000) { 
                emo[0] = 38; 
        } else if (co2 < 4990) { 
                emo[0] = 41; 
        }
        
        char ct [10];
        char ch [10];
        st.toCharArray(ct, 10);
        sh.toCharArray(ch, 10);
        // temp
        u8g2.setFont(u8g2_font_fub11_tf);
        x = 0;
        y = 0 + u8g2.getAscent();
        u8g2.setCursor(x,y);
        u8g2.printf("%.1f %cC", t, 176);
       // u8g2.drawStr(x, y, ct);
        // humidity
        //u8g2.setFont(u8g2_font_9x18_mf);
        x = 128 - u8g2.getStrWidth(ch);
        y = 0 + u8g2.getAscent();
        u8g2.drawStr(x, y, ch);
        // CO2
        if (co2 > -1) {
                char co2a [5];
                const char ppm[4] = "ppm";
                const char cotwo[] {"CO2"};

                // CO2
                u8g2.setFont(u8g2_font_tom_thumb_4x6_mf);
                x = 4;
                y = 22 + u8g2.getAscent();
                u8g2.drawStr(x, y, cotwo);
                x = x + u8g2.getStrWidth(cotwo);
                //DIGITS
                sprintf (co2a, "%i", co2);
                u8g2.setFont(u8g2_font_fub30_tf);
                x =  128 - u8g2.getStrWidth(co2a) - 14;
                y = 16 + u8g2.getAscent() - u8g2.getDescent();
                u8g2.drawStr(x, y, co2a);
                //x = x + u8g2.getStrWidth(co2a) + 2;
                //EMOJI
                //u8g2.setFont(u8g2_font_emoticons21_tr);
                //x = 0;
                //y = y;
                //u8g2.drawStr(x, y, emo);
                u8g2.setFont(u8g2_font_fub11_tf);
                u8g2.setCursor(0,y);
                u8g2.print(Blynk.connected());

                //ppm
                u8g2.setFont(u8g2_font_u8glib_4_tf);
                x = 128 - u8g2.getStrWidth(ppm);
                //y = y;
                u8g2.drawStr(x, y, "ppm");
        } else {
                loading();
                u8g2.setFont(u8g2_font_inb19_mf);
                x = (128 - u8g2.getStrWidth(loader)) / 2;
                y = 16 + u8g2.getAscent() - u8g2.getDescent();
                u8g2.drawStr(x, y, loader);
        }

    
        u8g2.sendBuffer();
}

void drawBoot(String msg = "Loading...") {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_9x18_mf);
        x = (128 - u8g2.getStrWidth(msg.c_str())) / 2;
        y = 32 + u8g2.getAscent() / 2;
        u8g2.drawStr(x, y, msg.c_str());
        u8g2.sendBuffer();
}

/*
void drawConnectionDetails(String ssid, String pass, String url) {
        String msg {""};
        u8g2.clearBuffer();

        msg = "Connect to WiFi:";
        u8g2.setFont(u8g2_font_7x13_mf);
        x = (128 - u8g2.getStrWidth(msg.c_str())) / 2;
        y = u8g2.getAscent() - u8g2.getDescent();
        u8g2.drawStr(x, y, msg.c_str());

        msg = "net: " + ssid;
        x = (128 - u8g2.getStrWidth(msg.c_str())) / 2;
        y = y + 3 + u8g2.getAscent() - u8g2.getDescent();
        u8g2.drawStr(x, y, msg.c_str());

        msg = "pw: "+ pass;
        x = (128 - u8g2.getStrWidth(msg.c_str())) / 2;
        y = y + 1 + u8g2.getAscent() - u8g2.getDescent();
        u8g2.drawStr(x, y, msg.c_str());

        msg = "Open browser:";
        x = (128 - u8g2.getStrWidth(msg.c_str())) / 2;
        y = y + 1 + u8g2.getAscent() - u8g2.getDescent();
        u8g2.drawStr(x, y, msg.c_str());

        // URL
        // u8g2.setFont(u8g2_font_6x12_mf);
        x = (128 - u8g2.getStrWidth(url.c_str())) / 2;
        y = y + 1 + u8g2.getAscent() - u8g2.getDescent();
        u8g2.drawStr(x, y, url.c_str());

        u8g2.sendBuffer();
}

*/
void myConnectWiFi(const char* ssid, const char* pass)
    {
        BLYNK_LOG2(BLYNK_F("WiFi connecting to "), ssid);
        WiFi.mode(WIFI_STA);
        if (WiFi.status() != WL_CONNECTED) {
            if (pass && strlen(pass)) {
                WiFi.begin(ssid, pass);
            } else {
                WiFi.begin(ssid);
            }
        }
    }

void myCheckWIfi() {
static bool check = true;
        if (WiFi.status() != WL_CONNECTED) {
                if(check) {
                check = !check;
                myConnectWiFi(ssidMobile, passMobile);
                } else {
                check = !check;
                myConnectWiFi(ssidHome, passHome);
                }
        } else if (WiFi.status() == WL_CONNECTED) {
                if (check) {
                BLYNK_LOG2(BLYNK_F("WiFi connected to "), ssidHome);
                } else {
                BLYNK_LOG2(BLYNK_F("WiFi connected to "), ssidMobile);
                }
                
                IPAddress myip = WiFi.localIP();
                BLYNK_LOG_IP("IP: ", myip);
        }
}

void setup() {

        Serial.begin(9600);
        SENSOR_SERIAL.begin(9600);
        Wire.begin(I2C_SDA, I2C_SCL);
        u8g2.begin();
        //drawBoot();
        if (!myHTU21D.begin(I2C_SDA, I2C_SCL)) {
                BLYNK_LOG("Could not find a valid HTU21D sensor, check wiring!");
        } else {
                BLYNK_LOG("HTU21D sensor OK!");
        }
        if (!bme.begin()) {
                BLYNK_LOG("Could not find a valid BMP280 sensor, check wiring!");
        } else {
                BLYNK_LOG("BMP280 sensor OK!");
        }

        
        //Blynk.begin(auth, ssidHome, passHome);
        //Blynk.begin(auth, ssidMobile, passMobile);
        myConnectWiFi(ssidHome, passHome);
        Blynk.config(auth);
        Blynk.connect(10);

        //drawBoot("Connecting...");
        timer.setInterval(10000L, getSensors);
        timer.setInterval(30000L, myCheckWIfi);

}  //setup

void loop() {
        Blynk.run();
        timer.run();
        draw();
}
