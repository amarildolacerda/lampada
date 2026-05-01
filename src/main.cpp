#include <Arduino.h>

#ifdef LAMPADA_APP
#include "lampada/lampada_app.h"
#endif

#ifdef NIVEL_AGUA_APP
#include "nivel_agua/nivel_agua_app.h"
#endif

#ifdef TUYA_TEMP_APP
#include "tuya_temp/tuya_app.h"
#endif

void setup()
{
#ifdef LAMPADA_APP
  setup_client();
#endif

#ifdef NIVEL_AGUA_APP
  setup_nivel_agua();
#endif
#ifdef TUYA_TEMP_APP
  tuya_setup();
#endif
}

void loop()
{
#ifdef LAMPADA_APP
  loop_client();
#endif

#ifdef NIVEL_AGUA_APP
  loop_nivel_agua();
#endif
#ifdef TUYA_TEMP_APP
  tuya_loop();
#endif
}
