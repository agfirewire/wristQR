#include <pebble.h>
#include "sync.h"
#include "model.h"
#include "app_state.h"
#include "storage.h"
#include "menu_window.h"
#include "qr_window.h"

static QrStaging s_staging;

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *done_t = dict_find(iter, MESSAGE_KEY_DONE);
  if (done_t) {
    Tuple *count_t = dict_find(iter, MESSAGE_KEY_COUNT);
    int count = count_t ? (int)count_t->value->int32 : -1;
    if (staging_complete(&s_staging, count)) {
      staging_to_list(&s_staging, count, &g_list);
      storage_save(&g_list);
      qr_window_pop_if_open();   /* viewed code may no longer exist */
      menu_window_refresh();
      APP_LOG(APP_LOG_LEVEL_INFO, "sync committed %d entries", count);
    } else {
      APP_LOG(APP_LOG_LEVEL_WARNING, "sync incomplete (count=%d), kept old list", count);
    }
    staging_reset(&s_staging);
    return;
  }

  Tuple *index_t = dict_find(iter, MESSAGE_KEY_INDEX);
  Tuple *label_t = dict_find(iter, MESSAGE_KEY_LABEL);
  Tuple *text_t = dict_find(iter, MESSAGE_KEY_TEXT);
  if (index_t && label_t && text_t) {
    int index = (int)index_t->value->int32;
    if (index == 0) staging_reset(&s_staging);  /* every sync starts at 0 */
    staging_add(&s_staging, index,
                label_t->value->cstring, text_t->value->cstring);
  }
}

void sync_init(void) {
  staging_reset(&s_staging);
  app_message_register_inbox_received(inbox_received);
  /* inbox: index + 21B label + 201B text + dict overhead; outbox unused */
  app_message_open(512, 64);
}
