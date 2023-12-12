#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <ESP32Servo.h>
#include <Keypad.h>

// WiFi
const char* ssid = "CovidWiFi"; 
const char* wifi_password = "12345678"; 
WiFiClient espClient;
PubSubClient client(espClient);

// MQTT Broker
const char *mqtt_broker = "132.248.51.184";//"mqtt-dashboard.com"; //"192.168.59.119";
const char *topic = "data_unam";
const char *topic2 = "control_unam";
//const char *mqtt_username = "1234";
//const char *mqtt_password = "1234";
const int mqtt_port = 1883; //1883;

// DHT11
#define DHTPIN 4    // Pin digital conectado al sensor DHT
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

// HC-SR04
const int trigPin = 5;
const int echoPin = 18;
#define SOUND_SPEED 0.034 // Velocidad del sonido en cm/uS
#define CM_TO_INCH 0.393701
long duration;
float distanceCm;
const int NUM_SAMPLES = 10; // Número de muestras para la moda
float distances[NUM_SAMPLES]; // Arreglo para almacenar las distancias medidas

// Pin del LED
const int ledPin = 22;

// Bomba de agua
const int pump = 2;
const float limit = 24.1;

// Servo
#define PIN_SG90 17 // Pin de salida utilizado
Servo sg90;

// Lógica del payload
int firstComma, secondComma, thirdComma, fourthComma, fifthComma;
float water_amount, max_humidity;
int activate_pump, servo;
String password_in;

// Datos
float aux_temperature;
float aux_humidity;

// Fotoresistencia
#define LIGHT_SENSOR_PIN 36 // GPIO36 (ADC0) , ESP32 pin GIOP15 (ADC13)

// Teclado
#define ROW_NUM     4 // Cuatro filas
#define COLUMN_NUM  4 // Cuatro columnas

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM]      = {13, 12, 14, 27}; // GPIO19, GPIO18, GPIO5, GPIO17 conectados a los pines de fila
byte pin_column[COLUMN_NUM] = {26, 25, 33, 32};   // GPIO16, GPIO4, GPIO0, GPIO2 conectados a los pines de columna

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );
const String password = "7890"; // cambia tu contraseña aquí
String input_password; 
char key;
int is_access;

void setup() {
    Serial.println("*********************************************************************");
    // Establece la velocidad de comunicación serial a 115200 bits por segundo
    Serial.begin(115200);
    // Conectándose a una red WiFi
    WiFi.begin(ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Conectándose a WiFi..");
    }
    Serial.println("Conectado a la red Wi-Fi");
    Serial.println(WiFi.RSSI());  // fuerza de la señal

    // Conectándose a un broker MQTT
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);

    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("El cliente %s se conecta al broker MQTT público\n", client_id.c_str());
        if (client.connect(client_id.c_str())) {
            Serial.println("Conectado al broker MQTT público EMQX");
        } else {
            Serial.print("Error con estado ");
            Serial.println(client.state());
            /*#define MQTT_CONNECTION_TIMEOUT     -4
              #define MQTT_CONNECTION_LOST        -3
              #define MQTT_CONNECT_FAILED         -2
              #define MQTT_DISCONNECTED           -1
              #define MQTT_CONNECTED               0
              #define MQTT_CONNECT_BAD_PROTOCOL    1
              #define MQTT_CONNECT_BAD_CLIENT_ID   2
              #define MQTT_CONNECT_UNAVAILABLE     3
              #define MQTT_CONNECT_BAD_CREDENTIALS 4
              #define MQTT_CONNECT_UNAUTHORIZED    5 */
            delay(2000);
        }
    }
    // Publicar y suscribir
    client.subscribe(topic2);
    delay(2000);
    //client.publish(topic2, "Prueba");
    Serial.print("****Conectado al topic: "); Serial.print(topic2); Serial.println("****");
    delay(5000);

    pinMode(ledPin, OUTPUT);

  // Pines para HC-SR04
  pinMode(trigPin, OUTPUT); // Establece el trigPin como salida
  pinMode(echoPin, INPUT); // Establece el echoPin como entrada

  // Pin de la bomba
  pinMode(pump, OUTPUT);

  // Pin del led
  pinMode(ledPin, OUTPUT);

  // Servo
  sg90.setPeriodHertz(50); // Frecuencia PWM para SG90
  sg90.attach(PIN_SG90, 500, 2400); // Ancho de pulso mínimo y máximo (en µs) para ir de 0° a 180
 
  // Teclado
  input_password.reserve(32); // máxima cantidad de caracteres de entrada es 33, cambia si es necesario
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Mensaje recibido en el topic: ");
    Serial.println(topic2);
    Serial.print("Mensaje: "); 

    //[water_amount, pump_on/off, max_humidity, password, servo_sun/shadow]
    // Crea un arreglo de caracteres para almacenar el payload
    char payloadArray[length + 1];
    
    // Copia los datos del payload al arreglo
    for (int i = 0; i < length; i++) {
        payloadArray[i] = (char)payload[i];
    }

    // Añade el carácter nulo al final del arreglo
    payloadArray[length] = '\0';

    // Crea un objeto String con el arreglo
    String data = String(payloadArray);
    Serial.println(data);
    Serial.println("*****");

    // Extrae cada dato usando substring
    firstComma = data.indexOf(",");
    water_amount = data.substring(0, firstComma).toFloat(); 
    
    secondComma = data.indexOf(",", firstComma + 1);
    activate_pump = data.substring(firstComma + 1, secondComma).toInt();   

    thirdComma = data.indexOf(",", secondComma + 1);
    max_humidity = data.substring(secondComma + 1, thirdComma).toFloat();   

    fourthComma = data.indexOf(",", thirdComma + 1);
    password_in = data.substring(thirdComma + 1, fourthComma);   

    fifthComma = data.indexOf(",", fourthComma + 1);
    servo = data.substring(fourthComma + 1, fifthComma).toInt();   


    Serial.println(water_amount);
    Serial.println(activate_pump);
    Serial.println(max_humidity);
    Serial.println(password_in);
    Serial.println(servo);

    Serial.println("*****");


    // LÓGICA DE CONTROL

    // Si la humedad excede el máximo admitido, ignora encender la bomba de agua
    float humidity = dht.readHumidity();
    if (humidity <= max_humidity){
      // Activa la bomba de agua si así lo indica el control
      if (activate_pump == 1){
      float water_level = getDistanceCm();
      while (getDistanceCm() <= (water_level + water_amount)){
        Serial.println(getDistanceCm());
          digitalWrite(pump, HIGH);
      }
      digitalWrite(pump, LOW);
      }else {
        digitalWrite(pump, LOW);
      }
    }

    // Movimiento de la planta al sol en 1 y a la sombra en 0
    if (servo == 1){
      for (int pos = 0; pos <= 180; pos += 1) {
      sg90.write(pos);   
      }
    }else {
      for (int pos = 180; pos >= 0; pos -= 1) {
      sg90.write(pos);
      }
    }
}

void loop() {
  client.loop();

  key = keypad.getKey();
  if (key == '*'){
    is_access = getAccess();
  }
  
  if (is_access == 1){
    Serial.println("");
    digitalWrite(ledPin, HIGH);
  }else {
    int light_value = analogRead(LIGHT_SENSOR_PIN);
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float water_level = getDistanceCm();

    // Verifica si hubo errores en la lectura.
    // Si los datos del DHT son incorrectos, envía los últimos datos correctos leídos de este sensor.
    if (isnan(temperature) || isnan(humidity)) {
      Serial.print(F("Error al leer desde el sensor DHT!: "));
      temperature = aux_temperature;
      humidity = aux_humidity;
    }else{
      aux_temperature =  temperature;
      aux_humidity = humidity;
    }

    String mensaje = "[" + String(temperature) + "," + String(humidity) + "," + String(water_level) + "," + String(light_value) + "]";
    Serial.println(mensaje);
    client.publish(topic, mensaje.c_str());
  }
  delay(1000);
}

// FUNCIONES

float getDistanceCm() {
  // Limpia el trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Realiza varias mediciones
  for (int i = 0; i < NUM_SAMPLES; i++) {
    // Establece el trigPin en estado ALTO durante 10 microsegundos
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    // Lee el echoPin, devuelve el tiempo de viaje de la onda de sonido en microsegundos
    duration = pulseIn(echoPin, HIGH);
    // Calcula la distancia en centímetros y almacena en el arreglo
    distances[i] = duration * SOUND_SPEED / 2;
    //Serial.println(distances[i]);
    delay(10); // Espera breve entre mediciones
  }

  // Calcula la moda
  float mode = getMode(distances, NUM_SAMPLES);

  // Almacena la moda en la variable distanceCm
  distanceCm = mode;
  return distanceCm;
}


float getMode(float arr[], int size) {
  // Cuenta la frecuencia de cada valor en el array
  int max_freq = 0;
  float mode = arr[0];
  for (int i = 0; i < size; i++) {
    int actual_freq = 1;
    for (int j = i + 1; j < size; j++) {
      if (arr[i] == arr[j]) {
        actual_freq++;
      }
    }
    if (actual_freq > max_freq) {
      max_freq = actual_freq;
      mode = arr[i];
    }
  }
  return mode;
}

int getAccess(){
  Serial.print("Ingresa contraseña: ");
  input_password = ""; // limpia la contraseña de entrada
  while (input_password.length() < 5){
    key = keypad.getKey();
      if (key) {
    Serial.print(key);
    if (key == '*') {
      Serial.println("La entrada se ha borrado");
      input_password = ""; // limpia la contraseña de entrada
    } else if (key == '#') {
      //Serial.print(password_in);  Serial.print(" is equal to? ");  Serial.print(input_password);
      if (password_in == input_password) {
        Serial.println("");
        Serial.println("La contraseña es correcta, ¡ACCESO CONCEDIDO!");
        return 1;

      } else {
        Serial.println("");
        Serial.println("La contraseña es incorrecta, ¡ACCESO DENEGADO!");
        return 0;
      }

      input_password = ""; // limpia la contraseña de entrada
      } else {
      input_password += key; // añade un nuevo carácter a la cadena de contraseña introducida
      }
    }
  } return 0;
}

