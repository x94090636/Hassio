#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

WiFiClient espClient;
PubSubClient client(espClient);
  // Variables for WiFi Setup
  const char* ssid = "HomeAI";
  const char* password = "SnowRotBear";
  const char* host = "LightOutSunroom";
  const char* mqtt_server = "192.168.71.3";
  const char* mqtt_user = "Lights";
  const char* mqtt_pass = "ohsxn90QXxByGnnRIc1cSbEJqiJRnTV1ukNNBV5PRsqU7R5ElC";
  const char* in_topic = "/light/out/sunroom/in";
  const char* out_topic = "/light/out/sunroom/out";
  unsigned long previousMillis = 0;
  int statCounter = 0;
  int RELAY_PIN = 12;
  int LED_PIN = 13;
  int BUTTON_TOGGLE = 0;
  int button_bounce=0;
  int last_button=0;
  int FailCounter=0;
  int buttonStatus = 0;
  int last_on = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_TOGGLE, INPUT);
  initializeWifi();
  initializeMQTT();
  initializeOTA();
}

void loop() {
  // put your main code here, to run repeatedly:
    buttonChk();
    connectWifi();
    connectMQTT();
    ArduinoOTA.handle();
    client.loop();
}

void initializeWifi() {
  WiFi.hostname(host);
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA); // Makes it only a Wifi client, not an AP
}

void initializeMQTT() {
  Serial.println("Initializing MQTT");
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH); //High Off, Low Green
    client.publish(out_topic, "1");
  } else {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW); //High Off, Low Green
    client.publish(out_topic, "0");
  }
}

void connectWifi() {
  // Variables for Delays
  const long interval = 1000;
  unsigned long currentMillis = millis();
  //
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Wifi Disconnected...retrying...");
      previousMillis = currentMillis;
      statCounter=statCounter+1; // Increment counter
      Serial.print(statCounter);
      Serial.print(" attempts. ");  
      Serial.println(WiFi.status());
    }
  //buttonChk();
  }
}

void connectMQTT() {
  // Variables for Delays
  unsigned long currentMillis = millis();
  const char* message_buffer;
  //
  if (last_on == 1) {
    message_buffer = "1";
  }
  else {
    message_buffer = "0";
  }
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    if (currentMillis - previousMillis >= 5000) {
      Serial.println("Attempting MQTT connection...");
      // Create a random client ID
      //String clientId = "ESP8266Client-";
      //clientId += String(random(0xffff), HEX);
      // Attempt to connect
      if (client.connect(host,mqtt_user,mqtt_pass)) {
        Serial.println("connected");
        // Once connected, publish current status...
        client.publish(out_topic, message_buffer, 1);
        client.publish(in_topic, message_buffer, 1);
        Serial.println("Message Buffer:");
        Serial.println(message_buffer);
        Serial.println("Last on:");
        Serial.println(last_on);
        // ... and resubscribe
        client.subscribe(in_topic);
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
      }
    }
    //buttonChk();
  }
}

void buttonChk(void) {
  buttonStatus = digitalRead(BUTTON_TOGGLE);
  // if the button is pressed
  if (buttonStatus == LOW) {

    // if the button is newly pressed, last_button=0.  If still being held, last_button=1
    if (!last_button) {
      button_bounce = 1;
      last_button = 1;
      last_on = digitalRead(RELAY_PIN);
      if (last_on) {
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW); //High Off, Low Green
        client.publish(out_topic, "0");
        Serial.println("Button last on Pressed");
      }
      else
      {
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH); //High Off, Low Green
        client.publish(out_topic, "1");
        Serial.println("Button last off Pressed");
      }
        delay(50); // do a little bounce delay
        last_on = digitalRead(RELAY_PIN);
        Serial.println("last on");
        Serial.println(last_on);
        last_button = 1;
    }
  }
  else
  last_button = 0;
}

void initializeOTA() {
  Serial.println("Booting");
  // Port defaults to 8266
  //ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(host);

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
