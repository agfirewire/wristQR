# Pebble QR Wallet — Design Spec

**Date:** 2026-06-11
**Target:** Pebble 2 (diorite platform), Pebble SDK / Rebble toolchain

## Purpose

A watchapp that stores a small personal set of QR codes (WhatsApp join,
LinkedIn connect, etc.) and displays any of them full-screen on demand, so the
wearer can let someone scan a code straight off the watch. Codes are added
from the phone via the app's configuration page — by pasting a URL or by
uploading a screenshot of an existing QR code, which is decoded in the
browser.

## Constraints (drive every decision below)

- **Display:** 144×168 px, 1-bit black & white, no touch; 4 physical buttons.
- **No camera, no photo access, no network on the watch.** All input and
  decoding happens on the phone-side config page; the watch receives data
  over Bluetooth via AppMessage.
- **Persistent storage:** ~4KB total per app; 256 bytes max per key.
- **Scannability:** modules need ≥2 px each plus a 4-module quiet zone to
  scan reliably off the small screen. This caps payloads at QR version 10
  (57×57 modules), which at error-correction level M holds 213 bytes —
  hence the 200-char text limit.

## Architecture

Two components, one repo:

```
┌─────────────── Phone ───────────────┐      ┌──────── Pebble 2 ────────┐
│ Pebble app → Settings → Config page │      │  C app (Pebble SDK)      │
│  • paste URL  OR  upload screenshot │ ───▶ │  • stores label+text     │
│  • jsQR decodes screenshot          │ AppMessage  (persist storage)   │
│  • label + live preview + density   │      │  • MenuLayer of labels   │
│    warning                          │      │  • Select → qrcodegen    │
│  • saves list of {label, text}      │      │    encodes → full-screen │
└─────────────────────────────────────┘      │    B&W QR render         │
                                             └──────────────────────────┘
```

- **Watch app (C):** owns the saved list, renders the menu and QR screens.
  Fully offline once codes are synced; the phone is only needed to add or
  edit codes.
- **Config page (static HTML/JS):** the only place codes are created or
  edited. Hosted on GitHub Pages. Uses `jsQR` (Apache-2.0) to decode uploaded
  screenshots and a JS QR encoder for the live preview. A custom page (not
  Clay) because Clay has no image-upload component.
- **QR encoding on watch:** `qrcodegen` C library (Nayuki, MIT) — single
  .c/.h pair designed for embedded use. The watch stores text and re-encodes
  on display; no images cross Bluetooth or sit in storage.

## Data Model

Watch persist storage, all keys versioned under a schema-version key:

| Key | Contents |
|---|---|
| `SCHEMA_VERSION` | uint8, currently 1 |
| `COUNT` | uint8, number of entries (0–8) |
| `LABEL_0..7` | string, ≤20 chars + NUL |
| `TEXT_0..7` | string, ≤200 chars + NUL |

Worst case: 8 × (21 + 201) + overhead ≈ 1.8KB — under the 4KB cap. Limits
(8 entries, 20-char label, 200-char text) are shared constants mirrored in
the config page JS.

## Watch UI

Two screens:

1. **Menu screen** (launch): standard `MenuLayer` listing labels. Up/Down
   navigates, **Select** opens the QR screen, **Back** exits. If `COUNT` is
   0: full-screen message — "No codes yet — add them in the app settings on
   your phone."
2. **QR screen:**
   - Fixed **24px label strip at top**: label text in `GOTHIC_18_BOLD`,
     centered, black on white. Always shown — identifying which code is up
     is a requirement.
   - QR rendered in the remaining 144×144 area: white background, black
     modules, 4-module quiet zone on all sides. Worst case (v10, 65 modules
     incl. quiet zone, 2px/module = 130px) fits.
   - **Back** returns to the menu. **Up/Down switches to the previous/next
     code in place** (wrapping), so flipping WhatsApp→LinkedIn is one press.
   - Backlight held on while this screen is visible (`light_enable(true)`),
     restored to normal (`light_enable(false)`) when leaving the screen.

## QR Rendering

- `qrcodegen` encodes at error-correction level **M**, library picks the
  smallest version that fits (`qrcodegen_encodeText` with auto version).
- Scale = `floor(144 / (modules + 8))` px per module (the QR area is 144×144
  after the label strip; +8 is the quiet zone) — whole-pixel
  scaling only, so modules stay crisp on the 1-bit display. Centered in the
  QR area.
- Drawn with `graphics_fill_rect` per black module onto a white-filled layer.

## Sync Protocol (config page → watch)

- On save, the page sends entries one at a time: `{index, label, text}`,
  then a final `{done: true, count: N}` message. Each message is well under
  AppMessage size limits; per-message ACK/NACK with retry (3 attempts).
- The watch writes incoming entries to a **staging area in RAM**, and only
  commits to persist storage (and refreshes the menu) when `done` arrives
  with a count matching the entries received. An interrupted sync leaves the
  previous list untouched.
- The page sends the full list every time (wholesale replace) — no partial
  updates, no delete protocol.
- If a sync commits while the QR screen is open, the watch pops back to the
  refreshed menu (the code being viewed may no longer exist; the menu is the
  only safe place to land).

## Config Page

- Static HTML/JS, no backend. Entry list UI: add (paste URL or upload
  screenshot), edit label, delete, reorder; "Save to watch" returns the list
  via the standard `pebblejs://close#<data>` mechanism to the app's
  PebbleKit JS, which runs the sync protocol.
- **Screenshot path:** file input → canvas → `jsQR`. On failure: "Couldn't
  find a QR code in that image — try a tighter crop."
- **Validation:** text >200 chars rejected ("too long to display scannably");
  label truncated at 20 chars; duplicate labels allowed but warned.
- **Live preview:** JS-encoded QR at the same version/EC level the watch
  will use, so the preview is representative of on-watch density.

## Error Handling

| Failure | Behavior |
|---|---|
| Screenshot doesn't decode | Config page error message, no entry added |
| Text too long | Rejected at config page with explanation |
| Sync interrupted | Watch keeps previous list (staging never committed) |
| AppMessage NACK | Retry up to 3×, then config-side "sync failed, try again" |
| `qrcodegen` encode failure on watch | "Can't display this code" screen instead of crash (defensive; 200-char cap should prevent it) |
| Empty list | "No codes yet" message |

## Testing

- **End-to-end (the test that matters):** build with `pebble build`, run in
  `pebble install --emulator diorite`, screenshot the emulator, and scan the
  rendered QR with a real phone. Repeat at the 200-char worst case.
- **Config page:** open in a browser; fixtures = real exported
  WhatsApp/LinkedIn QR screenshots (decode path) plus pasted URLs.
- **Watch logic:** sync state machine (interrupted sync keeps old list;
  count mismatch discards staging) and persist round-trip, exercised in the
  emulator with logged assertions.
- **On-hardware check:** final pass on the physical Pebble 2 — emulator
  rendering and the real memory LCD can differ in contrast.

## Out of Scope (YAGNI)

- More than 8 codes, folders, or categories
- Non-QR barcodes (Code 128 etc.)
- Generating codes on the watch from scratch (no text input on watch)
- Color/round-display platforms (aplite/basalt/chalk builds can come later;
  diorite first)
- Any server-side component
