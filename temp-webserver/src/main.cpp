#include <Arduino.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "index_page.h"

#define TASKSTACK1  1024 
#define TASKSTACK2  8192
#define TASKSTACK3  16384
#define TASKSTACK4  16384

// Pins
static const int led_pin = LED_BUILTIN;

// Task Handles
static TaskHandle_t task_1 = NULL; //blink task
static TaskHandle_t task_2 = NULL; //sensorsTask
static TaskHandle_t task_3 = NULL; //wifi setup task
static TaskHandle_t task_4 = NULL; //webserver task

// Queue Setting
static const uint8_t temp_msg_queue_len = 5;
static QueueHandle_t temp_msg_queue;
static const uint8_t adc_msg_queue_len = 5;
static QueueHandle_t adc_msg_queue;

// global vars
volatile uint16_t led_on_time = (300 / portTICK_PERIOD_MS);
volatile uint16_t led_off_time = (700 / portTICK_PERIOD_MS);

// temperature API, for internal sensor
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

String readTemperature();
String readADC();
String processor(const String& var);

// AsyncWebServer object on port 80
AsyncWebServer server(80);


void toggleLED(void *parameter){
  while(1){
    digitalWrite(led_pin, LOW);
    vTaskDelay(led_off_time);
    digitalWrite(led_pin, HIGH);
    vTaskDelay(led_on_time);
  }
}

void sensorHub(void *parameter){
  float temp_val = (temprature_sens_read() - 32) / 1.8;
  uint16_t raw_adc = analogRead(32);
  while(1){
    temp_val = (temprature_sens_read() - 32) / 1.8;
    if(xQueueSend(temp_msg_queue, (void *)&temp_val, 10) != pdTRUE){
      ESP_LOGW("sensorHub", "Temperature Queue Full!");
    }
    ESP_LOGI("sensor", "Core temperature: %f deg. C", temp_val);
    //onboard sensor discontinued thus constantly at 53.3 deg
    
    raw_adc = analogRead(33);
    if(xQueueSend(adc_msg_queue, (void *)&raw_adc, 10) != pdTRUE){
      ESP_LOGW("sensorHub", "ADC Queue Full!");
    }
    ESP_LOGI("sensor", "Raw ADC value at pin 33: %u", raw_adc);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void wifiStWs(void *parameter){
  Serial.begin(115200);
  WiFi.softAP(FACTORY_WIFI_SSID, FACTORY_WIFI_PSSWD);
  Serial.println(WiFi.softAPIP());
  while(1){
    vTaskDelay(led_on_time);
  }
}

void asnWebServ(void *parameter){
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readTemperature().c_str());
  });
  server.on("/adc", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readADC().c_str());
  });

  // Start server
  server.begin();

  while(1){
    vTaskDelay(led_on_time);
  }
}



void setup() {
  // Config GPIO
  pinMode(led_pin, OUTPUT);

  // Create queues
  temp_msg_queue = xQueueCreate(temp_msg_queue_len, sizeof(int));
  adc_msg_queue = xQueueCreate(adc_msg_queue_len, sizeof(int));

  // Create task: Task to run forever
  xTaskCreatePinnedToCore(    // use xTaskCreate in original FreeRTOS
              toggleLED,      // Function to be called
              "Toggle LED",   // Name of the task
              TASKSTACK1,     // Stack size (bytes in ESP32, words in FreeRTOS)
              NULL,           // Parameter to be passed to the function
              1,              // Task priority (0 to configMAX_PRIORITIES - 1)
              &task_1,        // Task handle - define a pointer to get stats, interact with other task, etc.
              1               // Core ID
  );

  xTaskCreatePinnedToCore(
              sensorHub,
              "Sensor Task",
              TASKSTACK2,
              NULL,
              2,
              &task_2,
              1
  );
  
  xTaskCreatePinnedToCore(
              wifiStWs,
              "WiFi AP WS",
              TASKSTACK3,
              NULL,
              3,
              &task_3,
              1
  );

  xTaskCreatePinnedToCore(
              asnWebServ,
              "Async Webserver",
              TASKSTACK4,
              NULL,
              4,
              &task_4,
              1
  );
}

void loop() {
  // put your main code here, to run repeatedly:
}

String readTemperature(){
  float t = 0.0;
  if(xQueueReceive(temp_msg_queue, (void *)&t, 10) == pdTRUE){
    ESP_LOGV("tempQ","Received Packet value: %f", t);
  }

  // Check if any reads failed and exit early (to try again).
  if(t == 0){    
    // Serial.println("Failed to read from DHT sensor!");
    return "--";
  }else{
    return String(t);
  }
}

String readADC(){
  uint16_t a = 0;
  if(xQueueReceive(adc_msg_queue, (void *)&a, 10) == pdTRUE){
    ESP_LOGV("adcQ","Received Packet value: %u", a);
  }
  if(a == 0){
    return "--";
  }else {
    return String(a);
  }
}

// Replaces placeholder with Queue values
String processor(const String& var){
  if(var == "TEMPERATURE"){
    return readTemperature();
  }
  else if(var == "ADC"){
    return readADC();
  }
  return String();
}