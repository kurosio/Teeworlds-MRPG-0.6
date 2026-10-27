// Optional real Chromium regression test, entirely local and using an in-memory
// API fixture (never connects to a game DB). See STRUCTURE.md for installation.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import { fileURLToPath } from 'node:url';
import puppeteer from 'puppeteer-core';
import chromium from '@sparticuz/chromium';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const rows = {
  items: [{ ID: 42, Name: 'Sword', Description: '', Type: 'Equipment', Group: 'Weapons' }],
  worlds: [{ ID: 0, Name: 'Main', Path: 'main', Type: 'default', Flags: '', RequiredLevel: 1, RespawnWorldID: 0, JailWorldID: 0 }],
  bots_info: [{ ID: 7, Name: 'Blacksmith', JsonTeeInfo: '{}', SlotHammer: 42 }],
  quests: [{ ID: 9, Name: 'First quest', NextQuestID: 0, Money: 10, Exp: 10, Flags: 'Type main' }],
  quest_board_links: [{ ID: 1, QuestID: 9, DailyBoardID: 3 }],
  quest_boards: [{ ID: 3, Name: 'Town board', WorldID: 0, PosX: 32, PosY: 32 }],
};
// Guest phase state: the fixture switches roles without touching a real DB.
let role = 'admin';
let guestPages = ['items', 'bots_info', 'worlds', 'crafts', 'quests', 'quest_boards'];
const dbWrites = [];
const requests = [];
const referenceSchema = JSON.parse(await fs.readFile(path.join(root, 'editor-core', 'reference-schema.json'), 'utf8'));
const guestResources = {
  items: { title: 'Items', pk: 'ID', label: 'Name', json: ['Data'], refs: {}, columns: ['Comment', 'Name', 'Description', 'Group', 'Type', 'Flags', 'ScenarioMode', 'ScenarioData', 'InitialPrice', 'RequiresProducts', 'AT1', 'AT2', 'ATValue1', 'ATValue2', 'Data'] },
  bots_info: { title: 'Bots', pk: 'ID', label: 'Name', json: [], refs: { SlotHammer: 'tw_items_list' }, columns: ['Name', 'JsonTeeInfo', 'EquippedModules', 'SlotHammer', 'SlotGun', 'SlotShotgun', 'SlotGrenade', 'SlotRifle', 'SlotArmor'] },
  worlds: { title: 'Worlds', pk: 'ID', label: 'Name', json: [], refs: {}, columns: ['Name', 'Path', 'Type', 'Flags', 'RespawnWorldID', 'JailWorldID', 'RequiredLevel'] },
  crafts: { title: 'Crafts', pk: 'ID', label: 'GroupName', json: [], refs: { ItemID: 'tw_items_list', WorldID: 'tw_worlds' }, columns: ['GroupName', 'ItemID', 'ItemValue', 'RequiredItems', 'Price', 'WorldID'] },
  quests: { title: 'Quests', pk: 'ID', label: 'Name', json: [], refs: { NextQuestID: 'tw_quests_list' }, columns: ['NextQuestID', 'Name', 'Money', 'Exp', 'Flags'] },
  quest_boards: { title: 'Quest boards', pk: 'ID', label: 'Name', json: [], refs: { WorldID: 'tw_worlds' }, columns: ['Name', 'PosX', 'PosY', 'WorldID'] },
};
const readBody = async (req) => { let body = ''; for await (const chunk of req) body += chunk; return body; };
const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://fixture.local');
  if (url.pathname.endsWith('.php')) {
    res.setHeader('Content-Type', 'application/json');
    const action = url.searchParams.get('action');
    let result = { ok: true };
    if (url.pathname.endsWith('/auth.php')) {
      result = {
        ok: true, authorized: true, role, csrf: 'a'.repeat(64),
        guest: role === 'guest'
          ? { enabled: true, accounts: 1, id: 'a1', login: 'guest', name: 'Guest', pages: guestPages }
          : { enabled: true, accounts: 1, id: null, login: null, name: null, pages: [] },
      };
    } else if (url.pathname.endsWith('/change-requests.php')) {
      if (action === 'config') {
        result = {
          ok: true, role, unread: 0,
          account: role === 'guest' ? { id: 'a1', login: 'guest', name: 'Guest', pages: guestPages } : null,
          limits: { nickname: { min: 2, max: 32 }, message: { min: 10, max: 2000 }, max_operations: 200, max_pending: 5, cooldown: 0, custom: false },
          features: { idempotent_submit: true, mark_read: true, bulk: false, audit: false },
          temp_id: { base: 2000000000, max: 2000999999 },
          reference_schema: referenceSchema,
          resources: guestResources,
        };
      } else if (action === 'mine') {
        result = { ok: true, rows: requests.map((r) => ({ id: r.id, status: r.status, version: r.version })) };
      } else if (action === 'submit') {
        const body = JSON.parse(await readBody(req) || '{}');
        const request = { id: `CR-${requests.length + 1}`, status: 'pending', version: 1, operations: body.operations || [], review: null };
        requests.push(request);
        result = { ok: true, duplicate: false, request: { id: request.id, status: request.status, version: 1 } };
      } else if (action === 'get') {
        const found = requests.find((r) => r.id === url.searchParams.get('id'));
        result = found ? { ok: true, request: found } : { ok: false, error: 'Not found' };
      } else if (action === 'x-approve') {
        // Test hook: mimics cr_apply() assigning real AUTO_INCREMENT ids.
        let next = 80;
        const idMap = {};
        for (const op of requests.at(-1).operations) if (op.type === 'create') idMap[op.tempId] = ++next;
        requests.at(-1).status = 'approved';
        requests.at(-1).review = { at: new Date().toISOString(), comment: '', idMap };
        result = { ok: true, idMap };
      } else result = { ok: true };
    } else if (url.pathname.endsWith('/db-crud.php')) {
      if (req.method === 'POST') dbWrites.push({ url: req.url, role });
      const resource = url.searchParams.get('resource');
      const list = rows[resource] || [];
      if (action === 'list') result = { ok: true, rows: list, has_more: false };
      else if (action === 'get') result = { ok: true, row: list.find((row) => row.ID === Number(url.searchParams.get('id'))) || null };
      else if (action === 'update') {
        const body = await readBody(req);
        Object.assign(list.find((row) => row.ID === Number(url.searchParams.get('id'))), JSON.parse(body).data);
      }
    } else if (url.pathname.endsWith('/db.php')) {
      const source = url.searchParams.get('source');
      const list = (rows[source === 'bots' ? 'bots_info' : source] || []).map((row) => ({ value: row.ID, label: row.Name }));
      if (action === 'get_config') result = { ok: true, config: {} };
      else if (action === 'get_one') result = { ok: true, item: list.find((item) => item.value === Number(url.searchParams.get('id'))) || null };
      else result = { ok: true, items: list, has_more: false };
    }
    res.end(JSON.stringify(result)); return;
  }
  try {
    const filename = path.resolve(root, '.' + decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname));
    if (!filename.startsWith(root + path.sep)) { res.writeHead(403).end(); return; }
    const content = await fs.readFile(filename);
    const types = { '.html': 'text/html', '.css': 'text/css', '.js': 'application/javascript', '.woff2': 'font/woff2' };
    res.setHeader('Content-Type', types[path.extname(filename)] || 'application/octet-stream'); res.end(content);
  } catch { res.writeHead(404).end(); }
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
let browser;
try {
  browser = await puppeteer.launch({ executablePath: await chromium.executablePath(), args: chromium.args, headless: true });
  const page = await browser.newPage();
  page.setDefaultTimeout(10000);
  page.on('dialog', (dialog) => dialog.accept());
  const errors = [];
  if (process.env.DEBUG_BROWSER) {
    page.on('console', (message) => console.log('browser:', message.text()));
    page.on('pageerror', (error) => console.error('browser error:', error));
  }
  page.on('pageerror', (error) => errors.push(error.message));
  const base = `http://127.0.0.1:${server.address().port}`;
  for (const width of [360, 390, 768, 1280]) {
    await page.setViewport({ width, height: 844, isMobile: width < 640, hasTouch: width < 640 });
    await page.goto(`${base}/worlds-editor.html?resource=worlds&record=0`);
    await page.waitForSelector('[data-path="Name"]');
    assert.equal(await page.$eval('[data-path="Name"]', (input) => input.value), 'Main');
    await page.$eval('[data-path="Name"]', (input) => {
      input.value = 'Unsaved source'; input.dispatchEvent(new Event('input', { bubbles: true }));
    });
    const sourceInput = await page.$('[data-path="Name"]');
    await page.$eval('.related-record-link:not([disabled])', (button) => button.scrollIntoView({ block: 'center', behavior: 'instant' }));
    await page.click('.related-record-link:not([disabled])');
    await page.waitForSelector('.related-record-dialog[open] > iframe');
    const child = await (await page.$('.related-record-dialog > iframe')).contentFrame();
    await child.waitForSelector('[data-path="Name"]');
    assert.equal(await child.$eval('[data-path="Name"]', (input) => input.value), 'Main');
    // Drill down again from a child. There must still be exactly ONE workbench.
    await child.$eval('.related-record-link:not([disabled])', (button) => button.scrollIntoView({ block: 'center', behavior: 'instant' }));
    await child.click('.related-record-link:not([disabled])');
    await page.waitForFunction(() => document.querySelectorAll('.related-record-dialog > iframe').length === 2);
    assert.equal(await page.$$eval('.related-record-dialog', (dialogs) => dialogs.length), 1);
    const second = await (await page.$('.related-record-dialog > iframe:not([hidden])')).contentFrame();
    await second.waitForSelector('[data-path="Name"]');
    await page.click('.related-record-header button');
    await page.waitForFunction(() => document.querySelectorAll('.related-record-dialog > iframe').length === 1);
    await child.$eval('[data-path="Name"]', (input) => {
      input.value = 'Unsaved linked'; input.dispatchEvent(new Event('input', { bubbles: true }));
    });
    await page.click('.related-record-header button');
    await page.waitForSelector('.related-record-dialog .ec-cancel');
    await page.click('.ec-cancel');
    assert.equal(await child.$eval('[data-path="Name"]', (input) => input.value), 'Unsaved linked');
    await page.click('.related-record-header button');
    await page.waitForSelector('.related-record-dialog .ec-ok');
    await page.click('.ec-ok');
    await page.waitForFunction(() => !document.querySelector('.related-record-dialog'));
    assert.equal(await sourceInput.evaluate((input) => input.value), 'Unsaved source');
    assert.equal(await sourceInput.evaluate((input) => input.isConnected), true);
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), `overflow at ${width}px`);
    console.log(`Chromium ${width}px: world 0, nested navigation, dirty cancel/confirm and source retention OK`);
  }
  // Saving an existing world 0 must update it, never create a new record.
  await page.click('[data-editor-role="save"]');
  await page.waitForFunction(() => document.getElementById('dirty-pill').classList.contains('hidden'));
  assert.equal(rows.worlds[0].Name, 'Unsaved source');
  console.log('Chromium: existing world 0 saved through UPDATE OK');
  // Custom board popup deep link and its related world control.
  await page.goto(`${base}/quest-boards-editor.html?resource=quest_boards&record=3`);
  await page.waitForSelector('#board-dialog[open] [data-path="Name"]');
  assert.equal(await page.$eval('#board-dialog [data-path="Name"]', (input) => input.value), 'Town board');
  await page.waitForSelector('#board-dialog .related-record-link:not([disabled])');
  await page.click('#board-dialog .related-record-link:not([disabled])');
  await page.waitForSelector('.related-record-dialog[open]');
  const worldFrame = await (await page.$('.related-record-dialog > iframe')).contentFrame();
  await worldFrame.waitForSelector('[data-path="Name"]');
  await page.click('.related-record-header button');
  await page.waitForFunction(() => !document.querySelector('.related-record-dialog'));
  assert.ok(await page.$('#board-dialog[open]'));
  console.log('Chromium: quest board deep link and native modal drill-down OK');
  await page.goto(`${base}/quests-editor.html?resource=quests&record=9`);
  await page.waitForSelector('[data-path="Name"]');
  assert.equal(await page.$eval('[data-path="Name"]', (input) => input.value), 'First quest');
  await page.waitForSelector('[data-field-path="BoardID"] .related-record-link:not([disabled])');
  await page.click('[data-field-path="BoardID"] .related-record-link');
  await page.waitForSelector('.related-record-dialog > iframe');
  const boardFrame = await (await page.$('.related-record-dialog > iframe')).contentFrame();
  await boardFrame.waitForSelector('#board-dialog[open] [data-path="Name"]');
  assert.equal(await boardFrame.$eval('#board-dialog [data-path="Name"]', (input) => input.value), 'Town board');
  console.log('Chromium: quest → board cross-editor relation OK');

  // ── Guest mode: drafts, links to draft records and the request pipeline ──
  role = 'guest';
  const guest = await browser.newPage();
  guest.setDefaultTimeout(15000);
  guest.on('dialog', (dialog) => dialog.accept());
  guest.on('pageerror', (error) => errors.push(`guest: ${error.message}`));
  await guest.setViewport({ width: 1280, height: 900 });
  const draftOps = () => guest.evaluate(() => JSON.parse(localStorage.getItem('editorStudio.guestDraft.v2:a1') || '{"ops":[]}').ops);

  await guest.goto(`${base}/items-editor.html`);
  await guest.waitForSelector('[data-editor-role="new"]');
  await guest.click('[data-editor-role="new"]');
  await guest.waitForSelector('[data-path="Name"]');
  await guest.type('[data-path="Name"]', 'Draft sword');
  await guest.click('[data-editor-role="save"]');
  await guest.waitForFunction(() => JSON.parse(localStorage.getItem('editorStudio.guestDraft.v2:a1') || '{"ops":[]}')
    .ops.some((op) => op.type === 'create' && op.resource === 'items' && op.data.Name === 'Draft sword'));
  const [tempId] = await guest.evaluate(() => {
    const op = JSON.parse(localStorage.getItem('editorStudio.guestDraft.v2:a1')).ops.find((o) => o.type === 'create');
    return [op.tempId];
  });
  assert.ok(tempId >= 2000000000 && tempId <= 2000999999, 'a reserved draft ID is used');
  assert.deepEqual(dbWrites.filter((write) => write.role === 'guest'), [], 'a guest never writes to the DB directly');

  // The draft record is reachable through the related-record link of another editor.
  await guest.goto(`${base}/bots-info-editor.html?resource=bots_info&record=7`);
  // Wait for the loaded row, not just the empty skeleton: setting a value while
  // the form is still being filled would be overwritten by the record data.
  await guest.waitForFunction(() => document.querySelector('[data-path="Name"]')?.value === 'Blacksmith');
  await guest.waitForSelector('[data-path="EquippedModules"]');
  await guest.$eval('[data-path="EquippedModules"]', (input, id) => {
    input.value = `17,${id}`;
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }, tempId);
  await guest.waitForFunction((id) => [...document.querySelectorAll('.related-record-list button')]
    .some((button) => button.textContent.includes(`#${id}`)), { polling: 100 }, tempId);
  await guest.evaluate((id) => [...document.querySelectorAll('.related-record-list button')]
    .find((button) => button.textContent.includes(`#${id}`)).click(), tempId);
  await guest.waitForSelector('.related-record-dialog[open] > iframe');
  const draftFrame = await (await guest.$('.related-record-dialog > iframe')).contentFrame();
  await draftFrame.waitForSelector('[data-path="Name"]');
  assert.equal(await draftFrame.$eval('[data-path="Name"]', (input) => input.value), 'Draft sword');
  await draftFrame.$eval('[data-path="Name"]', (input) => {
    input.value = 'Draft sword 2';
    input.dispatchEvent(new Event('input', { bubbles: true }));
  });
  await draftFrame.click('[data-editor-role="save"]');
  await guest.waitForFunction(() => JSON.parse(localStorage.getItem('editorStudio.guestDraft.v2:a1') || '{"ops":[]}')
    .ops.some((op) => op.type === 'create' && op.data.Name === 'Draft sword 2'));
  assert.equal((await draftOps()).filter((op) => op.type === 'create').length, 1, 'editing a draft record never creates a second one');

  // Returning keeps the source form, and saving it stores the reserved ID.
  await guest.click('.related-record-header button');
  await guest.waitForFunction(() => !document.querySelector('.related-record-dialog'));
  assert.equal(await guest.$eval('[data-path="EquippedModules"]', (input) => input.value), `17,${tempId}`);
  await guest.click('[data-editor-role="save"]');
  await guest.waitForFunction(() => JSON.parse(localStorage.getItem('editorStudio.guestDraft.v2:a1') || '{"ops":[]}')
    .ops.some((op) => op.type === 'update' && op.resource === 'bots_info' && Number(op.id) === 7));
  const botOp = (await draftOps()).find((op) => op.type === 'update');
  assert.ok(String(botOp.data.EquippedModules).split(',').map((v) => v.trim()).includes(String(tempId)));

  // The draft becomes one request; the operations keep their reserved IDs.
  await guest.goto(`${base}/change-requests.html`);
  await guest.waitForSelector('#g-nickname');
  await guest.type('#g-nickname', 'Tester');
  await guest.type('#g-message', 'A new item and its module link for the blacksmith bot.');
  await guest.waitForFunction(() => !document.getElementById('g-btn-submit').disabled);
  await guest.click('#g-btn-submit');
  await new Promise((resolve, reject) => {
    const timer = setInterval(() => { if (requests.length) { clearInterval(timer); resolve(); } }, 50);
    setTimeout(() => { clearInterval(timer); reject(new Error('the draft was not submitted')); }, 15000);
  });
  const sent = requests[0].operations;
  assert.equal(sent.filter((op) => op.type === 'create').length, 1);
  assert.equal(sent.find((op) => op.type === 'create').tempId, tempId);
  assert.ok(String(sent.find((op) => op.type === 'update').data.EquippedModules).includes(String(tempId)));
  assert.deepEqual(dbWrites.filter((write) => write.role === 'guest'), []);

  // Approval: the local copy is dropped, because the database now owns the values.
  await guest.evaluate(() => fetch('api/change-requests.php?action=x-approve', { method: 'POST' }).then((r) => r.json()));
  assert.deepEqual(Object.keys(requests[0].review.idMap), [String(tempId)]);
  await guest.goto(`${base}/items-editor.html`);
  await guest.waitForFunction(() => !JSON.parse(localStorage.getItem('editorStudio.guestDraft.v2:a1') || '{"ops":[]}')
    .ops.some((op) => op.reqId === 'CR-1'));

  // A tab the administrator did not grant is refused instead of half-loading.
  guestPages = ['items'];
  await guest.evaluate(() => sessionStorage.clear());
  await guest.goto(`${base}/bots-info-editor.html?resource=bots_info&record=7`);
  await guest.waitForSelector('#guest-access-denied');
  console.log('Chromium guest: draft links, request submission, approval sync and tab access OK');
  await guest.close();

  assert.deepEqual(errors, []);
} finally {
  await browser?.close();
  await new Promise((resolve) => server.close(resolve));
}
