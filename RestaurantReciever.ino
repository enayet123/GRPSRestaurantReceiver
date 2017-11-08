#include <SoftReset.h>
#include <SoftwareSerial.h>
#include <Adafruit_Thermal.h>
#include "dilshadwide.h"
#define PRINTER_TX_PIN 12
#define PRINTER_RX_PIN 13
#define SIM_TX_PIN 8
#define SIM_RX_PIN 7
#define SWITCH 11

char getHeader[48]="GET /temp/index.php?id=dilshad HTTP/1.1";
char getHost[32]="Host: www.justyourip.com";
char pageBuffer[768];
int SERIAL_PORT = 19200;
int switchState = 0;
int saveCount = 0;
bool runOnce=false;
bool saveData=false;
bool printOnce=false;
bool resetOnce=false;
int printCount=0;
int printStage=0;
bool orderType=false;
float orderPrice;

SoftwareSerial SIM900(SIM_RX_PIN, SIM_TX_PIN);
SoftwareSerial printerSerial(PRINTER_RX_PIN, PRINTER_TX_PIN);
Adafruit_Thermal printer(&printerSerial);

void setup() {
  pinMode(SWITCH, INPUT);
  Serial.begin(SERIAL_PORT);
  SIM900.begin(SERIAL_PORT);
  delay(1000);
  // check Arduino and SIM900 health
  if (!checkCmd("AT+CGATT?", 3000, 0)) {
    powerToggle(1); // toggle power once
    if (!checkCmd("AT+CGATT?", 3000, 0)) {
      powerToggle(1); //toggle power second time
      if (!checkCmd("AT+CGATT?", 3000, 0)) softRestart(); //restart arduino
    }
  }

  // if SIM900 APN not set
  if (!checkCmd("AT+CIFSR", 3000, 0)) {
    // set APN
    if (!checkCmd("AT+CSTT=\"mobile.o2.co.uk\",\"o2web\",\"password\"", 3000, 1)) {
      if (!checkCmd("AT+CIICR", 5000, 1));
    }
  }
  checkCmd("AT+CIPSPRT=2", 1000, 1);
}

void loop() {
  saveIncoming();
  //buttonCheck();
  printIncoming();
  if (!runOnce) {
    runOnce=true;
    if (!checkCmd("AT+CIPSTART=\"tcp\",\"www.justyourip.com\",\"80\"", 3000, 0)) softRestart();
    runCmd("AT+CIPSEND=100", 3000);
    runCmd(getHeader, 0);
    runCmd(getHost, 0);
    runCmd("Connection: keep-alive", 0);
    SIM900.println((char)26);
    SIM900.println("");
    SIM900.println("AT+CIPCLOSE");
    SIM900.println("");
  }
}

void whileAvailable() {
  char data;
  bool gotData=false;
  long startMillis=millis();
  while (!gotData) {
    unsigned long currentMillis=millis();
    if (currentMillis - startMillis > 10000) break;
    while (SIM900.available()) {
      data=SIM900.read();
      if (data>0) {
        Serial.write(data);
        gotData=true;
      }
    }
  }
}

bool whileAvailableSave() {
  memset(&pageBuffer[0], 0, sizeof(pageBuffer));
  char data;
  int count=0;
  bool gotData=false;
  long startMillis=millis();
  while (!gotData) {
    unsigned long currentMillis=millis();
    if (currentMillis - startMillis > 10000) return false;
    while (SIM900.available()) {
      data=SIM900.read();
      if (data>0) {
        pageBuffer[count]=data;
        count++;
      }
    } if (strlen(pageBuffer)>0) return true;
  }
}

void buttonCheck() {
  switchState = digitalRead(SWITCH);
  if (switchState == HIGH) {
    char order[] = "- This is an item\n- This is another item\n- This is the final item";
    //printMessage(order, 999.99);
    delay(3000);
  }  
}

void printMessage(char string[], float price, bool delivery) {
  printerSerial.begin(SERIAL_PORT);
  printer.begin();
  if (delivery) printHeader(true); else printHeader(false);
  printer.println(string);
  printer.print("\nTotal: ");
  printer.write(0x9c);
  printer.println(price);
  printer.feed(3);
  printer.sleep();
  delay(3000L);
  printer.wake();
  printer.setDefault();
}

void printHeader(bool delivery) {
  printer.printBitmap(384, 200, dilshadwide);
  printer.feed(1);
  printer.inverseOn();
  printer.doubleHeightOn();
  printer.setSize('M');
  printer.justify('C');
  if (!delivery) {
    printer.println("           Collection           ");
  } else { printer.println("            Delivery            "); }
  printer.justify('L');
  printer.doubleHeightOff();
  printer.inverseOff();
  printer.feed(1);
}

void runCmd(char *cmd, int delayTime) {
  SIM900.println(cmd);
  delay(100);
  whileAvailable();
  delay(delayTime);
}

bool checkCmd(char *cmd, int delayTime, bool repeatAllowed) {
  //delayTime = delayTime*10; //delete me
  SIM900.println(cmd);
  Serial.print(cmd);
  delay(100);
  if (!whileAvailableSave()) {
    Serial.println(": TIMED OUT");
    delay(delayTime);
    return false;
  } else if (pageBuffer[strlen(pageBuffer)-3]=='R') {
    Serial.println(": ERROR");
    delay(delayTime);
    return false;
  } else if (pageBuffer[strlen(pageBuffer)-3]=='K') {
    Serial.println(": OK");
    delay(delayTime);
    return true;
  } else {
    if (repeatAllowed) {
      Serial.println(": UNKNOWN ERROR, REPEATING COMMAND");
      Serial.println(pageBuffer);
      delay(delayTime);
      return checkCmd(cmd, delayTime, 1);
    } else {
      Serial.print(": ");
      for (int x=(strlen(cmd)+3);x<strlen(pageBuffer);x++) {
        if (pageBuffer[x]!=0xA) Serial.print(pageBuffer[x]);
      }
      Serial.println("");
      if (pageBuffer[strlen(pageBuffer)-3]=='3') return false;
    } 
  }
}

void powerToggle(int repeat) {
  for (int x=0;x<repeat;x++) {
    digitalWrite(9, HIGH);
    delay(1000);
    digitalWrite(9, LOW);
    delay(20000);
  }
}

void saveIncoming() {
  char data;
  long startMillis=millis();
  if (!resetOnce) {
    memset(&pageBuffer[0], 0, sizeof(pageBuffer));
    resetOnce=true;
  }
  while (!saveData && runOnce) {
    unsigned long currentMillis=millis();
    if (currentMillis - startMillis > 10000) {
      Serial.println("Save Finished");
      //Serial.println(pageBuffer);
      saveData=true;
      break;
    }
    while (SIM900.available()) {
      data=SIM900.read();
      if (data>0) {
        //Serial.write(data);
        pageBuffer[saveCount]=data;
        saveCount++;
      }
    }
  }
}

void printIncoming() {
  char price[8];
  int count=0;
  if (saveData) {
    if (printStage==0) {
      do printCount++; while (pageBuffer[printCount]!='#');
      if (pageBuffer[printCount+1]=='D') orderType=true;
      do printCount++; while (pageBuffer[printCount]!='#');
      printCount=printCount+2;
      while (pageBuffer[printCount]!='.') {
        price[count]=pageBuffer[printCount];
        printCount++;
        count++;
      }
      price[count]=pageBuffer[printCount];
      price[count+1]=pageBuffer[printCount+1];
      price[count+2]=pageBuffer[printCount+2];
      price[count+3]='\0';
      orderPrice = atof(price);
      printCount=printCount+5;
      count=0;
      while (pageBuffer[printCount]!='#') {
        pageBuffer[count]=pageBuffer[printCount];
        printCount++;
        count++;
      }
      pageBuffer[count]='\0';
      printStage=1;
    }
    if (printStage==1) {
      Serial.println(orderPrice);
      Serial.println(pageBuffer);
      printMessage(pageBuffer, orderPrice, orderType);
      printStage=2;
    }
  }
}
