/**
 * Google Apps Script for ESP32 scene control
 *
 * Spreadsheet structure:
 * Sheet1 (latest snapshot)
 * Header: Scene,r,w,b,fr,ser,sew,seb,sefr
 *
 * Sheet2 (history log)
 * Header: Operation Date and Time,Function,Scene,r,w,b,fr,ser,sew,seb,sefr
 */

const SPREADSHEET_ID = 'XXXXXXXXXXX';
const SHEET_LATEST = 'Sheet1';
const SHEET_LOG = 'Sheet2';

const HEADER_LATEST = ['Scene', 'r', 'w', 'b', 'fr', 'ser', 'sew', 'seb', 'sefr'];
const HEADER_LOG = [
  'Operation Date and Time',
  'Function',
  'Scene',
  'r',
  'w',
  'b',
  'fr',
  'ser',
  'sew',
  'seb',
  'sefr',
];

function doPost(e) {
  try {
    const payload = JSON.parse(e.postData.contents || '{}');
    const fn = String(payload.function || '').trim().toUpperCase();

    if (!fn) {
      return jsonOutput({ ok: false, message: 'Missing function.' });
    }

    ensureHeaders_();

    switch (fn) {
      case 'READ':
        return jsonOutput({ ok: true, data: getAllLatestRows_() });
      case 'ADD SCENE':
      case 'UPDATE':
        upsertLatestRow_(payload);
        appendLog_(fn, payload);
        return jsonOutput({ ok: true, message: `${fn} success.` });
      case 'DELETE':
        deleteLatestRow_(payload.Scene);
        appendLog_(fn, { Scene: payload.Scene });
        return jsonOutput({ ok: true, message: 'DELETE success.' });
      case 'UPDATE ALL':
        updateAllRows_(payload.data || []);
        appendLogBulk_(fn, payload.data || []);
        return jsonOutput({ ok: true, message: 'UPDATE ALL success.' });
      case 'DELETE ALL':
        deleteAllRows_();
        appendLog_(fn, {});
        return jsonOutput({ ok: true, message: 'DELETE ALL success.' });
      default:
        return jsonOutput({ ok: false, message: `Unsupported function: ${fn}` });
    }
  } catch (err) {
    return jsonOutput({ ok: false, message: String(err) });
  }
}

function doGet(e) {
  // Optional: allow browser debug read
  const fn = String((e.parameter && e.parameter.function) || '').trim().toUpperCase();
  if (fn === 'READ') {
    ensureHeaders_();
    return jsonOutput({ ok: true, data: getAllLatestRows_() });
  }
  return jsonOutput({ ok: false, message: 'Use POST JSON or GET ?function=READ.' });
}

function ensureHeaders_() {
  const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
  const sheet1 = ss.getSheetByName(SHEET_LATEST);
  const sheet2 = ss.getSheetByName(SHEET_LOG);
  if (!sheet1 || !sheet2) {
    throw new Error('Missing Sheet1 or Sheet2.');
  }

  writeHeaderIfNeeded_(sheet1, HEADER_LATEST);
  writeHeaderIfNeeded_(sheet2, HEADER_LOG);
}

function writeHeaderIfNeeded_(sheet, header) {
  const firstRow = sheet.getRange(1, 1, 1, header.length).getValues()[0];
  const isDifferent = header.some((h, i) => String(firstRow[i] || '') !== h);
  if (isDifferent) {
    sheet.getRange(1, 1, 1, header.length).setValues([header]);
  }
}

function getAllLatestRows_() {
  const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_LATEST);
  const lastRow = sheet.getLastRow();
  if (lastRow <= 1) return [];

  const values = sheet.getRange(2, 1, lastRow - 1, HEADER_LATEST.length).getValues();
  const rows = [];
  values.forEach((v) => {
    if (!v[0]) return;
    rows.push({
      Scene: Number(v[0]),
      r: Number(v[1]),
      w: Number(v[2]),
      b: Number(v[3]),
      fr: Number(v[4]),
      ser: Number(v[5]),
      sew: Number(v[6]),
      seb: Number(v[7]),
      sefr: Number(v[8]),
    });
  });
  return rows;
}

function upsertLatestRow_(payload) {
  const scene = Number(payload.Scene);
  validateScene_(scene);

  const row = [
    scene,
    Number(payload.r || 0),
    Number(payload.w || 0),
    Number(payload.b || 0),
    Number(payload.fr || 0),
    Number(payload.ser || 0),
    Number(payload.sew || 0),
    Number(payload.seb || 0),
    Number(payload.sefr || 0),
  ];

  const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_LATEST);
  const hitRow = findSceneRow_(sheet, scene);

  if (hitRow > 0) {
    sheet.getRange(hitRow, 1, 1, HEADER_LATEST.length).setValues([row]);
  } else {
    sheet.appendRow(row);
  }
}

function deleteLatestRow_(sceneValue) {
  const scene = Number(sceneValue);
  validateScene_(scene);

  const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_LATEST);
  const hitRow = findSceneRow_(sheet, scene);
  if (hitRow > 0) {
    sheet.deleteRow(hitRow);
  }
}

function updateAllRows_(rows) {
  const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_LATEST);

  // Keep header, clear old data rows.
  const lastRow = sheet.getLastRow();
  if (lastRow > 1) {
    sheet.getRange(2, 1, lastRow - 1, HEADER_LATEST.length).clearContent();
  }

  const normalized = [];
  (rows || []).forEach((r) => {
    const scene = Number(r.Scene);
    if (!scene) return;
    validateScene_(scene);
    normalized.push([
      scene,
      Number(r.r || 0),
      Number(r.w || 0),
      Number(r.b || 0),
      Number(r.fr || 0),
      Number(r.ser || 0),
      Number(r.sew || 0),
      Number(r.seb || 0),
      Number(r.sefr || 0),
    ]);
  });

  if (normalized.length > 0) {
    sheet.getRange(2, 1, normalized.length, HEADER_LATEST.length).setValues(normalized);
  }
}

function deleteAllRows_() {
  const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_LATEST);
  const lastRow = sheet.getLastRow();
  if (lastRow > 1) {
    sheet.getRange(2, 1, lastRow - 1, HEADER_LATEST.length).clearContent();
  }
}

function findSceneRow_(sheet, scene) {
  const lastRow = sheet.getLastRow();
  if (lastRow <= 1) return -1;

  const values = sheet.getRange(2, 1, lastRow - 1, 1).getValues();
  for (let i = 0; i < values.length; i++) {
    if (Number(values[i][0]) === scene) {
      return i + 2;
    }
  }
  return -1;
}

function validateScene_(scene) {
  if (!Number.isFinite(scene) || scene < 1 || scene > 10) {
    throw new Error('Scene must be 1..10');
  }
}

function appendLog_(fn, payload) {
  const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_LOG);
  const now = Utilities.formatDate(new Date(), Session.getScriptTimeZone(), 'yyyy-MM-dd HH:mm:ss');
  sheet.appendRow([
    now,
    fn,
    payload.Scene || '',
    payload.r || '',
    payload.w || '',
    payload.b || '',
    payload.fr || '',
    payload.ser || '',
    payload.sew || '',
    payload.seb || '',
    payload.sefr || '',
  ]);
}

function appendLogBulk_(fn, rows) {
  const sheet = SpreadsheetApp.openById(SPREADSHEET_ID).getSheetByName(SHEET_LOG);
  const now = Utilities.formatDate(new Date(), Session.getScriptTimeZone(), 'yyyy-MM-dd HH:mm:ss');

  if (!rows || rows.length === 0) {
    sheet.appendRow([now, fn, '', '', '', '', '', '', '', '', '']);
    return;
  }

  rows.forEach((r) => {
    sheet.appendRow([
      now,
      fn,
      r.Scene || '',
      r.r || '',
      r.w || '',
      r.b || '',
      r.fr || '',
      r.ser || '',
      r.sew || '',
      r.seb || '',
      r.sefr || '',
    ]);
  });
}

function jsonOutput(obj) {
  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}
