 #include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

//Biblioteca do clientMQTT
//http://pubsubclient.knolleary.net/api.html
//https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

#include <ebase64.h>
#include <AES_config.h>
#include <AES.h>

#include <Crypto.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#define RESET_PIN 16  // -> CHANGE TO D3 - GPIO0
#define RED_LED 4
#define GREEN_LED 5
#define BEEP 15

#include <SPI.h>
#include "MFRC522.h"
#define RST_PIN 0 // RST-PIN for RC522 - RFID - SPI - Modul GPIO0 - D3  --> CHANGE TO D8 - GPIO15?
#define SS_PIN 2  // SDA-PIN for RC522 - RFID - SPI - Modul GPIO2 - D4  

/* The key can be of any length, 16 and 32 are common */
#define KEY_LENGTH 16

int cnt, cnt_r, cnt_rold;

/* Define our */
byte key_hmac[KEY_LENGTH] = {77, 49, 107, 51, 121, 49, 115, 100, 65, 98, 51, 83, 116, 48, 110, 51}; // 'M1k3y1sdAb3St0n3'

byte authCode[SHA256HMAC_SIZE];
char authCodeb64[200];

//define your default values here, 
//if there are different values in config.json, 
//they are overwritten.
//length should be max size + 1

char mqtt_server[15];

char nodeMCUClient[15];
char userMQTT[15];
char passwordMQTT[15];

char buf[512];

char rfidstr[15];
 char rfid_b64[200];

int flag_init = 1;

//AES parameters

AES aes;

char key[20];//N_BLOCK+1]="M1k3y1sdAb3St0n3";//"76rTy8aaSc34dLqw"; // key for the AES encryption of the payload
char iv_py[20];

//MQTT parametros.
//const char* mqtt_server = "192.168.1.37";
const int mqtt_port = 1883;

//flag for saving data
bool shouldSaveConfig = true;
String currentCard = "";
String currentCardold = "";

WiFiManager wifiManager;
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance

WiFiClient espClient;
//Criando o clientMQTT com o wificlient
PubSubClient client(espClient);

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

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
  //clean FS, for testing
  //SPIFFS.format();

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
          Serial.println("\nHASTA AQUI 1");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(key, json["key"]);
          strcpy(nodeMCUClient, json["nodeMCUClient"]);
          strcpy(userMQTT, json["userMQTT"]);
          strcpy(passwordMQTT, json["passwordMQTT"]);
          Serial.println("\nHASTA AQUI 2");
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
  wifiManager.setTimeout(120);

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

  CharToByte(key, key_hmac, KEY_LENGTH);

  Serial.println("KEY HMAC + KEY : **********");
  Serial.println((char*)key_hmac);
  Serial.println(key);

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
  Serial.println(WiFi.SSID());

  //Set mqtt server data
  client.setServer(mqtt_server, mqtt_port);

  client.setCallback(callback);

  Serial.println(F("Ready!"));

  Serial.println("#############################################################################");

  cnt = 0;
  cnt_r = 0;

}

void conectMqtt() {
  while (!client.connected()) {
    Serial.print("ConnectingMQTT ...");

    //Parameters nodeMCUClient, userMQTT, passwordMQTT
    if (client.connect(nodeMCUClient,userMQTT,passwordMQTT)){//"esp8266","mqtt_rfid","password")) {
      Serial.println("Connected");
      //Subscriing to topic retorno.
      client.subscribe("retorno");
      client.subscribe("ack");
    } else {
      Serial.print("Error");
      Serial.print(client.state());
      Serial.println("Retry in 5 seconds");
      // Wait 5 seconds before retrying
      delay(1000);
      
    }
  }
}

//Parameters: topic_name, msg , msg length
void callback(char* topic, byte* payload, unsigned int length) {
  SHA256HMAC hmac(key_hmac, KEY_LENGTH);  
  Serial.println();
  Serial.print("Welcome [");
  Serial.print(topic);
  Serial.print("]: ");
  String mensagem = "";

  //Convert msg from byte to string
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  mensagem.toCharArray(iv_py, sizeof iv_py);
  Serial.println(mensagem);
  Serial.println();
  if(strcmp(topic, "retorno") == 0){
    
    cnt = 0;

    if (mensagem == "FALSE") {
      digitalWrite(RED_LED, HIGH);
      tone(BEEP, 1930);
        delay(150);
        tone(BEEP, 1630);
        delay(100);
        noTone(BEEP);
      delay(1000);
      digitalWrite(RED_LED, LOW);
      delay(250);
    }
    else {
      if(mensagem == "NOAUTH"){
        digitalWrite(RED_LED, HIGH);
        delay(500);
        digitalWrite(GREEN_LED, HIGH);
        delay(500);
        digitalWrite(RED_LED, LOW);
        delay(250);
        digitalWrite(GREEN_LED, LOW);
        delay(250);
      } else {
        digitalWrite(GREEN_LED, HIGH);
        tone(BEEP, 1630);
        delay(150);
        tone(BEEP, 1930);
        delay(100);
        noTone(BEEP);
        delay(1000);
        digitalWrite(GREEN_LED, LOW);
        delay(250);
        Serial.println("#############################################################################");
    }
  }
  currentCardold = currentCard;
  currentCard = "";
  Serial.println("current card emptied");
  } else if(strcmp(topic, "ack") == 0){
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
  } else Serial.println("ELSE " + mensagem);
}

void loop() {
  if (!client.connected()) {
    conectMqtt();
  }
  client.loop();

  if(flag_init){
    client.publish("init","INIT");
    flag_init = 0;
    Serial.println("");
    Serial.println("ACK ASKED");
    Serial.println("");
  }

  Serial.print("CURRENT RFID: " + String(currentCard));
  Serial.println("");
  cnt = cnt + 1;
  cnt_r = cnt_r + 1;
   Serial.print("CONTADOR: " + String(cnt));
  Serial.println("");
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
  cnt_r = cnt_r - 1;
  // Show some details of the PICC (that is: the tag/card)
  Serial.println("");

  if (currentCard == "" && cnt > 25) { // this cnt allows to make a wait between card reads 
    base64_encode(authCodeb64, (char *)authCode, SHA256HMAC_SIZE);
    client.publish("hmac",(char *)authCodeb64);

    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    encrypt_rfid(rfidstr,iv_py);
    Serial.println("");
    Serial.print("CURRENT CARD: " + String(currentCard));
    Serial.println("");
    Serial.print("CURRENT CARD OLD: " + String(currentCardold));
    
    Serial.println("");
    Serial.print("MESSAGE: " + String(rfid_b64));
    if(currentCard != currentCardold || cnt > 60){ // this cnt allows to set the time between card reads for the same card
      client.publish("acceso", rfid_b64);
    } else {
      currentCard = "";
    }
    memset(rfid_b64, 0, sizeof(rfid_b64));
    memset(rfidstr, 0, sizeof(rfidstr));

    Serial.println();
    flag_init = 1;
  }

}

void encrypt_rfid(char rfidstr[], char iv_py[])
{
  byte cipher_rfid[1000];
  
  int len = base64_encode(rfid_b64, rfidstr, strlen(rfidstr));
  aes.do_aes_encrypt((byte *)rfid_b64, len, cipher_rfid, (byte *)key, 128, (byte *)iv_py);
  base64_encode(rfid_b64, (char *)cipher_rfid, aes.get_size());

  Serial.println("RFID B64: " + String((char *) rfid_b64));
  Serial.println("");

}

// Helper routine to dump a byte array as hex values to Serial
void dump_byte_array(byte *buffer, byte bufferSize) {
  
  String rfid;
  char rfid_b[8];
  char s[100];
  byte cipher_iv[1000];
 
  for (byte i = 0; i < bufferSize; i++) {
    //Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    //Serial.print(buffer[i], HEX);
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

void CharToByte(char* chars, byte* bytes, unsigned int count){
    for(unsigned int i = 0; i < count; i++)
        bytes[i] = (byte)chars[i];
}

