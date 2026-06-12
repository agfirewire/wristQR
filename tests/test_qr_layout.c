#include <stdio.h>
#include "qr_layout.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
  printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void) {
  /* Spec worst case: v10 = 57 modules, +8 quiet = 65; 144/65 = 2 px/module */
  CHECK(qr_scale_for(57, 144) == 2);
  /* drawn = 65*2 = 130; margin = (144-130)/2 = 7; data starts at 7 + 4*2 = 15 */
  CHECK(qr_data_origin(57, 144, 2) == 15);

  /* v2 = 25 modules, +8 = 33; 144/33 = 4 */
  CHECK(qr_scale_for(25, 144) == 4);
  /* drawn = 132; margin = 6; data at 6 + 16 = 22 */
  CHECK(qr_data_origin(25, 144, 4) == 22);

  /* v1 = 21 modules, +8 = 29; 144/29 = 4 */
  CHECK(qr_scale_for(21, 144) == 4);

  /* Too dense to fit at even 1px: v40 = 177 modules */
  CHECK(qr_scale_for(177, 144) == 0);

  printf(failures ? "test_qr_layout: %d FAILURES\n" : "test_qr_layout: OK\n", failures);
  return failures ? 1 : 0;
}
