#include <pebble.h>
#include "app_state.h"
#include "storage.h"
#include "menu_window.h"

QrList g_list;

int main(void) {
  storage_load(&g_list);
  menu_window_push();
  app_event_loop();
}
