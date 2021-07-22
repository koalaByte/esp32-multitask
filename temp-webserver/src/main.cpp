#include <Arduino.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define TASKSTACK1  1024 
#define TASKSTACK2  8192
#define TASKSTACK3  16384
#define TASKSTACK4  16384

// Pins
static const int led_pin = LED_BUILTIN;

// Task Handles
static TaskHandle_t task_1 = NULL; //blink task
static TaskHandle_t task_2 = NULL; //sensorsTask
static TaskHandle_t task_3 = NULL; //wifi comm. task
static TaskHandle_t task_4 = NULL; //wifi comm. task

// Queue Setting
static const uint8_t msg_queue_len = 5;
static QueueHandle_t msg_queue;

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

// IPv4 setup
IPAddress local_IP(192, 168, 1, 5);
// Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

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
    ESP_LOGI("sensor", "Core temperature: %f deg. C", temp_val);
    //sensor discontinued thus constantly at 53.3 deg
    
    raw_adc = analogRead(32);
    ESP_LOGI("sensor", "Raw ADC value at pin 32: %u", raw_adc);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void wifiStWs(void *parameter){
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    ESP_LOGE("wifi","STA Failed to configure");
  }
  WiFi.softAP(FACTORY_WIFI_SSID, FACTORY_WIFI_PSSWD);
  while(1){
    vTaskDelay(led_on_time);
  }
}

void setup() {
  // Config GPIO
  pinMode(led_pin, OUTPUT);

  // Create queue
  msg_queue = xQueueCreate(msg_queue_len, sizeof(int));

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
              wifiStWs,
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