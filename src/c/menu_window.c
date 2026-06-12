#include <pebble.h>
#include "menu_window.h"
#include "app_state.h"

static Window *s_window;
static MenuLayer *s_menu_layer;
static TextLayer *s_empty_layer;

static uint16_t get_num_rows(MenuLayer *layer, uint16_t section, void *ctx) {
  return g_list.count;
}

static void draw_row(GContext *gctx, const Layer *cell_layer,
                     MenuIndex *idx, void *ctx) {
  menu_cell_basic_draw(gctx, cell_layer, g_list.entries[idx->row].label,
                       NULL, NULL);
}

static void select_click(MenuLayer *layer, MenuIndex *idx, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_INFO, "select row %d", (int)idx->row);  /* wired in Task 7 */
}

static void update_visibility(void) {
  bool empty = (g_list.count == 0);
  layer_set_hidden(menu_layer_get_layer(s_menu_layer), empty);
  layer_set_hidden(text_layer_get_layer(s_empty_layer), !empty);
}

void menu_window_refresh(void) {
  if (!s_window) return;
  menu_layer_reload_data(s_menu_layer);
  update_visibility();
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_rows = get_num_rows,
    .draw_row = draw_row,
    .select_click = select_click,
  });
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));

  s_empty_layer = text_layer_create(GRect(8, 8, bounds.size.w - 16, bounds.size.h - 16));
  text_layer_set_font(s_empty_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_empty_layer, GTextAlignmentCenter);
  text_layer_set_text(s_empty_layer,
      "No codes yet. Add them in the watchapp settings on your phone.");
  layer_add_child(root, text_layer_get_layer(s_empty_layer));

  update_visibility();
}

static void window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_empty_layer);
  window_destroy(s_window);
  s_window = NULL;
}

void menu_window_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
