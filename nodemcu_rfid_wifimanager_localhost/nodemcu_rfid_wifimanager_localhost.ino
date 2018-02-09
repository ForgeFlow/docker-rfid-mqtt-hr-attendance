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

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#define RESET_PIN 16  // -> CHANGE TO D3 - GPIO0
#define RED_LED 4
#define GREEN_LED 5
#define BEEP 10

#include <SPI.h>
#include "MFRC522.h"
#define RST_PIN 0 // RST-PIN for RC522 - RFID - SPI - Modul GPIO0 - D3  --> CHANGE TO D8 - GPIO15?
#define SS_PIN  2  // SDA-PIN for RC522 - RFID - SPI - Modul GPIO2 - D4  


//define your default values here, 
//if there are different values in config.json, 
//they are overwritten.
//length should be max size + 1
char odoo_host[90];
char odoo_port[6] = "8069";
char odoo_user[33] = "admin";
char odoo_password[33] = "admin";
char odoo_database[24];

//MQTT parametros.
const char* mqtt_server = "192.168.1.37";
const int mqtt_port = 1883;

//flag for saving data
bool shouldSaveConfig = false;
String currentCard = "";

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

  pinMode(RESET_PIN, INPUT); // IF FINALLY WE USE THE INTERNAL_PULLUP, SUBSTITUTE FOR pinMode(RESET_PIN, INPUT_PULLUP);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522

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

          strcpy(odoo_host, json["odoo_host"]);
          strcpy(odoo_port, json["odoo_port"]);
          strcpy(odoo_user, json["odoo_user"]);
          strcpy(odoo_password, json["odoo_password"]);
          strcpy(odoo_database, json["odoo_database"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  Serial.println(odoo_host);
  Serial.println(odoo_port);
  Serial.println(odoo_user);
  Serial.println(odoo_password);
  Serial.println(odoo_database);


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_odoo_host("host", "Odoo host", odoo_host, 89);
  WiFiManagerParameter custom_odoo_port("port", "Odoo port", odoo_port, 5);
  WiFiManagerParameter custom_odoo_user("user", "Odoo user", odoo_user, 32);
  WiFiManagerParameter custom_odoo_password("password", "Odoo password", odoo_password, 32);
  WiFiManagerParameter custom_odoo_database("database", "Odoo database", odoo_database, 23);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  //WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_odoo_host);
  wifiManager.addParameter(&custom_odoo_port);
  wifiManager.addParameter(&custom_odoo_user);
  wifiManager.addParameter(&custom_odoo_password);
  wifiManager.addParameter(&custom_odoo_database);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

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
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(odoo_host, custom_odoo_host.getValue());
  strcpy(odoo_port, custom_odoo_port.getValue());
  strcpy(odoo_user, custom_odoo_user.getValue());
  strcpy(odoo_password, custom_odoo_password.getValue());
  strcpy(odoo_database, custom_odoo_database.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["odoo_host"] = odoo_host;
    json["odoo_port"] = odoo_port;
    json["odoo_user"] = odoo_user;
    json["odoo_password"] = odoo_password;
    json["odoo_database"] = odoo_database;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
  Serial.println(WiFi.SSID());

  //Set mqtt server data
  client.setServer(mqtt_server, mqtt_port);

  client.setCallback(callback);

  Serial.println(F("Ready!"));
  Serial.println(F("======================================================"));
  Serial.println(F("Scan for Card and print UID:"));
}

void conectMqtt() {
  while (!client.connected()) {
    Serial.print("ConnectingMQTT ...");

    //Parameters nodeMCUClient, userMQTT, passwordMQTT
    if (client.connect("esp8266", "mqtt_rfid", "password")) {
      Serial.println("Connected");
      //Subscriing to topic retorno.
      client.subscribe("retorno");
    } else {
      Serial.print("Error");
      Serial.print(client.state());
      Serial.println("Retry in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//Parameters: topic_name, msg , msg length
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println();
  Serial.print("Welcome [");
  Serial.print(topic);
  Serial.print("]: ");
  String mensagem = "";
  //Convert msg from byte to string
  for (int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }
  Serial.println(mensagem);
  Serial.println();
  if (mensagem == "FALSE") {
    digitalWrite(RED_LED, HIGH);
    delay(1000);
    digitalWrite(RED_LED, LOW);
    delay(250);
  }
  else {
    digitalWrite(GREEN_LED, HIGH);
    tone(BEEP, 1630);
    delay(150);
    tone(BEEP, 1930);
    delay(100);
    noTone(BEEP);
    delay(1000);
    digitalWrite(GREEN_LED, LOW);
    delay(250);
  }
  currentCard = "";
}

void loop() {
  if (!client.connected()) {
    conectMqtt();
  }
  client.loop();

  if ( digitalRead(RESET_PIN) == LOW ) {
    Serial.println("RESET");
    wifiManager.resetSettings();
    delay(3000);
    ESP.reset();
    delay(5000);
  }

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
  Serial.print(F("Card UID:"));
  if (currentCard == "") {
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
  }

}

// Helper routine to dump a byte array as hex values to Serial
void dump_byte_array(byte *buffer, byte bufferSize) {
  char rfidstr[15];
  char s[100];
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
    //Convert from byte to Hexadecimal
    sprintf(s, "%s%x", buffer[i] < 0x10 ? "0" : "", mfrc522.uid.uidByte[i]);
    //Concatenate msg
    strcat( &rfidstr[i] , s);
  }
  char buf[512];
  snprintf(buf, sizeof buf, "%s###%s###%s###%s###%s###%s", odoo_host, odoo_port, odoo_user, odoo_password, odoo_database, rfidstr);
  currentCard = rfidstr;
  client.publish("acesso", buf);
  Serial.println(" Verifing...");
}


