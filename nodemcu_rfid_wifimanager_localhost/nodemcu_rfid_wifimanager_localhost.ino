/****************************************** CODE FOR NODEMCU: AES-HMAC *******************************************/
/*                                                                                                               */
/* This is the Arduino-based code necessary to implement the RFID data sending through MQTT to the Python client */
/*                                                                                                               */
/* The config.json file stored in the SPIFFS system has the following elements:                                  */
/*                                                                                                               */
/*     - mqtt_server: Domain of the MQTT server                                                                  */
/*                                                                                                               */
/*     - key: Password for AES encryption and HMAC authentication                                                */
/*                                                                                                               */
/*     - nodeMCUClient: Device ID for the MQTT communication                                                     */
/*                                                                                                               */
/*     - userMQTT: Username for this device at the MQTT communication                                            */
/*                                                                                                               */
/*     - passwordMQTT: Password for this device at the MQTT communication                                        */
/*                                                                                                               */
/*****************************************************************************************************************/
 

/********************************************* LIBRARIES AND DEFINES *********************************************/

#include <FS.h> // Manage the nodeMCU filesystem
#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>         //https://github.com/knolleary/pubsubclient - //http://pubsubclient.knolleary.net/api.html
#include <ebase64.h>
#include <AES_config.h>
#include <AES.h>
#include <Crypto.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <SPI.h>
#include "MFRC522.h"

#define RST_PIN 0 // RST-PIN for RC522 - RFID 
#define SS_PIN 2  // SDA-PIN for RC522 - RFID  
#define RESET_PIN 16  // -> CHANGE TO D3 - GPIO0
#define RED_LED 4
#define GREEN_LED 5
#define BEEP 15

#define KEY_LENGTH 16 

/*****************************************************************************************************************/

/************************************************ GLOBAL VARIABLES ***********************************************/

/*  Counters and flags  */

int cnt=0; // Counter used to assure that one RFID card can't be considered twice in a time interval
int cnt_ack = 0; // Counter used to manage the possibility of the Python client to be disconnected

int flag_init = 1; // Flag that allows to manage the starting and end of a session
int flag_ack = 0; // Flag to manage the ACK counter

/*  Variables for the config.json file  */

char mqtt_server[15];
char key[20];
char nodeMCUClient[15];
char userMQTT[15];
char passwordMQTT[15];

/*  AES-HMAC-Base64 variables  */

byte key_hmac[KEY_LENGTH]={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
byte authCode[SHA256HMAC_SIZE];
char authCodeb64[200];
char rfid_b64[200];
AES aes;
char iv_py[20]; // This variable stores the session ID sent from the Python client, to compute the HMAC and it is also used as IV for the AES encryption
char comp_info[100];

/*  Buffers  */

char buf[512];
char buf_init[20];
char buf_hmac[256];
char buf_acceso[256];

/*  Other variables  */

WiFiManager wifiManager;
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
WiFiClient espClient;
PubSubClient client(espClient);
const int mqtt_port = 1883;
char rfidstr[15];
bool shouldSaveConfig = true;
String currentCard = "";
String currentCardold = "";

/*****************************************************************************************************************/

/******************************************** CALLBACKS AND FUNCTIONS ********************************************/

/*  Callback notifying us of the need to save config  */

void saveConfigCallback () { 
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/*  Function to convert a char array to a byte array  */

void CharToByte(char* chars, byte* bytes, unsigned int count){
    for(unsigned int i = 0; i < count; i++)
        bytes[i] = (byte)chars[i];
}

/*  Function utilized to carry out the encryption process --> out = Base64(AES(Base64(in)))  */

void encrypt_rfid(char rfidstr[], char iv_py[])
{
  byte cipher_rfid[1000];
  
  int len = base64_encode(rfid_b64, rfidstr, strlen(rfidstr));
  aes.do_aes_encrypt((byte *)rfid_b64, len, cipher_rfid, (byte *)key, 128, (byte *)iv_py);
  base64_encode(rfid_b64, (char *)cipher_rfid, aes.get_size());
}

/*  Routine employed to read the buffer where the RFID ID is stored and transform it to ASCII code  */

void dump_byte_array(byte *buffer, byte bufferSize) {
  
  String rfid;
  char s[100];
 
  for (byte i = 0; i < bufferSize; i++) {
    //Convert from byte to Hexadecimal
    sprintf(s, "%s%x", buffer[i] < 0x10 ? "0" : "", mfrc522.uid.uidByte[i]);
    //Concatenate msg
    strcat( &rfidstr[i] , s);
  }
  
  currentCard = rfidstr;
  Serial.println(" ");
  Serial.print("RFID: " + String(rfidstr));

  rfid = String(rfidstr).substring(strlen(rfidstr)-8,strlen(rfidstr));
   
  Serial.println("");
  Serial.println("Verifing...");
  Serial.println("");
}

/*  Function used to connect the nodeMCU to the MQTT server  */

void conectMqtt() {
  while (!client.connected()) {
    Serial.print("ConnectingMQTT ...");
    if (client.connect(nodeMCUClient,userMQTT,passwordMQTT)){  //"esp8266","mqtt_rfid","password"
      Serial.println("Connected");
      //Subscribing to topics
      client.subscribe("response");
      client.subscribe("ack");
    } else {
      digitalWrite(RED_LED, HIGH);
      Serial.print("Error");
      Serial.print(client.state());
      Serial.println("Retry in 5 seconds");
    }
    delay(500);
    digitalWrite(RED_LED, LOW);
    delay(500);
  }
}

/*  Callback called when a MQTT message arrives, to distinguish bewteen topics to make distinct actions  */

void callback(char* topic, byte* payload, unsigned int length) {
  
  currentCardold = currentCard;
  currentCard = "";
  SHA256HMAC hmac(key_hmac, KEY_LENGTH);  
  Serial.println();
  Serial.print("Welcome [");
  Serial.print(topic);
  Serial.print("]: ");
  String mensagem = "";
  char * id;
  char * msg;
  
  //Convert msg from byte to string
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  
  mensagem.toCharArray(comp_info, sizeof comp_info);

  Serial.println(mensagem);
  Serial.println();
  Serial.println(comp_info);
  Serial.println();

  id = strtok (comp_info,"###");
  Serial.println("Second time:");
  Serial.println(comp_info);
  Serial.println();
  msg = strtok (NULL, "###");
  
  Serial.println(id);
  Serial.println();
  Serial.println(msg);
  Serial.println();

  if(strcmp(id, nodeMCUClient) == 0){
      if(strcmp(topic, "response") == 0){
          cnt = 0;
          if(strcmp(msg, "NOAUTH") == 0){
              digitalWrite(RED_LED, HIGH);
              delay(500);
              digitalWrite(RED_LED, LOW);
              delay(500);
              digitalWrite(RED_LED, HIGH);
              delay(150);
              digitalWrite(RED_LED, LOW);
              delay(150);
          }
          else {
              if(strcmp(msg, "check_in") == 0){
                  digitalWrite(GREEN_LED, HIGH);
                  tone(BEEP, 1630);
                  delay(150);
                  tone(BEEP, 1930);
                  delay(100);
                  noTone(BEEP);
                  delay(1000);
                  digitalWrite(GREEN_LED, LOW);
                  delay(250);
              } else {
                  if(strcmp(msg, "check_out") == 0){
                      digitalWrite(GREEN_LED, HIGH);
                      tone(BEEP, 1930);
                      delay(150);
                      tone(BEEP, 1630);
                      delay(100);
                      noTone(BEEP);
                      delay(1000);
                      digitalWrite(GREEN_LED, LOW);
                      delay(250);
                  } else {
                      digitalWrite(RED_LED, HIGH);
                      tone(BEEP, 2030);
                      delay(150);
                      tone(BEEP, 2030);
                      delay(100);
                      noTone(BEEP);
                      delay(1000);
                      digitalWrite(RED_LED, LOW);
                      delay(250);
                  }
              }
          }
      } else if(strcmp(topic, "ack") == 0){
          if(strcmp(msg, "otherID") == 0){
            Serial.println("OTHER ID");
            Serial.println();
            ESP.reset();
          } else {
            cnt_ack = 0;
            cnt=0;
            flag_ack = 0;
            strcpy(iv_py,msg);
            Serial.println(" ");
            Serial.println(iv_py);
            Serial.println(" ");
            hmac.doUpdate(iv_py,strlen(iv_py));
            hmac.doFinal(authCode);
            Serial.println("AUTH CODE");
            Serial.println();
          
            for (byte i=0; i < SHA256HMAC_SIZE; i++)
            {
              Serial.print("0123456789abcdef"[authCode[i]>>4]);
              Serial.print("0123456789abcdef"[authCode[i]&0xf]);
            }
            Serial.println(" ");
            flag_init = 0;
          }
      } else Serial.println("ELSE " + mensagem);
  }
}

/*****************************************************************************************************************/

/************************************************* SETUP FUNCTION ************************************************/

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);                      
  Serial.println();

  pinMode(RESET_PIN, INPUT); 
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  Serial.println("MFRC522 Initialized");

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(key, json["key"]);
          strcpy(nodeMCUClient, json["nodeMCUClient"]);
          strcpy(userMQTT, json["userMQTT"]);
          strcpy(passwordMQTT, json["passwordMQTT"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  
  Serial.println(mqtt_server);
  Serial.println(key);
  Serial.println(nodeMCUClient);
  Serial.println(userMQTT);
  Serial.println(passwordMQTT);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length

  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", mqtt_server, 14);
  WiFiManagerParameter custom_key("key", "AES key", key, 19);
  WiFiManagerParameter custom_nodeMCUClient("nodeMCUClient", "NodeMCU Client", nodeMCUClient, 14);
  WiFiManagerParameter custom_userMQTT("userMQTT", "MQTT Username", userMQTT, 14);
  WiFiManagerParameter custom_passwordMQTT("passwordMQTT", "MQTT Password", passwordMQTT, 14);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  //WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_key);
  wifiManager.addParameter(&custom_nodeMCUClient);
  wifiManager.addParameter(&custom_userMQTT);
  wifiManager.addParameter(&custom_passwordMQTT);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(180);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration

  if (!wifiManager.autoConnect("AutoConnectAP", "12345678")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } else {
    Serial.println("password:12345678");
  }

  //if you get here you have connected to the WiFi
  Serial.println("*CONNECTED*");

  //read updated parameters

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(key, custom_key.getValue());
  strcpy(nodeMCUClient, custom_nodeMCUClient.getValue());
  strcpy(userMQTT, custom_userMQTT.getValue());
  strcpy(passwordMQTT, custom_passwordMQTT.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

    json["mqtt_server"] = mqtt_server;
    json["key"] = key;//"M1k3y1sdAb3St0n3";
    json["nodeMCUClient"] = nodeMCUClient;
    json["userMQTT"] = userMQTT;
    json["passwordMQTT"] = passwordMQTT;   

    Serial.println("+++++++++++++++++");

    Serial.println(nodeMCUClient);
    Serial.println(userMQTT);
    Serial.println(passwordMQTT);

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  Serial.println((char*)key_hmac);
  CharToByte(key, key_hmac, KEY_LENGTH);

  Serial.println("KEY HMAC + KEY : **********");
  Serial.println((char*)key_hmac);
  Serial.println(key);

  /*Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
  Serial.println(WiFi.SSID());*/

  //Set mqtt server data
  client.setServer(mqtt_server, mqtt_port);

  client.setCallback(callback);

  Serial.println(F("Ready!"));

  Serial.println("#############################################################################");

  cnt = 0;

  snprintf(buf_init, sizeof buf_init, "%s###%s", nodeMCUClient, "INIT");

}

/*****************************************************************************************************************/

/************************************************* LOOP FUNCTION *************************************************/

void loop() {
  if (!client.connected()) {
    conectMqtt();
  }
  
  client.loop();

  if(flag_init){
    client.publish("init",buf_init);
    flag_init = 0;
    Serial.println("");
    Serial.println("ACK ASKED");
    Serial.println("");
    flag_ack = 1;
  }

  if(flag_ack){
    cnt_ack = cnt_ack + 1;
  }
  if(cnt_ack >= 100){
    digitalWrite(RED_LED, HIGH);
    delay(500);
    digitalWrite(RED_LED, LOW);
    delay(500);
    flag_init=1;
    
  }
  
  cnt = cnt + 1;

  if(cnt%50 == 0){
    Serial.print("CONTADOR: " + String(cnt));
    Serial.println("");
    Serial.print("CURRENT RFID: " + String(currentCard));
    Serial.println("");
  }
  /*if(cnt>=200){
    Serial.println("<<<<<<<<<<<<<<<<<<<<<RESET>>>>>>>>>>>>>>>>>>>>>>>>>");
    wifiManager.resetSettings();
    delay(3000);
    ESP.reset();
    delay(5000);
  }*/
   
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  // Show some details of the PICC (that is: the tag/card)
  Serial.println("");

  if (currentCard == "" && cnt > 60) { // this cnt allows to make a wait between card reads 
    base64_encode(authCodeb64, (char *)authCode, SHA256HMAC_SIZE);
    snprintf(buf_hmac, sizeof buf_hmac, "%s###%s", nodeMCUClient, (char *)authCodeb64);
    client.publish("hmac", buf_hmac);

    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    encrypt_rfid(rfidstr,iv_py);
    Serial.println("");
    Serial.print("CURRENT CARD: " + String(currentCard));
    Serial.println("");
    Serial.print("CURRENT CARD OLD: " + String(currentCardold));
    
    Serial.println("");
    Serial.print("MESSAGE: " + String(rfid_b64));
    if(currentCard != currentCardold || cnt > 90){ // this cnt allows to set the time between card reads for the same card
      snprintf(buf_acceso, sizeof buf_acceso, "%s###%s", nodeMCUClient, rfid_b64);
      client.publish("acceso", buf_acceso);
    } else {
      currentCard = "";
    }
    memset(rfid_b64, 0, sizeof(rfid_b64));
    memset(rfidstr, 0, sizeof(rfidstr));
    memset(buf_hmac, 0, sizeof(buf_hmac));
    memset(buf_acceso, 0, sizeof(buf_acceso));

    Serial.println();
    flag_init = 1;
    currentCardold = currentCard;
    currentCard = "";
    cnt = 0;
  }
}
