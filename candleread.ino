#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Pins
const int INPUT_PIN = A0;
const int A_PIN = D2;
const int B_PIN = D3;

// Mux selectors
const int X_SELECT_A = LOW;
const int X_SELECT_B = LOW;
const int Y_SELECT_A = HIGH;
const int Y_SELECT_B = LOW;
const int FAN_SELECT_A = LOW;
const int FAN_SELECT_B = HIGH;
const int BUTTON_SELECT_A = HIGH;
const int BUTTON_SELECT_B = HIGH;

// Speeds
const int SIGN_ROTATE = -1; // Direction of rotation
const int SIGN_TILT = -1; // Direction of tilt
const int MIN_ROTATE_SPD = -1;
const int MAX_ROTATE_SPD = 1;
const int MIN_TILT_SPD = -1;
const int MAX_TILT_SPD = 1;
int tilt_spd, rotate_spd;
int button_state;
int blow_spd;
int last_tilt_spd, last_rotate_spd;
int last_button_state;
int last_blow_spd;

// Networking
// WIFI
#define wifi_ssid "WIFI NAME"
#define wifi_password "WIFI PASSWORD"
WiFiClient espClient;

// MQTT
const int MQTT_PORT = 1883;
const int MSG_DELAY = 250;
const int BLOW_DELAY = 250; // extra delay for blow messages

#define mqtt_server "mqtt.flespi.io"
#define mqtt_user "MQTTUSER"
#define mqtt_password ""
#define candle_blow_topic "candle/blow"
#define candle_rotate_topic "candle/rotate"
#define candle_tilt_topic "candle/tilt"
#define candle_button_topic "candle/button"
PubSubClient client(espClient);
long last_msg = 0;
long last_blow_msg = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    //if (client.connect("ESP8266Client")) {
    if (client.connect("CandleReader", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void read_inputs() {
  // NodeMCU has only a single input, so multiplex between them
  digitalWrite(A_PIN, X_SELECT_A);
  digitalWrite(B_PIN, X_SELECT_B);
  rotate_spd = SIGN_ROTATE * map(analogRead(INPUT_PIN), 0, 1024, MIN_ROTATE_SPD, MAX_ROTATE_SPD);
  if (rotate_spd != last_rotate_spd) {
    Serial.print("ROTATE SPD: ");
    Serial.print(rotate_spd);
    Serial.print("\n");
  }

  digitalWrite(A_PIN, Y_SELECT_A);
  digitalWrite(B_PIN, Y_SELECT_B);
  tilt_spd = SIGN_TILT * map(analogRead(INPUT_PIN), 0, 1024, MIN_TILT_SPD, MAX_TILT_SPD);
  if (tilt_spd != last_tilt_spd) {
    Serial.print("TILT SPD: ");
    Serial.print(tilt_spd);
    Serial.print("\n");
  }

  digitalWrite(A_PIN, FAN_SELECT_A);
  digitalWrite(B_PIN, FAN_SELECT_B);
  blow_spd = analogRead(INPUT_PIN);
  // Clean up some low-level noise
  if (blow_spd < 10) {
    blow_spd = 0;
  }
  if (blow_spd != last_blow_spd) {
    Serial.print("blow speed: ");
    Serial.print(blow_spd);
    Serial.print("\n");
  }

  digitalWrite(A_PIN, BUTTON_SELECT_A);
  digitalWrite(B_PIN, BUTTON_SELECT_B);
  button_state = map(analogRead(INPUT_PIN), 0, 1024, 1, 0);
  if (button_state != last_button_state) {
    Serial.print("button state: ");
    Serial.print(button_state);
    Serial.print("\n");
  }
}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, MQTT_PORT);
  pinMode(A_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  blow_spd = 0;
  rotate_spd = 0;
  tilt_spd = 0;
  button_state = 1024;

  last_blow_spd = -999;
  last_rotate_spd = -999;
  last_tilt_spd = -999;
  last_button_state = -999;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Only publish when readings change
  long now = millis();
  if (now - last_msg > MSG_DELAY) {
    
    read_inputs();
    last_msg = now;

    if ((now - last_blow_msg > BLOW_DELAY) && (blow_spd != last_blow_spd)) {
      // Need fewer blow measurements, because it's very variable
      last_blow_msg = now;
      last_blow_spd = blow_spd;
      client.publish(candle_blow_topic, String(blow_spd).c_str());
    }

    if (button_state != last_button_state) {
      client.publish(candle_button_topic, String(button_state).c_str());
      last_button_state = button_state;
    }

    if (rotate_spd != last_rotate_spd) {
      client.publish(candle_rotate_topic, String(rotate_spd).c_str());
      last_rotate_spd = rotate_spd;
    }

    if (tilt_spd != last_tilt_spd) {
      client.publish(candle_tilt_topic, String(tilt_spd).c_str());
      last_tilt_spd = tilt_spd;
    }
  }
}
