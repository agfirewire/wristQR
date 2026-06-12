var MAX_ENTRIES = 8;
var LABEL_MAX = 20;
var TEXT_MAX = 200;
var MAX_MODULES = 57; // QR version 10 — matches watch QR_MAX_VERSION

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
  if (text.length > TEXT_MAX) {
    setError('Too long to display scannably (' + text.length + '/' + TEXT_MAX + ' chars).');
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
  if (text.length > TEXT_MAX) {
    setError('Too long to display scannably (' + text.length + '/' + TEXT_MAX + ' chars).');
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
