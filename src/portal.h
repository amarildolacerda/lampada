#pragma once

#include <ESP8266WiFi.h>
#include <WiFiManager.h>

// =============================================
//  CONSTANTES PARA EEPROM E CONFIGURACOES
// =============================================
#define EEPROM_SIZE 128
#define PIN_CONFIG_ADDR 0
#define NOME_CONFIG_ADDR 1
#define TIMEOUT_CONFIG_ADDR 2
#define MAGIC_NUMBER_ADDR 100
#define MAGIC_NUMBER 0xA5
// #define DEFAULT_RELE_PIN D8 - movido para platformio.ini
// #define DEFAULT_BUTTON_PIN D2
#define AP_NAME "ESP-Store"
#define BUTTON_HOLD_TIME_MS 5000 // Tempo para reset (5 segundos)
// #define MAX_TIMED_ON_MINUTES 60      // 1 hora

// Lista de pinos seguros no ESP8266
extern const int pinos_validos[];
extern const int num_pinos_validos;

extern bool portalAtivo;
extern bool shouldSaveConfig;
extern bool lampadaLigada;
extern String nome_alexa;
extern int rele_pin;
extern int timeout_minutes;

void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
String gerarAPName();
void iniciarPortalConfiguracao();
bool isPinoSeguro(int pino);
void salvarPinoEEPROM(int pino);
void salvarNomeEEPROM(String nome);
void salvarTimeoutEEPROM(int timeout);