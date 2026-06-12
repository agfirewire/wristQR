#include <stdio.h>
#include "model.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
  printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
  QrStaging s;
  char long_text[TEXT_LEN + 50];

  /* reset clears received flags */
  staging_reset(&s);
  for (int i = 0; i < MAX_ENTRIES; i++) CHECK(!s.received[i]);

  /* valid add */
  CHECK(staging_add(&s, 0, "WhatsApp", "https://wa.me/qr/ABC123"));
  CHECK(s.received[0]);
  CHECK(strcmp(s.entries[0].label, "WhatsApp") == 0);
  CHECK(strcmp(s.entries[0].text, "https://wa.me/qr/ABC123") == 0);

  /* bad index / NULLs rejected */
  CHECK(!staging_add(&s, -1, "x", "y"));
  CHECK(!staging_add(&s, MAX_ENTRIES, "x", "y"));
  CHECK(!staging_add(&s, 1, NULL, "y"));
  CHECK(!staging_add(&s, 1, "x", NULL));

  /* exactly 200 chars accepted, 201 rejected */
  memset(long_text, 'a', sizeof(long_text));
  long_text[200] = '\0';
  CHECK(staging_add(&s, 1, "Max", long_text));
  long_text[200] = 'a'; long_text[201] = '\0';
  CHECK(!staging_add(&s, 2, "TooLong", long_text));
  CHECK(!s.received[2]);

  /* over-long label truncated to 20 chars */
  CHECK(staging_add(&s, 2, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "t"));
  CHECK(strlen(s.entries[2].label) == 20);
  CHECK(strcmp(s.entries[2].label, "ABCDEFGHIJKLMNOPQRST") == 0);

  /* completeness: 0..2 received -> complete(3) true, complete(4) false */
  CHECK(staging_complete(&s, 3));
  CHECK(!staging_complete(&s, 4));
  CHECK(staging_complete(&s, 0));               /* empty sync is valid */
  CHECK(!staging_complete(&s, MAX_ENTRIES + 1)); /* count out of range */
  CHECK(!staging_complete(&s, -1));

  /* commit copies exactly count entries */
  QrList list;
  staging_to_list(&s, 3, &list);
  CHECK(list.count == 3);
  CHECK(strcmp(list.entries[0].label, "WhatsApp") == 0);
  CHECK(strcmp(list.entries[2].label, "ABCDEFGHIJKLMNOPQRST") == 0);

  /* reset wipes a previous session's flags */
  staging_reset(&s);
  CHECK(!staging_complete(&s, 1));

  printf(failures ? "test_model: %d FAILURES\n" : "test_model: OK\n", failures);
  return failures ? 1 : 0;
}
