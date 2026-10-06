#include <Arduino.h>
#include <DHT.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <WiFi.h>

#include "config.h"

DHT dht(DHT_PIN, DHT22);
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);

struct Leituras {
  float tempAmbiente;
  float umidAmbiente;
  float tempComposteira;
  int gasRaw;
};

// Sobrevive ao deep sleep (RTC memory), util para diagnostico
RTC_DATA_ATTR uint32_t bootCount = 0;

bool lerDht(Leituras &l) {
  for (uint8_t i = 0; i < SENSOR_RETRIES; i++) {
    l.umidAmbiente = dht.readHumidity();
    l.tempAmbiente = dht.readTemperature();
    if (!isnan(l.umidAmbiente) && !isnan(l.tempAmbiente)) return true;
    delay(2000);  // DHT22 precisa de ~2 s entre leituras
  }
  return false;
}

bool lerDs18b20(Leituras &l) {
  for (uint8_t i = 0; i < SENSOR_RETRIES; i++) {
    ds18b20.requestTemperatures();
    l.tempComposteira = ds18b20.getTempCByIndex(0);
    if (l.tempComposteira != DEVICE_DISCONNECTED_C) return true;
    delay(500);
  }
  return false;
}

int lerGas() {
  uint32_t soma = 0;
  for (uint8_t i = 0; i < MQ_SAMPLES; i++) {
    soma += analogRead(MQ_PIN);
    delay(10);
  }
  return soma / MQ_SAMPLES;
}

void ligarMq(bool ligado) {
  digitalWrite(MQ_POWER_PIN, ligado ? MQ_POWER_ON_LEVEL : !MQ_POWER_ON_LEVEL);
}

bool conectarWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - inicio > WIFI_TIMEOUT_MS) return false;
    delay(250);
  }
  Serial.printf("Wi-Fi OK, IP %s\n", WiFi.localIP().toString().c_str());
  return true;
}

bool enviarLeituras(const Leituras &l) {
  char corpo[160];
  snprintf(corpo, sizeof(corpo),
           "{\"temperatura_ambiente\":%.2f,\"umidade_ambiente\":%.2f,"
           "\"temperatura_composteira\":%.2f,\"gas_amonia_raw\":%d}",
           l.tempAmbiente, l.umidAmbiente, l.tempComposteira, l.gasRaw);

  for (uint8_t i = 1; i <= SEND_RETRIES; i++) {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");
    int codigo = http.POST(corpo);
    http.end();

    Serial.printf("POST tentativa %u: HTTP %d\n", i, codigo);
    if (codigo == 201) return true;
    if (codigo == 400) return false;  // payload invalido, repetir nao adianta
    delay(2000);
  }
  return false;
}

[[noreturn]] void dormir(uint64_t segundos) {
  Serial.printf("Dormindo %llu s\n", segundos);
  Serial.flush();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Garante o MQ desligado e trava o nivel do pino durante o deep sleep
  ligarMq(false);
  gpio_hold_en(MQ_POWER_PIN);

  // Apaga o LED azul e trava o nivel durante o sleep
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  gpio_hold_en(LED_PIN);

  gpio_deep_sleep_hold_en();
  esp_sleep_enable_timer_wakeup(segundos * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  bootCount++;
  Serial.printf("\nCiclo #%u\n", bootCount);

  // Libera o pino travado no sleep anterior e liga o MQ-135
  gpio_hold_dis(MQ_POWER_PIN);
  gpio_hold_dis(LED_PIN);
  pinMode(MQ_POWER_PIN, OUTPUT);
  ligarMq(true);

  dht.begin();
  ds18b20.begin();
  analogReadResolution(12);
  analogSetPinAttenuation(MQ_PIN, ADC_11db);

  // Aquece o MQ-135 antes de medir, com o Wi-Fi ainda desligado
  if (MQ_WARMUP_MS > 0) {
    Serial.printf("Aquecendo MQ-135 por %u s...\n", (unsigned)(MQ_WARMUP_MS / 1000));
    delay(MQ_WARMUP_MS);
  }

  Leituras l{};
  bool ok = lerDht(l);
  if (!ok) Serial.println("Falha no DHT22");
  if (!lerDs18b20(l)) {
    Serial.println("Falha no DS18B20 (cheque o resistor de pull-up e os cabos)");
    ok = false;
  }
  l.gasRaw = lerGas();
  ligarMq(false);  // ja mediu: desliga o aquecedor antes de usar o Wi-Fi

  Serial.printf("Amb: %.2f C / %.2f %% | Composteira: %.2f C | Gas: %d\n",
                l.tempAmbiente, l.umidAmbiente, l.tempComposteira, l.gasRaw);

  // A API devolve 400 se faltar campo numerico, entao nao enviamos NaN
  if (!ok) dormir(RETRY_INTERVAL_S);

  if (!conectarWifi()) {
    Serial.println("Sem Wi-Fi");
    dormir(RETRY_INTERVAL_S);
  }

  dormir(enviarLeituras(l) ? SLEEP_INTERVAL_S : RETRY_INTERVAL_S);
}

void loop() {}  // nunca chega aqui: o ESP32 reinicia a cada despertar
