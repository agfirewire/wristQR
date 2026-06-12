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

## Caveat: settings live on the phone

The settings page seeds from the phone app's local storage, not from the
watch. On a new phone (or after reinstalling the phone app) the settings
list starts empty even if the watch still shows codes — re-add codes before
pressing "Save to watch", or saving the empty list will clear the watch.

## Publishing the config page

`config/` is static — host it anywhere (GitHub Pages). Then set
`CONFIG_URL` in `src/pkjs/index.js` to the published URL and rebuild.

## Spec & plan

See `docs/superpowers/specs/` and `docs/superpowers/plans/`.
