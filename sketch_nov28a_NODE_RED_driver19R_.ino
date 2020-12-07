/*THIS SKETCH WORKS WITH THE MDUINO 19R+
  
*/

// included library depending of M-Duino version
#ifdef MDUINO_PLUS
#include <Ethernet2.h>
#else
#include <Ethernet.h>
#endif

// libraries needed for MQTT communication
#include <ArduinoJson.h>
#include <PubSubClient.h>
#define MQTT_ID "demo"
#define NUM_ZONES 1
#define NUM_DIGITAL_OUTPUTS_PER_ZONE 3
#define DIGITAL_OUTPUTS_OFFSET 0

const int digitalOutputs[NUM_ZONES][NUM_DIGITAL_OUTPUTS_PER_ZONE] = {
  {Q0_0, Q0_1, Q0_2},    //adapt it to the M-Duino model that you are using
};
#define NUM_ANALOG_OUTPUTS_PER_ZONE 3
#define ANALOG_OUTPUTS_OFFSET 5

const int analogOutputs[NUM_ZONES][NUM_DIGITAL_OUTPUTS_PER_ZONE] = {
  {A0_0, A0_1, A0_2},    //adapt it to the M-Duino model that you are using
};
#define NUM_DIGITAL_INPUTS_PER_ZONE 2
#define DIGITAL_INPUTS_OFFSET 0

const int digitalInputs[NUM_ZONES][NUM_DIGITAL_INPUTS_PER_ZONE] = {
  {I0_0, I0_1},    //adapt it to the M-Duino model that you are using
};
#define NUM_ANALOG_INPUTS_PER_ZONE 4
#define ANALOG_INPUTS_OFFSET 2
#define ANALOG_INPUTS_THRESHOLD 1 // Filtering threshold

const int analogInputs[NUM_ZONES][NUM_ANALOG_INPUTS_PER_ZONE] = {
{I0_2, I0_3, I0_4, I0_5},    //adapt it to the M-Duino model that you are using
};
byte mac[] = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xAE };

IPAddress broker(192, 168, 0, 128);
unsigned port = 1883;
// Initialize client
EthernetClient client;
PubSubClient mqtt(client);
int digitalInputsValues[NUM_ZONES][NUM_DIGITAL_INPUTS_PER_ZONE];
int analogInputsValues[NUM_ZONES][NUM_ANALOG_INPUTS_PER_ZONE];

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac);
  Serial.print("Connect to ethernet ");
  Serial.println(Ethernet.localIP());
  mqtt.setServer(broker, port);
  mqtt.setCallback(receiveMqttMessage);
  Serial.println("Connect to MQTT");
  // Init variables
  for (int i = 0; i < NUM_ZONES; ++i) {
    for (int j = 0; j < NUM_DIGITAL_INPUTS_PER_ZONE; ++j) {
      digitalInputsValues[i][j] = digitalRead(digitalInputs[i][j]);
    }
    for (int j = 0; j < NUM_ANALOG_INPUTS_PER_ZONE; ++j) {
      analogInputsValues[i][j] = analogRead(analogInputs[i][j]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  if (!mqtt.connected()) {
    Serial.println("MQTT disconnected");
    reconnect();
  } else {
    mqtt.loop();
    Serial.print(".");
  }
  updateInputs();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void updateInputs() {
  for (int i = 0; i < NUM_ZONES; ++i) {
    for (int j = 0; j < NUM_DIGITAL_INPUTS_PER_ZONE; ++j) {
      updateDigitalInput(i, j);
    }
    for (int j = 0; j < NUM_ANALOG_INPUTS_PER_ZONE; ++j) {
      updateAnalogInput(i, j);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void updateDigitalInput(int zone, int index) {
  int value = digitalRead(digitalInputs[zone][index]);
  if (value != digitalInputsValues[zone][index]) {
    digitalInputsValues[zone][index] = value;
    publishInput(zone, index + DIGITAL_INPUTS_OFFSET, value);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void updateAnalogInput(int zone, int index) {
  int value = analogRead(analogInputs[zone][index]);
  int minValue = value > ANALOG_INPUTS_THRESHOLD ? value - ANALOG_INPUTS_THRESHOLD : 0;
  int maxValue = value < 1023 - ANALOG_INPUTS_THRESHOLD ? value + ANALOG_INPUTS_THRESHOLD : 1023;
  if ((analogInputsValues[zone][index] < minValue) || (analogInputsValues[zone][index] > maxValue)) {
    analogInputsValues[zone][index] = value;
    publishInput(zone, index + ANALOG_INPUTS_OFFSET, value);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setDigitalOutput(char *payload, unsigned int len) {
  DynamicJsonBuffer json(JSON_OBJECT_SIZE(3));
  JsonObject &root = json.parseObject(payload, len);
  if (root.success()) {
    int zone = root["zone"];
    if (zone >= NUM_ZONES) {
      // Invalid zone
      return;
    }
    int index = root["index"];
    index -= DIGITAL_OUTPUTS_OFFSET;
    if (index >= NUM_DIGITAL_OUTPUTS_PER_ZONE) {
      // Invalid digital output
      return;
    }
    int value = root["value"];
    digitalWrite(digitalOutputs[zone][index], value);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setAnalogOutput(char *payload, unsigned int len) {
  DynamicJsonBuffer json(JSON_OBJECT_SIZE(3));
  JsonObject &root = json.parseObject(payload, len);
  if (root.success()) {
    int zone = root["zone"];
    if (zone >= NUM_ZONES) {
      // Invalid zone
      return;
    }
    int index = root["index"];
    index -= ANALOG_OUTPUTS_OFFSET;
    if (index >= NUM_ANALOG_OUTPUTS_PER_ZONE) {
      // Invalid analog output
      return;
    }
    int value = root["value"];
    analogWrite(analogOutputs[zone][index], value);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void reconnect() {
  if (mqtt.connect(MQTT_ID)) {
    mqtt.subscribe("Q");
    mqtt.subscribe("A");
  } else {
    // MQTT connect fail
    client.stop();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void receiveMqttMessage(char* topic, byte* payload, unsigned int len) {
  if (strcmp(topic, "Q") == 0) {
    // Set digital output
    setDigitalOutput((char*) payload, len);
  } else if (strcmp(topic, "A") == 0) {
    // Set analog output
    setAnalogOutput((char*) payload, len);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void publishInput(int zone, int index, int value) {
  DynamicJsonBuffer json(JSON_OBJECT_SIZE(3));
  JsonObject &root = json.createObject();
  if (root.success()) {
    root["zone"] = zone;
    root["index"] = index;
    root["value"] = value;
    publish("I", root);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void publish(const char *topic, JsonObject &root) {
  unsigned len = root.measureLength();
  if (len > 0) {
    char *payload = new char[len + 1];
    if (payload) {
      root.printTo(payload, len + 1);
      publish(topic, payload);
      delete[] payload;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void publish(const char *topic, const char *payload) {
  if (mqtt.connected()) {
    mqtt.publish(topic, payload);
  }
}
