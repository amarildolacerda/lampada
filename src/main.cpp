#include <Arduino.h>

#ifdef MQTT_SERVER
#include "mainServer.h"
#else
#include "mainClient.h"
#endif

void setup()
{
#ifdef MQTT_SERVER
  setup_server();
#else
  setup_client();
#endif
}

void loop()
{
#ifdef MQTT_SERVER
  loop_server();
#else
  loop_client();
#endif
}
