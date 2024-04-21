#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const int TRIGGER_PIN = 14;
const int ECHO_PIN = 12;
const int RELE_OPERACAO_PIN = 27;
const int RELE_PARTIDA_PIN = 26;
bool CONECTOU = false;

long tempo_inicial = 0;
bool estado_rele_operacao = false;
bool estado_rele_partida = false;
int estado_reles = 0;
bool dataFetched = false;

WiFiClient client;

TaskHandle_t postTaskHandle = NULL;
TaskHandle_t getTaskHandle = NULL;

void postTask(void *pvParameters);
void getTask(void *pvParameters);

void setup() {
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELE_OPERACAO_PIN, OUTPUT);
  pinMode(RELE_PARTIDA_PIN, OUTPUT);

  Serial.begin(115200);
  WiFi.begin("Derli", "25112002"); // Substitua com suas credenciais

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }

  Serial.println("Conectado ao WiFi.");

  xTaskCreatePinnedToCore(
    postTask,         // Função a ser executada
    "postTask",       // Nome da tarefa
    10000,            // Tamanho da pilha
    NULL,             // Parâmetros da tarefa
    1,                // Prioridade da tarefa
    &postTaskHandle,  // Handle da tarefa
    0                 // Núcleo para execução (núcleo 0)
  );

  xTaskCreatePinnedToCore(
    getTask,          // Função a ser executada
    "getTask",        // Nome da tarefa
    10000,            // Tamanho da pilha
    NULL,             // Parâmetros da tarefa
    1,                // Prioridade da tarefa
    &getTaskHandle,   // Handle da tarefa
    1                 // Núcleo para execução (núcleo 1)
  );
}

void loop() {
  // O loop principal não faz nada aqui
}

void postTask(void *pvParameters) {
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      getData(); // Obtenha os dados da API

      // Verifique se doc["manual"] é false antes de chamar post
      if (!dataFetched["manual"]) {
        float distance = measure_distance();
        aciona_reles(distance);
        post(distance, estado_rele_operacao, estado_rele_partida);
      }

      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}



void getTask(void *pvParameters) {
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      getData();

      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

float measure_distance() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  long pulse_time = pulseIn(ECHO_PIN, HIGH);
  float distance = pulse_time / 58.0;

  return distance;
}

void aciona_reles(float distance) {
  if (distance > 60) {
    if (estado_reles != 0) {
      digitalWrite(RELE_PARTIDA_PIN, LOW);
      digitalWrite(RELE_OPERACAO_PIN, LOW);
      estado_rele_operacao = false;
      estado_rele_partida = false;
      estado_reles = 0;
    }
  } else if (distance <= 30) {
    if (estado_reles == 0) {
      digitalWrite(RELE_PARTIDA_PIN, HIGH);
      estado_rele_partida = true;
      estado_reles = 1;
      tempo_inicial = millis();
    } else if (estado_reles == 1 && millis() - tempo_inicial >= 5000) {
      digitalWrite(RELE_PARTIDA_PIN, LOW);
      digitalWrite(RELE_OPERACAO_PIN, HIGH);
      estado_rele_partida = false;
      estado_rele_operacao = true;
      estado_reles = 2;
    } else if (estado_reles == 2 && millis() - tempo_inicial < 5000) {
      digitalWrite(RELE_PARTIDA_PIN, LOW);
    }
  } else {
    if (estado_reles != 3) {
      digitalWrite(RELE_OPERACAO_PIN, HIGH);
      estado_rele_operacao = true;
      estado_reles = 3;
    }
  }
}

void post(float distancia, bool estado_rele_operacao, bool estado_rele_partida) {
  DynamicJsonDocument doc(200);

  doc["nome"] = "BELONE TESTE esp a";
  doc["rele_operacao"] = estado_rele_operacao;
  doc["rele_acionamento"] = estado_rele_partida;
  doc["corrente"] = corrente;

  String json;
  serializeJson(doc, json);

  HTTPClient http;
  http.begin("https://backendcoontroledasaguas-1uq94rfn6-belonedf.vercel.app/bomba/98");
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PUT(json);

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Error in HTTP request: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}


