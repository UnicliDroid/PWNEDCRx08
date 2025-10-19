//*---------------------------------------------------------------------*
//* TITULO: Wardriving                                                  *
//* DESCRIPCIÓN: Escanea redes wifi y registra su inforación            *
//*              junto a su posición GPS en la tarjeta SD.              *
//*              Mostrando en pantalla inforamción para el usuario      *
//*---------------------------------------------------------------------*
//* Fecha de creación: 02/09/2025                                       *
//* Elaborado por: Jaime Mora                                           *
//* Licencia: MIT                                                       *
//* Libreria creada por RicardoOliveira                                 *
//*---------------------------------------------------------------------*
//* Proyectos consultados:                                              *
//*---------------------------------------------------------------------*
//* Nombre del proyecto             Autor                               *
//* wardriver3000                   cifertech                           *
//* ESP8266-Wardriving              AlexLynd                            *
//* TinyGPSPlus                     mikalhart                           *
//* Guide for I2C OLED Display      Rui Santos                          *
//*---------------------------------------------------------------------*

#include "./esppl_functions.h"
#include "icons.h"
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <TimeLib.h>

#define UTC_offset -6  // PDT
#define SD_CS D8
#define LOG_RATE 500

Adafruit_SSD1306 display(128, 64, &Wire, -1);  // shared reset

#if (SSD1306_LCDHEIGHT != 64)  // Check the screen size. If this warning shows, you have to make changes on the library.
#error("Incorrect screen height, fix Adafruit_SSD1306.h");
#endif

//Variable definitions.
String log_file_name = "";
int networks = 0;
int unique_SSIDs = 0;
String discovered_SSIDs;
char current_time[5];
SoftwareSerial ss(D4, D3);  // RX, TX
TinyGPSPlus tinyGPS;
String network_info = "";
String net_info = "";
int number_satellite;


const int BUFFER_SIZE = 10;
String buffer[BUFFER_SIZE];
int head = 0;
int tail = 0;
bool isFull = false;



void writeFile(SDClass &fs, String path, const char *message);
void initializeSD();
static void smartDelay(unsigned long ms);
void addToBuffer(String data);



void cb(esppl_frame_info *info) {
  if (((int)info->frametype) == 0 && (((int)info->framesubtype) == 4 || ((int)info->framesubtype) == 5 || ((int)info->framesubtype) == 8)) {
    network_info = String((int)info->frametype) + ";" + String((int)info->framesubtype) + ";";
    for (int i = 0; i < 6; i++) network_info += String(info->sourceaddr[i], HEX);
    network_info += ";";
    for (int i = 0; i < 6; i++) network_info += String(info->receiveraddr[i], HEX);
    network_info += ";" + String(info->rssi) + ";" + String(info->seq_num) + ";" + String(info->channel) + ";";
    if (info->ssid_length > 0) {
      for (int i = 0; i < info->ssid_length; i++) {
        network_info += String((char)info->ssid[i]);
      }
    } else {
      network_info += "none";
    }
    addToBuffer(network_info.c_str());
  }
}

void setup() {
  delay(500);
  Serial.begin(115200);
  esppl_init(cb);
  // Serial configuration
  ss.begin(9600);
  // initialization of OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // OLED address
  display.setTextSize(1);
  display.setTextColor(WHITE);
  // Logo Display
  display.clearDisplay();
  display.drawBitmap(32, 0, pwndcr, 64, 64, 1);
  display.display();
  delay(2000);
  display.clearDisplay();
  display.drawBitmap(41, 0, normal, 46, 64, 1);
  display.display();
  delay(2000);
  display.clearDisplay();
  // Change of the layout of the screen.
  display.drawBitmap(0, 48, unicli, 11, 16, 1);
  display.drawBitmap(108, 48, pwnedcrpeq, 20, 16, 1);
  display.display();
  // Set SD Card
  if (!SD.begin(SD_CS)) {
    display.drawBitmap(90, 0, sdb, 32, 32, 1);
    display.display();
    while (!SD.begin(SD_CS))
      ;
  }
  //Change Layout of the screen.
  display.clearDisplay();
  display.drawBitmap(48, 0, wifi, 32, 32, 1);
  display.drawBitmap(0, 48, unicli, 11, 16, 1);
  display.drawBitmap(108, 48, pwnedcrpeq, 20, 16, 1);
  display.drawBitmap(90, 0, sd, 32, 32, 1);
  display.display();
  // SD initialization.
  initializeSD();
  delay(500);
  //GPS Initialization
  if (ss.available() > 0) {
    display.drawBitmap(0, 0, gps, 32, 32, 1);
    display.display();
  } else {
    display.drawBitmap(0, 0, gpsb, 32, 32, 1);
    display.display();
  }
  int count=0;
  //Wait until the gps is on sync
  while (!tinyGPS.location.isValid()) {
    Serial.println(tinyGPS.satellites.value());
    display.clearDisplay();
    //*---------------- GPS blink ---------------------*
    display.drawBitmap(0, 0, gps, 32, 32, 1);
    display.drawBitmap(48, 0, wifi, 32, 32, 1);
    display.drawBitmap(90, 0, sd, 32, 32, 1);
    display.drawBitmap(0, 48, unicli, 11, 16, 1);
    display.drawBitmap(108, 48, pwnedcrpeq, 20, 16, 1);

    display.display();
    smartDelay(500);
    display.clearDisplay();

    display.drawBitmap(0, 0, gpsb, 32, 32, 1);
    display.drawBitmap(48, 0, wifi, 32, 32, 1);
    display.drawBitmap(90, 0, sd, 32, 32, 1);
    display.drawBitmap(0, 48, unicli, 11, 16, 1);
    display.drawBitmap(108, 48, pwnedcrpeq, 20, 16, 1);

    display.display();
    smartDelay(500);
    /*count++;
    if (count > 10){
      break;
    }*/
    //*---------------- GPS blink ---------------------*
  }
  //Set layout
  display.clearDisplay();
  display.drawBitmap(48, 0, wifi, 32, 32, 1);
  display.drawBitmap(0, 0, gps, 32, 32, 1);
  display.drawBitmap(90, 0, sd, 32, 32, 1);
  display.drawBitmap(0, 48, unicli, 11, 16, 1);
  display.drawBitmap(108, 48, pwnedcrpeq, 20, 16, 1);
  display.display();
}

void loop() {
  esppl_sniffing_start();
  while (true) {
    for (int i = 1; i < 15; i++) {
      esppl_set_channel(i);
      while (esppl_process_frames()) {
        // Recive frames
      }
      //Set time acording to the GPS data.
      if (tinyGPS.location.isValid()) {
        setTime(tinyGPS.time.hour(), tinyGPS.time.minute(), tinyGPS.time.second(), tinyGPS.date.day(), tinyGPS.date.month(), tinyGPS.date.year());
        adjustTime(UTC_offset * SECS_PER_HOUR);
      }
      //Extract from Buffer and add information about gps
      int index = head;
      while (index != tail || isFull) {
        net_info = buffer[index];
        net_info += ";" + String(year()) + "-" + String(month()) + "-" + String(day()) + ";" + String(hour()) + ":" + String(minute()) + ":" + String(second());
        if (tinyGPS.location.isValid()) {
          net_info += ";" + String(tinyGPS.location.lat(), 11) + ";" + String(tinyGPS.location.lng(), 12) + ";" + String(tinyGPS.altitude.meters(), 1) + ";" + String(tinyGPS.hdop.hdop(), 1) + "\n";
        } else {
          net_info += String(";;;;\n");
        }
        Serial.println(net_info);
        writeFile(SD, log_file_name, net_info.c_str());
        index = (index + 1) % BUFFER_SIZE;
        if (isFull && index == tail) break;
      }
      
      //Update data on the screen.
      display.clearDisplay();
      display.drawBitmap(48, 0, wifi, 32, 32, 1);
      display.drawBitmap(0, 0, gps, 32, 32, 1);
      display.drawBitmap(90, 0, sd, 32, 32, 1);

      display.setCursor(14, 36);
      display.println(tinyGPS.satellites.value());

      display.setCursor(62, 36);
      display.println(String(unique_SSIDs));// Is going to be zero, unless you change the writeFile function to the one is commented 

      display.setCursor(90, 36);
      display.println(log_file_name.substring(0, 5));

      display.drawBitmap(0, 48, unicli, 11, 16, 1);
      display.drawBitmap(108, 48, pwnedcrpeq, 20, 16, 1);

      display.display();
      smartDelay(100);
    }
  }
}


void addToBuffer(String data) {
  buffer[tail] = data;
  tail = (tail + 1) % BUFFER_SIZE;

  if (isFull) {
    head = (head + 1) % BUFFER_SIZE;  // Sobrescribe el más antiguo
  }

  if (tail == head) {
    isFull = true;
  }
}


void writeFile(SDClass &fs, String path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  // Open the file
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }

  // Writte data without checking is if already there
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }

  file.close();
}


/*
void writeFile(SDClass &fs, String path, const char *message) {
    Serial.printf("Appending to file: %s\n", path);

    // Open the file for reading and check for existing content
    File file = fs.open(path);
    if (!file) {
        Serial.println("Failed to open file for reading");
    } else {
        // Read existing content and store it in a string
        String existingContent;
        while (file.available()) {
            existingContent += (char)file.read();
        }
        file.close();

        // Extract SSID from the message
        String newSSID = String(message);
        newSSID = newSSID.split(";")[7].trim();

        // Check if the new SSID already exists in the file. If you are using the probe request, has to change this logic since only admit one ssid.
        if (existingContent.indexOf(newSSID) != -1 || discovered_SSIDs.indexOf(newSSID) != -1) {
            Serial.println("SSID already exists. Skipping append.");
            return;
        } else {
            unique_SSIDs++; // Increment unique SSID count
            discovered_SSIDs += newSSID + ','; // Add new SSID to discovered list
        }
    }

    // If SSID doesn't exist, open the file for appending and write new data
    file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message)) {
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}
*/

void initializeSD() {  // create new CSV file and add WiGLE headers
  int i = 0;
  log_file_name = "log0.csv";
  while (SD.exists(log_file_name)) {
    i++;
    log_file_name = "log" + String(i) + ".csv";
  }
  File logFile = SD.open(log_file_name, FILE_WRITE);
  //display.println("Created: " + logFileName); display.display();
  if (logFile) {
    logFile.println("FT;FST;MAC_SRC;MAC_DEST;RSSI;SEQ;CH;SSID;Date;Time;CurrentLatitude;CurrentLongitude;AltitudeMeters;HDOP");
  }
  logFile.close();
}



String getEncryption(uint8_t network) {  // return encryption for WiGLE or print
  byte encryption = WiFi.encryptionType(network);
  switch (encryption) {
    case 2:
      return "[WPA-PSK-CCMP+TKIP][ESS]";
    case 5:
      return "[WEP][ESS]";
    case 4:
      return "[WPA2-PSK-CCMP+TKIP][ESS]";
    case 7:
      return "[ESS]";
  }
  return "[WPA-PSK-CCMP+TKIP][WPA2-PSK-CCMP+TKIP][ESS]";
}


// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (ss.available())
      tinyGPS.encode(ss.read());
  } while (millis() - start < ms);
}
