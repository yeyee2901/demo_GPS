// TO DO:
// - Benerin Display Data dari Satellites on View
// - SMS info GPS tsb ke HP

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>

#define PROGRAM_VERSION     "1.0"
#define LCD_SIMBOL_DERAJAT  0xDF  // dari datasheet HD44780 :)

// NMEA sentence standard format u/ GPS output
#define GGA_FORMAT  2   // GPS - Fixed Data
#define GLL_FORMAT  4   // Geographic Latitude-Longitude
#define GSA_FORMAT  8   // GNSS (P,H,V)DOP and Active Satellites
#define GSV_FORMAT  16  // GNSS Satellites in View
#define RMC_FORMAT  32  // "Recommended Specific Minimum" for GNSS data
#define VTG_FORMAT  64  // Course Over Ground & Ground Velocity
#define ZDA_FORMAT  128 // Time & Date

// FUNCTION PROTOTYPE---------------------------------------------
void init_LCD();
void updateSerialBuffer();
void rapikan_SIM_response();
void init_SIM868();
void init_GPS();
void showGPSlocation();
void sendSMS(String text);

// GLOBAL VARIABLES & OBJECT INSTANTIATIONS-----------------------
// SIM_TX -> digital pin 2 (Arduino RX)
// SIM_RX -> digital pin 3 (Arduino TX)
const int SIM_TX = 10,
          SIM_RX = 11,
          SIM_PWREN = 12;
LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);
SoftwareSerial SIM868(SIM_TX, SIM_RX);

String HardwareSerial_buffer, SIM_response;
String stripped_command, stripped_response;
String URL_googlemaps = "https://www.google.com/maps/place/maps?q=";
bool URL_get = false;
bool SIMcard_inserted = false;

// MAIN PROGRAM -----------------------------------------
void setup()
{
  Serial.begin(9600);

  init_LCD();

  lcd.setCursor(0,0);
  String sambutan = "Demo SIM868 v";
  sambutan += PROGRAM_VERSION;
  lcd.print(sambutan);
  lcd.setCursor(0,2);
  lcd.print("github repo:");
  lcd.setCursor(0,3);
  lcd.print("yeyee2901/demo_GPS");
  
  // Inisialisasi bisa gagal bila pin RX-TX terbalik
  init_SIM868();

  // note: di dalam init_GPS terdapat while(1) loop,
  // karena location akan terus di "get" sampai Location 2D/3D Fix
  init_GPS();

}

void loop()
{
  showGPSlocation();
}


//--------------------------------------------------------------------


// FUNCTION DECLARATIONS ---------------------------------------------
// LCD --------------------------------------------
void init_LCD(){
  while (lcd.begin(20, 4) != 1) //colums - 20, rows - 4
  {
    Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong. Only pins numbers: 4,5,6,16,11,12,13,14 are legal."));
    delay(5000);   
  }
}

// SIM868 -----------------------------------------
void init_SIM868()
{
  pinMode(SIM_PWREN, OUTPUT);

  // Berdasarkan datasheet:
  // Pin PWREN harus di toggle LOW 2 detik (minimal),
  // agar SIM868 dapat digunakan untuk komunikasi UART
  // indikator: LED merah pada modul SIM868
  digitalWrite(SIM_PWREN, HIGH);
  delay(500);
  digitalWrite(SIM_PWREN, LOW);
  delay(2000);
  digitalWrite(SIM_PWREN, HIGH);
  delay(4000);

  // AT untuk sinkronisasi baud rate
  // jika sinkronisasi gagal, maka inisialisasi diulang
  // cek pin TX & RX 
  while(1)
  {
    SIM868.begin(9600);
    SIM868.println("AT");
    delay(2000);
    updateSerialBuffer();

    bool init_status_ok = false;
    if(SIM_response.indexOf("OK") >= 0){
      init_status_ok = true;
    }

    rapikan_SIM_response();
    if(init_status_ok){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("SIM init success!");
      delay(3000);

      // cek SIM card
      SIM868.println("AT+CCID");
      updateSerialBuffer();
      rapikan_SIM_response();
      lcd.setCursor(0,2);
      if(SIM_response.length() >= 10 && SIM_response.indexOf("OK") != -1){
        lcd.print("SIM card OK...");
      
        // final check SIM card
        SIM868.println("AT+CMGF=1"); // set SMS to text mode
        updateSerialBuffer();
        rapikan_SIM_response();
        delay(2000);
        if(SIM_response.indexOf("OK") >= 0){
          lcd.setCursor(0,3);
          lcd.print("SMS OK!");
          SIMcard_inserted = true;
        }
        else{
          lcd.setCursor(0,3);
          lcd.print("SMS not OK :(");
        }
      }
      else{
        lcd.print("SIM card not OK...");
      }

      break;  // keluar loop
    }
    else{
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("SIM init failed :(");
      lcd.setCursor(0,1);
      lcd.print("Coba cek:");
      lcd.setCursor(0,2);
      lcd.print(">Pin TX-RX SIM868");
      lcd.setCursor(0,3);
      lcd.print(">Pin power SIM868");
    }
  }
}

void init_GPS()
{
  SIM868.println("AT+CGPSPWR=1");
  updateSerialBuffer();
  rapikan_SIM_response();

  // cold reset
  SIM868.println("AT+CGPSRST=0");
  updateSerialBuffer();
  rapikan_SIM_response();

  delay(3000);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Initializing GPS...");
  lcd.setCursor(0,1);
  lcd.print("Restarting GPS in");
  lcd.setCursor(0,2);
  lcd.print("Cold Reset mode...");
  lcd.setCursor(0,3);
  lcd.print("Wait >30 detik :)");

  delay(30000);

  /* Possible output:
  - Location Unknown
  - Location Not Fix
  - Location 2D Fix
  - Location 3D Fix
  */
  String GPS_loc_status;
  while(1){
    SIM_response = "";
    SIM868.println("AT+CGPSSTATUS?"); // Get status

    while(SIM868.available()){
      SIM_response += (char) SIM868.read();
    }
    SIM_response.replace("OK", "");
    rapikan_SIM_response();

    GPS_loc_status = SIM_response.substring(SIM_response.indexOf("Location"));

    // tampilkan info
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(GPS_loc_status);
    delay(3000);

    // if loc status = 2D or 3D Fix, break the loop
    if(GPS_loc_status.indexOf("D Fix") >= 0)
      break;

    lcd.setCursor(0,3);
    lcd.print("Retry until fix..");
    delay(3000);

  }
}

void showGPSlocation()
{
  SIM_response = "";
  SIM868.println("AT+CGPSINF=2"); // GGA sentence
    
  while(SIM868.available()){
    SIM_response += (char) SIM868.read();
  }
  SIM_response.replace("OK", "");

  rapikan_SIM_response();
  int start_idx = SIM_response.indexOf("+CGPSINF:");

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(SIM_response.substring(start_idx, start_idx+20));
  lcd.setCursor(0,1);
  lcd.print(SIM_response.substring(start_idx+21, start_idx+41));
  lcd.setCursor(0,2);
  lcd.print(SIM_response.substring(start_idx+41, start_idx+61));
  lcd.setCursor(0,3);
  lcd.print(SIM_response.substring(start_idx+61));

  delay(3000);

  // Extract GPS informations, lakukan hanya jika sentence
  // GPS diterima saja
  if(SIM_response.indexOf(": 2,") != -1)
  {
    start_idx = SIM_response.indexOf(": 2,") + 4;

    // Extract information
    String UTC_time = SIM_response.substring(start_idx, start_idx+9);
    String Latitude = SIM_response.substring(start_idx+11, start_idx+22);
    String Longitude = SIM_response.substring(start_idx+23, start_idx+35);
    String num_of_satellites = SIM_response.substring(start_idx+35);
    num_of_satellites = num_of_satellites.substring(0, 4);

    // add Latitude & Longitude data to URL
    if( !URL_get ){
      URL_googlemaps += (Latitude + "," + Longitude);
      URL_get = true;
    }

    // conversion (?) gatau perlu atau ngga
    float latitude_degree = 0.0,
          longitude_degree = 0.0;
    float latitude_minute = 0.0;
    float longitude_minute = 0.0;
    latitude_degree += (latitude_minute / 60.0);
    longitude_degree += (longitude_minute / 60.0);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("UTC    "); 
    lcd.print(UTC_time.substring(0,2));
    lcd.print(":");
    lcd.print(UTC_time.substring(2,4));
    lcd.print(":");
    lcd.print(UTC_time.substring(4));

    lcd.setCursor(0,1);
    lcd.print("Lat    "); 
    lcd.print(Latitude);

    lcd.setCursor(0,2);
    lcd.print("Long  "); 
    lcd.print(Longitude);

    lcd.setCursor(0,3);
    lcd.print("Header $GPGGA");

    delay(5000);

    lcd.clear();
    // lcd.setCursor(0,0);
    // lcd.print("Satellites On View: ");
    // lcd.setCursor(0,1);
    // lcd.print(num_of_satellites);
    lcd.setCursor(0,3);
    lcd.print("*NOTE Indo=UTC+7");

    delay(5000);
  }
}
// HELPER FUNCTION ---------------------------------------
void updateSerialBuffer()
{
  HardwareSerial_buffer = "";
  SIM_response = "";

  while(Serial.available())
  {
    HardwareSerial_buffer += Serial.read();
  }

  while(SIM868.available())
  {
    SIM_response = SIM868.readString();
  }

  Serial.print(SIM_response);
  SIM868.print(HardwareSerial_buffer);
}

void rapikan_SIM_response()
{
  SIM_response.replace("\r", "");
  SIM_response.replace("\n", " ");
}