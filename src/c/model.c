#include "model.h"

void staging_reset(QrStaging *s) {
  memset(s, 0, sizeof(*s));
}

bool staging_add(QrStaging *s, int index, const char *label, const char *text) {
  if (index < 0 || index >= MAX_ENTRIES || !label || !text) return false;
  if (strlen(text) >= TEXT_LEN) return false;       /* too long: reject whole entry */
  strncpy(s->entries[index].label, label, LABEL_LEN - 1);
  s->entries[index].label[LABEL_LEN - 1] = '\0';    /* long label: truncate */
  strcpy(s->entries[index].text, text);
  s->received[index] = true;
  return true;
}

bool staging_complete(const QrStaging *s, int count) {
  if (count < 0 || count > MAX_ENTRIES) return false;
  for (int i = 0; i < count; i++) {
    if (!s->received[i]) return false;
  }
  return true;
}

void staging_to_list(const QrStaging *s, int count, QrList *out) {
  out->count = count;
  for (int i = 0; i < count; i++) {
    out->entries[i] = s->entries[i];
  }
}
