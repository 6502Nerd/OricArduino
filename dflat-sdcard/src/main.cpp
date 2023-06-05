/*
  Oric dflat file server

  This is a mini file server using the Pro Micro / Leonardo unit
  Connects to the Oric-1/Atmos printer port as follows;
  Printer Pin #     Arduino Pin #     Function
  3                 2 IN              Data bit 0: Clock
  5                 3 IN/OUT          Data bit 1: Nibble data in/out
  7                 4 IN/OUT          Data bit 2: Nibble data in/out
  9                 5 IN/OUT          Data bit 3: Nibble data in/out
  11                6 IN/OUT          Data bit 4: Nibble data in/out
  13                7 OUT             Data bit 5: Arduino ready signal
  15                8 IN              Data bit 6: Select (low=active)
  17                9 IN              Data bit 7: Select (low=active)
  1                 21                Strobe (low=active)
  19                20                ACK (not used)
  20                GND               Ground

  An SD card needs to be present on the SPI bus as follows:
  Arduino Pin #     Function
  10                CS
  11                MOSI
  12                MISO
  13                CLK

  Theory of operation:
  The file server is only active when STB and Select lines are low.
  If at any time these lines indicate not active, the file server
  is in mode 'waiting for select'.  Any open files are close.

  Once selected, the Arduino expects a command byte as follows;
  Command byte      Function
  0<filename>       Open file <filename> for reading
  1<filename>       Open file <filename> for writing (file is created/overwritten)
  2                 Close currently open file
  3<filename>       Delete file <filename>
  4                 Directory listing of root folder
  <filename> indicates a stream of nul terminated bytes providing a filename.
  No folder paths are expected - keep files short, ideally 8.3 standard.

  The Oric client software only tries to send and receive bytes if the Ready signal
  (Arduino pin 7) is high.  If the Arduino is not ready for too long then the Oric
  times out and de-selects the file server resulting in it going back to waiting for
  select.

  This mechanism is used by the Oric to test for errors; When reading or deleting a file,
  if file does not exist, file server goes busy for 500ms so the Oric times out.
  When reading or writing bytes, the Oric will de-deselect the file server when done.

  Data is sent to/from the Oric nibble (4 bits) at a time, synchronised by the Clock pin
  which is controlled by the Oric.  Data is only valid when the Clock goes from low to
  high.  Clearly two clock transitions from low to high are required for a single byte.
  The Oric controls the speed of the clock and has been set during trial and error
  experiments - it appears the Arduino can keep up with sensing signals and responding
  to actions with a clock rate of around 7.5KHz - this results in a real-world speed
  of around 2KB/s transfer speed (high-res screen loads in less than 4 seconds).

  No claim this code is either the most efficient or robust, but seems to work ok!

*/

// include the SD library:
#include <SPI.h>
#include <SD.h>

// Include project defs
#include "dflatsd.h"

// Uncomment next line if using USB serial to get some debug stuff out
//#define DFLATSER

File myFile, dir;
char path[100];
char dflat_fqname[120];
char dflat_fname[20];

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// MKRZero SD: SDCARD_SS_PIN
const int chipSelect = 10;

// state machine
void (*state)();
void dflat_initialise();
void dflat_open_read();
void dflat_open_write();
void dflat_close();
void dflat_directory();
void dflat_delete();
void dflat_wait_for_select();
void dflat_get_command();
void dflat_save_byte();
void dflat_load_byte();
void dflat_send_cr();



// Initial state when not selected
void dflat_initialise() {
  // Set strobe pins to input
  pinMode(PIN_STB, INPUT);

  // Set port A pins to input to Arduino
  // to not intefere with 8912 operation
  pinMode(PIN_PA0, INPUT);
  pinMode(PIN_PA1, INPUT);
  pinMode(PIN_PA2, INPUT);
  pinMode(PIN_PA3, INPUT);
  pinMode(PIN_PA4, INPUT);
  pinMode(PIN_PA5, INPUT);
  pinMode(PIN_PA6, INPUT);
  pinMode(PIN_PA7, INPUT);
  pinMode(PIN_ACK, INPUT);

  if(myFile) {
#ifdef DFLATSER
    Serial.print("Closing : ");
    Serial.println(myFile.name());
  #endif
    myFile.close();
  }
  // wait for strobe
  state=dflat_wait_for_select;
}

// Is sd select asserted?
bool dflat_sd_cs() {
  // If STB, PA6 and PA7 low then selected
  if((digitalRead(PIN_STB)+digitalRead(PIN_PA6)+digitalRead(PIN_PA7))==0)
    return true;
  // if not selected low then always reset state
  state=dflat_initialise;
  return false;
}


// If selected then set port pins
// and wait for command
void dflat_wait_for_select() {
#ifdef DFLATSER
  Serial.println("Waiting for select");
#endif
  if (dflat_sd_cs()) {
    state=dflat_get_command;
    // Allow PA5 to be an output and indicate we're ready
    pinMode(PIN_PA5, OUTPUT);
    READY;
  }
}


void dflat_set_write() {
  pinMode(PIN_PA1, OUTPUT);
  pinMode(PIN_PA2, OUTPUT);
  pinMode(PIN_PA3, OUTPUT);
  pinMode(PIN_PA4, OUTPUT);

}

void dflat_set_read() {
  pinMode(PIN_PA1, INPUT);
  pinMode(PIN_PA2, INPUT);
  pinMode(PIN_PA3, INPUT);
  pinMode(PIN_PA4, INPUT);

}

// Read a byte
int dflat_read_byte() {
  int a=0;

  if(!dflat_sd_cs())
    return 0;

  dflat_set_read();

  // clock has to be low first
  do {} while(digitalRead(PIN_PA0)==1);
    // wait for clock high before sampling
  do {} while(digitalRead(PIN_PA0)==0);

  // Now sample into lower nibble of rx byte
  a=digitalRead(PIN_PA1)+(digitalRead(PIN_PA2)<<1)+(digitalRead(PIN_PA3)<<2)+(digitalRead(PIN_PA4)<<3);

  // clock has to be low first
  do {} while(digitalRead(PIN_PA0)==1);
    // wait for clock high before sampling
  do {} while(digitalRead(PIN_PA0)==0);

  // Now sample into upper nibble of rx byte
  a=a+(digitalRead(PIN_PA1)<<4)+(digitalRead(PIN_PA2)<<5)+(digitalRead(PIN_PA3)<<6)+(digitalRead(PIN_PA4)<<7);

  return a;
}

// Write a byte
void dflat_write_byte(int a) {
  if(!dflat_sd_cs())
    return;
    
  dflat_set_write();
  // clock has to be low first
  do {} while(digitalRead(PIN_PA0)==1);

  // Now write lower nibble of tx byte
  digitalWrite(PIN_PA1,a & 0x01);
  digitalWrite(PIN_PA2,a & 0x02);
  digitalWrite(PIN_PA3,a & 0x04);
  digitalWrite(PIN_PA4,a & 0x08);

   // wait for clock high before writing
  do {} while(digitalRead(PIN_PA0)==0);

  // clock has to be low first
  do {} while(digitalRead(PIN_PA0)==1);

  // Now write upper nibble of tx byte
  digitalWrite(PIN_PA1,a & 0x10);
  digitalWrite(PIN_PA2,a & 0x20);
  digitalWrite(PIN_PA3,a & 0x40);
  digitalWrite(PIN_PA4,a & 0x80);


  // wait for clock high before writing
  do {} while(digitalRead(PIN_PA0)==0);

}

void dflat_get_filename() {
  int a;
  char *ptr=dflat_fname;

  do
  {
    if(!dflat_sd_cs())
      return;
    
    READY;
    a=dflat_read_byte();
    BUSY;
    *ptr++=a;
  } while (a!=0);
#ifdef DFLATSER
  Serial.println(dflat_fname);
#endif
}

// Used as a dummy when opening directory
void dflat_send_cr() {
#ifdef DFLATSER
  Serial.println("Opening directory");
#endif
  // Send $ first
  READY;
  dflat_write_byte('$');
  BUSY;
  // Send 9 bytes header
  for (int i=0;i<9;i++) {
    READY;
    dflat_write_byte('X');
    BUSY;
  }
  // Send nul terminated filename
  READY;
  dflat_write_byte(0x0);
  BUSY;
  // Send Block 00
  READY;
  dflat_write_byte(0x0);
  BUSY;
  READY;
  dflat_write_byte(0x0);
  BUSY;
  // Send CR as block of 256 bytes
  for (int i=0;i<256;i++) {
    READY;
    dflat_write_byte(0xd);
    BUSY;
  }
  // If going back to root
  if (strcmp(dflat_fname,"/")==0) {
    strcpy(path,"/");         // Initialie as root
  }
  else {
    strcat(path,myFile.name());
    strcat(path,"/");
  }
  READY;
  dflat_close();

}


void dflat_open_read() {
#ifdef DFLATSER
  Serial.print("Read filename: ");
#endif
  dflat_get_filename();
  strcpy(dflat_fqname,path);
  strcat(dflat_fqname,dflat_fname);
#ifdef DFLATSER
  Serial.print("FQ filename: ");
  Serial.println(dflat_fqname);
#endif
  myFile = SD.open(dflat_fqname,FILE_READ);
  if (!myFile) {
#ifdef DFLATSER
    Serial.println("File Error");
#endif
    state=dflat_initialise;
    return;
  } 
#ifdef DFLATSER
  Serial.print("SD card name: ");
  Serial.println(myFile.name());
#endif
  if (myFile.isDirectory())
    state=dflat_send_cr;
  else
    state=dflat_load_byte;
}

void dflat_open_write() {
#ifdef DFLATSER
  Serial.print("Write filename: ");
#endif
  dflat_get_filename();
  strcpy(dflat_fqname,path);
  strcat(dflat_fqname,dflat_fname);
  if(SD.exists(dflat_fqname))
    SD.remove(dflat_fqname);
  myFile = SD.open(dflat_fqname,O_CREAT|O_WRITE);
  if (!myFile) {
#ifdef DFLATSER
    Serial.println("File Error");
#endif
    state=dflat_initialise;
    return;
  }
  state=dflat_save_byte;
}

void dflat_save_byte() {
  int a;

  READY;
  a=dflat_read_byte();
  BUSY;
  if(dflat_sd_cs()) {
    myFile.write((uint8_t)a);
  }
  else {
    dflat_close();
  }
}

void dflat_load_byte() {
  int a;

  if(dflat_sd_cs()) {
    a=myFile.read();
    READY;
    dflat_write_byte(a);
    BUSY;
  }
  else {
    dflat_close();
  }
}

void dflat_close() {
  myFile.close();
  state=dflat_initialise;
}

void dflat_directory() {
  char dirStr[40];
  BUSY;

  dir=SD.open(path);     // Current path
  dir.rewindDirectory();

#ifdef DFLATSER
  Serial.print("Directory ");
  Serial.println(path);
#endif
  while (true) {
    BUSY;
    myFile=dir.openNextFile();
    if (!myFile) {
      // no more files
      break;
    }
//    if (!myFile.isDirectory()) {
    if (1) {
      sprintf(dirStr,"%-13s%-5u ",myFile.name(),(unsigned int)myFile.size());
      char *ptr=dirStr;
      while(*ptr!=0) {
        char c=*ptr++;
        READY;
        dflat_write_byte(c);
          if(!dflat_sd_cs())
            return;    
      }
//      dflat_write_byte(0xd);    
    }
    myFile.close();
  }
  dir.close();
  READY;
  dflat_write_byte(0xd);    
  dflat_write_byte(0x0);
  state=dflat_initialise;
}

void dflat_delete() {
#ifdef DFLATSER
  Serial.println("Delete");
#endif
  dflat_get_filename();
  strcpy(dflat_fqname,path);
  strcat(dflat_fqname,dflat_fname);
  BUSY;
  if(!SD.exists(dflat_fqname)) {
#ifdef DFLATSER
    Serial.println("File Error");
#endif
    state=dflat_initialise;
    delay(500);
    return;
  }
  SD.remove(dflat_fqname);
  READY;
  state=dflat_initialise;
}

void dflat_get_command() {
  int a;

  READY;
  a=dflat_read_byte();
  BUSY;
  if(!dflat_sd_cs())
    return;    
  switch (a) {
    case cmd_openread:
      state=dflat_open_read;
      break;
    case cmd_openwrite:
      state=dflat_open_write;
      break;
    case cmd_close:
      state=dflat_close;
      break;
    case cmd_delete:
      state=dflat_delete;
      break;
    case cmd_dir:
      state=dflat_directory;
      break;    
    default:
#ifdef DFLATSER
      Serial.println("Unknown command");
#endif
      state=dflat_initialise;
      BUSY;
      delay(500);
      break;
  }
}



void setup() {
#ifdef DFLATSER
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("dflat sd card server");
  Serial.print("Initializing SD card...");
#endif

  if (!SD.begin(chipSelect)) {
#ifdef DFLATSER
    Serial.println("initialization failed!");
#endif
    while (1);
  }
#ifdef DFLATSER
  Serial.println("initialization done.");
#endif
  strcpy(path, "/");
}

void loop(void) {
  state=dflat_initialise;
  while(1) {
    (*state)();         // Call the function;
    // If ever the card is not selected go back to initialisation state
    if (!dflat_sd_cs()) {
      state=dflat_initialise;
    }
  }
}
