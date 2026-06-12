#include <pebble.h>
#include "storage.h"

#define PERSIST_KEY_SCHEMA 0
#define PERSIST_KEY_COUNT 1
#define PERSIST_KEY_LABEL(i) (10 + (i))
#define PERSIST_KEY_TEXT(i) (30 + (i))
#define SCHEMA_VERSION 1

void storage_load(QrList *list) {
  memset(list, 0, sizeof(*list));
  if (persist_read_int(PERSIST_KEY_SCHEMA) != SCHEMA_VERSION) return;
  int count = persist_read_int(PERSIST_KEY_COUNT);
  if (count < 0 || count > MAX_ENTRIES) return;
  for (int i = 0; i < count; i++) {
    if (persist_read_string(PERSIST_KEY_LABEL(i),
                            list->entries[i].label, LABEL_LEN) < 0 ||
        persist_read_string(PERSIST_KEY_TEXT(i),
                            list->entries[i].text, TEXT_LEN) < 0) {
      memset(list, 0, sizeof(*list));  /* partial data: treat as empty */
      return;
    }
  }
  list->count = count;
}

void storage_save(const QrList *list) {
  persist_write_int(PERSIST_KEY_SCHEMA, SCHEMA_VERSION);
  persist_write_int(PERSIST_KEY_COUNT, list->count);
  for (int i = 0; i < list->count; i++) {
    persist_write_string(PERSIST_KEY_LABEL(i), list->entries[i].label);
    persist_write_string(PERSIST_KEY_TEXT(i), list->entries[i].text);
  }
  for (int i = list->count; i < MAX_ENTRIES; i++) {
    persist_delete(PERSIST_KEY_LABEL(i));
    persist_delete(PERSIST_KEY_TEXT(i));
  }
}
