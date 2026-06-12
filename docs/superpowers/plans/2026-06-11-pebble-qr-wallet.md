# Pebble QR Wallet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Pebble 2 watchapp that stores up to 8 labeled QR codes (synced from a phone config page by pasting a URL or decoding a screenshot) and displays any of them full-screen for scanning.

**Architecture:** A C watchapp (diorite platform) owns the persisted list and renders QRs on-watch with the vendored `qrcodegen` library. A static HTML/JS config page (decodes screenshots with `jsQR`, previews with `qrcode-generator`) is the only editor; PebbleKit JS relays the saved list to the watch over AppMessage, committed atomically via a staging buffer.

**Tech Stack:** Pebble SDK 4.x via Rebble/Core Devices `pebble-tool` (Python 3.13 under uv), C11, vendored `qrcodegen` (Nayuki, MIT), vanilla HTML/JS config page with `jsQR` (Apache-2.0) and `qrcode-generator` (MIT), host-side C unit tests with `cc`.

**Spec:** `docs/superpowers/specs/2026-06-11-pebble-qr-wallet-design.md`

**Environment facts (verified 2026-06-11):** macOS arm64, Homebrew + Node installed, `cc` present, **no pebble tool installed**, system Python is 3.14 (too new — pin 3.13 via uv), no `uv` installed.

---

## File Structure

```
/Users/matt/Env/pebble/
├── package.json              # Pebble project manifest (messageKeys, diorite, configurable)
├── wscript                   # Pebble build script (generated; +NDEBUG flag)
├── .gitignore                # build/, build-host/
├── src/
│   ├── c/
│   │   ├── main.c            # entry point: load storage, init sync, push menu
│   │   ├── app_state.h       # extern QrList g_list
│   │   ├── model.h / model.c # QrEntry/QrList/QrStaging + staging logic (PURE, host-tested)
│   │   ├── qr_layout.h / .c  # scale & centering math (PURE, host-tested)
│   │   ├── storage.h / .c    # persist read/write glue (watch-only)
│   │   ├── menu_window.h / .c# MenuLayer of labels + empty state
│   │   ├── qr_window.h / .c  # label strip + QR render + up/down cycling + backlight
│   │   ├── sync.h / .c       # AppMessage inbox → staging → commit
│   │   ├── qrcodegen.h / .c  # VENDORED — do not edit
│   └── pkjs/
│       └── index.js          # config URL open + sequential AppMessage sync w/ retry
├── config/                   # static config page (later: GitHub Pages)
│   ├── index.html
│   ├── app.js
│   ├── jsQR.js               # VENDORED
│   └── qrcode.js             # VENDORED
└── tests/
    ├── run_tests.sh          # host-side build & run
    ├── test_model.c
    ├── test_qr_layout.c
    └── fixtures/             # sample QR png for decode testing
```

Pure modules (`model`, `qr_layout`) include no Pebble headers so they compile with host `cc` for TDD. Watch-only modules are thin glue verified in the emulator.

---

### Task 1: Install the Pebble toolchain

**Files:** none (machine setup)

- [ ] **Step 1: Install Rosetta 2** (SDK ships x86 binaries; this Mac is arm64)

```bash
softwareupdate --install-rosetta --agree-to-license
```

Expected: completes or reports already installed.

- [ ] **Step 2: Install uv**

```bash
brew install uv
```

Run: `uv --version` — expected: prints a version.

- [ ] **Step 3: Install pebble-tool pinned to Python 3.13** (does not support 3.14)

```bash
uv tool install pebble-tool --python 3.13
```

Run: `pebble --version` — expected: prints pebble-tool version. If `pebble` is not found, run `uv tool update-shell` and open a new shell.

- [ ] **Step 4: Install the SDK core**

```bash
pebble sdk install latest
```

Expected: downloads toolchain + QEMU, ends without error. Verify: `pebble sdk list` shows an installed SDK.

- [ ] **Step 5: Smoke-test the emulator**

```bash
cd /tmp && pebble new-project smoke && cd smoke && pebble build && pebble install --emulator diorite
```

Expected: build succeeds, QEMU window opens showing a Pebble 2 with the default app. Close the emulator: `pebble kill`. Clean up: `rm -rf /tmp/smoke`.

---

### Task 2: Scaffold the project

**Files:**
- Create: `package.json`, `wscript`, `src/c/qrwallet.c` (generated; deleted in Task 6), `src/pkjs/index.js` (generated; replaced in Task 9), `.gitignore`

- [ ] **Step 1: Generate a project and move it into the repo root**

```bash
cd /Users/matt/Env/pebble
pebble new-project qrwallet
mv qrwallet/package.json qrwallet/wscript .
rm -rf src && mv qrwallet/src .
rm -rf qrwallet
```

- [ ] **Step 2: Edit `package.json`** — keep the generated `uuid` line exactly as generated; set everything else to:

```json
{
  "name": "qrwallet",
  "author": "Matt Eggertson",
  "version": "1.0.0",
  "private": true,
  "keywords": ["pebble-app"],
  "dependencies": {},
  "pebble": {
    "displayName": "QR Wallet",
    "uuid": "<KEEP THE GENERATED VALUE>",
    "sdkVersion": "3",
    "enableMultiJS": true,
    "targetPlatforms": ["diorite"],
    "watchapp": { "watchface": false },
    "capabilities": ["configurable"],
    "messageKeys": ["INDEX", "LABEL", "TEXT", "DONE", "COUNT"],
    "resources": { "media": [] }
  }
}
```

`configurable` makes the gear icon appear in the phone app; `messageKeys` are auto-exposed to C as `MESSAGE_KEY_INDEX` etc. and to JS by name.

- [ ] **Step 3: Create `.gitignore`**

```
build/
build-host/
.DS_Store
```

- [ ] **Step 4: Build and run in the emulator**

```bash
pebble build && pebble install --emulator diorite
```

Expected: default generated app runs on the diorite emulator.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "chore: scaffold Pebble project (diorite, configurable, message keys)"
```

---

### Task 3: Vendor qrcodegen

**Files:**
- Create: `src/c/qrcodegen.c`, `src/c/qrcodegen.h`
- Modify: `wscript`

- [ ] **Step 1: Download the library (pinned tag for reproducibility)**

```bash
curl -fsSL https://raw.githubusercontent.com/nayuki/QR-Code-generator/v1.8.0/c/qrcodegen.c -o src/c/qrcodegen.c
curl -fsSL https://raw.githubusercontent.com/nayuki/QR-Code-generator/v1.8.0/c/qrcodegen.h -o src/c/qrcodegen.h
head -5 src/c/qrcodegen.h
```

Expected: header shows the Nayuki copyright/MIT banner.

- [ ] **Step 2: Add `-DNDEBUG` to the watch build** — qrcodegen uses `assert()`; NDEBUG compiles it out (Pebble's runtime has no abort handler). In `wscript`, inside `def build(ctx):`, find the per-platform loop and add one line right after the env assignment:

```python
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.env.append_value('CFLAGS', ['-DNDEBUG'])   # <-- add this line
```

- [ ] **Step 3: Verify it compiles into the app**

```bash
pebble build
```

Expected: build succeeds (qrcodegen.c is picked up by the `src/c/**/*.c` glob).

- [ ] **Step 4: Commit**

```bash
git add src/c/qrcodegen.c src/c/qrcodegen.h wscript
git commit -m "feat: vendor qrcodegen v1.8.0 (MIT), compile with NDEBUG"
```

---

### Task 4: Data model + staging state machine (TDD, host-side)

**Files:**
- Create: `src/c/model.h`, `src/c/model.c`, `tests/test_model.c`, `tests/run_tests.sh`

- [ ] **Step 1: Write the header** — `src/c/model.h` (pure: no Pebble includes):

```c
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
```

- [ ] **Step 2: Write the failing tests** — `tests/test_model.c`:

```c
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
```

- [ ] **Step 3: Write the test runner** — `tests/run_tests.sh`:

```bash
#!/bin/sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build-host
cc -std=c11 -Wall -Wextra -Werror -I src/c -o build-host/test_model tests/test_model.c src/c/model.c
./build-host/test_model
if [ -f tests/test_qr_layout.c ]; then
  cc -std=c11 -Wall -Wextra -Werror -I src/c -o build-host/test_qr_layout tests/test_qr_layout.c src/c/qr_layout.c
  ./build-host/test_qr_layout
fi
echo "ALL TESTS PASSED"
```

Then: `chmod +x tests/run_tests.sh`

- [ ] **Step 4: Run to verify failure**

Run: `tests/run_tests.sh`
Expected: FAIL — `cc` errors with `model.c: No such file or directory`.

- [ ] **Step 5: Implement** — `src/c/model.c`:

```c
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
```

- [ ] **Step 6: Run to verify pass**

Run: `tests/run_tests.sh`
Expected: `test_model: OK` then `ALL TESTS PASSED`.

- [ ] **Step 7: Commit**

```bash
git add src/c/model.h src/c/model.c tests/test_model.c tests/run_tests.sh
git commit -m "feat: data model and sync staging state machine with host tests"
```

---

### Task 5: QR layout math (TDD, host-side)

**Files:**
- Create: `src/c/qr_layout.h`, `src/c/qr_layout.c`, `tests/test_qr_layout.c`

- [ ] **Step 1: Write the header** — `src/c/qr_layout.h`:

```c
#pragma once

#define QR_QUIET_MODULES 4  /* white quiet zone on each side, in modules */

/* Whole pixels per module so (modules + 2*quiet) fits in area_px. 0 = can't fit. */
int qr_scale_for(int modules, int area_px);

/* Pixel offset (one axis) of the first DATA module, centering quiet zone + data. */
int qr_data_origin(int modules, int area_px, int scale);
```

- [ ] **Step 2: Write the failing tests** — `tests/test_qr_layout.c`:

```c
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
```

- [ ] **Step 3: Run to verify failure**

Run: `tests/run_tests.sh`
Expected: FAIL compiling test_qr_layout (`qr_layout.c: No such file or directory`).

- [ ] **Step 4: Implement** — `src/c/qr_layout.c`:

```c
#include "qr_layout.h"

int qr_scale_for(int modules, int area_px) {
  return area_px / (modules + 2 * QR_QUIET_MODULES);
}

int qr_data_origin(int modules, int area_px, int scale) {
  int total = modules + 2 * QR_QUIET_MODULES;
  int margin = (area_px - total * scale) / 2;
  return margin + QR_QUIET_MODULES * scale;
}
```

- [ ] **Step 5: Run to verify pass**

Run: `tests/run_tests.sh`
Expected: `test_model: OK`, `test_qr_layout: OK`, `ALL TESTS PASSED`.

- [ ] **Step 6: Commit**

```bash
git add src/c/qr_layout.h src/c/qr_layout.c tests/test_qr_layout.c
git commit -m "feat: QR scale and centering math with host tests"
```

---

### Task 6: App skeleton — storage, main, menu with empty state

**Files:**
- Create: `src/c/app_state.h`, `src/c/storage.h`, `src/c/storage.c`, `src/c/menu_window.h`, `src/c/menu_window.c`
- Create: `src/c/main.c`
- Delete: the generated scaffold source `src/c/qrwallet.c`

- [ ] **Step 1: `src/c/app_state.h`**

```c
#pragma once
#include "model.h"

extern QrList g_list;
```

- [ ] **Step 2: `src/c/storage.h`**

```c
#pragma once
#include "model.h"

void storage_load(QrList *list);   /* zeroes list on missing/invalid data */
void storage_save(const QrList *list);
```

- [ ] **Step 3: `src/c/storage.c`** — persist glue. Keys: schema=0, count=1, labels 10..17, texts 30..37 (matches spec data model):

```c
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
```

- [ ] **Step 4: `src/c/menu_window.h`**

```c
#pragma once

void menu_window_push(void);
void menu_window_refresh(void);  /* call after g_list changes */
```

- [ ] **Step 5: `src/c/menu_window.c`** — note: `qr_window.h` doesn't exist yet; the select handler logs for now and is wired up in Task 7:

```c
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
```

- [ ] **Step 6: Create `src/c/main.c`** (and delete the generated scaffold source: `rm -f src/c/qrwallet.c`):

```c
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
```

- [ ] **Step 7: Build and verify the empty state in the emulator**

```bash
pebble build && pebble install --emulator diorite
```

Expected: app launches showing the "No codes yet..." message. Back exits.

- [ ] **Step 8: Run host tests still pass**

Run: `tests/run_tests.sh` — expected: `ALL TESTS PASSED`.

- [ ] **Step 9: Commit**

```bash
git add -A && git commit -m "feat: app skeleton with persist storage and menu empty state"
```

---

### Task 7: QR display window (with temporary seed data)

**Files:**
- Create: `src/c/qr_window.h`, `src/c/qr_window.c`
- Modify: `src/c/menu_window.c` (wire select), `src/c/main.c` (temporary seed)

- [ ] **Step 1: `src/c/qr_window.h`**

```c
#pragma once

void qr_window_push(int index);
void qr_window_pop_if_open(void);  /* used when a sync replaces the list */
```

- [ ] **Step 2: `src/c/qr_window.c`**

```c
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
```

- [ ] **Step 3: Wire the menu select handler** — in `src/c/menu_window.c`, add `#include "qr_window.h"` to the includes and replace the `select_click` body:

```c
static void select_click(MenuLayer *layer, MenuIndex *idx, void *ctx) {
  qr_window_push(idx->row);
}
```

- [ ] **Step 4: Add TEMPORARY seed data** — in `src/c/main.c`, so the menu/QR can be tested before sync exists (removed in Task 9). Replace `main` with:

```c
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
```

- [ ] **Step 5: Build, run, and scan-test**

```bash
pebble build && pebble install --emulator diorite
```

Expected: menu shows "WhatsApp" and "LinkedIn". Select shows label strip + QR. Up/Down flips between the two codes with the label changing.

```bash
pebble screenshot --emulator diorite tests/fixtures/emulator-qr.png
```

**Scan `tests/fixtures/emulator-qr.png` with a phone camera** (open the file on screen, point phone at it). Expected: phone decodes `https://wa.me/qr/DEMO12345` (or whichever code is displayed).

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat: QR display window with label strip, code cycling, backlight hold"
```

---

### Task 8: Config page

**Files:**
- Create: `config/index.html`, `config/app.js`, `config/jsQR.js` (vendored), `config/qrcode.js` (vendored), `tests/fixtures/sample-qr.png`

- [ ] **Step 1: Vendor the JS libraries**

```bash
mkdir -p config
curl -fsSL https://unpkg.com/jsqr@1.4.0/dist/jsQR.js -o config/jsQR.js
curl -fsSL https://unpkg.com/qrcode-generator@1.4.4/qrcode.js -o config/qrcode.js
head -3 config/jsQR.js config/qrcode.js
```

Expected: both files non-empty with library banners/code.

- [ ] **Step 2: `config/index.html`**

```html
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>QR Wallet</title>
<style>
  body { font-family: -apple-system, sans-serif; margin: 16px; max-width: 420px; }
  h1 { font-size: 1.3em; }
  ul { list-style: none; padding: 0; }
  li { border: 1px solid #ccc; border-radius: 6px; padding: 8px; margin: 6px 0;
       display: flex; align-items: center; gap: 6px; }
  li .info { flex: 1; min-width: 0; }
  li .text { color: #666; font-size: 0.8em; overflow: hidden;
             text-overflow: ellipsis; white-space: nowrap; }
  label { display: block; margin: 8px 0; }
  input[type=text] { width: 100%; box-sizing: border-box; padding: 6px; }
  button { padding: 8px 14px; margin: 4px 4px 4px 0; }
  #save-btn { background: #ff4700; color: white; border: none;
              border-radius: 6px; font-size: 1em; }
  .error { color: #c00; }
  .warn { color: #a60; }
  #preview img { image-rendering: pixelated; }
</style>
</head>
<body>
  <h1>QR Wallet</h1>
  <ul id="entry-list"></ul>
  <p id="capacity"></p>

  <h2>Add a code</h2>
  <label>Label (max 20 chars)
    <input type="text" id="label-input" maxlength="20" placeholder="WhatsApp">
  </label>
  <label>URL / text
    <input type="text" id="text-input" placeholder="https://...">
  </label>
  <label>…or decode from a screenshot
    <input type="file" id="file-input" accept="image/*">
  </label>
  <div id="preview"></div>
  <p id="error" class="error"></p>
  <p id="warning" class="warn"></p>
  <button id="add-btn">Add</button>
  <hr>
  <button id="save-btn">Save to watch</button>

  <script src="jsQR.js"></script>
  <script src="qrcode.js"></script>
  <script src="app.js"></script>
</body>
</html>
```

- [ ] **Step 3: `config/app.js`** — limits mirror `src/c/model.h` (MAX_ENTRIES=8, label 20, text 200):

```js
var MAX_ENTRIES = 8;
var LABEL_MAX = 20;
var TEXT_MAX = 200;
var MAX_MODULES = 57; // QR version 10 — matches watch QR_MAX_VERSION

qrcode.stringToBytes = qrcode.stringToBytesFuncs['UTF-8'];

function utf8Length(s) {
  return new Blob([s]).size;
}

var entries = [];

function $(id) { return document.getElementById(id); }

(function init() {
  // Entries arrive as ?entries=<json>; the emulator may append &return_to=...
  var match = location.search.match(/[?&]entries=([^&]*)/);
  if (match) {
    try { entries = JSON.parse(decodeURIComponent(match[1])); }
    catch (e) { entries = []; }
  }
  renderList();
  $('add-btn').addEventListener('click', addEntry);
  $('save-btn').addEventListener('click', save);
  $('text-input').addEventListener('input', updatePreview);
  $('file-input').addEventListener('change', decodeScreenshot);
})();

function setError(msg) { $('error').textContent = msg || ''; }
function setWarning(msg) { $('warning').textContent = msg || ''; }

function buildQr(text) {
  // Returns a qrcode-generator object, or null if too dense for the watch.
  try {
    var qr = qrcode(0, 'M'); // type 0 = auto-pick smallest
    qr.addData(text);
    qr.make();
    if (qr.getModuleCount() > MAX_MODULES) return null;
    return qr;
  } catch (e) {
    return null; // length overflow = way too dense
  }
}

function updatePreview() {
  var text = $('text-input').value.trim();
  $('preview').innerHTML = '';
  setError('');
  if (!text) return;
  if (utf8Length(text) > TEXT_MAX) {
    setError('Too long to display scannably (' + utf8Length(text) + '/' + TEXT_MAX + ' bytes).');
    return;
  }
  var qr = buildQr(text);
  if (!qr) {
    setError('This code is too dense to scan reliably off the watch — try a shorter URL.');
    return;
  }
  $('preview').innerHTML = qr.createImgTag(4, 8);
}

function decodeScreenshot(evt) {
  var file = evt.target.files[0];
  if (!file) return;
  var img = new Image();
  img.onload = function () {
    var canvas = document.createElement('canvas');
    canvas.width = img.width;
    canvas.height = img.height;
    var ctx = canvas.getContext('2d');
    ctx.drawImage(img, 0, 0);
    var data = ctx.getImageData(0, 0, canvas.width, canvas.height);
    var result = jsQR(data.data, data.width, data.height);
    URL.revokeObjectURL(img.src);
    if (result && result.data) {
      $('text-input').value = result.data;
      updatePreview();
      setWarning('');
    } else {
      setError("Couldn't find a QR code in that image — try a tighter crop.");
    }
  };
  img.src = URL.createObjectURL(file);
}

function addEntry() {
  setError('');
  setWarning('');
  var label = $('label-input').value.trim().slice(0, LABEL_MAX);
  var text = $('text-input').value.trim();
  if (!label) { setError('Give this code a label.'); return; }
  if (!text) { setError('Paste a URL or decode a screenshot first.'); return; }
  if (utf8Length(text) > TEXT_MAX) {
    setError('Too long to display scannably (' + utf8Length(text) + '/' + TEXT_MAX + ' bytes).');
    return;
  }
  if (!buildQr(text)) {
    setError('This code is too dense to scan reliably off the watch — try a shorter URL.');
    return;
  }
  if (entries.length >= MAX_ENTRIES) {
    setError('The watch holds at most ' + MAX_ENTRIES + ' codes — delete one first.');
    return;
  }
  if (entries.some(function (e) { return e.label === label; })) {
    setWarning('Heads up: another code already has that label.');
  }
  entries.push({ label: label, text: text });
  $('label-input').value = '';
  $('text-input').value = '';
  $('preview').innerHTML = '';
  renderList();
}

function renderList() {
  var ul = $('entry-list');
  ul.innerHTML = '';
  entries.forEach(function (entry, i) {
    var li = document.createElement('li');

    var info = document.createElement('div');
    info.className = 'info';
    var strong = document.createElement('strong');
    strong.textContent = entry.label;
    var text = document.createElement('div');
    text.className = 'text';
    text.textContent = entry.text;
    info.appendChild(strong);
    info.appendChild(text);
    li.appendChild(info);

    var editBtn = document.createElement('button');
    editBtn.textContent = '✎';
    editBtn.onclick = (function (idx) {
      return function () {
        $('label-input').value = entries[idx].label;
        $('text-input').value = entries[idx].text;
        entries.splice(idx, 1);
        setError('');
        setWarning('');
        updatePreview();
        renderList();
      };
    }(i));
    li.appendChild(editBtn);

    [['↑', -1], ['↓', 1]].forEach(function (move) {
      var btn = document.createElement('button');
      btn.textContent = move[0];
      btn.onclick = function () {
        var j = i + move[1];
        if (j < 0 || j >= entries.length) return;
        var tmp = entries[i]; entries[i] = entries[j]; entries[j] = tmp;
        renderList();
      };
      li.appendChild(btn);
    });

    var del = document.createElement('button');
    del.textContent = '✕';
    del.onclick = function () { entries.splice(i, 1); renderList(); };
    li.appendChild(del);

    ul.appendChild(li);
  });
  $('capacity').textContent = entries.length + '/' + MAX_ENTRIES + ' codes';
}

function save() {
  // On the phone, pebblejs://close#<data> hands data to pkjs.
  // Under `pebble emu-app-config`, a return_to URL is supplied instead.
  var match = location.search.match(/[?&]return_to=([^&]*)/);
  var base = match ? decodeURIComponent(match[1]) : 'pebblejs://close#';
  document.location = base + encodeURIComponent(JSON.stringify(entries));
}
```

- [ ] **Step 4: Create a decode fixture** (uses Node, already installed):

```bash
mkdir -p tests/fixtures
npx --yes qrcode -o tests/fixtures/sample-qr.png "https://wa.me/qr/FIXTURE99"
```

Expected: `tests/fixtures/sample-qr.png` exists.

- [ ] **Step 5: Manual browser test**

```bash
python3 -m http.server 8000 --directory config &
open "http://localhost:8000/index.html"
```

Verify, in the browser:
1. Paste `https://www.linkedin.com/in/demo` + label "LinkedIn" → live preview appears → Add → appears in list.
2. Upload `tests/fixtures/sample-qr.png` via the file input → text field fills with `https://wa.me/qr/FIXTURE99`.
3. Paste a 250-char string → "Too long" error, Add blocked.
4. Add a second entry with label "LinkedIn" → duplicate warning shown, still added.
5. Reorder with ↑/↓ and delete with ✕ — list updates.

Then stop the server: `kill %1`

- [ ] **Step 6: Commit**

```bash
git add config tests/fixtures
git commit -m "feat: config page with screenshot decode, validation, live preview"
```

---

### Task 9: Sync — pkjs relay + watch AppMessage handling

**Files:**
- Create: `src/c/sync.h`, `src/c/sync.c`
- Replace: `src/pkjs/index.js`
- Modify: `src/c/main.c` (init sync, REMOVE seed)

- [ ] **Step 1: `src/c/sync.h`**

```c
#pragma once

void sync_init(void);
```

- [ ] **Step 2: `src/c/sync.c`** — message keys from `package.json` are auto-exposed as `MESSAGE_KEY_*` by the build:

```c
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
```

- [ ] **Step 3: Replace `src/pkjs/index.js`**

```js
// Change to the GitHub Pages URL when the config page is published.
var CONFIG_URL = 'http://localhost:8000/index.html';

var MAX_RETRIES = 3;

Pebble.addEventListener('showConfiguration', function () {
  var saved = localStorage.getItem('entries') || '[]';
  Pebble.openURL(CONFIG_URL + '?entries=' + encodeURIComponent(saved));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e.response) return; // user backed out without saving
  var entries;
  try {
    entries = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('Bad config response: ' + err);
    return;
  }
  if (!Array.isArray(entries)) {
    console.log('Bad config response: not an array');
    return;
  }
  localStorage.setItem('entries', JSON.stringify(entries));
  sendOne(entries, 0);
});

function sendOne(entries, i) {
  if (i >= entries.length) {
    sendWithRetry({ DONE: 1, COUNT: entries.length }, MAX_RETRIES, function () {
      console.log('Sync complete: ' + entries.length + ' entries');
    });
    return;
  }
  var msg = { INDEX: i, LABEL: entries[i].label, TEXT: entries[i].text };
  sendWithRetry(msg, MAX_RETRIES, function () {
    sendOne(entries, i + 1);
  });
}

function sendWithRetry(msg, retries, onSuccess) {
  Pebble.sendAppMessage(msg, onSuccess, function () {
    if (retries > 0) {
      sendWithRetry(msg, retries - 1, onSuccess);
    } else {
      console.log('Sync failed sending: ' + JSON.stringify(msg));
      // Spec: surface failure to the user (the config page is closed by now)
      Pebble.showSimpleNotificationOnPebble('QR Wallet',
        'Sync failed - open settings on your phone and save again.');
    }
  });
}
```

- [ ] **Step 4: Update `src/c/main.c`** — remove the temporary seed, init sync. Full final content:

```c
#include <pebble.h>
#include "app_state.h"
#include "storage.h"
#include "sync.h"
#include "menu_window.h"

QrList g_list;

int main(void) {
  storage_load(&g_list);
  sync_init();
  menu_window_push();
  app_event_loop();
}
```

- [ ] **Step 5: Run host tests** (model/staging logic is what sync leans on)

Run: `tests/run_tests.sh` — expected: `ALL TESTS PASSED`.

- [ ] **Step 6: End-to-end sync test in the emulator**

```bash
python3 -m http.server 8000 --directory config &
pebble build && pebble install --emulator diorite
pebble emu-app-config --emulator diorite
```

Expected: browser opens the config page. Add "WhatsApp" → `https://wa.me/qr/DEMO12345`, add "LinkedIn" → `https://www.linkedin.com/in/demo`, click **Save to watch**. Watch the logs:

```bash
pebble logs --emulator diorite
```

Expected log line: `sync committed 2 entries`. On the emulator: menu now lists both labels; Select shows the QR.

First fresh launch shows the empty state (seed is gone) — verify before configuring by wiping: `pebble wipe && pebble install --emulator diorite`.

Then stop the server: `kill %1`

- [ ] **Step 7: Commit**

```bash
git add -A && git commit -m "feat: config-to-watch sync with atomic staged commit; drop seed data"
```

---

### Task 10: Final verification & README

**Files:**
- Create: `README.md`

- [ ] **Step 1: Worst-case density scan test** — through the real pipeline: with the server running and app installed, `pebble emu-app-config --emulator diorite`, add an entry labeled "Max" whose text is exactly 200 chars, e.g.:

```
https://example.com/very/long/path/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
```

(Verify length with `echo -n '<text>' | wc -c` → 200.) Save, open it on the watch, then:

```bash
pebble screenshot --emulator diorite tests/fixtures/worst-case-qr.png
```

**Scan with a phone camera.** Expected: decodes to the exact 200-char URL. This validates the spec's 2px/module worst case.

- [ ] **Step 2: Back-to-back sync check** — incomplete-commit logic is covered by the Task 4 host tests; this verifies the live path stays consistent under successive syncs. With entries on the watch: open the config page (`pebble emu-app-config --emulator diorite`), delete all entries, add a single new one labeled "Only", save. Expected: watch menu shows exactly "Only". Repeat with a different single entry. Expected: menu shows only the latest save — never a blend of old and new lists.

- [ ] **Step 3: Write `README.md`**

```markdown
# Pebble QR Wallet

Show your WhatsApp / LinkedIn / any QR code from a Pebble 2, instantly.

- **Watch:** menu of labeled codes → Select → full-screen scannable QR.
  Up/Down flips between codes. Backlight stays on while displayed.
- **Phone:** open the watchapp's settings to add codes — paste a URL, or
  upload a screenshot of any QR code and it's decoded in the browser.

## Develop

    tests/run_tests.sh                      # host-side unit tests
    pebble build                            # watch app
    pebble install --emulator diorite       # run on emulated Pebble 2
    python3 -m http.server 8000 --directory config   # serve config page
    pebble emu-app-config --emulator diorite         # open settings flow

## Install on the watch

    pebble install --phone <PHONE_IP>       # Developer Connection in the
                                            # Pebble/Rebble phone app

## Publishing the config page

`config/` is static — host it anywhere (GitHub Pages). Then set
`CONFIG_URL` in `src/pkjs/index.js` to the published URL and rebuild.

## Spec & plan

See `docs/superpowers/specs/` and `docs/superpowers/plans/`.
```

- [ ] **Step 4: Full clean check**

```bash
tests/run_tests.sh && pebble clean && pebble build
```

Expected: tests pass, clean build succeeds.

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "docs: README; verify worst-case QR density end-to-end"
```

---

## Deferred (per spec, out of scope)

- GitHub Pages hosting of `config/` + flipping `CONFIG_URL` (needs a repo on GitHub; one-line change documented in README)
- On-hardware check on the physical Pebble 2 (requires the phone's Developer Connection; emulator rendering vs. memory LCD contrast noted in spec)
- aplite/basalt/chalk platform builds
