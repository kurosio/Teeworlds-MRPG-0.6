# DDNet Web Map Preview (vendored)

В `web-tools/editor-studio/vendor/ddnet-map/` лежит урезанная копия рендера карт
DDNet. Она нужна для выбора координат «прямо на карте» в редакторах
(`editor-core/point-map.js`).

Источник: <https://github.com/Djahmo/DDNet-Web-Map-Preview> (ветка `master`),
каталог `core/`. Оригинальный рендер — `twwebgl.js` из клиента DDNet/Teeworlds
(те же структуры карт `.map`: datafile v4, группы, слои, тайлы, квады).

## Что взято

| Файл | Назначение | Лицензия |
| --- | --- | --- |
| `twwebgl.js` | разбор и WebGL-рендер карты, камера, зум | DDNet/Teeworlds, zlib |
| `twdatafile.js` | контейнер datafile v4 | DDNet/Teeworlds, zlib |
| `dataviewext.js` | little-endian чтение из `DataView` | DDNet/Teeworlds, zlib |
| `zlib.min.js` | распаковка сжатых данных карты | MIT (imaya/zlib.js) |
| `gl-matrix.min.js` | матрицы камеры | MIT |
| `jquery-4.0.0-slim.min.js` | события мыши/тача в рендере | MIT |
| `jquery.mousewheel.js` | колесо мыши | MIT |

`crc32.min.js` **не** вендорится: он нужен был только для отладочной печати
контрольной суммы (Apache-2.0). В `twdatafile.js` проверка обёрнута в `typeof`.

## Что изменено

- `twdatafile.js` — убраны отладочные `console.log`, расчёт CRC32 выполняется
  только если библиотека `CRC32` подключена. Изменения помечены `PATCH (Editor Studio)`.
- `picker.html`, `picker.css`, `picker.js` — наши собственные: полноэкранная
  карта с точкой, панорамой, зумом колесом и кнопками «Применить / Отмена».
  Оригинальный `index.html`/`bootstrap.js`/`config.js` не используются — вместо
  них редактор сам задаёт `window.twConfig` (в том числе адрес картинок
  `mapres`, чтобы карта рисовалась без интернета).

## Как это работает в редакторе

1. Страница редактора открывает `vendor/ddnet-map/picker.html` в модальном окне.
2. В адресе передаются: `map` (адрес `.map` через `api/map-file.php`),
   `mapres` (адрес текстур через `api/map-asset.php`), `x`, `y`, `world`, `lang`.
3. Средство просмотра грузит `.map` тем же XHR, что и оригинальная библиотека,
   ставит камеру на точку и рисует перекрестие.
4. Клик по карте (или правка полей X/Y) меняет точку; «Применить» отправляет
   координаты в родительское окно через `postMessage`.

Карты берутся из репозитория игры (`maps/`), текстуры — из `data/mapres/`
(см. `api/map-file.php` и `api/map-asset.php`, там же настраиваемые пути).
