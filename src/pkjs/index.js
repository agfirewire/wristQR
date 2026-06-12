// Published from config/ via the gh-pages branch (see README).
var CONFIG_URL = 'https://agfirewire.github.io/wristQR/index.html';

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
