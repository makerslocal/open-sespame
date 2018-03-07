#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>

extern "C" {
#include "user_interface.h"
}

// Function definitions:
extern void setState(bool state);
extern void startTimeout(uint32 msec);
extern void mqttCallback(char* topic, byte* payload, unsigned int length);
extern void timeoutCallback(void *pArg);
extern void inputCallback();
extern void inputWentHigh();
extern void inputWentLow();
extern void connectToWiFi();
extern void connectToMQTT();
extern void scheduleMessage(String subTopic, String message);
extern void sendMessages();

#define INPUT_PIN   (D0)
#define OUTPUT_PIN  (D1)

#include "config.h"

WiFiClient wclient;
PubSubClient client(wclient);

ESP8266WebServer server(80);

void handleRoot() {
  server.send(200, "text/plain", "Hi there! You found the alert light controller! Congrats! You should buy yourself a cookie!");
}

void returnOK() {
  server.send(200, "text/plain", "");
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

bool currentState = false;

String scheduledMessages[8][2];
int numScheduledMessages = 0;

// Define a timer to use for timeouts:
os_timer_t timer;

void setup(void) {

  client.setServer(mqttHost, mqttPort);
  client.setCallback(mqttCallback);

  Serial.begin(115200);
  Serial.println("Booting rq-switch...");

  Serial.print("Max packet size: "); Serial.println(MQTT_MAX_PACKET_SIZE);

  scheduleMessage("/controller", "{\"status\":\"booted\"}");

  // Enable GPIO interrupts:
  pinMode(INPUT_PIN, INPUT);
  attachInterrupt(INPUT_PIN, inputCallback, CHANGE);
  Serial.print("input on pin "); Serial.println(INPUT_PIN);

  pinMode(OUTPUT_PIN, OUTPUT);
  Serial.print("output on pin "); Serial.println(OUTPUT_PIN);

  // Connect to WiFi:
  connectToWiFi();

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("boot done.");
  
}

void loop(void) {
  sendMessages();
  yield();
  client.loop();
  server.handleClient();
}

void setState(bool state) {
    currentState = state;
    if ( state ) {
        digitalWrite(OUTPUT_PIN, HIGH);
        scheduleMessage("", "{\"status\":\"on\"}");
    } else {
        digitalWrite(OUTPUT_PIN, LOW);
        scheduleMessage("", "{\"status\":\"off\"}");
    }
}

void startTimeout(uint32 msec) {
  os_timer_setfn(&timer, timeoutCallback, NULL);
  os_timer_arm(&timer, msec, false);
  Serial.print("Timeout set for "); Serial.println(msec);
}

void timeoutCallback(void *pArg) {
  Serial.println("Timeout elapsed.");
  setState(false);
}

void inputCallback() {
  bool status = digitalRead(INPUT_PIN);
  if (status) {
    inputWentLow();
  } else {
    inputWentHigh();
  }
}

void inputWentHigh() {
  Serial.println("input high");
}

void inputWentLow() {
  Serial.println("input low");
}

void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, pass);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection not successful.");
      return;
    }
    
    Serial.print("WiFi connected; IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi already connected!");
  }
}

void connectToMQTT() {
    if (!client.connected()) {
        if (client.connect("rq-switch")) {
            Serial.println("MQTT connected.");
            client.subscribe("ml256/irc/makerslocal/command/alert");
            Serial.println("MQTT subscribed.");
        } else {
            Serial.println("MQTT failed to connect.");
        }
    }
}

void scheduleMessage(String subTopic, String message) {
  if (numScheduledMessages <= 8) {
    ++numScheduledMessages;
    scheduledMessages[numScheduledMessages-1][0] = message;
    scheduledMessages[numScheduledMessages-1][1] = subTopic;
  }
}

void sendMessages() {
  for (int i=1; i<=numScheduledMessages; ++i) {
    connectToMQTT();
    String message = mqttTopic+scheduledMessages[i-1][1];
    String topic = scheduledMessages[i-1][0].c_str();
    client.publish(message.c_str(), topic.c_str());
  }
  numScheduledMessages = 0;
}

extern void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("boom roasted");
    setState(true);
    startTimeout(10000);
}

