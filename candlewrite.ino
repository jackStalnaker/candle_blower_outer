#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>

// MOTORS AND SERVOS
const int BLOWER_DURATION = 250;
Servo tilt_servo;
Servo rotate_servo;
const int MIN_ROTATE = 0;
const int MAX_ROTATE = 180;
const int MIN_TILT = 0;
const int MAX_TILT = 135;
const int SERVO_DELAY = 20; // Slow down servos!
const int BLOWER_IN_MAX_SPEED = 275; // Top empirically observed blow speed (0-1023)
const int BLOWER_OUT_MAX_SPEED = 750; // A PWM number between 0-1023, use to throttle motor
int tilt_pos, rotate_pos;
int last_tilt, last_rotate;
int tilt_spd, rotate_spd;
int blower_spd, button_state;
int motor_spd;

// PINS
const int ROTATE_PIN = D2;
const int TILT_PIN = D3;
const int BLOWER_PIN = D5;

// WIFI
#define wifi_ssid "WIFI NAME"
#define wifi_password "WIFI PASSWORD"
WiFiClient espClient;

// MQTT
const int MQTT_PORT = 1883;
#define mqtt_server "mqtt.flespi.io"
#define mqtt_user "MQTT NAME"
#define mqtt_password ""
#define candle_blow_topic "candle/blow"
#define candle_rotate_topic "candle/rotate"
#define candle_tilt_topic "candle/tilt"
#define candle_button_topic "candle/button"
PubSubClient client(espClient);
long lastMsg = 0;

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
    if (client.connect("CandleWriter", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  client.subscribe(candle_blow_topic);
  client.subscribe(candle_tilt_topic);
  client.subscribe(candle_rotate_topic);
  client.subscribe(candle_button_topic);
}

int payload_to_int(byte* payload, unsigned int length) {
  // Convert payload to string by null terminating it. This looks like an array overrun to me
  // and it probably is, but literally everything I've seen recommends it anyway. Probably not
  // totally safe, but whatever.
  payload[length] = '\0';
  return atoi((char *)payload);
}

void handle_mqtt(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.print(topic);
  Serial.print("\n");

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.print("\n");

  if (strcmp(topic, candle_blow_topic) == 0) {
    Serial.println("Got blower topic");
    blower_spd = payload_to_int(payload, length);
    motor_spd = map(blower_spd, 0, BLOWER_IN_MAX_SPEED, 0, BLOWER_OUT_MAX_SPEED);
    Serial.print("Blower speed: ");
    Serial.print(blower_spd);
    Serial.print("\n");

  }

  if (strcmp(topic, candle_button_topic) == 0) {
    Serial.println("Got button topic");
    button_state = payload_to_int(payload, length);
    motor_spd = map(button_state, 0, 1, 0, BLOWER_OUT_MAX_SPEED);
    Serial.print("Button state: ");
    Serial.print(button_state);
    Serial.print("\n");
  }

  if (strcmp(topic, candle_tilt_topic) == 0) {
    Serial.println("Got tilt topic");
    tilt_spd = payload_to_int(payload, length);
    Serial.print("Tilt speed: ");
    Serial.print(tilt_spd);
    Serial.print("\n");
  }

  if (strcmp(topic, candle_rotate_topic) == 0) {
    Serial.println("Got rotate topic");
    rotate_spd = payload_to_int(payload, length);
    Serial.print("Rotate speed: ");
    Serial.print(rotate_spd);
    Serial.print("\n");
  }
}

void activate_moving_parts() {
  // Apply speed to blower
  if (motor_spd > 0) {
    analogWrite(BLOWER_PIN, motor_spd);
    delay(BLOWER_DURATION);
    digitalWrite(BLOWER_PIN, LOW);
  }

  // Tilt and rotate servos
  tilt_pos = last_tilt + tilt_spd;
  if (tilt_pos < MIN_TILT) {
    tilt_pos = MIN_TILT;
  } else if (tilt_pos > MAX_TILT) {
    tilt_pos = MAX_TILT;
  }
  rotate_pos = last_rotate + rotate_spd;
  if (rotate_pos < MIN_ROTATE) {
    rotate_pos = MIN_ROTATE;
  } else if (rotate_pos > MAX_ROTATE) {
    rotate_pos = MAX_ROTATE;
  }
  if (tilt_spd != 0) {
    Serial.print("TILT POS: ");
    Serial.print(tilt_pos);
    Serial.print("\n");
  }
  if (rotate_spd != 0) {
    Serial.print("ROTATE POS: ");
    Serial.print(rotate_pos);
    Serial.print("\n");
  }

  tilt_servo.write(tilt_pos);
  last_tilt = tilt_pos;

  rotate_servo.write(rotate_pos);
  last_rotate = rotate_pos;
}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, MQTT_PORT);
  client.setCallback(handle_mqtt);

  pinMode(BLOWER_PIN, OUTPUT);
  tilt_servo.attach(TILT_PIN);
  rotate_servo.attach(ROTATE_PIN);
  tilt_servo.write(90);
  rotate_servo.write(90);
  last_tilt = 90;
  last_rotate = 90;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  activate_moving_parts();
  delay(SERVO_DELAY);
}
