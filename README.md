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

The page is hosted at https://agfirewire.github.io/wristQR/ (GitHub Pages,
`gh-pages` branch), and `CONFIG_URL` in `src/pkjs/index.js` points there.
After changing anything in `config/`, republish with:

    git subtree push --prefix config origin gh-pages

For local development, serve `config/` (see Develop above) and temporarily
point `CONFIG_URL` back to `http://localhost:8000/index.html`.

## License

MIT (see `LICENSE`). Vendored libraries keep their own licenses — see
`THIRD_PARTY_LICENSES.md`.

## Spec & plan

See `docs/superpowers/specs/` and `docs/superpowers/plans/`.
