#include <pebble.h>
#include "qr_window.h"
#include "app_state.h"
#include "qr_layout.h"
#include "qrcodegen.h"

#define LABEL_STRIP_H 24
#define QR_MAX_VERSION 10  /* spec cap: keeps modules scannable on 144px */

static Window *s_window;
static TextLayer *s_label_layer;
static Layer *s_qr_layer;
static int s_index;
static bool s_encode_ok;
static uint8_t s_qr[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
static uint8_t s_temp[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];

static void encode_current(void) {
  s_encode_ok = qrcodegen_encodeText(g_list.entries[s_index].text,
      s_temp, s_qr, qrcodegen_Ecc_MEDIUM,
      qrcodegen_VERSION_MIN, QR_MAX_VERSION, qrcodegen_Mask_AUTO, true);
  text_layer_set_text(s_label_layer, g_list.entries[s_index].label);
  layer_mark_dirty(s_qr_layer);
}

static void draw_error(GContext *ctx, GRect bounds) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "Can't display this code",
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(4, 40, bounds.size.w - 8, 80),
      GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void qr_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (!s_encode_ok) {
    draw_error(ctx, bounds);
    return;
  }

  int modules = qrcodegen_getSize(s_qr);
  int area = bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h;
  int scale = qr_scale_for(modules, area);
  if (scale < 1) {
    draw_error(ctx, bounds);
    return;
  }
  int ox = qr_data_origin(modules, bounds.size.w, scale);
  int oy = qr_data_origin(modules, bounds.size.h, scale);

  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int y = 0; y < modules; y++) {
    for (int x = 0; x < modules; x++) {
      if (qrcodegen_getModule(s_qr, x, y)) {
        graphics_fill_rect(ctx,
            GRect(ox + x * scale, oy + y * scale, scale, scale),
            0, GCornerNone);
      }
    }
  }
}

static void up_click(ClickRecognizerRef recognizer, void *ctx) {
  if (g_list.count < 2) return;
  s_index = (s_index + g_list.count - 1) % g_list.count;
  encode_current();
}

static void down_click(ClickRecognizerRef recognizer, void *ctx) {
  if (g_list.count < 2) return;
  s_index = (s_index + 1) % g_list.count;
  encode_current();
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

static void window_appear(Window *window) {
  light_enable(true);   /* hold backlight on while someone scans */
}

static void window_disappear(Window *window) {
  light_enable(false);  /* restore automatic backlight */
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);

  s_label_layer = text_layer_create(GRect(0, 0, bounds.size.w, LABEL_STRIP_H));
  text_layer_set_font(s_label_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_label_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_label_layer, GColorWhite);
  text_layer_set_text_color(s_label_layer, GColorBlack);
  layer_add_child(root, text_layer_get_layer(s_label_layer));

  s_qr_layer = layer_create(GRect(0, LABEL_STRIP_H, bounds.size.w,
                                  bounds.size.h - LABEL_STRIP_H));
  layer_set_update_proc(s_qr_layer, qr_layer_update);
  layer_add_child(root, s_qr_layer);
}

static void window_unload(Window *window) {
  text_layer_destroy(s_label_layer);
  layer_destroy(s_qr_layer);
  window_destroy(s_window);
  s_window = NULL;
}

void qr_window_push(int index) {
  s_index = index;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_set_click_config_provider(s_window, click_config);
  window_stack_push(s_window, true);
  encode_current();
}

void qr_window_pop_if_open(void) {
  if (s_window) {
    window_stack_remove(s_window, false);
  }
}
