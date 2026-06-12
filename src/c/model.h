#pragma once
#include <stdbool.h>
#include <string.h>

#define MAX_ENTRIES 8
#define LABEL_LEN 21   /* 20 chars + NUL */
#define TEXT_LEN 201   /* 200 chars + NUL */

typedef struct {
  char label[LABEL_LEN];
  char text[TEXT_LEN];
} QrEntry;

typedef struct {
  QrEntry entries[MAX_ENTRIES];
  int count;
} QrList;

/* Sync staging: entries land here and only commit when complete. */
typedef struct {
  QrEntry entries[MAX_ENTRIES];
  bool received[MAX_ENTRIES];
} QrStaging;

void staging_reset(QrStaging *s);
/* Returns false on bad index/NULL/oversized text. Long labels are truncated. */
bool staging_add(QrStaging *s, int index, const char *label, const char *text);
/* True iff entries 0..count-1 have all been received. */
bool staging_complete(const QrStaging *s, int count);
void staging_to_list(const QrStaging *s, int count, QrList *out);
