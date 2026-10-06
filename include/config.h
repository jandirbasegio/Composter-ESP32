#pragma once

#include <Arduino.h>
#include <driver/gpio.h>

#include "secrets.h"

// ---- Pinos ----
constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t ONE_WIRE_PIN = 18;
constexpr uint8_t MQ_PIN = 34;  // ADC1, funciona com Wi-Fi ligado

// Comando de alimentacao do MQ-135 (vai na base/gate de um transistor ou MOSFET,
// nunca direto no aquecedor: ele puxa ~150 mA e o GPIO aguenta ~40 mA).
constexpr gpio_num_t MQ_POWER_PIN = GPIO_NUM_26;
// Nivel que LIGA o sensor: HIGH para NPN / MOSFET canal N (low-side),
// LOW para PNP / MOSFET canal P (high-side).
constexpr uint8_t MQ_POWER_ON_LEVEL = HIGH;

// LED azul da placa (GPIO2), apagado antes do deep sleep
constexpr gpio_num_t LED_PIN = GPIO_NUM_2;

// ---- Ciclo de energia ----
constexpr uint64_t SLEEP_INTERVAL_S = 60;  // TESTE (producao: 15 * 60)
constexpr uint64_t RETRY_INTERVAL_S = 2 * 60;   // se o envio falhar, tenta de novo em 2 min

// Aquecimento do MQ-135 antes da leitura. O ideal para leitura confiavel e
// bem maior (minutos); ajuste conforme a calibracao e o consumo da bateria.
constexpr uint32_t MQ_WARMUP_MS = 0;  // TESTE (producao: 5 * 60 * 1000)

// ---- Leituras ----
constexpr uint8_t MQ_SAMPLES = 16;     // media de amostras do ADC
constexpr uint8_t SENSOR_RETRIES = 3;  // tentativas por sensor

// ---- Rede ----
constexpr uint32_t WIFI_TIMEOUT_MS = 15000;
constexpr uint32_t HTTP_TIMEOUT_MS = 8000;
constexpr uint8_t SEND_RETRIES = 3;

// URL completa da API (IP do PC na rede local)
#define API_URL "http://" API_HOST ":3000/api/leituras"
