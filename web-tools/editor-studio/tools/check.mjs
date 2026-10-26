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
 *   4.2 оболочка студии: меню — рельс иконок, который раскрывается при
 *       наведении/фокусе, а поиск по редакторам убран;
 *   5. английский каталог покрывает статический текст страниц (разметку и
 *      подсказки) и текст интерфейса из JS-литералов; строки, которых нет в
 *      каталоге, печатаются списком или считаются ошибкой.
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
  // toast принимает (message, type); объект {message, type} покажется как «[object Object]».
  if (/\.toast\(\s*\{/.test(text)) {
    problems.push(`${name}: toast({...}) — сигнатура toast(message, type)`);
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
// Позиция — один общий UI-элемент PositionPicker: карточка точки, мир, кнопка
// карты и кнопка ручного ввода. Карта рендерится локально
// (vendor/ddnet-map): ни один адрес не должен уходить в интернет.

const positionPickerPath = path.join(CORE_DIR, 'position-picker.js');
const positionPickerCode = fs.existsSync(positionPickerPath) ? read(positionPickerPath) : '';
if (!positionPickerCode) {
  problems.push('editor-core/position-picker.js: нет единого UI-элемента позиции');
} else {
  for (const token of [
    'data-position-picker', 'data-position-axis', 'data-position-manual',
    'data-position-world-select', 'data-map-pick', 'data-map-world-path',
    'openMap', 'setWorldId',
  ]) {
    if (!positionPickerCode.includes(token)) {
      problems.push(`editor-core/position-picker.js: элемент позиции потерял ${token}`);
    }
  }
}
if (!fs.existsSync(path.join(CORE_DIR, 'position-picker.css'))) {
  problems.push('editor-core/position-picker.css: нет оформления единого элемента позиции');
}
if (!fieldCode.includes('PositionPicker.render')) {
  problems.push('editor-core/field-renderer.js: vec2 должен рендериться через PositionPicker.render');
}
if (/grid-cols-2[^`]*vecFields|vecFields[^`]*grid-cols-2/.test(fieldCode)) {
  problems.push('editor-core/field-renderer.js: X и Y снова в двух колонках вместо карточки позиции');
}

const loaderCode = read(path.join(CORE_DIR, 'page-loader.js'));
if (!loaderCode.includes('editor-core/point-map.js')) {
  problems.push('editor-core/page-loader.js: не подключено окно карты (editor-core/point-map.js)');
}
if (!loaderCode.includes('editor-core/map-worlds.js')) {
  problems.push('editor-core/page-loader.js: не подключён общий модуль миров (editor-core/map-worlds.js)');
}
if (!loaderCode.includes('editor-core/position-picker.js') || !loaderCode.includes('editor-core/position-picker.css')) {
  problems.push('editor-core/page-loader.js: не подключён PositionPicker (JS + CSS)');
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

// Кастомные позиции задач квестов тоже используют общий элемент, а не свою строку X/Y.
const questsPage = read(path.join(ROOT, 'quests-editor.html'));
for (const token of [
  'PositionPicker.render',
  'PositionPicker.openMap',
  'data-step-action="pin"',
  'data-map-picker="quest-step-position"',
  'data-position-picker-host="quest-tasks"',
  "mapWorldPath: 'WorldID'",
  "mapPickerTag: 'quest-step-position'",
]) {
  if (!questsPage.includes(token)) {
    problems.push(`quests-editor.html: позиции шагов потеряли ${token}`);
  }
}
if (/class="editor-vec2"/.test(questsPage)) {
  problems.push('quests-editor.html: осталась старая строка X/Y вместо PositionPicker');
}

// ── 2.4. Флаги map-picker ───────────────────────────────────────────────────
// Запрет COLLIDE/DEATH/UNHOOK включается при объявлении элемента позиции.
// Дополнительные флаги подсвечивают нужные динамические слои на карте.

for (const token of ['data-map-options', 'safeTiles', 'highlightTiles', 'radiusPath', 'showTiles', 'editor-map:validity']) {
  if (!positionPickerCode.includes(token)) {
    problems.push(`editor-core/position-picker.js: потерян флаг/событие карты ${token}`);
  }
}
if (!fieldCode.includes('safeTiles: ui.safeTiles')) {
  problems.push('editor-core/field-renderer.js: флаги карты не проброшены из ui в PositionPicker.render');
}
for (const [page, token] of [
  ['mobs-editor.html', 'safeTiles: false'],
  ['mobs-editor.html', 'highlightTiles: [209]'],
  ['mobs-editor.html', "radiusPath: 'Radius'"],
  ['bots-npc-editor.html', 'safeTiles: true'],
  ['quests-editor.html', 'safeTiles: true'],
  ['aethers-editor.html', 'showTiles: true'],
  ['dungeons-editor.html', 'showTiles: true'],
  ['warehouse-editor.html', 'showTiles: true'],
]) {
  if (!read(path.join(ROOT, page)).includes(token)) {
    problems.push(`${page}: не используется флаг карты ${token}`);
  }
}
if (!read(path.join(CORE_DIR, 'point-map.js')).includes("params.set('safe'")) {
  problems.push('editor-core/point-map.js: флаг safeTiles не передаётся в окно карты');
}
if (fs.existsSync(pickerDir)) {
  const pickerJsCode = read(path.join(pickerDir, 'picker.js'));
  const overlaysCode = read(path.join(pickerDir, 'picker-overlays.js'));
  const pickerHtmlCode = read(pickerHtmlPath);
  for (const token of ["qs.get('safe')", 'editor-map:validity', 'state.invalid', 'drawPointMarker']) {
    if (!pickerJsCode.includes(token)) problems.push(`vendor/ddnet-map/picker.js: нет проверки тайлов (${token})`);
  }
  for (const token of ['blockedTile', 'drawHighlightedTiles', 'drawTileBox']) {
    if (!overlaysCode.includes(token)) problems.push(`vendor/ddnet-map/picker-overlays.js: нет подсветки слоёв (${token})`);
  }
  if (!pickerHtmlCode.includes('pk-validity')) {
    problems.push('vendor/ddnet-map/picker.html: нет индикатора невалидной точки');
  }
}

// World 0 (Tutorilishe) is a real tw_worlds row. Never confuse it with the
// absence of a selected world in the picker or map API.
const pointMapCode = read(path.join(CORE_DIR, 'point-map.js'));
const mapWorldsPath = path.join(CORE_DIR, 'map-worlds.js');
const mapWorldsCode = fs.existsSync(mapWorldsPath) ? read(mapWorldsPath) : '';
if (!mapWorldsCode) {
  problems.push('editor-core/map-worlds.js: нет общего модуля миров (карточка и окно карты должны делить список)');
} else if (!mapWorldsCode.includes('Number.isSafeInteger(n) && n >= 0')) {
  problems.push('editor-core/map-worlds.js: world ID 0 должен оставаться валидным');
}
if (!pointMapCode.includes('parseWorldId(worldId) !== null')) {
  problems.push('editor-core/point-map.js: world ID 0 должен оставаться валидным при открытии карты');
}
for (const token of ['editor-map:applied', 'editor-map:world', 'editor-map:closed', 'editor-map:validity', 'data-map-x-path', 'mapOptions']) {
  if (!pointMapCode.includes(token)) {
    problems.push(`editor-core/point-map.js: нет связи с PositionPicker (${token})`);
  }
}
// Две точки открытия карты (глобальный слушатель + PositionPicker) были
// источником рассинхрона мира. Оставляем одну — у элемента позиции.
if (/data-map-pick/.test(pointMapCode)) {
  problems.push('editor-core/point-map.js: кнопку карты открывает PositionPicker, второй слушатель клика не нужен');
}
if (!positionPickerCode.includes("action === 'map'") || !positionPickerCode.includes('PointMap.open')) {
  problems.push('editor-core/position-picker.js: кнопка карты должна вызывать PointMap.open');
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

// ── 4.2. Оболочка студии: рельс меню, фильтр и палитра команд ────────────────
//
// Меню по умолчанию сжато до столбца иконок и раскрывается при наведении или
// фокусе. Панель вынесена из потока, поэтому раскрытие не пересобирает iframe
// редакторов (их теперь несколько — keep-alive вкладки). Навигацию дополняют
// встроенный фильтр списка (navFilter) и палитра команд Ctrl+K
// (editor-core/studio-palette.js); стили оболочки лежат в
// editor-core/studio-shell.css, поэтому токены ищем в обоих файлах.

const studioShell = read(path.join(ROOT, 'index.html'))
  + '\n' + read(path.join(ROOT, 'editor-core', 'studio-shell.css'));
for (const token of [
  '--studio-rail-w', '--studio-panel-w', '.studio-sidebar__panel',
  '.studio-sidebar.is-rail', '.studio-sidebar.is-open',
  'body.studio-sidebar-pinned', 'setSidebarPinned', 'setSidebarOpen',
  'pinSidebarBtn', 'id="navFilter"', 'studio-palette.js', 'id="studioFrames"',
]) {
  if (!studioShell.includes(token)) {
    problems.push(`оболочка студии потеряла ${token} — проверьте index.html и editor-core/studio-shell.css`);
  }
}
for (const token of ['id="studioSearch"', 'id="mobileSearch"', 'studio-search-block', 'studio-sidebar-collapsed']) {
  if (studioShell.includes(token)) {
    problems.push(`оболочка студии: устаревший поиск вернулся — ${token} заменён фильтром navFilter и палитрой Ctrl+K`);
  }
}

// ── 2.2. Языковые файлы ──────────────────────────────────────────────────────
//
// Источник строк интерфейса — английский: на нём написаны разметка и код.
// Остальные языки лежат в `localization/`: файл `lang_<код>.js` регистрирует
// себя в реестре и содержит словарь «английский текст → перевод». Здесь мы
// проверяем, что файлы валидны, что манифест их подключает и что русский
// словарь реально покрывает интерфейс.

const CYRILLIC = /[А-Яа-яЁё]/;
const normalize = (text) => text.replace(/\s+/g, ' ').trim();
const LOCALIZATION_DIR = path.join(ROOT, 'localization');

const langFiles = fs.readdirSync(LOCALIZATION_DIR)
  .filter((name) => /^lang_[a-z-]+\.js$/.test(name))
  .sort();

let registry = null;
try {
  const sandboxWindow = {};
  sandboxWindow.window = sandboxWindow;
  const context = vm.createContext({ window: sandboxWindow, console });
  vm.runInContext(read(path.join(LOCALIZATION_DIR, 'index.js')), context);
  registry = sandboxWindow.EditorCoreLocalization;
  for (const file of registry.files || []) {
    if (!fs.existsSync(path.join(LOCALIZATION_DIR, file))) {
      problems.push(`localization/index.js: в манифесте указан отсутствующий файл ${file}`);
      continue;
    }
    try {
      vm.runInContext(read(path.join(LOCALIZATION_DIR, file)), context);
    } catch (error) {
      problems.push(`localization/${file}: не удалось выполнить — ${error.message}`);
    }
  }
} catch (error) {
  problems.push(`localization/index.js: не удалось выполнить — ${error.message}`);
}

const languages = registry?.languages || {};
const sourceLanguage = registry?.sourceLanguage || 'en';

if (registry) {
  for (const file of langFiles) {
    if (!(registry.files || []).includes(file)) {
      problems.push(`localization/${file}: файл не указан в манифесте localization/index.js`);
    }
  }
  if (!registry.language(sourceLanguage)) {
    problems.push(`localization: язык-источник «${sourceLanguage}» не зарегистрирован (нет localization/lang_${sourceLanguage}.js)`);
  }
  if (!registry.language(registry.defaultLanguage)) {
    problems.push(`localization: язык по умолчанию «${registry.defaultLanguage}» не зарегистрирован`);
  }
  for (const [code, language] of Object.entries(languages)) {
    if (!language.name || !language.locale) problems.push(`localization/lang_${code}.js: нет name или locale`);
    if (code === sourceLanguage) {
      if (Object.keys(language.strings).length) {
        problems.push(`localization/lang_${code}.js: у языка-источника не должно быть словаря`);
      }
      continue;
    }
    if (!Object.keys(language.strings).length) {
      problems.push(`localization/lang_${code}.js: пустой словарь`);
    }
    for (const [key, value] of Object.entries(language.strings)) {
      if (CYRILLIC.test(key)) problems.push(`localization/lang_${code}.js: ключ с кириллицей «${key}» (источник должен быть английским)`);
      if (!value || value === key) problems.push(`localization/lang_${code}.js: «${key}» не переведён`);
    }
  }
}

// Словарь русского языка: «английский текст» → «русский текст».
const dict = new Map(Object.entries(registry?.strings('ru') || {}));

// ── 3. Источник строк — английский, перевод на русский есть ──────────────────

/** Код без комментариев: иначе пояснение на русском засчитается за строку UI. */
const stripComments = (text) => {
  let out = '';
  let i = 0;
  while (i < text.length) {
    const ch = text[i];
    const next = text[i + 1];
    if (ch === '/' && next === '/') { while (i < text.length && text[i] !== '\n') i += 1; continue; }
    if (ch === '/' && next === '*') {
      i += 2;
      while (i < text.length && !(text[i] === '*' && text[i + 1] === '/')) i += 1;
      i += 2;
      continue;
    }
    if (ch === '"' || ch === "'" || ch === '`') {
      const quote = ch;
      out += ch;
      i += 1;
      while (i < text.length) {
        out += text[i];
        if (text[i] === '\\') { i += 1; if (i < text.length) out += text[i]; i += 1; continue; }
        if (text[i] === quote) { i += 1; break; }
        i += 1;
      }
      continue;
    }
    out += ch;
    i += 1;
  }
  return out;
};

/** Подстановки `${...}` заменяем метками, чтобы получить ключ «ID {0} · {1}». */
const withMarkers = (markup) => {
  let index = 0;
  return markup.replace(/\$\{[^}]*\}/g, () => `\u0001${index++}\u0001`);
};
const unmark = (text) => {
  let next = 0;
  const map = new Map();
  return text.replace(/\u0001(\d+)\u0001/g, (whole, n) => {
    if (!map.has(n)) map.set(n, `{${next++}}`);
    return map.get(n);
  });
};

/** Текст интерфейса внутри разметки: текстовые узлы и подписи атрибутов. */
const markupTexts = (markup) => {
  const found = [];
  const marked = withMarkers(markup);
  for (const match of marked.matchAll(/>([^<>]+)</g)) {
    const text = normalize(unmark(match[1]));
    if (text) found.push(text);
  }
  for (const attr of ['title', 'placeholder', 'aria-label', 'alt', 'data-tooltip']) {
    const re = new RegExp(`${attr}="([^"]*)"`, 'gi');
    for (const match of marked.matchAll(re)) {
      const text = normalize(unmark(match[1]));
      if (text) found.push(text);
    }
  }
  return found;
};

/** Экранирования в литералах: «\n» в коде — это перевод строки. */
const decodeEscapes = (value) => value
  .replace(/\\n/g, '\n')
  .replace(/\\t/g, '\t')
  .replace(/\\"/g, '"')
  .replace(/\\'/g, "'")
  .replace(/\\\\/g, '\\');

/** Строки интерфейса из кода: литералы и шаблоны с подстановками. */
const codeTexts = (code) => {
  const found = [];
  const re = /"((?:[^"\\\n]|\\.)*)"|'((?:[^'\\\n]|\\.)*)'|`((?:[^`\\]|\\.)*)`/g;
  let match;
  while ((match = re.exec(code))) {
    // Сообщения разработчику (`new Error('…')`, `console.warn('…')`) — не интерфейс.
    const context = code.slice(Math.max(0, match.index - 40), match.index);
    if (/(?:Error|console\.(?:\w+)|throw)\s*\(\s*$/.test(context)) continue;
    const template = match[3] !== undefined;
    const value = template ? match[3] : (match[1] ?? match[2] ?? '');
    if (!value.trim()) continue;
    if (/<[a-z!/][^>]*>/i.test(value)) { found.push(...markupTexts(value)); continue; }
    if (!template) { found.push(normalize(decodeEscapes(value))); continue; }
    let index = 0;
    const candidate = decodeEscapes(value).replace(/\$\{[^}]*\}/g, () => `{${index++}}`);
    for (const piece of candidate.split(/[<>]/)) {
      const text = normalize(piece);
      if (text) found.push(text);
    }
  }
  return found;
};

/** Ключ словаря с подстановками: «Found: {0}» → регулярное выражение. */
const templateKeys = [...dict.keys()]
  .filter((key) => key.includes('{'))
  .map((key) => new RegExp(`^${key.replace(/[.*+?^${}()|[\]\\]/g, '\\$&').replace(/\\{\d+\\}/g, '[\\s\\S]*?')}$`));
const translatedText = (text) => {
  const core = normalize(text);
  if (!core) return true;
  if (dict.has(core)) return true;
  return templateKeys.some((re) => re.test(core));
};

/**
 * Значения из базы: остаются английскими в обоих режимах (перечисления
 * предметов, флаги квестов, эффекты мобов). Это не интерфейс, а данные.
 */
const DATA_STRINGS = new Set([
  'Default', 'Usable', 'Resource', 'Decoration', 'Equipment', 'Potion', 'Currency', 'Quest', 'Settings', 'Other',
  'Resource harvestable', 'Resource mineable', 'Resource fishes', 'Single use x1', 'Multiple use x99',
  'Equip hammer', 'Equip gun', 'Equip shotgun', 'Equip grenade', 'Equip rifle', 'Equip pickaxe', 'Equip rake',
  'Equip fishrod', 'Equip gloves', 'Equip armor (tank)', 'Equip armor (dps)', 'Equip armor (healer)',
  'Equip helmet (tank)', 'Equip helmet (dps)', 'Equip helmet (healer)', 'Equip eidolon', 'Equip title',
  'Equip potion HP', 'Equip potion MP', 'Equip potion Miner', "Can't droppable", "Can't tradeable",
  'Tiny heal', 'Type main', 'Type side', 'Type daily', 'Type weekly', 'Type repeatable', "Can't refuse",
  'Tutorial', 'Partial', 'Slowdown', 'Poison', 'Fire', 'Happy', 'Angry', 'Misc', 'Path', 'Unnamed',
  'Без точки активности',
  // имена собственные и технические токены
  'Editor Studio', 'English', 'Liberation Mono', 'Backspace', 'use strict', 'unhandled rejection',
  // технические значения, которые не переводятся
  'NULL', 'OK', 'OUT', 'POST', 'GET', 'PUT', 'SELECT', 'TRUE', 'FALSE', 'Title', 'Skin.', 'text/plain',
  'Enter', 'Escape', 'Shift', 'Delete', 'Tab', 'Backspace', 'Home', 'End', 'Business', 'Data', 'Personal',
  'Esc', 'Ctrl K', 'Ctrl B', 'Ctrl Tab', 'Ctrl Shift T', 'Alt 1…9', 'Alt ←/→', 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789',
  'Default (components/default.h)', 'application/json', 'NPC • Editor Studio', 'Editor registry requires an id.',
]);

/**
 * Похоже ли на текст интерфейса, а не на техническое значение.
 *
 * Источник теперь английский, поэтому фильтровать приходится именно английский
 * текст: иначе в «непереведённые» попадут идентификаторы, css-переменные, имена
 * файлов и значения вида `world_id`. Считаем текстом интерфейса фразу с
 * пробелами или подпись с заглавной буквы — без служебных символов кода.
 */
const looksLikeProse = (text) => {
  if (text.length < 2 || !/[A-Za-z]/.test(text)) return false;
  if (/[{}$#@\\_=<>|]/.test(text)) return false;
  // CSS и инлайновые стили: «position:fixed», «padding:4px 10px».
  if (/[a-z-]+:\s*[-0-9a-z.,()]/.test(text)) return false;
  if (/\b\d+px\b|\b\d+deg\b|rgba?\(/i.test(text)) return false;
  // Список css-классов: все слова строчные, без знаков предложения.
  const tokens = text.split(/\s+/).filter(Boolean);
  if (tokens.length > 1 && tokens.every((token) => /^[a-z0-9[\].:_-]+$/.test(token))) return false;
  if (/\.[a-z]{2,5}\b/i.test(text) && /(?:html|js|css|php|json|map|sql|png|svg)/i.test(text)) return false;
  if (text.includes('var(--')) return false;             // css-выражение с переменной
  if (/^https?:\/\//.test(text)) return false;            // адрес
  if (/^var\(/.test(text)) return false;
  // css-функция: «minmax(0,1fr)»
  if (/^(?:min|max|minmax|calc|clamp|repeat|rgba?|hsla?)\(/i.test(text)) return false;
  // Технические значения: горячие клавиши, пути, css-селекторы, консольные
  // префиксы, свойства вида `Position.x`, аббревиатуры (ID, JSON, NPC).
  if (/^(?:Ctrl|Alt|Shift|Meta|Cmd)\+/.test(text)) return false;
  if (/^[[/.\\]/.test(text)) return false;
  if (/^[A-Z]{1,5}$/.test(text)) return false;
  if (/^[A-Za-z]+(?:\.[A-Za-z]+)+$/.test(text)) return false;
  if (/^[.,#\w-]+(?:, [.#\w-]+)+$/.test(text)) return false;
  // css-селектор: «input, textarea, select, [contenteditable]»
  if (/^[a-z][\w-]*(?:\s*,\s*(?:[a-z.#*][\w-]*|\[[^\]]*\]))+$/i.test(text)) return false;
  // фрагмент кода: «lim.message.min ?»
  if (/^[A-Za-z_$][\w$]*(?:\.[\w$]+)*\s*\?$/.test(text)) return false;
  if (/[a-z][A-Z]/.test(text)) return false;                 // camelCase-идентификатор
  if (/^[a-z0-9-]+$/.test(text)) return false;               // слаг или имя поля
  if (/[A-Za-z]-[A-Za-z]/.test(text) && !/ /.test(text)) return false;
  if (/^\d/.test(text)) return false;
  const words = text.match(/[A-Za-z]{2,}/g) || [];
  if (!words.length) return false;
  if (words.length === 1 && !/^[A-Z]/.test(text)) return false;
  return true;
};
// Служебные слова — тоже не подписи.
const NOT_UI = new Set(['a', 'an', 'the', 'of', 'in', 'on', 'at', 'to', 'for', 'and', 'or', 'is', 'are']);

const untranslated = new Map(); // строка интерфейса → где встретилась
const russianSource = new Map(); // русский текст в источнике → где встретился
const checkText = (text, where) => {
  const core = normalize(text);
  if (!core || core.length < 2) return;
  if (DATA_STRINGS.has(core)) return;
  if (CYRILLIC.test(core)) {
    if (!russianSource.has(core)) russianSource.set(core, new Set());
    russianSource.get(core).add(where);
    return;
  }
  if (!looksLikeProse(core)) return;              // числа, идентификаторы, слаги
  if (NOT_UI.has(core.toLowerCase())) return;
  if (translatedText(core)) return;
  if (!untranslated.has(core)) untranslated.set(core, new Set());
  untranslated.get(core).add(where);
};

for (const page of list(ROOT, '.html')) {
  const html = read(path.join(ROOT, page));
  const withoutComments = html.replace(/<!--[\s\S]*?-->/g, ' ');
  // Стили и скрипты — не разметка: css-селекторы и комментарии в них не интерфейс.
  const markupOnly = withoutComments
    .replace(/<script[\s\S]*?<\/script>/gi, ' ')
    .replace(/<style[\s\S]*?<\/style>/gi, ' ');
  for (const text of markupTexts(markupOnly)) {
    checkText(text, page);
  }
  for (const match of withoutComments.matchAll(/<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/gi)) {
    for (const text of codeTexts(stripComments(match[1]))) checkText(text, page);
  }
}
for (const name of list(CORE_DIR, '.js')) {
  if (name === 'i18n.js') continue;
  for (const text of codeTexts(stripComments(read(path.join(CORE_DIR, name))))) {
    checkText(text, `editor-core/${name}`);
  }
}
for (const text of codeTexts(stripComments(read(path.join(ROOT, 'studio.manifest.js'))))) {
  checkText(text, 'studio.manifest.js');
}

for (const [text, where] of [...russianSource].sort((a, b) => a[0].localeCompare(b[0], 'ru'))) {
  problems.push(`русский текст в источнике: «${text}» (${[...where].slice(0, 3).join(', ')})`);
}
for (const [text, where] of [...untranslated].sort((a, b) => a[0].localeCompare(b[0], 'en'))) {
  problems.push(`нет русского перевода: «${text}» (${[...where].slice(0, 3).join(', ')})`);
}

// ── Отчёт ────────────────────────────────────────────────────────────────────

console.log(`editor-core: ${list(CORE_DIR, '.js').length} модулей, страниц: ${list(ROOT, '.html').length}`);
console.log(`языки: ${Object.keys(languages).join(', ') || '—'} · русский словарь: ${dict.size} строк`);

if (untranslated.size) {
  console.log(`\nСтроки без русского перевода (${untranslated.size}):`);
  for (const [text, where] of [...untranslated].slice(0, 20)) {
    console.log(`  «${text}» — ${[...where].slice(0, 2).join(', ')}`);
  }
  if (untranslated.size > 20) console.log(`  … и ещё ${untranslated.size - 20}`);
}
if (russianSource.size) {
  console.log(`\nРусский текст в источнике (${russianSource.size}):`);
  for (const [text, where] of [...russianSource].slice(0, 20)) {
    console.log(`  «${text}» — ${[...where].slice(0, 2).join(', ')}`);
  }
  if (russianSource.size > 20) console.log(`  … и ещё ${russianSource.size - 20}`);
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
