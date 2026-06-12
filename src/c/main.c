#include <pebble.h>
#include "app_state.h"
#include "storage.h"
#include "menu_window.h"

QrList g_list;

/* TEMPORARY seed so menu/QR are testable before sync lands. Removed in Task 9. */
static void seed_demo_entries(void) {
  if (g_list.count > 0) return;
  strcpy(g_list.entries[0].label, "WhatsApp");
  strcpy(g_list.entries[0].text, "https://wa.me/qr/DEMO12345");
  strcpy(g_list.entries[1].label, "LinkedIn");
  strcpy(g_list.entries[1].text, "https://www.linkedin.com/in/demo");
  g_list.count = 2;
}

int main(void) {
  storage_load(&g_list);
  seed_demo_entries();
  menu_window_push();
  app_event_loop();
}
