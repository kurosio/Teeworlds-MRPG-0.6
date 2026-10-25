/**
 * tools/i18n-audit.mjs — аудит двуязычности отрисованной страницы.
 *
 * `tools/check.mjs` работает со статикой (разметка и литералы), а этот скрипт
 * прогоняет страницу в jsdom дважды — в английском (язык источника, по
 * умолчанию) и в русском режимах — и сверяет отрисованный текст:
 *
 *   • EN-проход: кириллицы быть не должно — английский это язык исходников;
 *   • RU-проход: строки интерфейса должны отличаться от английских. Текст,
 *     который остался дословно тем же в обоих режимах, — непереведённая
 *     подпись (значения из базы в подсчёт не идут, см. DATA_TERMS).
 *
 * jsdom — только для разработки, в зависимостях проекта его нет:
 *   npm install --no-save jsdom
 *   node tools/i18n-audit.mjs index.html items-editor.html
 *   node tools/i18n-audit.mjs *.html          # все страницы
 *
 * Код возврата 1, если хоть на одной странице остались непереведённые строки.
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const ORIGIN = 'http://localhost:4173';

let jsdomPkg;
try {
  jsdomPkg = (await import('jsdom')).default;
} catch {
  console.error('Нужен jsdom: npm install --no-save jsdom');
  process.exit(2);
}
const { JSDOM, VirtualConsole, requestInterceptor } = jsdomPkg;

const TYPES = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8', '.json': 'application/json; charset=utf-8',
  '.png': 'image/png', '.svg': 'image/svg+xml', '.woff2': 'font/woff2',
};
const CYRILLIC = /[А-Яа-яЁё]/;
const SKIP_TAGS = new Set(['SCRIPT', 'STYLE', 'TEXTAREA', 'CODE', 'PRE', 'KBD']);
const EMPTY_API = '{"ok":true,"rows":[],"data":[],"profiles":[]}';

/**
 * Значения из базы (перечисления предметов, эффекты, группы склада) — это не
 * интерфейс: они одинаковы в обоих режимах и уходят обратно в базу.
 */
const DATA_TERMS = new Set([
  'Default', 'Usable', 'Resource', 'Decoration', 'Equipment', 'Potion', 'Currency', 'Quest', 'Settings', 'Other',
  'Slowdown', 'Poison', 'Fire', 'Happy', 'Angry', 'Misc', 'Tutorial', 'Partial',
  "Can't droppable", "Can't tradeable", 'Tiny heal',
  'Type main', 'Type side', 'Type daily', 'Type weekly', 'Type repeatable', "Can't refuse",
  'Equip hammer', 'Equip gun', 'Equip shotgun', 'Equip grenade', 'Equip rifle', 'Equip pickaxe',
  'Equip rake', 'Equip fishrod', 'Equip gloves', 'Equip armor (tank)', 'Equip armor (dps)',
  'Equip armor (healer)', 'Equip helmet (tank)', 'Equip helmet (dps)', 'Equip helmet (healer)',
  'Equip eidolon', 'Equip title', 'Equip potion HP', 'Equip potion MP', 'Equip potion Miner',
  'Resource harvestable', 'Resource mineable', 'Resource fishes',
  'Single use x1', 'Multiple use x99',
  'Без точки активности',
  // заголовок страницы с названием студии
  'NPC • Editor Studio',
  // название продукта не переводится
  'Editor Studio',
]);

// Кнопки и строки, раскрывающие формы: без клика аудит видит половину подписей.
const ACTIVATORS = [
  '#btn-new', '#btn-add', '[data-action="new"]', '#p-add', '#add-line', '[data-dialog-add]',
  '#newQuestBtn', '#new-record', '[data-role="new"]',
  '.editor-list-item', 'tbody tr', '[data-id]', '.editor-row', '[role="tab"]', '[data-tab]',
];

const interceptor = requestInterceptor(async (request) => {
  let url;
  try { url = new URL(request.url); } catch { return new Response('', { status: 404 }); }
  if (!['localhost', '127.0.0.1'].includes(url.hostname)) {
    return new Response('', { status: 200, headers: { 'content-type': 'text/css' } });
  }
  const file = url.pathname === '/' ? '/index.html' : url.pathname;
  if (file.startsWith('/api/')) return new Response(EMPTY_API, { headers: { 'content-type': 'application/json' } });
  const body = await fs.promises.readFile(path.join(ROOT, file)).catch(() => null);
  if (!body) return new Response('', { status: 404 });
  return new Response(body, { headers: { 'content-type': TYPES[path.extname(file)] || 'application/octet-stream' } });
});

const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const audit = async (page, lang) => {
  const html = fs.readFileSync(path.join(ROOT, page), 'utf8');
  const virtualConsole = new VirtualConsole();
  virtualConsole.on('jsdomError', (error) => {
    const message = String(error.message || error);
    // Страница в jsdom неполноценна: canvas, шрифты, часть модулей.
    if (/Not implemented|Could not parse CSS|getContext|createObjectURL|Not found|is not defined/i.test(message)) return;
  });

  const dom = new JSDOM(html, {
    url: `${ORIGIN}/${page}`,
    runScripts: 'dangerously',
    pretendToBeVisual: true,
    resources: { interceptors: [interceptor] },
    virtualConsole,
    beforeParse(window) {
      window.localStorage.setItem('editorStudio.lang', lang);
      window.matchMedia = (query) => ({
        matches: false, media: String(query),
        addEventListener() {}, removeEventListener() {}, addListener() {}, removeListener() {},
      });
      window.fetch = async () => ({
        ok: true,
        status: 200,
        async text() { return EMPTY_API; },
        async json() { return JSON.parse(EMPTY_API); },
        headers: { get: () => 'application/json' },
      });
      window.HTMLCanvasElement.prototype.getContext = () => null;
    },
  });

  const { window } = dom;
  await new Promise((resolve) => window.addEventListener('load', resolve, { once: true }));
  await wait(400);
  if (window.EditorCore?.I18n && window.EditorCore.I18n.lang !== lang) {
    window.EditorCore.I18n.set(lang, { force: true });
  }

  // jsdom не исполняет модульные скрипты — запускаем их сами.
  for (const match of html.matchAll(/<script[^>]*type="module"[^>]*>([\s\S]*?)<\/script>/gi)) {
    if (!match[1].trim() || /^\s*import\s/m.test(match[1])) continue;
    try { window.eval(match[1]); } catch { /* блок может зависеть от модулей */ }
  }
  await wait(500);
  // В браузере page-loader пишет ядро синхронно и скрипт страницы видит
  // EditorCore; в jsdom повторяем скрипт страницы после загрузки ядра.
  for (const match of html.matchAll(/<script(?![^>]*\bsrc=)(?![^>]*type="module")[^>]*>([\s\S]*?)<\/script>/gi)) {
    if (!match[1].trim()) continue;
    // `document.write` после загрузки переоткрывает документ и стирает страницу —
    // в браузере этот тег отрабатывает при разборе, здесь его не повторяем.
    if (match[1].includes('document.write')) continue;
    try { window.eval(match[1]); } catch { /* «EditorCore is not defined» — артефакт */ }
  }
  await wait(700);

  for (const selector of ACTIVATORS) {
    for (const element of [...window.document.querySelectorAll(selector)].slice(0, 2)) {
      try { element.click(); } catch { /* не интерактивный узел */ }
    }
  }
  await wait(500);

  const doc = window.document;
  const strings = new Map();
  const remember = (text, where) => {
    const value = String(text || '').replace(/\s+/g, ' ').trim();
    if (!value) return;
    if (!strings.has(value)) strings.set(value, new Set());
    strings.get(value).add(where);
  };
  const walker = doc.createTreeWalker(doc.documentElement, window.NodeFilter.SHOW_TEXT);
  let node;
  while ((node = walker.nextNode())) {
    const parent = node.parentElement;
    if (!parent || SKIP_TAGS.has(parent.tagName)) continue;
    if (parent.closest?.('[data-i18n-skip]')) continue;
    const context = parent.tagName.toLowerCase() + (parent.className ? `.${String(parent.className).split(' ')[0]}` : '');
    remember(node.nodeValue, context);
  }
  for (const element of doc.querySelectorAll('[title],[placeholder],[aria-label]')) {
    for (const name of ['title', 'placeholder', 'aria-label']) {
      remember(element.getAttribute(name), `${element.tagName.toLowerCase()}[${name}]`);
    }
  }
  remember(doc.title, '<title>');
  window.close();
  return strings;
};

/**
 * Похоже ли на подпись интерфейса. Отсекаем данные, коды, пути, аббревиатуры
 * и одиночные токены: их совпадение в двух режимах — норма.
 */
const looksLikeLabel = (raw) => {
  if (!raw || raw.length < 3) return false;
  if (DATA_TERMS.has(raw)) return false;
  // Значения в кавычках — данные (имя профиля, название записи): «OK — «Default»».
  const text = raw.replace(/«[^»]*»|“[^”]*”|"[^"]*"/g, ' ').trim();
  if (!text || text.length < 3) return false;
  if (DATA_TERMS.has(text)) return false;
  if (/^[^A-Za-z]*$/.test(text)) return false;          // числа, знаки, эмодзи
  if (/[{}$\\_=<>|]/.test(text)) return false;
  if (/^(?:Ctrl|Alt|Shift|Meta)\+/.test(text)) return false;
  if (/^[./#[]/.test(text)) return false;               // селекторы и пути
  if (/^https?:\/\//.test(text)) return false;
  if (/^[A-Z]{1,6}$/.test(text)) return false;          // ID, JSON, NPC
  if (/^[A-Za-z_$][\w$]*(?:\.[\w$]+)+$/.test(text)) return false;
  if (/^[a-z0-9._-]+$/.test(text)) return false;        // слаги, имена полей
  if (/[a-z][A-Z]/.test(text)) return false;            // camelCase
  if (/--[\w-]+/.test(text)) return false;              // css-переменные
  if (/^[\w-]+:\s/.test(text)) return false;            // css-объявление
  const words = text.match(/[A-Za-z]{2,}/g) || [];
  if (words.length >= 2) return true;
  // Одиночное слово считаем подписью только если это название с заглавной.
  return words.length === 1 && /^[A-Z][a-z]{2,}$/.test(words[0]) && text.trim() === words[0];
};

const pages = process.argv.slice(2);
if (!pages.length) {
  console.error('Укажите страницы: node tools/i18n-audit.mjs index.html ...');
  process.exit(2);
}

let failed = false;
for (const page of pages) {
  const en = await audit(page, 'en');
  const ru = await audit(page, 'ru');

  // 1. Английский — язык источника: кириллицы в нём быть не должно.
  const russianInEnglish = [...en.keys()].filter((text) => CYRILLIC.test(text) && !DATA_TERMS.has(text));
  // 2. Русский режим: подписи, которые остались английскими.
  const untranslated = [...ru.keys()]
    .filter((text) => en.has(text) && !CYRILLIC.test(text) && looksLikeLabel(text))
    .sort((a, b) => a.localeCompare(b, 'en'));

  const rendered = en.size > 0 || ru.size > 0;
  let verdict = 'ок';
  if (!rendered) verdict = 'интерфейс не отрисован — проверка ничего не значит';
  else if (russianInEnglish.length || untranslated.length) { verdict = 'есть непереведённые строки'; failed = true; }
  else if (!ru.size) verdict = 'русский режим пуст — перевод не подключился';

  console.log(`### ${page}: EN ${en.size} / RU ${ru.size} — ${verdict}`);
  for (const text of russianInEnglish.slice(0, 15)) console.log(`  RU в EN: «${text}» [${[...en.get(text)].slice(0, 2).join(', ')}]`);
  for (const text of untranslated.slice(0, 15)) console.log(`  EN в RU: «${text}» [${[...ru.get(text)].slice(0, 2).join(', ')}]`);
}

if (failed) process.exit(1);
