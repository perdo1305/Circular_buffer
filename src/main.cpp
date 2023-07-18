/*@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
                DATALOGGER ESP32 2023
                  Pedro Ferreira
              Eletronics Department
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@*/

#include <Arduino.h>
#include "FS.h"
#include "SD_MMC.h"
#include <SPI.h>
#include <EEPROM.h>
#include <Wire.h>
#include <RTClib.h>
#include "C:\Users\pedro\OneDrive - IPLeiria\Documents\PlatformIO\Projects\Circular_buffer\lib\ESP32universal_CAN-master\ESP32universal_CAN-master\src\ESP32_CAN.h"
//---------------------------------------------------------------------------------------------
#define MAX_SIZE 40      // MAX NUMBER OF DIFFERENT CAN ID'S
#define SIZE_OF_BUFFER 8 // CIRCULAR BUFFER SIZE
#define SDA 33           // i2c rtc pins
#define SCL 32           // i2c rtc pins
#define ON_BOARD_LED 25  // led pcb datalogger
#define BUTTON_PIN 35    // pcb button
#define RXD1 26          // Serie para a telemetria
#define TXD1 27          // Serie para a telemetria
#define simulation 1     // 1 - simulation mode, 0 - normal mode        <<<<<<<<<<-------------
//---------------------------------------------------------------------------------------------
//@@@@@@@@@@@@@@ GLOBAL VARIABLES @@@@@@@@@@@@@@

//  REAL TIME CLOCK THINGS------------------------------
RTC_DS3231 rtc;
char rtc_time[32];
char buffer[20] = {0};        // rtc update function
int lastSecond = -1;          // RTC
unsigned long lastMillis = 0; // RTC
DateTime now;
int milissegundos, seconds, minutes, hours;

// CIRCULAR BUFFER THINGS------------------------------
uint8_t circularBuffer[SIZE_OF_BUFFER];
int bufferLength = 0;
int readIndex = 0;
int writeIndex = 0;
long int lastTime_1 = 0;
long int lastTime_2 = 0;
long int lastTime_3 = 0;
long int lastTime_4 = 0;

// SD CARD THINGS--------------------------------------
char file_name[20]; // main file data name
char NUM_file = 0;

// LED and BUTTONS STATES--------------------------------
boolean Button_State = 0;
boolean LED_25 = 0; // state led pcb
boolean LED_5 = 0;  // state led esp32

// CAN THINGS------------------------------------------
typedef struct
{
  unsigned int id;
  unsigned int module_nr;
  uint8_t data[8];
} CAN_message_t;

CAN_message_t msg[MAX_SIZE];

unsigned int CURRENT_max_index = 0; // index of the last CAN message in the structS
TWAI_Interface CAN1(1000, 21, 22);  // argument 1 - BaudRate,  argument 2 - CAN_TX PIN,  argument 3 - CAN_RX PIN

// FUNCTIONS--------------------------------------------
void CAN_read(void); // read CAN and save in struct
void SD_DATA_STRING(void);
void CLEAR_STRUCT(void);
void RTC_milliseconds(void);
void write_buffer(void);
void read_buffer(void);
int simulateID(void);
int simulateData(void);

void writeFile(fs::FS &fs, const char *path, const char *message)
{

  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{

  // Serial.printf("Appending to file: %s\n", path);
  File file = fs.open(path, FILE_APPEND);
  /*
  if (!file) {
    //Serial.println("Failed to open file for appending");
    return;
  }*/
  if (file.print(message))
  {
    // LED_5 = !LED_5;
    // digitalWrite(5,LED_5);
    // Serial.println("Message appended");
  }
  else
  {
    // digitalWrite(5, LOW);
    //  Serial.println("Append failed");
  }
  file.close();
}

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");

  char str[10] = {0};
  int i = 0;
  while (file.available())
  {
    // Serial.write(file.read());
    // NUM_file=file.read();
    str[i] = file.read();
    i++;
  }
  NUM_file = atoi(str);
}

void Init_Sd_Card()
{ // Setup SDcard
  pinMode(2, INPUT_PULLUP);

  if (!SD_MMC.begin())
  {
    Serial.println("Card Mount Failed");
    return;
  }

  File file = SD_MMC.open("/C");

  readFile(SD_MMC, "/config.txt");
  int y = NUM_file;
  y++;
  char buffer_y[5];
  sprintf(buffer_y, "%d", y);
  writeFile(SD_MMC, "/config.txt", buffer_y);

  if (!file)
  {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    /*
    eeprom_count = EEPROM.read(0);
    eeprom_print = eeprom_count;
    */

    // sprintf(file_name,"/DATA_%d.csv",eeprom_count);
    sprintf(file_name, "/DATA_%d.csv", NUM_file);

    now = rtc.now();
    sprintf(rtc_time, "%02d:%02d:%02d %02d/%02d/%02d\n", now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
    minutes = now.minute();
    hours = now.hour();
    seconds = now.second();

    // writeFile(SD, file_name, Header.c_str());
    writeFile(SD_MMC, file_name, rtc_time);

    /*
    eeprom_count++;
    EEPROM.write(0, eeprom_count);
    EEPROM.commit();
    */
  }
  else
  {
    Serial.println("File already exists");
  }
  file.close();
}

void IRAM_ATTR Ext_INT1_ISR()
{
  // debouncing
  if (millis() - lastTime_3 < 200)
  {
    return;
  }
  lastTime_3 = millis();
  Button_State = !Button_State;
  // digitalWrite(ON_BOARD_LED, Button_State);
  // digitalWrite(5, Button_State);
}

void Task0(void *pvParameters)
{
  for (;;)
  {
    SD_DATA_STRING();

    // vTaskDelay(1);
  }
}

void setup()
{
  Serial.begin(115200);                        // Starts serial monitor
  Serial1.begin(4800, SERIAL_8N1, RXD1, TXD1); // comunicação com a telemetria

  Wire.setPins(SDA, SCL);
  Wire.begin();
  rtc.begin();
  // rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));

  pinMode(ON_BOARD_LED, OUTPUT);
  pinMode(5, OUTPUT);

  Init_Sd_Card(); // initializes the SDcard
  delayMicroseconds(50);
  CLEAR_STRUCT(); // clears the struct
  delay(100);     // just a litle break before it starts to read

  // external interrupt for the button on pin 35 ISR
  attachInterrupt(BUTTON_PIN, Ext_INT1_ISR, RISING);

  xTaskCreatePinnedToCore(
      Task0,            /* Function to implement the task */
      "Task0",          /* Name of the task */
      10000,            /* Stack size in words */
      NULL,             /* Task input parameter */
      tskIDLE_PRIORITY, /* Priority of the task */
      NULL,             /* Task handle. */
      0);               /* Core where the task should run */
}

void loop()
{
  // RECORDING DATA ##############
  if (Button_State && (millis() - lastTime_4 > 500))
  {
    LED_25 = !LED_25;
    digitalWrite(ON_BOARD_LED, LED_25);
    lastTime_4 = millis();
  }
  else if (!Button_State)
  {
    digitalWrite(ON_BOARD_LED, LOW);
  }
  // #############################
  //show the struct in the serial monitor
  //id module_nr data[0] data[1] data[2] data[3] data[4] data[5] data[6] data[7]
  for(int i=0;i<MAX_SIZE;i++)
  {
    if(msg[i].id!=0)
    {
      printf("%d ",msg[i].id);
      printf("%d",msg[i].module_nr);
      for(int j=0;j<8;j++)
      {
        printf("%d ",msg[i].data[j]);
      }
      printf("\n");
    }
  }
  printf("___");
  
  CAN_read();
}

void CAN_read(void)
{
  // piscar led 5 cada vez que recebe um pacote CAN
  LED_5 = !LED_5;
  digitalWrite(5, LED_5);
  printf("CAN packet received\n");

  int ID = 0;
  if (simulation)
  {
    ID = simulateID();
  }
  else
  {
    ID = CAN1.RXpacketBegin();
  }

  boolean found = false;
  unsigned int index;

  for (int i = 0; i < MAX_SIZE; i++)
  {
    if (msg[i].id == ID)
    {
      index = i;
      found = true;
      break;
    }
  }

  if (!found)
  {
    CURRENT_max_index++;
    index = CURRENT_max_index;
    msg[index].id = ID;
  }

  if (simulation)
  {
    for (int j = 0; j < 8; j++)
    {
      msg[index].data[j] = simulateData();
    }
  }
  else
  {
    for (int j = 0; j < 8; j++)
    {
      msg[index].data[j] = CAN1.RXpacketRead(j);
    }
  }
}

void SD_DATA_STRING()
{
  // take data from struct and put in a string
  // save the data in this format: TIME,ID,MODULE_NR,DATA[0],DATA[1],DATA[2],DATA[3],DATA[4],DATA[5],DATA[6],DATA[7]
  char dataMessage[1000];
  dataMessage[0] = '\0';

  for (int i = 0; i < MAX_SIZE; i++)
  {
    if (msg[i].id != 0)
    {
      sprintf(dataMessage + strlen(dataMessage), "%d;", msg[i].id);
      sprintf(dataMessage + strlen(dataMessage), "%d;", msg[i].module_nr);
      for (int j = 0; j < 8; j++)
      {
        sprintf(dataMessage + strlen(dataMessage), "%d;", msg[i].data[j]);
      }
      // appendFile(SD_MMC, file_name, dataMessage);
    }
  }
  appendFile(SD_MMC, file_name, dataMessage);
}

void CLEAR_STRUCT()
{
  // clear struct
  for (int i = 0; i < MAX_SIZE; i++)
  {
    msg[i].id = 0;
    msg[i].module_nr = 0;
    for (int j = 0; j < 8; j++)
    {
      msg[i].data[j] = 0;
    }
  }
}

void RTC_milliseconds()
{
  // diferenca de tempo
  long int myMillis = millis();

  long int diferenca = myMillis - lastMillis;
  // converter para horas minutos segundos e milisegundos
  long int diferenca_horas = diferenca / 3600000;
  long int diferenca_minutos = (diferenca % 3600000) / 60000;
  long int diferenca_segundos = ((diferenca % 3600000) % 60000) / 1000;
  long int diferenca_milisegundos = ((diferenca % 3600000) % 60000) % 1000;

  // somar
  seconds += diferenca_segundos;
  minutes += diferenca_minutos;
  hours += diferenca_horas;
  milissegundos += diferenca_milisegundos;
  if (milissegundos >= 1000)
  {
    milissegundos -= 1000;
    seconds++;
  }
  if (seconds >= 60)
  {
    seconds -= 60;
    minutes++;
  }
  if (minutes >= 60)
  {
    minutes -= 60;
    hours++;
  }
  if (hours >= 24)
  {
    hours -= 24;
  }
  sprintf(buffer, "%02d:%02d:%02d:%03d\n", hours, minutes, seconds, milissegundos);
  lastMillis = myMillis;
}

void write_buffer()
{
  // write to buffer every 10ms millis
  if (millis() - lastTime_1 >= 10)
  {
    if (bufferLength == SIZE_OF_BUFFER)
    {
      printf("Buffer is full\n");
    }
    else
    {
      // readCAN(&circularBuffer[writeIndex]);
      bufferLength++;
      writeIndex++;
    }
    lastTime_1 = millis();
  }
}

void read_buffer()
{

  // read from buffer every 30ms millis
  if (millis() - lastTime_2 >= 30)
  {
    char dataMessage[1000];
    dataMessage[0] = '\0';

    if (bufferLength == 0)
    {
      printf("Buffer is empty\n");
    }
    else
    {
      
      for (int i = 0; i < bufferLength; i++)
      {
        
        sprintf(dataMessage + strlen(dataMessage), "%d,", circularBuffer[readIndex]);
        
        bufferLength--;
        readIndex++;
      }
      
      appendFile(SD_MMC, file_name, dataMessage);
    }
    lastTime_2 = millis();
  }
}

int simulateID()
{
  int id = random(1, 30);
  return id;
}

int simulateData()
{
  int data = random(0, 255);
  return data;
}