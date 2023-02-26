#include "MyMqtt.h"

static char TAG[] = "MyMqtt";

/* 内部 */
static void initQueue();
static void initSema();
static void connectMqtt();
static void pubSubErr(int8_t MQTTErr);
static void clearMqttData(MQTTData* p, size_t topic_len, size_t data_len);
static void mqttCallback(char* topic, byte* payload, unsigned int length);
/* 内部 */

WiFiClientSecure              httpsClient;
PubSubClient                  mqttClient(httpsClient);
volatile QueueHandle_t        pubQueue = NULL;
static volatile QueueHandle_t subQueue = NULL;

volatile SemaphoreHandle_t pub_success_semaphore_handle = NULL;
volatile SemaphoreHandle_t pub_failed_semaphore_handle = NULL;

/*
参考
https://github.com/debsahu/ESP-MQTT-AWS-IoT-Core/blob/master/Arduino/PubSubClient/PubSubClient.ino

IoT Core KeepAlive時間
https://blog.denet.co.jp/aws-iot-monitoring-temperature/
https://docs.aws.amazon.com/ja_jp/general/latest/gr/iot-core.html
*/


void initMqtt() {
  httpsClient.setCACert(ROOT_CA);
  httpsClient.setCertificate(CLIENT_CERT);
  httpsClient.setPrivateKey(PRIVATE_KEY);
  mqttClient.setServer(MQTT_ENDPOINT, MQTT_PORT);
  mqttClient.setBufferSize(2048);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(1200);
  mqttClient.setSocketTimeout(1200);
  connectMqtt();
  initQueue();
  initSema();
  ESP_LOGI(TAG, "Buffer Size: %d", mqttClient.getBufferSize());
}

void mqttTask(void* pvParameters) {
  static MQTTData mqtt_data;
  int             retry_count = 0;
  int             max_retry   = 5;
  while (1) {
    if (!mqttClient.loop()) {
      connectMqtt();
      mqttClient.loop();
    }

    /* Publish Retry */
    if (retry_count > 0) {
      if (mqttClient.publish(mqtt_data.topic, mqtt_data.data)) {
        retry_count = 0;
        continue;
      } else {
        retry_count++;
        delay(1500);
      }
      if (retry_count > max_retry) {
        retry_count = 0;
      }
      continue;
    }

    if (xQueueReceive(pubQueue, &mqtt_data, 0) == pdPASS) {
      if (!mqttClient.publish(mqtt_data.topic, mqtt_data.data)) {
        retry_count++;
        ESP_LOGE(TAG, "publish error");
        xSemaphoreGive(pub_failed_semaphore_handle);
      } else {
        ESP_LOGI(TAG, "Publish success");
        xSemaphoreGive(pub_success_semaphore_handle);
      }
      ESP_LOGI(TAG, "(publish) Topic: %s, Data: %s", mqtt_data.topic, mqtt_data.data);
    }
    delay(2000);
  }
  vTaskDelete(NULL);
}


static void clearMqttData(MQTTData* p, size_t topic_len, size_t data_len) {
  memset(p->topic, '\0', topic_len);
  memset(p->data, '\0', data_len);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  ESP_LOGI(TAG, "Message arrived");
  ESP_LOGI(TAG, "topic: %s", topic);
  static MQTTData receive_data;
  if (length < sizeof(receive_data.data)) {
    for (int i = 0; i < length; i++) {
      receive_data.data[i] = (char)payload[i];
    }
    sprintf(receive_data.topic, "%s", topic);
    xQueueSend(subQueue, &receive_data, 0);
  } else {
    ESP_LOGE(TAG, "Message length: %d is too larger(buf) %d", length, sizeof(receive_data.data));
  }
}

static void initQueue() {
  pubQueue = xQueueCreate(5, sizeof(MQTTData));
  subQueue = xQueueCreate(2, sizeof(MQTTData));
  if (pubQueue == NULL || subQueue == NULL) {
    ESP_LOGE(TAG, "Queueの作成に失敗しました");
    delay(3000);
    ESP.restart();
  }
}

static void initSema() {
  pub_success_semaphore_handle = xSemaphoreCreateBinary();
  pub_failed_semaphore_handle = xSemaphoreCreateBinary();
}

static void connectMqtt() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(THING_NAME)) {
      if (!mqttClient.subscribe(CONF_SUB_TOPIC)) {
        pubSubErr(mqttClient.state());
      }
    } else {
      ESP_LOGE(TAG, "MQTT Connect Failed");
      pubSubErr(mqttClient.state());
      ESP_LOGI(TAG, "try again in 1 seconds");
      delay(1000);
    }
  }
}

static void pubSubErr(int8_t MQTTErr) {
  switch (MQTTErr) {
    case MQTT_CONNECTION_TIMEOUT:
      ESP_LOGE(TAG, "Connection tiemout");
      break;
    case MQTT_CONNECTION_LOST:
      ESP_LOGE(TAG, "Connection lost");
      break;
    case MQTT_CONNECT_FAILED:
      ESP_LOGE(TAG, "Connect failed");
      break;
    case MQTT_DISCONNECTED:
      ESP_LOGE(TAG, "Disconnected");
      break;
    case MQTT_CONNECTED:
      ESP_LOGI(TAG, "Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      ESP_LOGE(TAG, "Connect bad protocol");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      ESP_LOGE(TAG, "Connect bad Client-ID");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      ESP_LOGE(TAG, "Connect unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      ESP_LOGE(TAG, "Connect bad credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      ESP_LOGE(TAG, "Connect unauthorized");
      break;
    default:
      ESP_LOGE(TAG, "default err");
      break;
  }
}
