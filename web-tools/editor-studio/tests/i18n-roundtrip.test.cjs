const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

const root = path.join(__dirname, '..');
const source = (name) => fs.readFileSync(path.join(root, 'editor-core', name), 'utf8');
const language = (name) => fs.readFileSync(path.join(root, 'localization', name), 'utf8');

/**
 * Минимальный фейковый DOM: i18n.js нужны только узлы, их атрибуты и подписи
 * кнопок переключателя, поэтому браузер целиком здесь не требуется.
 */
function createDom() {
  const makeText = (value) => ({
    nodeType: 3,
    nodeValue: value,
    parentElement: null,
    isConnected: true,
  });
  const makeElement = (tagName, children = []) => {
    const attrs = new Map();
    const el = {
      nodeType: 1,
      tagName,
      isContentEditable: false,
      isConnected: true,
      childNodes: [],
      parentElement: null,
      style: {},
      classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
      hasAttribute: (name) => attrs.has(name),
      getAttribute: (name) => (attrs.has(name) ? attrs.get(name) : null),
      setAttribute: (name, value) => { attrs.set(name, String(value)); },
      appendChild: (child) => { el.childNodes.push(child); child.parentElement = el; return child; },
      querySelector: () => null,
      querySelectorAll: () => [],
      addEventListener() {},
      set textContent(value) { el.childNodes = [makeText(value)]; },
    };
    for (const child of children) el.appendChild(child);
    return el;
  };
  return { makeText, makeElement };
}

function harness() {
  const { makeText, makeElement } = createDom();
  const body = makeElement('BODY');
  const root = makeElement('DIV');

  const store = new Map();
  const localStorage = {
    getItem: (key) => (store.has(key) ? store.get(key) : null),
    setItem: (key, value) => store.set(key, String(value)),
    removeItem: (key) => store.delete(key),
  };

  const document = {
    title: 'Scenarios • Editor Studio',
    readyState: 'complete',
    documentElement: makeElement('HTML'),
    head: makeElement('HEAD'),
    body,
    createElement: (tag) => makeElement(String(tag).toUpperCase()),
    createTextNode: (value) => makeText(value),
    querySelector: () => null,
    querySelectorAll: () => [],
    addEventListener() {},
    getElementById: () => null,
  };

  const window = {
    document,
    localStorage,
    navigator: { language: 'en-US' },
    location: { origin: 'http://localhost', href: 'http://localhost/' },
    addEventListener() {},
    dispatchEvent() {},
    setTimeout,
    clearTimeout,
  };
  window.self = window;
  window.top = window;
  window.EditorCoreLocalization = {};

  const context = vm.createContext({
    window,
    document,
    localStorage,
    navigator: window.navigator,
    console,
    setTimeout,
    clearTimeout,
    CustomEvent: class {
      constructor(type, options = {}) { this.type = type; this.detail = options.detail; }
    },
  });
  // Порядок как в браузере: реестр → языки из манифеста → движок.
  vm.runInContext(language('index.js'), context);
  const registry = window.EditorCoreLocalization;
  for (const file of registry.files) vm.runInContext(language(file), context);
  vm.runInContext(source('i18n.js'), context);
  return { window, document, root, makeText, makeElement, I18n: window.EditorCore.I18n };
}

test('русский перевод большой страницы и возврат на английский', () => {
  const { root, makeElement, makeText, I18n, document } = harness();
  assert.equal(I18n.lang, 'en', 'по умолчанию интерфейс английский');

  // 5000 узлов — как в большом редакторе: раньше список исходников обрезался
  // и первые записи (заголовок вкладки, шапки окон) оставались непереведёнными.
  const nodes = [];
  for (let i = 0; i < 5000; i += 1) {
    const node = makeText('The editor list is empty.');
    nodes.push(node);
    root.appendChild(node);
  }
  const titled = makeElement('BUTTON');
  titled.setAttribute('title', 'The editor list is empty.');
  root.appendChild(titled);

  document.documentElement.appendChild(root);
  I18n.set('ru', { force: true });
  assert.equal(nodes[0].nodeValue, 'Список редакторов пуст.', 'первый узел переведён');
  assert.equal(nodes[4999].nodeValue, 'Список редакторов пуст.', 'последний узел тоже переведён');
  assert.equal(titled.getAttribute('title'), 'Список редакторов пуст.');
  assert.equal(document.title, 'Сценарии • Editor Studio');

  I18n.set('en');
  assert.equal(nodes[0].nodeValue, 'The editor list is empty.', 'текст вернулся к источнику');
  assert.equal(nodes[2500].nodeValue, 'The editor list is empty.');
  assert.equal(nodes[4999].nodeValue, 'The editor list is empty.');
  assert.equal(titled.getAttribute('title'), 'The editor list is empty.');
  assert.equal(document.title, 'Scenarios • Editor Studio');
});

test('повторная отрисовка при переводе снова переводится, а не теряется', () => {
  const { root, makeElement, makeText, I18n, document } = harness();
  const node = makeText('The editor list is empty.');
  root.appendChild(node);
  document.documentElement.appendChild(root);
  // Узел в документе: мутационный наблюдатель тоже участвует в переводе.
  const host = makeElement('DIV');
  host.appendChild(node);
  root.appendChild(host);

  I18n.set('ru', { force: true });
  assert.equal(node.nodeValue, 'Список редакторов пуст.');

  // Страница перерисовала подпись ещё раз, пока интерфейс был русским.
  node.nodeValue = 'The editor list is empty.';
  I18n.apply(root);
  assert.equal(node.nodeValue, 'Список редакторов пуст.');

  I18n.set('en');
  assert.equal(node.nodeValue, 'The editor list is empty.');
});

test('удалённые узлы не мешают вернуть английский остальным', () => {
  const { root, makeText, I18n, document } = harness();
  const alive = makeText('The editor list is empty.');
  const removed = makeText('The editor list is empty.');
  root.appendChild(alive);
  root.appendChild(removed);
  document.documentElement.appendChild(root);

  I18n.set('ru', { force: true });
  removed.isConnected = false;
  root.childNodes = root.childNodes.filter((node) => node !== removed);

  I18n.set('en');
  assert.equal(alive.nodeValue, 'The editor list is empty.');
});

test('составная подпись переводится по частям и возвращается целиком', () => {
  const { root, makeText, I18n, document } = harness();
  const node = makeText('World #3 · buy');
  root.appendChild(node);
  document.documentElement.appendChild(root);

  I18n.set('ru', { force: true });
  assert.equal(node.nodeValue, 'Мир #3 · покупка', 'части склеенной строки переводятся');

  I18n.set('en');
  assert.equal(node.nodeValue, 'World #3 · buy', 'вернулась исходная составная строка');
});

test('точный шаблон важнее общего: подстановка не съедает соседнюю часть', () => {
  const { root, makeText, I18n, document } = harness();
  const node = makeText('ID 5 · World: Лес · X 0, Y 0');
  root.appendChild(node);
  document.documentElement.appendChild(root);

  I18n.set('ru', { force: true });
  assert.equal(node.nodeValue, 'ID 5 · Мир: Лес · X 0, Y 0',
    'переведена вся строка, а не только её начало');

  I18n.set('en');
  assert.equal(node.nodeValue, 'ID 5 · World: Лес · X 0, Y 0');
});

test('подставленное значение переводится внутри фразы', () => {
  const { I18n } = harness();
  I18n.set('ru', { force: true });
  // Имя профиля собрано кодом страницы по-английски, а фраза уже русская.
  assert.equal(
    I18n.t('New profile “{0}”', ['Connection 2']),
    'Новый профиль «Подключение 2»',
    'в подставленном значении тоже переводится интерфейсный текст',
  );
});

test('числительные: русские формы выбираются по числу', () => {
  const { I18n } = harness();
  I18n.set('ru', { force: true });
  assert.equal(I18n.plural(1, 'line', 'lines'), 'строка');
  assert.equal(I18n.plural(2, 'line', 'lines'), 'строки');
  assert.equal(I18n.plural(5, 'line', 'lines'), 'строк');
  assert.equal(I18n.plural(21, 'line', 'lines'), 'строка');
  I18n.set('en');
  assert.equal(I18n.plural(1, 'line', 'lines'), 'line');
});
