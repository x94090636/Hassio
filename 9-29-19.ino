#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

// Wifi and MQTT types
WiFiClient espClient;
PubSubClient client(espClient);

  unsigned long currentMillis = millis();
  
  int RelayPin = 12;
  int LedPin = 13;
  int ButtonPin = 0;
  
// Variables for Wifi
  const char* ssid = "Media2.4ghz";
  const char* password = "Leonardo";
  const char* host = "devtest";
  unsigned long previousMillisWifi;

// Variables for MQTT
  const char* mqtt_server = "192.168.50.100";
  const char* mqtt_user = "nstrand";
  const char* mqtt_pass = "Porkchop912254";
  const char* InTopic = "/test/in";
  const char* OutTopic = "/test/out";
  unsigned long previousMillisMQTT;
  String clientId = "devtest-";

// Variables for Button/Math
  unsigned long previousMillisLongPress;
  bool pressPending = false;
  bool lastButton = false;
  bool lastOn = false;
  bool longPress = false;
  int heldTime = 0;
  int longPressAmount = 2000; // delay before it's a "long press"
  int longPressDelay = 10000; // 10 seconds delay
  
void setup() {
  Serial.begin(115200);
  pinMode(RelayPin, OUTPUT);
  pinMode(LedPin, OUTPUT);
  pinMode(ButtonPin, INPUT);
  Serial.println("MAC: " + WiFi.macAddress());
  initializeWifi();
  initializeMQTT();
  initializeOTA();
  Serial.println("Initializing Complete");
}

void loop() {
  // PubSub checks
  client.loop();
  // OTA Handle
  ArduinoOTA.handle();
  // Check the button and count
  buttonWatch();
  // Check Wifi Connection, reconnect if offline
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  // Check MQTT Connection, reconnect if offline
  } else if (!client.connected()) {
    if (currentMillis - previousMillisMQTT >= 5000) {
      connectMQTT();
      previousMillisMQTT = currentMillis;
    } else {
      // Variables for Delays
    }
  }
}

void initializeWifi() {
  Serial.println("Initializing WiFi");
  WiFi.hostname(host);
  delay(50);
  WiFi.begin(ssid, password);
  delay(50);
  WiFi.mode(WIFI_STA); // Makes it only a Wifi client, not an AP
  delay(50);
  connectWifi();
  Serial.println("WiFi Initialized");
}

void initializeMQTT() {
  Serial.println("Initializing MQTT");
  delay(50);
  client.setServer(mqtt_server, 1883);
  delay(50);
  client.setCallback(callback);
  delay(50);
  connectMQTT();
  Serial.println("MQTT Initialized");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if ((char)payload[0] == '1') {
    lightStatus(true);
  } else {
    lightStatus(false);
  }
}

void initializeOTA() {
  Serial.println("Booting OTA");
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

// Reconnect processes
void connectWifi() {
  // Variables for Delays
  const long interval = 1000;
  //
  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - previousMillisWifi >= interval) {
      Serial.println("Wifi Disconnected...retrying...");
      previousMillisWifi = currentMillis;
      Serial.println(WiFi.status());
    }
  }
}

void connectMQTT() {
  const char* message_buffer;
  lastOn = digitalRead(RelayPin);
  //
  if (lastOn == 1) {
    message_buffer = "1";
  }
  else {
    message_buffer = "0";
  }
  if (!client.connected() && WiFi.status() == WL_CONNECTED) {
    Serial.println("Attempting MQTT connection...");
    // Create a random client ID
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(host,mqtt_user,mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish current status...
      client.publish(OutTopic, message_buffer, 1);
      client.publish(InTopic, message_buffer, 1);
      Serial.println("Message Buffer:");
      Serial.println(message_buffer);
      Serial.println("Last on:");
      Serial.println(lastOn );
      // ... and resubscribe
      client.subscribe(InTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
    }
  }
}

void buttonWatch () {
  int buttonStatus = digitalRead(ButtonPin);
  lastOn = digitalRead(RelayPin);
  currentMillis = millis();
  if (buttonStatus == LOW) {
    if (!lastButton) {
      cleanup();
      Serial.print("fresh press");
      lastButton = true;
      previousMillisLongPress = currentMillis;
      pressPending = true;
    }
    buttonCounter();
  } else {
    lastButton = false;
    if (pressPending == true) {
      if (longPress == true) {
        if (currentMillis - previousMillisLongPress >= longPressDelay) {
          Serial.println("triggering longpress");
          lightToggle();
          previousMillisLongPress = currentMillis;
        }
      } else {
        Serial.println("triggering shortpress");
        lightToggle();
      }
    }
  }
}

void cleanup() {
  pressPending = false;
  longPress = false;
  heldTime = 0;
  currentMillis = millis();
}

void buttonCounter() {
  heldTime++;
  Serial.println(heldTime);
  if (heldTime >= longPressAmount) {
    longPress = true;
  } else {
    longPress = false;
  }
}

void lightToggle () {
  lastOn = digitalRead(RelayPin);
  if (lastOn == true) {
    digitalWrite(RelayPin, LOW);
    digitalWrite(LedPin, LOW);
  } else if (lastOn == false) {
    digitalWrite(RelayPin, HIGH);
    digitalWrite(LedPin, HIGH);
  }    
  delay(50); // do a little bounce delay
  lastOn = digitalRead(RelayPin);
  updateMQTT(lastOn);
}

void lightStatus (bool DesiredState) {
  lastOn = digitalRead(RelayPin);
  if (DesiredState == true) {
    if (lastOn == true) {
      Serial.println("Light Already On");
    } else {
      digitalWrite(RelayPin, HIGH);
      digitalWrite(LedPin, HIGH); //High Off, Low Green
    }
  } else if (DesiredState == false) {
      if (lastOn == true) {
        digitalWrite(RelayPin, LOW);
        digitalWrite(LedPin, LOW); //High Off, Low Green
        Serial.println("Light asked to be turned off");
      } else {
        Serial.println("Light Already Off");
      }
    }
    delay(50); // do a little bounce delay
    lastOn = digitalRead(RelayPin);
    updateMQTT(lastOn);
    Serial.println("last on");
    Serial.println(lastOn);
}

void updateMQTT(int PinStatus) {
  if (PinStatus == HIGH) {
    client.publish(OutTopic, "1");
  } else {
    client.publish(OutTopic, "0");
  }
  cleanup();
}
