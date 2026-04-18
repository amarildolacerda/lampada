#pragma once

#include <ESP8266WiFi.h>
#include <WiFiManager.h>

// =============================================
//  CONSTANTES PARA EEPROM E CONFIGURACOES
// =============================================
#define EEPROM_SIZE 128
#define PIN_CONFIG_ADDR 0
#define NOME_CONFIG_ADDR 1
#define MAGIC_NUMBER_ADDR 100
#define MAGIC_NUMBER 0xA5
#define DEFAULT_RELE_PIN D8
#define DEFAULT_BUTTON_PIN D1
#define AP_NAME "ESP-Store"
#define BUTTON_HOLD_TIME_MS 5000     // Tempo para reset (5 segundos)
#define WIFI_TIMEOUT_RESET_MS 120000 // 60 segundos sem Wi-Fi = reset automático

// Lista de pinos seguros no ESP8266
extern const int pinos_validos[];
extern const int num_pinos_validos;

extern bool portalAtivo;
extern bool shouldSaveConfig;
extern bool lampadaLigada;
extern String nome_alexa;
extern int rele_pin;

void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
String gerarAPName();
void iniciarPortalConfiguracao();
bool isPinoSeguro(int pino);
void salvarPinoEEPROM(int pino);
void salvarNomeEEPROM(String nome);