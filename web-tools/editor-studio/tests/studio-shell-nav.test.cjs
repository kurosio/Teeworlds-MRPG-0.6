const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

// Меню студии — рельс: по умолчанию видны только иконки, наведение/фокус
// раскрывают панель, уход курсора снова её сворачивает, а кнопка «закрепить»
// оставляет панель открытой в раскладке. Логика живёт inline-скриптом в
// index.html, поэтому берём её оттуда по маркерам разделов и гоняем на
// минимальном фейковом DOM — так регрессию видно без браузера.

const root = path.join(__dirname, '..');
const shell = fs.readFileSync(path.join(root, 'index.html'), 'utf8');

const START = '// ── Боковая панель: рельс из иконок';
const END = '// ── Переключатель темы оформления';

function sidebarSource() {
  const from = shell.indexOf(START);
  const to = shell.indexOf(END, from);
  assert.ok(from > 0, 'в index.html нет раздела «Боковая панель»');
  assert.ok(to > from, 'не найден конец раздела боковой панели');
  return shell.slice(from, to);
}

function makeClassList() {
  const classes = new Set();
  return {
    add: (...names) => names.forEach((name) => classes.add(name)),
    remove: (...names) => names.forEach((name) => classes.delete(name)),
    contains: (name) => classes.has(name),
    toggle: (name, force) => {
      const on = force === undefined ? !classes.has(name) : !!force;
      if (on) classes.add(name); else classes.delete(name);
      return on;
    },
  };
}

function makeElement() {
  const listeners = new Map();
  const el = {
    classList: makeClassList(),
    attributes: {},
    title: '',
    setAttribute(name, value) { this.attributes[name] = String(value); },
    getAttribute(name) { return this.attributes[name] ?? null; },
    addEventListener(type, fn) { listeners.set(type, fn); },
    dispatch(type, event = {}) { listeners.get(type)?.(event); },
    contains(other) { return other === el; },
  };
  return el;
}

function harness({ hover = true, stored = '' } = {}) {
  const sidebar = makeElement();
  const toggle = makeElement();
  const pin = makeElement();
  const bodyClassList = makeClassList();
  const store = new Map();
  if (stored) store.set('studio.sidebarPinned', stored);

  const window = {
    matchMedia: (query) => ({
      matches: /hover/.test(String(query)) ? hover : false,
      media: String(query),
    }),
  };
  const document = {
    body: { classList: bodyClassList },
    getElementById: (id) => ({ studioSidebar: sidebar, toggleSidebarBtn: toggle, pinSidebarBtn: pin }[id] || null),
  };
  const context = vm.createContext({
    window,
    document,
    console,
    storageGet: (key) => store.get(key) || '',
    storageSet: (key, value) => store.set(key, String(value)),
    toggleSidebarBtn: toggle,
    pinSidebarBtn: pin,
  });
  vm.runInContext(sidebarSource(), context);

  return {
    context,
    sidebar,
    toggle,
    pin,
    body: bodyClassList,
    store,
    isRail: () => sidebar.classList.contains('is-rail'),
    isOpen: () => sidebar.classList.contains('is-open'),
    isPinned: () => bodyClassList.contains('studio-sidebar-pinned'),
    pointer: (type) => sidebar.dispatch(type),
  };
}

test('меню по умолчанию свёрнуто до рельса и раскрывается при наведении', () => {
  const ui = harness();

  assert.equal(ui.isRail(), true, 'по умолчанию меню должно быть рельсом');
  assert.equal(ui.isOpen(), false);
  assert.equal(ui.isPinned(), false);

  ui.pointer('pointerenter');
  assert.equal(ui.isOpen(), true, 'наведение должно раскрывать панель');
  assert.equal(ui.isRail(), false, 'подписи возвращаются вместе с шириной панели');

  ui.pointer('pointerleave');
  assert.equal(ui.isOpen(), false, 'неактивная область снова сжимается');
  assert.equal(ui.isRail(), true);
});

test('закрепление панели не даёт наведению её свернуть', () => {
  const ui = harness();

  ui.pin.dispatch('click');
  assert.equal(ui.isPinned(), true);
  assert.equal(ui.isRail(), false, 'закреплённая панель не рельс');
  assert.equal(ui.store.get('studio.sidebarPinned'), '1');
  assert.equal(ui.pin.getAttribute('aria-pressed'), 'true');
  assert.match(ui.pin.getAttribute('aria-label'), /^Unpin the menu/);
  assert.equal(ui.toggle.getAttribute('aria-pressed'), 'true', 'кнопка в шапке показывает то же состояние');

  ui.pointer('pointerenter');
  ui.pointer('pointerleave');
  assert.equal(ui.isOpen(), false, 'закреплённую панель наведение не меняет');
  assert.equal(ui.isRail(), false);

  ui.toggle.dispatch('click');
  assert.equal(ui.isPinned(), false);
  assert.equal(ui.isRail(), true, 'открепление возвращает рельс');
  assert.equal(ui.store.get('studio.sidebarPinned'), '0');
  assert.match(ui.pin.getAttribute('aria-label'), /^Pin the menu/);
});

test('на тач-устройствах без наведения панель закреплена по умолчанию', () => {
  const ui = harness({ hover: false });
  assert.equal(ui.isPinned(), true);
  assert.equal(ui.isRail(), false);
  // Раскрытие по наведению не подключается вовсе.
  ui.pointer('pointerenter');
  assert.equal(ui.isOpen(), false);
});

test('сохранённый выбор пользователя важнее значения по умолчанию', () => {
  assert.equal(harness({ stored: '0' }).isRail(), true);
  assert.equal(harness({ stored: '1' }).isPinned(), true);
});

test('фокус с клавиатуры раскрывает рельс и сворачивает его при уходе', () => {
  const ui = harness();
  ui.sidebar.dispatch('focusin');
  assert.equal(ui.isOpen(), true);

  ui.sidebar.dispatch('focusout', { relatedTarget: {} });
  assert.equal(ui.isOpen(), false);
});
