const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

// StorageData складских записей больше не редактируется «сырым» JSON:
// при включённом флаге storage видны отдельные поля — количество товаров
// (BigInt-строка) и точка закрепления всплывающего текста. Тесты держат
// контракт сериализации, который ждёт сервер (CWarehouse::InitializeStorage).

const ROOT = path.join(__dirname, '..');
const read = (name) => fs.readFileSync(path.join(ROOT, name), 'utf8');
const html = read('warehouse-editor.html');
// vm-реалм создаёт объекты с другим прототипом — сравниваем через JSON.
const plain = (value) => JSON.parse(JSON.stringify(value));

const loadHelpers = () => {
  const start = html.indexOf('const parseStorageData =');
  const end = html.indexOf('const emptyModel =');
  assert.ok(start > 0 && end > start, 'storage helpers must exist in warehouse-editor.html');
  const context = { console: { warn: () => {} } };
  vm.createContext(context);
  vm.runInContext(
    `${html.slice(start, end)}
     this.parseStorageData = parseStorageData;
     this.buildStorageData = buildStorageData;`,
    context,
  );
  return context;
};

test('parseStorageData reads the server JSON shape and keeps value as text', () => {
  const { parseStorageData } = loadHelpers();
  // Реальная строка из дампа БД (tw_warehouses.StorageData).
  const parsed = parseStorageData('{"position":{"x":23053.0,"y":2970.0},"value":"100051"}');
  assert.deepEqual(plain(parsed), { StorageValue: '100051', StoragePosX: 23053, StoragePosY: 2970 });
  assert.equal(typeof parsed.StorageValue, 'string');

  // Значения больше 2^53 не теряют точность.
  const huge = parseStorageData('{"value":"9007199254740993123","position":{"x":16.5,"y":-8}}');
  assert.equal(huge.StorageValue, '9007199254740993123');
  assert.equal(huge.StoragePosX, 16.5);
  assert.equal(huge.StoragePosY, -8);

  // Числовой value (совместимость), пустые и битые данные не валят форму.
  assert.equal(parseStorageData('{"value":42}').StorageValue, '42');
  assert.deepEqual(plain(parseStorageData('')), { StorageValue: '0', StoragePosX: 0, StoragePosY: 0 });
  assert.deepEqual(plain(parseStorageData(null)), { StorageValue: '0', StoragePosX: 0, StoragePosY: 0 });
  assert.deepEqual(plain(parseStorageData('not json')), { StorageValue: '0', StoragePosX: 0, StoragePosY: 0 });
});

test('buildStorageData writes canonical JSON only with the storage flag', () => {
  const { buildStorageData } = loadHelpers();
  const model = { TypeArr: ['buying', 'storage'], StorageValue: '100051', StoragePosX: 23053, StoragePosY: 2970 };
  assert.deepEqual(JSON.parse(buildStorageData(model)), {
    value: '100051',
    position: { x: 23053, y: 2970 },
  });

  // Без флага поле не отправляется: сервер читает StorageData только с WF_STORAGE.
  assert.equal(buildStorageData({ TypeArr: ['buying'], StorageValue: '999' }), '');

  // Любые нецифровые символы вычищаются, пустое количество превращается в "0".
  assert.deepEqual(
    JSON.parse(buildStorageData({ TypeArr: ['storage'], StorageValue: '12abc34', StoragePosX: 0, StoragePosY: 0 })),
    { value: '1234', position: { x: 0, y: 0 } },
  );
  assert.deepEqual(
    JSON.parse(buildStorageData({ TypeArr: ['storage'], StorageValue: '', StoragePosX: 5, StoragePosY: 7 })),
    { value: '0', position: { x: 5, y: 7 } },
  );

  // Круговой цикл стабилен.
  const { parseStorageData } = loadHelpers();
  assert.deepEqual(plain(parseStorageData(buildStorageData(model))), plain(parseStorageData('{"position":{"x":23053.0,"y":2970.0},"value":"100051"}')));
});

test('storage section fields exist and are gated by the storage mode flag', () => {
  const frContext = { window: { EditorCore: {
    utils: {
      parsePath: (p) => String(p ?? '').split('.'),
      getAtPath: (data, p) => String(p ?? '').split('.').reduce((c, k) => (c == null ? undefined : c[k]), data),
      setAtPath: () => {},
      cloneData: (v) => JSON.parse(JSON.stringify(v)),
      escapeAttr: (v) => String(v).replace(/"/g, '&quot;'),
      escapeHtml: (v) => String(v),
    },
    DB: { resolveSource: () => '' },
    PositionPicker: { render: (o) => `<div data-position-picker="1" data-position-label="${o.label}">${o.xInputHtml}${o.yInputHtml}</div>` },
  } } };
  vm.createContext(frContext);
  vm.runInContext(read('editor-core/field-renderer.js'), frContext);
  const { renderFields } = frContext.window.EditorCore.FieldRenderer;

  const mfEnd = html.indexOf('const mountMainForm =');
  vm.runInContext(
    `${html.slice(html.indexOf('const isStorageMode'), mfEnd)}
     this.mainFields = mainFields;`,
    frContext,
  );
  const mainFields = frContext.mainFields;
  const options = { classes: {
    input: 'editor-input form-input', label: 'editor-label', nestedLabel: 'editor-nested-label',
    fieldWrapper: 'editor-field', listWrapper: 'editor-list', listAdd: 'editor-btn', listRemove: 'editor-icon-btn',
  } };

  const withStorage = renderFields({
    fields: mainFields,
    data: { TypeArr: ['buying', 'storage'], WorldID: 1, StorageValue: '100051', StoragePosX: 23053, StoragePosY: 2970 },
    options,
  });
  assert.match(withStorage, /Storage settings/);
  assert.match(withStorage, /Storage fill \(goods amount\)/);
  assert.match(withStorage, /Floating text position \(X, Y\)/);
  assert.ok(withStorage.includes('data-path="StoragePosX"') && withStorage.includes('data-path="StoragePosY"'));
  assert.match(withStorage, /data-validate=/);
  assert.match(withStorage, /fa-box-archive/);

  const withoutStorage = renderFields({ fields: mainFields, data: { TypeArr: ['buying'] }, options });
  assert.doesNotMatch(withoutStorage, /Storage settings/);
  assert.doesNotMatch(withoutStorage, /Storage fill \(goods amount\)/);
  assert.doesNotMatch(withoutStorage, /Floating text position \(X, Y\)/);
  // Обычные поля формы остаются на месте.
  assert.match(withoutStorage, /Operation modes/);
  assert.match(withoutStorage, /Payment currency/);
});

test('the form rebuilds when the operation-mode tags change', () => {
  // showWhen вычисляется только при рендере, поэтому изменение TypeArr
  // обязано вызывать перерисовку формы — иначе секция не появится/не скроется.
  assert.match(html, /if \(path === 'TypeArr'\) runtimeMain\?\.render\?\.\(\);/);
});

test('raw StorageData textarea is gone and Russian dictionary covers the new strings', () => {
  assert.doesNotMatch(html, /label: 'Additional data \(StorageData JSON\)'/);
  const ru = read('localization/lang_ru.js');
  for (const key of [
    'Storage settings',
    'Storage fill (goods amount)',
    'Floating text position (X, Y)',
    'Only digits are allowed',
    'Current amount of goods in the storage. Stored as text to keep precision for values larger than 2^53.',
    'World point where the in-game text with the goods amount is pinned. (0, 0) hides the text.',
  ]) {
    assert.ok(ru.includes(`"${key}":`), `missing RU translation for "${key}"`);
  }
  assert.doesNotMatch(ru, /Additional data \(StorageData JSON\)/);
});
