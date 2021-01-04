// GPS DATA LOGGER
// Ben Cook 
// 04/01/2021
// Credit: 'IForce2D' for GPS UBX protocol code 

#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>

// IO flags
int PPS = LOW;
int REC_SW = LOW;
int FIX_LIGHT = LOW;
int REC_LIGHT = LOW;

const int PPS_pin = 6;
const int REC_SW_pin = 7;
const int FIX_LIGHT_pin = 8;
const int REC_LIGHT_pin = 9;

// State Machine variables
const int NO_FIX = 0;
const int NOT_RECORDING = 1;
const int RECORDING = 2; 
const int DUMMY = 100;
int state = NO_FIX;
int last_state = DUMMY;
boolean FIX_OK = false; 
boolean PPS_OK = false;
boolean DISPLAY_GPS = false;
boolean SAVE_DATA = false;
boolean SD_ERROR = false;

// Data file variables
char* fname = "000000.csv";  //'HHMMSS.csv'
char* dirname = "00000000/";     //'YYYYMMDD/'
char* filename = "00000000/000000.csv";     // path
char timeString[9] = "00:00:00"; // time
File myFile;

//
unsigned long startTime = 0;

// Connect the GPS RX/TX to arduino pins 3 and 5
SoftwareSerial serial = SoftwareSerial(3,5); // RX, TX

const unsigned char UBX_HEADER[] = { 0xB5, 0x62 };

struct NAV_PVT {
  unsigned char cls;
  unsigned char id;
  unsigned short len;
  unsigned long iTOW;          // GPS time of week of the navigation epoch (ms)
  
  unsigned short year;         // Year (UTC) 
  unsigned char month;         // Month, range 1..12 (UTC)
  unsigned char day;           // Day of month, range 1..31 (UTC)
  unsigned char hour;          // Hour of day, range 0..23 (UTC)
  unsigned char minute;        // Minute of hour, range 0..59 (UTC)
  unsigned char second;        // Seconds of minute, range 0..60 (UTC)
  char valid;                  // Validity Flags (see graphic below)
  unsigned long tAcc;          // Time accuracy estimate (UTC) (ns)
  long nano;                   // Fraction of second, range -1e9 .. 1e9 (UTC) (ns)
  unsigned char fixType;       // GNSSfix Type, range 0..5
  char flags;                  // Fix Status Flags
  unsigned char reserved1;     // reserved
  unsigned char numSV;         // Number of satellites used in Nav Solution
  
  long lon;                    // Longitude (deg)
  long lat;                    // Latitude (deg)
  long height;                 // Height above Ellipsoid (mm)
  long hMSL;                   // Height above mean sea level (mm)
  unsigned long hAcc;          // Horizontal Accuracy Estimate (mm)
  unsigned long vAcc;          // Vertical Accuracy Estimate (mm)
  
  long velN;                   // NED north velocity (mm/s)
  long velE;                   // NED east velocity (mm/s)
  long velD;                   // NED down velocity (mm/s)
  long gSpeed;                 // Ground Speed (2-D) (mm/s)
  long heading;                // Heading of motion 2-D (deg)
  unsigned long sAcc;          // Speed Accuracy Estimate
  unsigned long headingAcc;    // Heading Accuracy Estimate
  unsigned short pDOP;         // Position dilution of precision
  short reserved2;             // Reserved
  unsigned long reserved3;     // Reserved

  long headVeh;
  short magDec;
  unsigned short magAcc;
  
};

NAV_PVT pvt;

void calcChecksum(unsigned char* CK) {
  memset(CK, 0, 2);
  for (int i = 0; i < (int)sizeof(NAV_PVT); i++) {
    CK[0] += ((unsigned char*)(&pvt))[i];
    CK[1] += CK[0];
  }
}

bool processGPS() {
  static int fpos = 0;
  static unsigned char checksum[2];
  const int payloadSize = sizeof(NAV_PVT);

  while ( serial.available() ) {
    byte c = serial.read();
    
    if ( fpos < 2 ) {
      if ( c == UBX_HEADER[fpos] ){
        fpos++;
      }
      else
        fpos = 0;
    }
    else {      
      if ( (fpos-2) < payloadSize )
        ((unsigned char*)(&pvt))[fpos-2] = c;

      fpos++;

      if ( fpos == (payloadSize+2) ) {
        calcChecksum(checksum);
      }
      else if ( fpos == (payloadSize+3) ) {
        if ( c != checksum[0] ) {
          fpos = 0;
        }
      }
      else if ( fpos == (payloadSize+4) ) {
        fpos = 0;
        if ( c == checksum[1] ) {
          return true;
        }
      }
      else if ( fpos > (payloadSize+4) ) {
        fpos = 0;
      }
    }
  }
  return false;
}

void setup() 
{
  // Configure digital IO pin directions
  pinMode(FIX_LIGHT_pin, OUTPUT);
  pinMode(REC_LIGHT_pin, OUTPUT);
  pinMode(PPS_pin, INPUT);
  pinMode(REC_SW_pin, INPUT);
  
  // Configure serial communications
  Serial.begin(115200);
  serial.begin(19200);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
    
  if (!SD.begin(4)) {
    Serial.println("SD Error");
    SD_ERROR = true;
    //while (1);
  }

}

void loop() {

  // Read the digital inputs
  PPS = digitalRead(PPS_pin); // read REC_SW
  REC_SW = digitalRead(REC_SW_pin); // read REC_SW

  FIX_OK = pvt.flags & 00000001; 

  if (PPS == HIGH){
    PPS_OK = true;
  }

  // Run state machine
  switch (state) {
    
    case NO_FIX:  
      // ENTRY
      if (last_state != NO_FIX){
        Serial.println("State : NO_FIX");
      } 
      
      // DO
      digitalWrite(FIX_LIGHT_pin, HIGH); // FIX_LIGHT functionality
      
      if (SD_ERROR == false){
        digitalWrite(REC_LIGHT_pin, LOW);  
      } else {
        digitalWrite(REC_LIGHT_pin, PPS);
      }
      
      DISPLAY_GPS = false;
      SAVE_DATA = false;

      // NEXT STATE
      if ( (pvt.hAcc < 5000) &&  (FIX_OK == true) && (PPS_OK == true) ){
        state = NOT_RECORDING;
      } else {
        state = NO_FIX;
      }
      last_state = NO_FIX;
      break;
    
    case NOT_RECORDING:
      // ENTRY
      if (last_state != NOT_RECORDING){
        Serial.println("State : NOT_RECORDING");
      }
       
      // DO
      digitalWrite(FIX_LIGHT_pin, PPS); // FIX_LIGHT functionality
      
      if (SD_ERROR == false){
        digitalWrite(REC_LIGHT_pin, LOW);  
      } else {
        digitalWrite(REC_LIGHT_pin, PPS);
      }

      DISPLAY_GPS = false;
      SAVE_DATA = false;

      // NEXT STATE
      if ( REC_SW == HIGH ){
        state = RECORDING;
      } else {
        state = NOT_RECORDING;
      }  
      last_state = NOT_RECORDING;
      break;
    
    case RECORDING:
      // ENTRY
      if (last_state != RECORDING){
        Serial.println("State : RECORDING");
        
        // generate new filename in format 'YYYYMMDD/HHMMSS.csv'
        sprintf(dirname, "%04d%02d%02d", pvt.year, pvt.month, pvt.day);
        sprintf(fname, "%02d%02d%02d.csv", pvt.hour, pvt.minute, pvt.second);
        sprintf(filename, "%s/%s", dirname, fname);

        // make directory
        if (SD.mkdir(dirname)){
          Serial.print("Created: "); Serial.print(dirname); Serial.println();
        }
            
        Serial.print("Saving: ");    Serial.print(filename); Serial.println();
        
        // Write the CSV file header
        myFile = SD.open(filename, FILE_WRITE);
        
        // if the file opened okay, write to it:
        if (myFile) {
          myFile.println("Time, Elapsed Time (ms),Latitude (deg),Longitude (deg),Height (m),Ground Speed (m/s),Heading (deg)");
          // close the file:
          myFile.close();
        } else {
          // if the file didn't open, print an error:
          SD_ERROR = true;
          Serial.print("State Machine - Error opening file: "); Serial.print(filename); Serial.println();
        }

        // Get the start time for the recording (ms)
        startTime = pvt.iTOW;
           
      } 

      // DO
      digitalWrite(FIX_LIGHT_pin, PPS); // FIX_LIGHT functionality
      
      if (SD_ERROR == false){
        digitalWrite(REC_LIGHT_pin, HIGH);  
      } else {
        digitalWrite(REC_LIGHT_pin, PPS);
      }
      
      DISPLAY_GPS = true;
      SAVE_DATA = true;

      // NEXT STATE
      if ( REC_SW == LOW ){
        state = NOT_RECORDING;
      } else {
        state = RECORDING;
      }  
      last_state = RECORDING;
      break;
      
    default:
      state = NO_FIX;
      last_state = NO_FIX;
      digitalWrite(FIX_LIGHT_pin, LOW); 
      digitalWrite(REC_LIGHT_pin, LOW);
      DISPLAY_GPS = false;
      SAVE_DATA = false;
      break;
  }

  
  // Get GPS data
  if ( processGPS() ) {
    
    // Print GPS data to monitor 
    if ( DISPLAY_GPS == true ){
      Serial.print(" iTOW: ");    Serial.print(pvt.iTOW);
      Serial.print(" #SV: ");     Serial.print(pvt.numSV);
      Serial.print(" fixType: "); Serial.print(pvt.fixType);
      Serial.print(" Date:");     Serial.print(pvt.year); Serial.print("/"); Serial.print(pvt.month); Serial.print("/"); Serial.print(pvt.day); Serial.print(" "); Serial.print(pvt.hour); Serial.print(":"); Serial.print(pvt.minute); Serial.print(":"); Serial.print(pvt.second);
      Serial.print(" lat/lon: "); Serial.print(pvt.lat/10000000.0f, 7); Serial.print(","); Serial.print(pvt.lon/10000000.0f, 7);
      Serial.print(" gSpeed: ");  Serial.print(pvt.gSpeed/1000.0f);
      Serial.print(" heading: "); Serial.print(pvt.heading/100000.0f);
      Serial.print(" hAcc: ");    Serial.print(pvt.hAcc/1000.0f);
      Serial.print(" state: ");   Serial.print(state);
      Serial.println();
    }

    sprintf(timeString, "%02d:%02d:%02d", pvt.hour, pvt.minute, pvt.second);

    // update data file on SD card
    if ( SAVE_DATA == true ){
      
      // open the file. note that only one file can be open at a time,
      // so you have to close this one before opening another.
      myFile = SD.open(filename, FILE_WRITE);
      
      // if the file opened okay, write to it:
      if (myFile) {
        //Serial.print("Writing to test.txt...");
        //myFile.println("testing 1, 2, 3.");
        myFile.print(timeString); myFile.print(","); // Time of day HH:MM:SS
        myFile.print(pvt.iTOW - startTime); myFile.print(","); // elapsed time (ms)
        myFile.print(pvt.lat/10000000.0f, 9); myFile.print(","); // Latitude (degrees)
        myFile.print(pvt.lon/10000000.0f, 9); myFile.print(",");// Longitude (degrees)
        myFile.print(pvt.height/1000.0f, 1); myFile.print(","); // Height (m)
        myFile.print(pvt.gSpeed/1000.0f, 1); myFile.print(",");// Ground Speed (m/s)
        myFile.print(pvt.heading/100000.0f, 1); // Heading (degrees)
        myFile.println();
        // close the file:
        myFile.close();
      } else {
        // if the file didn't open, print an error:
        SD_ERROR = true;
        Serial.print("Loop - Error opening file: "); Serial.print(filename);
        Serial.println();
      }
      
    }
        
  }
}
