#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <Servo.h>

// Pin Definitions
#define BTHC05_PIN_TXD  1         // Bluetooth
#define BTHC05_PIN_RXD  0         // Bluetooth
#define DS18B20_PIN_DQ  2         // Temperature sensor
#define ETHERNETMODULE_PIN_CS 4   // Ethernet
#define ETHERNETMODULE_PIN_INT  6 // Ethernet
#define LDR_PIN_SIG A3            // LDR sensor
#define LEDR_PIN_VIN  5           // LED
#define PUSHBUTTON_PIN_2  7       // Pushbutton
#define RFID_PIN_RST  8           // RFID
#define RFID_PIN_SDA  9           // RFID
#define SERVOSM_PIN_SIG A1        // Servo motor

byte mac[] = {
  0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54
  };
IPAddress myIp(192, 168, 1, 199);
IPAddress myDns(1, 1, 1, 1);
IPAddress myGateway(192, 168, 1, 1);
IPAddress mySubnet(255, 255, 255, 0);

EthernetClient client;
char server[] = "smart-homeiot.herokuapp.com"; // also change the Host line in httpRequest()

OneWire myWire(DS18B20_PIN_DQ);
unsigned int temperature = 25;

// For sending temperature data regularly
unsigned long lastConnectionTime = 0; // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 300000; // delay between updates, in milliseconds (5 minutes)

LiquidCrystal_I2C lcd(0x27, 16, 2); // 0x3f for some devices

MFRC522 rfid(RFID_PIN_SDA, RFID_PIN_RST); // select, reset

Servo servo;

void setup() {
  SPI.begin(); // SPI communication
  Ethernet.init(ETHERNETMODULE_PIN_CS); // Configuring CS (Chip Select) pin

  // start the Ethernet connection:
  // Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    // Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      // Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); // do nothing, no point running without Ethernet hardware
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      // Serial.println("Ethernet cable is not connected.");
    }
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, myIp, myDns);
    // Serial.print("My IP address: ");
    // Serial.println(Ethernet.localIP());
  } else {
    // Serial.print("  DHCP assigned IP ");
    // Serial.println(Ethernet.localIP());
  }
  // give the Ethernet shield a second to initialize:
  delay(1000);

  Serial.begin(38400); // For programming BT device
  // HC-05
  // 1234
  // 9600
  // Slave
  Serial.println("AT+ROLE=1"); // To work in master mode
  delay(1000);
  Serial.println("AT+BIND=98d3,31,b3739f"); // MAC of combi (example MAC address)
  delay(1000);
  Serial.println("AT+CMODE=0"); // Connect only to the combi
  Serial.end();
  Serial.begin(9600);

  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.display();
  lcd.noCursor();
  lcd.noBlink();

  rfid.PCD_Init(); // RC522 initialization

  pinMode(PUSHBUTTON_PIN_2, INPUT); // Pushbutton
  pinMode(LEDR_PIN_VIN, OUTPUT); // Red LED

  pinMode(SERVOSM_PIN_SIG, OUTPUT);
  servo.attach(SERVOSM_PIN_SIG);
  servo.write(180); // Initial position of the lock
}

void loop() {
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  /*if (client.available()) {
    char c = client.read();
    Serial.write(c);
  }*/

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) { // A card is present
    // Convert card id to a string
    char cardId[16];
    memcpy(cardId, rfid.uid.uidByte, sizeof(rfid.uid.uidByte));
    cardId[sizeof(rfid.uid.uidByte)] = '\0';
    
    if (digitalRead(PUSHBUTTON_PIN_2) == 1) { // Card register mode
      char route[] = "/registercard?id=";
      httpRequest(route, cardId);
      short int res;
      if (client.available()) {
        int res = parseResponse();
        if (res == 1) {
          lcd.setCursor(0, 1);
          lcd.print("Registered.");
          delay(3000); //3 seconds
          lcd.setCursor(0, 1);
          lcd.print("           ");
        } else {
          lcd.setCursor(0, 1);
          lcd.print("Not registered.");
          delay(3000); //3 seconds
          lcd.setCursor(0, 1);
          lcd.print("               ");
        }
      }
    } else { // Card authentication mode
      char route[] = "/authorizecard?id=";
      httpRequest(route, cardId);
      short int res;
      if (client.available()) {
        int res = parseResponse();
        if (res == 1) {
          // Authorization successful
          servo.write(0); // Unlock
          lcd.setCursor(0, 1);
          lcd.print("Authorized.");
          delay(5000); // 5 seconds
          lcd.setCursor(0, 1);
          lcd.print("           ");
          servo.write(180); // Lock
        } else {
          servo.write(180); // Lock
          lcd.setCursor(0, 1);
          lcd.print("Not authorized.");
          delay(3000); // 3 seconds
          lcd.setCursor(0, 1);
          lcd.print("               ");
        }
      }
    }
    rfid.PICC_HaltA();
  }

  // Automated lights
  int lights = analogRead(LDR_PIN_SIG);
  if (lights > 600) { // Too high
    digitalWrite(LEDR_PIN_VIN, LOW);
  } else if (lights <= 600 && lights > 500) { // Moderate
    ; // do nothing to prevent unstable switching
  } else { // Too low
    digitalWrite(LEDR_PIN_VIN, HIGH);
  }
  
  // if 5 minutes have passed since the last connection, then connect again and send temperature:
  if (millis() - lastConnectionTime > postingInterval || lastConnectionTime == 0) {
    lastConnectionTime = millis(); // note the time that the connection was made
    char route[] = "/temperature?temperature=";
    char data[5];
    snprintf(data, sizeof(data), "%f", currentTemperature());
    httpRequest(route, data); // Sending requests
    short int res;
    if (client.available()) {
      int res = parseResponse();
      if (res == 1) { // Start combi
        Serial.println("Start");
      } else if (res == 0) { // Close combi
        Serial.println("Close");
      } else return; // Parsing error
      /* 
       * if (Serial.available()>0) {
       *  data = Serial.read();
       * }
      */
    }
    lcd.setCursor(0, 0);
    lcd.print("Temperature: ");
    lcd.setCursor(13, 0);
    lcd.print(temperature);
  }
}

// this method makes a HTTP connection to the server:
void httpRequest(char route[], char data[]) {
  // close any connection before send a new request.
  client.stop();
  
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    // Serial.println("connecting...");
    // send the HTTP GET request:
    client.print("GET ");
    client.print(route);
    client.print(data);
    client.println(" HTTP/1.1");
    
    client.print("Host: ");
    client.println(server);
    
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();
  } else {
    // if you couldn't make a connection:
    // Serial.println("connection failed");
  }
  delay(1000);
}

int parseResponse() {
  int s = client.parseInt(); // parse status code
  int res = -1;
  char str[] = "1.1 vegur\n"; // our server's specific header
  if (s == 200 && client.find(str)) {
    int res = client.parseInt();
  }
  return res;
}

float currentTemperature() {
  byte data[12];
  byte address[8];
  
  if (!myWire.search(address)) { // No more addresses
    myWire.reset_search();
    delay(250);
    return 25;
  }
  
  if (OneWire::crc8(address, 7) != address[7]) { // CRC is not valid
    return 25;
  }
  
  if (address[0] != 0x28) { // Checks if the sensor model is correct (DS18B20)
    return 25;
  }
  
  myWire.reset();
  myWire.select(address);
  myWire.write(0x44, 1); // start conversion with parasite power on
  delay(750);
  myWire.reset();
  myWire.write(0xBE); // read scratchpad memory
  
  for (short int i = 0; i < 9; ++i) { // Reading sensor (9 bytes)
    data[i] = myWire.read();
  }
  
  int16_t raw = (data[1] << 8) | data[0];
  byte cfg = (data[4] & 0x60);
  if (cfg == 0x00) {
    raw = raw & ~7; // 9 bit resolution
  } else if (cfg == 0x20) {
    raw = raw & ~3; // 10 bit resolution
  } else if (cfg == 0x40) {
    raw = raw & ~1; // 11 bit resolution
  } // else default is 12 bit resolution
  
  float celsius = (float)raw / 16.0;
  return celsius;
}
