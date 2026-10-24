#!/usr/bin/env node
/**
 * tools/check.mjs — быстрая проверка веб-редактора без внешних зависимостей.
 *
 *   node web-tools/editor-studio/tools/check.mjs
 *
 * Что проверяется:
 *   1. синтаксис всех inline-скриптов страниц и всех модулей editor-core;
 *   2. «костыли», которые мы убрали, не вернулись: прямой fetch на страницах,
 *      нативный confirm, захардкоженная локаль дат, вызов модуля ядра без его
 *      подключения на странице;
 *   2.1 окно компонента: постоянные шапка/подвал, поля разной ширины,
 *      «Переход к шагу» отдельной строкой, data-field-path у булевых полей;
 *   2.2 подписи полей компонентов сценария есть в английском каталоге;
 *   3. секреты не утекают: data/ закрыт от веб-сервера, база и конфиг не в git,
 *      в Docker установлен pdo_sqlite;
 *   4. макет собран на общем каркасе: .editor-page + шапка, размеры из CSS,
 *      таблицы в прокручиваемых контейнерах, dvh вместо vh;
 *   4.1 оформление только через токены дизайн-системы, без палитры Tailwind;
 *   5. английский каталог покрывает статический текст страниц (разметку и
 *      подсказки); строки, которых нет в каталоге, печатаются списком.
 *
 * Код возврата 1, если есть ошибки (не покрытие перевода: это предупреждение).
 */
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const CORE_DIR = path.join(ROOT, 'editor-core');

const problems = [];
const warnings = [];
const rel = (file) => path.relative(ROOT, file).split(path.sep).join('/');

const read = (file) => fs.readFileSync(file, 'utf8');
const list = (dir, ext) => fs.readdirSync(dir).filter((name) => name.endsWith(ext)).sort();

// ── 1. Синтаксис ─────────────────────────────────────────────────────────────

for (const file of [...list(CORE_DIR, '.js').map((n) => path.join(CORE_DIR, n)), path.join(ROOT, 'studio.manifest.js')]) {
  try {
    new vm.Script(read(file), { filename: rel(file) });
  } catch (error) {
    problems.push(`${rel(file)}: ${error.message}`);
  }
}

const inlineScript = /<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/gi;
for (const page of list(ROOT, '.html')) {
  const html = read(path.join(ROOT, page));
  let index = 0;
  let match;
  while ((match = inlineScript.exec(html))) {
    index += 1;
    if (!match[1].trim()) continue;
    try {
      new vm.Script(match[1], { filename: `${page}#${index}` });
    } catch (error) {
      problems.push(`${page} (скрипт ${index}): ${error.message}`);
    }
  }
}

// ── 2. Запрещённые приёмы ────────────────────────────────────────────────────

const HTTP_MODULE = 'editor-core/http.js';
const PAGE_KIT = 'editor-core/page-kit.js';
const sources = [
  ...list(CORE_DIR, '.js').map((n) => path.join(CORE_DIR, n)),
  ...list(ROOT, '.html').map((n) => path.join(ROOT, n)),
];

for (const file of sources) {
  const name = rel(file);
  const text = read(file);
  // Комментарии и строки не разбираем: правило ищет именно вызовы.
  if (name !== HTTP_MODULE && /\bfetch\s*\(/.test(text)) {
    problems.push(`${name}: прямой fetch — используйте EditorCore.Http`);
  }
  if (name !== PAGE_KIT && /(?<![\w.$])confirm\s*\(/.test(text)) {
    problems.push(`${name}: нативный confirm — используйте EditorCore.Page.confirm`);
  }
  if (/toLocale(Date|Time)?String\(\s*'ru-RU'/.test(text)) {
    problems.push(`${name}: захардкоженная локаль — используйте I18n.locale()`);
  }
  if (name !== PAGE_KIT && /(?<![\w.$])alert\s*\(/.test(text)) {
    warnings.push(`${name}: нативный alert — лучше EditorCore.Page.alert`);
  }
}

// Модули ядра должны быть подключены прежде, чем их используют: именно на этом
// ломался вход в оболочку (EditorCore.utils без utils.js убивал весь скрипт).
const loaderModules = [...read(path.join(CORE_DIR, 'page-loader.js'))
  .matchAll(/<script src="([^"]+)"><\/script>/g)].map((m) => m[1]);
for (const page of list(ROOT, '.html')) {
  const html = read(path.join(ROOT, page));
  const own = [...html.matchAll(/<script[^>]+src="([^"]+)"/g)].map((m) => m[1]);
  const loaded = new Set([...own, ...(own.includes('editor-core/page-loader.js') ? loaderModules : [])]);
  const code = [...html.matchAll(inlineScript)].map((m) => m[1]).join('\n');
  for (const name of new Set([...code.matchAll(/EditorCore\.([A-Za-z_$][\w$]*)/g)].map((m) => m[1]))) {
    const available = [...loaded].some((src) => {
      const file = path.join(ROOT, src);
      return src.startsWith('editor-core/') && fs.existsSync(file) && read(file).includes(`EditorCore.${name}`);
    });
    if (!available) problems.push(`${page}: EditorCore.${name} используется, но модуль не подключён`);
  }
}

// ── 2.1. Окно компонента ────────────────────────────────────────────────────
//
// У окна редактирования компонента одна шапка, прокручиваемое тело и подвал с
// кнопками. Форма — сетка карточек, а не одна длинная колонка: на широком
// экране половина окна иначе оставалась пустой.

const uiModule = read(path.join(CORE_DIR, 'ui.js'));
for (const token of ['editor-modal-shell', 'editor-modal-head', 'editor-modal-title', 'editor-modal-body', 'editor-modal-foot', 'data-modal-close']) {
  if (!uiModule.includes(token)) problems.push(`editor-core/ui.js: окно компонента без ${token}`);
}

const scenarioPage = read(path.join(ROOT, 'scenario-editor.html'));
for (const token of ['sc-modal-fields', 'sc-modal-field--wide', 'sc-modal-field--narrow', 'sc-modal-nav', 'sc-modal-nav-row', 'sc-modal-nav-label', 'isWideModalField', 'modalFieldSize', 'pushNavRow']) {
  if (!scenarioPage.includes(token)) problems.push(`scenario-editor.html: форма компонента без ${token}`);
}

// Переход к шагу — отдельная строка «подпись слева, выбор справа» во всех
// компонентах: он не должен снова оказаться обычной карточкой в сетке полей.
const buildModal = scenarioPage.match(/buildModalForm\(data, step\)[^]*?return `<form[^`]*`;/);
if (buildModal) {
  if (/pushField\([^)]*next_step_id/.test(buildModal[0])) {
    problems.push('scenario-editor.html: «Переход к шагу» снова в сетке полей — нужна строка перехода (pushNavRow)');
  }
} else {
  problems.push('scenario-editor.html: не найден buildModalForm — проверка переходов пропущена');
}

// Без комментариев: иначе упоминание в пояснении «засчитывается» за код.
const fieldCode = read(path.join(CORE_DIR, 'field-renderer.js'))
  .replace(/\/\*[^]*?\*\//g, '')
  .replace(/\/\/[^\n]*/g, '');
for (const [type, render] of [['boolean', 'renderCheckbox'], ['toggle', 'renderToggle']]) {
  const block = fieldCode.match(new RegExp(`field\\.type === '${type}'[^]*?${render}\\(\\)`));
  if (block && !block[0].includes('data-field-path')) {
    problems.push(`editor-core/field-renderer.js: поле ${type} без data-field-path (ломает пометки и валидацию)`);
  }
}

// ── 2.3. Позиция (X/Y) и карта мира ─────────────────────────────────────────
//
// Позиция — один элемент во всех редакторах: X и Y в одной строке и кнопка
// «Местоположение». Карта рендерится локально (vendor/ddnet-map): ни один
// адрес не должен уходить в интернет, иначе окно пустое на закрытом сервере.

for (const token of ['editor-vec2', 'data-map-pick', 'data-map-axis', 'data-map-world-path']) {
  if (!fieldCode.includes(token)) {
    problems.push(`editor-core/field-renderer.js: позиция потеряла ${token} — сломается строка X/Y или кнопка карты`);
  }
}
if (/grid-cols-2[^`]*vecFields|vecFields[^`]*grid-cols-2/.test(fieldCode)) {
  problems.push('editor-core/field-renderer.js: X и Y снова в двух колонках вместо одной строки');
}

const loaderCode = read(path.join(CORE_DIR, 'page-loader.js'));
if (!loaderCode.includes('editor-core/point-map.js')) {
  problems.push('editor-core/page-loader.js: не подключено окно карты (editor-core/point-map.js)');
}
const pointMapPath = path.join(CORE_DIR, 'point-map.js');
if (!fs.existsSync(pointMapPath)) {
  problems.push('editor-core/point-map.js: нет модуля окна «Местоположение»');
} else if (!read(pointMapPath).includes('document.baseURI')) {
  problems.push('editor-core/point-map.js: адрес api/map.php должен считаться от страницы (document.baseURI), иначе 404 в подкаталоге');
}

const pickerDir = path.join(ROOT, 'vendor', 'ddnet-map');
const pickerHtmlPath = path.join(pickerDir, 'picker.html');
if (!fs.existsSync(pickerHtmlPath)) {
  problems.push('vendor/ddnet-map/picker.html: нет страницы выбора точки на карте');
} else {
  const pickerHtml = read(pickerHtmlPath);
  const configAt = pickerHtml.indexOf('window.twConfig');
  const coreAt = pickerHtml.indexOf('<script src="twwebgl.js"');
  if (configAt < 0 || coreAt < 0 || configAt > coreAt) {
    problems.push('vendor/ddnet-map/picker.html: twConfig должен задаваться до twwebgl.js');
  }
  if (/ddnet\.org|ddhost\.cc|https?:\/\//i.test(pickerHtml.replace(/<!--[^]*?-->/g, ''))) {
    problems.push('vendor/ddnet-map/picker.html: карта не должна тянуться из интернета');
  }
}
// Вендоренный рендер карт тоже обязан разбираться: после правки (например,
// вырезания отладочной печати) легко остаётся пустая ветка if/else, и карта
// молча перестаёт грузиться.
if (fs.existsSync(pickerDir)) {
  for (const name of list(pickerDir, '.js')) {
    const file = path.join(pickerDir, name);
    try {
      new vm.Script(read(file), { filename: `vendor/ddnet-map/${name}` });
    } catch (error) {
      problems.push(`vendor/ddnet-map/${name}: ${error.message}`);
    }
  }
  const pickerJs = path.join(pickerDir, 'picker.js');
  if (!fs.existsSync(pickerJs)) problems.push('vendor/ddnet-map/picker.js: нет скрипта выбора точки');

  const mapPickerTest = path.join(ROOT, 'tools', 'map-picker-check.mjs');
  if (!fs.existsSync(mapPickerTest)) {
    problems.push('tools/map-picker-check.mjs: нет регрессионного теста clipping/culling карты');
  } else {
    try {
      execFileSync(process.execPath, [mapPickerTest], { cwd: ROOT, stdio: 'pipe' });
    } catch (error) {
      problems.push(`map-picker clipping/culling: ${error.stderr?.toString().trim() || error.message}`);
    }
  }
}

const aetherPage = read(path.join(ROOT, 'aethers-editor.html'));
if (!aetherPage.includes("type: 'vec2'") || !aetherPage.includes("axes: { x: 'TeleX', y: 'TeleY' }") || !aetherPage.includes("mapWorldPath: 'WorldID'")) {
  problems.push('aethers-editor.html: координаты телепорта должны быть vec2 с картой мира по WorldID');
}
if (/Tele[XY]: \{ type: 'number'/.test(aetherPage)) {
  problems.push('aethers-editor.html: отдельные TeleX/TeleY поля должны использовать единый элемент позиции');
}

// World 0 (Tutorilishe) is a real tw_worlds row. Never confuse it with the
// absence of a selected world in the picker or map API.
const pointMapCode = read(path.join(CORE_DIR, 'point-map.js'));
if (!pointMapCode.includes('Number.isSafeInteger(n) && n >= 0') || !pointMapCode.includes('parseWorldId(worldId) !== null')) {
  problems.push('editor-core/point-map.js: world ID 0 должен оставаться валидным при открытии карты');
}
const mapApiCode = read(path.join(ROOT, 'api', 'map.php'));
if (!mapApiCode.includes('$hasWorldId') || mapApiCode.includes('$worldId <= 0')) {
  problems.push('api/map.php: world ID 0 должен передаваться в map-api и разрешаться из tw_worlds');
}
const dbComboCode = read(path.join(CORE_DIR, 'db.js'));
if (!fieldCode.includes('data-allow-zero') || !dbComboCode.includes("source.toLowerCase() === 'worlds'")) {
  problems.push('db_select: мир с ID 0 должен отображаться и выбираться как обычный мир');
}

const mapApiPath = path.join(ROOT, 'api', 'map.php');
if (!fs.existsSync(mapApiPath)) {
  problems.push('api/map.php: нет API карт и текстур для окна «Местоположение»');
} else {
  const mapApi = read(mapApiPath);
  for (const token of ["action === 'file'", "action === 'asset'", 'map_resolve', 'bootstrap_editor_api_roles']) {
    if (!mapApi.includes(token)) problems.push(`api/map.php: отсутствует ${token}`);
  }
  if (/https?:\/\//i.test(mapApi)) {
    problems.push('api/map.php: внешние адреса в API карт не нужны');
  }
}

// ── 3. Защита данных ────────────────────────────────────────────────────────
//
// data/ — единственное место с секретами (пароль администратора, доступ к
// игровой БД). Веб-сервер не должен отдавать оттуда ни один файл, а секреты
// не должны попадать в git: обе вещи легко потерять при рефакторинге.

const DATA_DIR = path.join(ROOT, 'data');
const htaccess = fs.existsSync(path.join(DATA_DIR, '.htaccess')) ? read(path.join(DATA_DIR, '.htaccess')) : '';
if (!/Require all denied/.test(htaccess) || !/Deny from all/.test(htaccess)) {
  problems.push('data/.htaccess: нет запрета прямого доступа (Require all denied / Deny from all)');
}

// Встроенный сервер PHP (php -S) не читает .htaccess — его закрывает router.php.
const router = fs.existsSync(path.join(ROOT, 'router.php')) ? read(path.join(ROOT, 'router.php')) : '';
if (!router) {
  problems.push('router.php: нет роутера для встроенного сервера PHP — data/ будет отдаваться браузеру');
} else if (!router.includes('data(/|$')) {
  problems.push('router.php: не закрывает каталог data/');
}

const scenariosHt = fs.existsSync(path.join(ROOT, 'scenarios', '.htaccess'))
  ? read(path.join(ROOT, 'scenarios', '.htaccess'))
  : '';
if (!/Require all denied/.test(scenariosHt)) {
  problems.push('scenarios/.htaccess: файлы сценариев должны отдаваться только через api.php');
}

const rootIgnore = fs.existsSync(path.join(ROOT, '..', '..', '.gitignore'))
  ? read(path.join(ROOT, '..', '..', '.gitignore'))
  : '';
for (const pattern of ['editor-studio/data/*.sqlite', 'editor-studio/data/*.json']) {
  if (!rootIgnore.includes(pattern)) problems.push(`.gitignore: не исключён ${pattern}`);
}

const dockerfile = fs.existsSync(path.join(ROOT, 'Dockerfile')) ? read(path.join(ROOT, 'Dockerfile')) : '';
if (dockerfile && !/pdo_sqlite/.test(dockerfile)) {
  problems.push('Dockerfile: не установлено расширение pdo_sqlite — SQLite-хранилище не запустится');
}
// Конфиг с паролем не должен вернуться в git (он остаётся на сервере).
if (fs.existsSync(path.join(ROOT, 'data', 'db-config.json')) && fs.existsSync(path.join(ROOT, '..', '..', '.git'))) {
  try {
    const tracked = execFileSync('git', ['ls-files', '--error-unmatch', 'web-tools/editor-studio/data/db-config.json'], {
      cwd: path.join(ROOT, '..', '..'), stdio: 'ignore',
    });
    if (tracked !== null) problems.push('data/db-config.json снова в git — конфиг с паролем должен оставаться на сервере');
  } catch { /* не отслеживается — так и нужно */ }
}

// ── 4. Макет и адаптивность ─────────────────────────────────────────────────
//
// Все вкладки строятся на одном каркасе (.editor-page + .editor-page-header +
// .editor-page-body), а размеры берутся из editor-template.css, а не из style="".
// Эти правила дешёвые и ловят именно те поломки, что видны на телефоне.

for (const page of list(ROOT, '.html')) {
  if (page === 'index.html') continue; // оболочка студии — свой каркас
  const html = read(path.join(ROOT, page));
  // Именно токены класса, а не подстрока: editor-page-header не должен
  // засчитываться за editor-page.
  const tokens = new Set([...html.matchAll(/class="([^"]*)"/g)].flatMap((m) => m[1].split(/\s+/)));
  if (!tokens.has('editor-page')) {
    problems.push(`${page}: нет каркаса .editor-page (страница должна использовать общий макет)`);
  } else if (!tokens.has('editor-page-header')) {
    problems.push(`${page}: нет .editor-page-header (шапка вкладки по общему макету)`);
  }
  const fixedSize = html.match(/style="[^"]*\b(?:min-width|max-height|width|height)\s*:\s*\d+px/g);
  if (fixedSize) {
    problems.push(`${page}: фиксированный размер в style (${fixedSize[0]}…) — используйте класс из editor-template.css`);
  }
  if (tokens.has('h-screen')) {
    problems.push(`${page}: h-screen вместо dvh — на телефоне обрежет нижнюю часть`);
  }
  if (html.includes('<table') && !/editor-table-wrap|editor-list-scroll|cr-table-scroll|editor-sync-map-wrap|editor-sync-pull-table-wrap/.test(html)) {
    problems.push(`${page}: таблица без прокручиваемого контейнера — на узком экране разъедется`);
  }
}

// ── 4.1. Оформление идёт через токены дизайн-системы ────────────────────────
//
// Страницы не должны знать, тёмная тема включена или светлая: цвет берётся из
// editor-theme.css (var(--editor-*)) или из семантических классов, а не из
// палитры Tailwind. Иначе в светлой теме появляется «белое на белом».

const LEGACY_PALETTE = /\b(?:text|bg|border|ring|divide)-(?:slate|indigo|zinc|neutral|stone|gray)-\d{2,3}(?:\/\d+)?\b/;
const paletteFiles = [...list(ROOT, '.html').map((f) => ['', f]), ...list(CORE_DIR, '.js').map((f) => ['editor-core/', f])];
for (const [dir, file] of paletteFiles) {
  const body = read(path.join(ROOT, dir, file));
  const hit = body.match(LEGACY_PALETTE);
  if (hit) {
    problems.push(`${dir}${file}: цвет из палитры Tailwind «${hit[0]}» — используйте токен или класс editor-*`);
  }
}

const themeCss = read(path.join(CORE_DIR, 'editor-theme.css'));
for (const token of ['--editor-surface-2', '--editor-line', '--editor-text-muted', '--editor-radius-md', '--editor-gap-3']) {
  if (!themeCss.includes(token + ':')) {
    problems.push(`editor-core/editor-theme.css: пропал токен ${token} — на нём держится оформление всех вкладок`);
  }
}
// Тени — только у «парящих» слоёв (модалки, выпадашки, тосты). Статичная
// карточка с тенью возвращает «решётку из рамок», от которой мы ушли.
if (/\.editor-card\{[^}]*box-shadow:\s*(?!none)/.test(themeCss)) {
  problems.push('editor-core/editor-theme.css: .editor-card снова с тенью — карточки живут на волосяной рамке');
}

// ── 2.2. Подписи компонентов сценария переведены ───────────────────────────
//
// Схемы компонентов живут в JS, поэтому проверка страниц их не видит: подпись
// вроде «disable_group_damage» проходила и в русский, и в английский интерфейс.

try {
  const schemaCtx = { window: {} };
  schemaCtx.window.window = schemaCtx.window;
  vm.createContext(schemaCtx);
  vm.runInContext(read(path.join(CORE_DIR, 'registry.js')), schemaCtx);
  vm.runInContext(read(path.join(CORE_DIR, 'i18n-en.js')), schemaCtx);
  const schemas = schemaCtx.window.EditorCore?.schemas?.componentSchemas || {};
  const en = new Set(Object.keys(schemaCtx.window.EditorCoreDict?.en || {}));
  const labels = new Set();
  const collect = (fields) => {
    for (const field of Object.values(fields || {})) {
      const label = String(field?.label || '').trim();
      if (label) labels.add(label);
      if (field?.type === 'list') collect(field.itemFields);
      for (const option of field?.ui?.options || []) {
        if (option && typeof option === 'object' && option.label) labels.add(String(option.label).trim());
      }
    }
  };
  Object.values(schemas).forEach((schema) => collect(schema.fields));
  for (const label of labels) {
    if (!/[А-Яа-яЁё]/.test(label)) continue; // латиница (ID, PvP) перевода не требует
    if (!en.has(label)) problems.push(`editor-core/registry.js: подпись «${label}» без английского перевода`);
  }
} catch (error) {
  problems.push(`editor-core/registry.js: не удалось разобрать схемы компонентов — ${error.message}`);
}

// ── 3. Покрытие английского каталога ─────────────────────────────────────────

const dictSource = read(path.join(CORE_DIR, 'i18n-en.js'));
const dict = new Map();
for (const match of dictSource.matchAll(/^\s*"((?:[^"\\]|\\.)*)":\s*"((?:[^"\\]|\\.)*)",$/gm)) {
  try {
    dict.set(JSON.parse(`"${match[1]}"`), JSON.parse(`"${match[2]}"`));
  } catch {
    problems.push(`editor-core/i18n-en.js: не удалось разобрать ключ ${match[1]}`);
  }
}

const CYRILLIC = /[А-Яа-яЁё]/;
const normalize = (text) => text.replace(/\s+/g, ' ').trim();
const entities = { '&amp;': '&', '&lt;': '<', '&gt;': '>', '&quot;': '"', '&#39;': "'", '&nbsp;': ' ' };
const decode = (text) => text.replace(/&(?:amp|lt|gt|quot|#39|nbsp);/g, (m) => entities[m] ?? m);

/** Статический текст страницы: текстовые узлы и подписи в атрибутах. */
const staticStrings = (html) => {
  const found = new Set();
  const cleaned = html
    .replace(/<script[\s\S]*?<\/script>/gi, ' ')
    .replace(/<style[\s\S]*?<\/style>/gi, ' ')
    .replace(/<!--[\s\S]*?-->/g, ' ');
  for (const match of cleaned.matchAll(/>([^<>]+)</g)) {
    const text = normalize(decode(match[1]));
    if (text && CYRILLIC.test(text)) found.add(text);
  }
  for (const attr of ['title', 'placeholder', 'aria-label', 'alt', 'data-tooltip']) {
    const re = new RegExp(`${attr}="([^"]*)"`, 'gi');
    for (const match of cleaned.matchAll(re)) {
      const text = normalize(decode(match[1]));
      if (text && CYRILLIC.test(text) && !text.includes('${')) found.add(text);
    }
  }
  return [...found];
};

const untranslated = new Map();
for (const page of list(ROOT, '.html')) {
  for (const text of staticStrings(read(path.join(ROOT, page)))) {
    if (dict.has(text)) continue;
    // Шаблонные ключи каталога тоже считаются покрытием.
    const covered = [...dict.keys()].some((key) => {
      if (!key.includes('{')) return false;
      const pattern = new RegExp(`^${key.replace(/[.*+?^${}()|[\]\\]/g, '\\$&').replace(/\\\{\d+\\\}/g, '.*?')}$`);
      return pattern.test(text);
    });
    if (!covered) untranslated.set(text, page);
  }
}

// ── Отчёт ────────────────────────────────────────────────────────────────────

console.log(`editor-core: ${list(CORE_DIR, '.js').length} модулей, страниц: ${list(ROOT, '.html').length}`);
console.log(`каталог EN: ${dict.size} строк`);

if (untranslated.size) {
  console.log(`\nБез перевода (${untranslated.size}):`);
  for (const [text, page] of [...untranslated].slice(0, 40)) console.log(`  ${page}: ${text}`);
  if (untranslated.size > 40) console.log(`  … и ещё ${untranslated.size - 40}`);
  warnings.push(`${untranslated.size} строк интерфейса без английского перевода`);
}

if (warnings.length) {
  console.log('\nПредупреждения:');
  for (const warning of warnings) console.log(`  • ${warning}`);
}

if (problems.length) {
  console.log('\nОшибки:');
  for (const problem of problems) console.log(`  ✗ ${problem}`);
  process.exit(1);
}

console.log('\nОК: синтаксис и правила соблюдены.');
