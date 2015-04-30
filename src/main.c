#include <pebble.h>
#include "settings.h"
  
int main(void) {
  init_settings_window();
  app_event_loop();
  deinit_settings_window();
}
