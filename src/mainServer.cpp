#include <Arduino.h>
#include "mainServer.h"

#ifdef MQTT_SERVER

void setup_server()
{
  Serial.begin(115200);
  Serial.println("MQTT server mode ativo.");
  // Adicione aqui a inicialização do servidor MQTT.
}

void loop_server()
{
  // Adicione aqui o loop do servidor MQTT.
}

#endif
