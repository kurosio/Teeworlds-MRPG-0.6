# AI API — руководство для автономных агентов (Editor Studio)

Единый программный интерфейс `api/ai.php` позволяет AI-агенту (локально или удалённо через гостевую учётную запись) **автономно изучать структуру и данные игры**, находить логические ошибки и битые ссылки, предлагать улучшения, создавать новые игровые сущности и сценарии, проверять изменения и отправлять их пакетами на одобрение администратору — **без чтения исходного кода проекта и без возможности напрямую изменить боевую БД или файлы сервера**.

---

## 1. Модель безопасности и изоляция гостя

1. **Никакой прямой записи в игровую БД или ФС:** гостевой сессии запрещены все мутирующие методы `api/db-crud.php`, `api/db.php`, `api/api.php`, а также админ-действия `approve` / `reject` / `delete` в `api/change-requests.php` (возвращается `403`, `code: "guest_forbidden"`).
2. **Персональные права по вкладкам (`pages`):** агент видит и может предлагать изменения **только** для ресурсов, включённых его гостевой учётной записи администратором. Доступ к административным разделам (например, `vouchers`) или невыданным вкладкам жёстко блокируется (`403`, `code: "resource_forbidden"`).
3. **Машиночитаемый контракт без прямого доступа к `.json`/`.md`:** `apache-config.conf` и `router.php` запрещают прямой HTTP-доступ к `.json` и `.md` файлам и каталогу `scenarios/*.json`. **Прямой `GET /openapi.json` работает только там, где активен rewrite на PHP (Docker-образ с `apache-config.conf`, `router.php` для `php -S`): на хостингах без rewrite он тоже закрыт правилом на `*.json` и отвечает `403`. Поэтому каноническая точка спецификации — `GET api/ai.php?action=openapi` (работает везде), а `/openapi.json` — алиас для re-written установок.** Сценарии читаются исключительно через `api/ai.php` (`action=list|get|analyze`) с проверкой имени файла и запретом symlink/path traversal.
4. **Двойная защита POST-запросов:** каждый `POST` требует cookie PHP-сессии (`PHPSESSID`), совпадающий заголовок `Origin`, `Content-Type: application/json` и актуальный `X-Editor-CSRF`.
5. **Оптимистичная блокировка и идемпотентность:** изменение сценариев защищено `baseHash` (SHA-256), доработка ожидающей заявки — полем `version`, повторная отправка по сети — ключом `idempotency_key`.

---

## 2. Машиночитаемое описание и `manifest`

### OpenAPI 3.1.0, Single-Link `/ai` Onboarding и `manifest`

1. **Единая точка подключения (`/ai`) и `/llms.txt`**:
   - `GET /ai` (или `/ai/`) — публичная страница подключения для браузера, LLM и веб-агентов (без секретов и локальных путей сервера), содержащая 4 готовые вкладки конфигурации (MCP HTTP, MCP Stdio, Chat Prompt, cURL), опциональную локальную подстановку токена в браузере (только в памяти) и интерактивный тест токена.
   - `GET /ai?format=json` (или `Accept: application/json`) — машиночитаемый JSON онбординга.
   - `GET /llms.txt` (и кросс-хостинговый `GET api/ai.php?action=llms_txt`) — публичный компактный Markdown-справочник для LLM и автономных агентов (протокол, эндпоинты, MCP-инструменты, символические ссылки `ref`/`$alias`, операторы фильтрации и рецепты).
   - `GET /.well-known/ai-plugin.json` (алиас `api/ai.php?action=discovery`) — стандартный дескриптор автообнаружения.
2. **OpenAPI 3.1.0**:
   - `GET api/ai.php?action=openapi` — публичная спецификация `3.1.0` (версия API `2.1.0`), работает на любом хостинге (**рекомендуемый основной URL**).
   - `GET /openapi.json` — тот же документ, но только при активном rewrite (Docker / `router.php`); без него — `403`.
3. **MCP Server v2.1.0 (JSON-RPC 2.0)**:
   - `POST /mcp` (или `api/mcp.php`) — согласование версий протокола (`2024-11-05`, `2025-03-26`, `2025-06-18`), **19 типизированных инструментов с `annotations`** (`readOnlyHint` / `idempotentHint` / `destructiveHint`): `get_manifest`, `get_schema`, `search_editor`, `get_context`, `list_records`, `get_record`, `get_references`, `analyze_scenario`, `analyze_map`, `get_map_screenshot`, `find_similar`, `validate_operations`, `preview_operations`, `apply_patch`, `propose_changes`, `copy_record`, `patch_json_record`, `get_history`, `manage_change_request`.
   - Динамические `resources/list`, `resources/templates/list` и `resources/read`: `mrpg://manifest`, `mrpg://openapi`, `mrpg://llms.txt`, `mrpg://cookbook`, `mrpg://schema/<resource>`, `mrpg://context/<resource>/<id>`.
   - 5 специализированных `prompts/*`: `create_mrpg_item`, `create_mrpg_quest_scene`, `safe_json_patch_edit`, `inspect_map_and_place_object`, `review_and_revise_request`.
   - Инструмент `get_map_screenshot` возвращает нативный мультимодальный MCP-блок `type: "image"` (`image/png`) + компактные метаданные координат и построек.
   - `node tools/mcp-stdio.mjs --url https://... --token esai_...` — stdio-мост для Claude Desktop, Codex CLI, Cursor, Windsurf и локальных клиентов (по умолчанию ходит на `/api/mcp.php`, поддерживает `--check` / `--health` для быстрой диагностики подключения, `--timeout <сек>` / `EDITOR_STUDIO_TIMEOUT` с таймаутом 30с по умолчанию и структурированными JSON-RPC ошибками `-32002`).

### Живой `manifest` (v2.1)
```http
GET /editor-studio/api/ai.php?action=manifest
```
После входа (Bearer-токеном или гостевой сессией) возвращает актуальное состояние:
- `capabilities` и `discovery_urls`: поддерживаемые возможности (`model_agnostic`, `auto_resource_discovery`, `mcp_jsonrpc_2_0`, `mcp_tool_annotations`, `llms_txt`, `global_search`, `entity_context`, `batch_get`, `rich_filter_operators`, `sorting`, `symbolic_refs`, `self_healing_validation`, `change_request_lifecycle`, `bearer_tokens`, `scoped_rbac`, `patch_json_merge`, `copy_records`, `dangerous_confirmation_gate`, `optimistic_concurrency`, `idempotent_propose`, `scenario_analysis`, `scenario_corpus_dependency_check`, `similar_records_search`, `similar_records_create_warning`, `verify_before_propose`, `map_intelligence`, `map_screenshot_render`). Поле `recommended_workflow` внутри manifest фиксирует безопасный цикл `search_editor → get_context → analyze → similar → apply_patch → validate → preview → propose → manage_change_request`.
- `auth`: роль (`guest` / `admin`), способ входа (`bearer_token` / `session`), активные `token_scopes` (`read`, `create`, `update`, `delete`), `granted_pages`, `direct_db_writes_allowed: false`.
- `limits`: персональные лимиты аккаунта (`max_operations`, `max_pending`, `cooldown`, длина `nickname`/`message`, `max_scenario_bytes`).
- `pagination`: `default_limit: 50`, `max_limit: 500`, `default_max_bytes: 524288`, `max_response_bytes: 2097152`.
- `error_format`: единая схема ответа об ошибке.
- `operation_types`: `create`, `update`, `copy`, `delete` (принимаются оба синонима ключа: `type` и `action`).
- `temp_id_convention`: диапазон временных ID `2000000000..2000999999`.
- `resources_summary` и `resources`: **полные схемы + примеры (`examples`) доступных ресурсов**, включая автоматически обнаруженные новые таблицы из `db-resources.php` (`auto_discovered: true`). Если нужен только компактный заголовок без схем, передайте `?include_schemas=0`.
- `scenario_schema` и `map_schema`: допустимые `components`, события `on_*`, правила задержек/текста/координат, идентификаторы тайлов DDNet v4 (`tile * 32 + 16`).

---

## 3. Аутентификация: Scoped AI-токены и Гостевая сессия

### Вариант A (рекомендуется для AI/MCP): Scoped Bearer AI Token
Администратор выпускает токен в разделе **Guest users** (`guests-editor.html` -> `New token`), привязывая его к гостевому аккаунту, пресету прав (`Read only` / `Content author` / `Full access` или произвольному набору `read`, `create`, `update`, `delete`) и, при желании, сужая список вкладок (`pages`) внутри уже выданных гостю. Сразу после выпуска модал показывает готовые к копированию конфиги (**MCP HTTP**, **MCP Stdio**, **Chat prompt**) с подставленным URL и токеном.
```http
GET /editor-studio/api/ai.php?action=manifest
Authorization: Bearer esai_<ваш_токен>
```
Токены работают как в REST API (`api/ai.php`, `api/change-requests.php`, `api/auth.php?action=status`), так и в MCP (`/mcp`, `tools/mcp-stdio.mjs`), не требуют cookie и `X-Editor-CSRF`, хранятся в SQLite в виде `sha256`-хеша и могут быть мгновенно отозваны. Проверить токен и подключение можно одной командой:
```bash
node tools/mcp-stdio.mjs --url https://your-host/editor-studio --token esai_<ваш_токен> --check
```

### Вариант B (браузерная сессия + CSRF)
```http
GET /editor-studio/api/auth.php?action=status
```
Затем:
```http
POST /editor-studio/api/auth.php?action=guest_login
Content-Type: application/json
Origin: http://127.0.0.1:8080
X-Editor-CSRF: <csrf>

{
  "login": "ai_assistant",
  "password": "your-guest-password"
}
```
В ответе вернётся `role: "guest"` и **обновлённый** `csrf` — для всех последующих cookie-`POST` запросов нужно использовать новый `csrf`.

---

## 4. Единый формат ответов и ошибок

Успешный ответ всегда содержит `"ok": true`.  
Любая ошибка (включая `400`, `401`, `403`, `404`, `405`, `409`, `413`, `415`, `429`) возвращается в едином JSON-формате:
```json
{
  "ok": false,
  "code": "quest_flags_conflict",
  "error": "В операции #0: несовместимые взаимоисключающие флаги квеста: Tutorial, Type main.",
  "field": "Flags",
  "errors": [
    {
      "index": 0,
      "code": "quest_flags_conflict",
      "field": "Flags",
      "message": "В операции #0: несовместимые взаимоисключающие флаги квеста: Tutorial, Type main."
    }
  ]
}
```

| HTTP | Типовые `code` | Значение |
|---|---|---|
| `400` | `missing_action`, `missing_required_field`, `invalid_json`, `invalid_id`, `invalid_filter`, `unknown_field`, `unknown_filter_field`, `validation_failed`, `quest_flags_conflict`, `too_many_operations`, `invalid_filename` | Ошибка параметров запроса или валидации данных |
| `401` | `auth_required`, `session_expired` | Сессия не авторизована |
| `403` | `resource_forbidden`, `guest_forbidden`, `csrf_token`, `missing_origin`, `cross_origin_forbidden` | Доступ к ресурсу/действию запрещён или ошибка безопасности POST |
| `404` | `unknown_resource`, `record_not_found`, `scenario_not_found`, `request_not_found` | Ресурс, запись, сценарий или запрос на изменение не найдены |
| `405` | `method_not_allowed` | GET-метод вызван через POST или наоборот |
| `409` | `version_conflict`, `base_hash_mismatch`, `request_not_pending`, `scenario_exists` | Конфликт версий, устаревший хеш сценария или заявка уже рассмотрена |
| `413` | `payload_too_large` | Тело запроса превышает 2 МБ (или сценарий > 1 МБ) |
| `415` | `unsupported_media_type` | `POST` отправлен без `Content-Type: application/json` |
| `429` | `rate_limited`, `too_many_pending`, `queue_full` | Превышен лимит частоты (`Retry-After` в заголовке) или лимит активных заявок |

---

## 5. Пагинация, фильтрация и ограничение размера ответа

Все списочные методы (`list` для таблиц БД, `scenarios`, `maps`, справочников и `references?source=...`) поддерживают единый контракт:
- `limit` (1..500, по умолчанию `50`; для справочников до `1000`);
- `offset` (`>= 0`, по умолчанию `0`);
- `max_bytes` (4096..2097152, по умолчанию `524288` = 512 КБ): мягкий бюджет сериализованного массива `rows`. Если очередная запись выведет ответ за бюджет, выборка останавливается (гарантируется возврат минимум 1 строки на страницу, чтобы `offset` всегда двигался вперёд);
- Поля ответа: `total`, `limit`, `offset`, `has_more`, `next_offset` (`null`, если конец списка), `truncated` (`true`, если сработало ограничение `max_bytes`), `max_bytes`, `bytes`.
- **Проекция полей (`fields`):** в `list` и `get` для таблиц БД можно перечислить только нужные колонки через запятую (`?fields=ID,Name,Type`). Первичный ключ добавляется автоматически; неизвестная колонка немедленно отклоняется (`400 unknown_field`).
- **Компактное чтение сценариев (`summary_only=1`):** в `get&resource=scenarios&id=...&summary_only=1` возвращается мета-информация, `sha256` и структурная сводка `summary` без сырого тела `content`.
- **Фильтрация и сортировка (`filter`, `sort`):**
  - `search=...` (поиск подстроки по индексным/поисковым колонкам или метаданным сценария);
  - `filter={"Col":"Val","Status":["A","B"],"Deleted":null,"Price":{"gte":100,"lt":1000},"Name":{"contains":"Sword"},"ScenarioData":{"not_null":true}}` — поддерживаются точные совпадения, `IS NULL`, `IN (...)` и объектные операторы `eq`, `ne`, `gt`, `gte`, `lt`, `lte`, `contains`, `prefix`, `like`, `in`, `is_null`, `not_null` (для `scenarios` поддерживаются `scenario_mode` и `valid_json`); при опечатке в имени колонки или оператора возвращается подсказка `did_you_mean`;
  - `sort=-ID` / `sort=Name` — сортировка по любой разрешённой колонке (`-` перед именем означает `DESC`, по умолчанию `ASC`);
  - либо плоские query-параметры `filter_key=...&filter_value=...` / `filter_values=a,b`.

### Пример запроса с фильтрацией, проекцией и бюджетом байт
```http
GET /editor-studio/api/ai.php?action=list&resource=items&filter={"Type":"Quest"}&fields=ID,Name,Description&limit=20&offset=0&max_bytes=16384
```
Ответ:
```json
{
  "ok": true,
  "resource": "items",
  "rows": [
    { "ID": 10, "Name": "Old Parchment", "Description": "Quest clue" }
  ],
  "total": 3,
  "limit": 20,
  "offset": 0,
  "has_more": true,
  "next_offset": 1,
  "truncated": true,
  "max_bytes": 16384,
  "bytes": 15980
}
```

---

## 6. Справочник методов `api/ai.php` и `api/change-requests.php`

### `GET api/ai.php?action=resources[&resource=items]`
Возвращает детальную схему одного разрешённого ресурса или карты всех разрешённых ресурсов.

### `GET api/ai.php?action=search&q=<запрос>[&resources=items,quests,scenarios][&per_resource_limit=5]`
Кросс-ресурсный поиск по всем разрешённым гостю таблицам БД, сценариям и картам за один вызов (MCP: `search_editor`). Убирает необходимость угадывать таблицу перед чтением; если передать числовой запрос (например, `q=42`), дополнительно ищет точные совпадения по первичным ключам. Возвращает ранжированный список `top_matches[]`, сгруппированные результаты `groups` по ресурсам и `searched_resources`.

### `GET api/ai.php?action=context&resource=<res>&id=<id>[&max_relations=25]`
360°-снимок сущности и её окружения за один запрос (MCP: `get_context`, resource template `mrpg://context/{resource}/{id}`). Возвращает саму запись (`record`), разрешённые исходящие ссылки с именами (`outbound`), входящие связанные строки из разрешённых таблиц (например, для квеста — квестовые шаги `quest_bots` с именами ботов, награды/предметы, ссылки на мир и упоминания в `scenarios`; для мира — дома, гильддома, склады, данжи, квесты, NPC, мобы и ссылка на `map`) и предупреждение о неполных ссылках.

### `GET api/ai.php?action=get`
- **БД (одна запись или batch до 50 ID):** `?action=get&resource=quests&id=12&fields=ID,Name,WorldID,ScenarioData&include_references=1` или пакетно `?action=get&resource=items&ids=10,11,12&fields=ID,Name,Type` (возвращает `rows[]`, `found_ids[]`, `missing_ids[]`).
- **Сценарий:** `?action=get&resource=scenarios&id=dungeon_gate.json&include_references=1` (или `&summary_only=1`)
- **Карта DDNet v4:** `?action=get&resource=maps&world=1&include_entities=1&include_minimap=1&x=18736&y=1584`
  Возвращает полные данные карты (а не только коллизии): размеры в тайлах/пикселях (`width_tiles * 32`), все слои (`game_layer`, `front_layer`, `switch_layer`, `tele_layer`, `speedup_layer`, `visual_layers` с категориями `architecture`/`interior`/`terrain`/`nature`/`water`/`props`/`sky_bg`, `quad_layers`, список тайлсетов `images`, `groups` с parallax, `map_info`), профиль рельефа `terrain` (композиция и высоты земли), распознанные постройки `structures` (`castle_or_guild_hall`, `house`, `shop_or_service`, `dungeon_or_ruins`, `cave_or_underground`, `building`) с bbox и числом этажей, именованные `#zone`/`#action_zone`/`#text`, коррелированные объекты БД `world_objects` (дома, гильддома, склады, эфиры, NPC, мобы, порталы, данжи) и расширенный `point_check` (`terrain_context` с крышей/полом/типом окружения и `nearest_standable_point`, перекрывающие визуальные слои `visual_layers_at_point`, `inside_structure`, `nearby_structures`, `nearby_objects`).

### `GET api/ai.php?action=references`
- По конкретной записи или файлу сценария: `?action=references&resource=items&id=10` (исходящие и входящие ссылки только по разрешённым гостю таблицам и сценариям).
- По справочнику: `?action=references&source=attributes&search=Str&limit=50`.
- Без параметров: полный граф зависимостей схемы между доступными ресурсами.

### `GET api/ai.php?action=analyze[&resource=scenarios|maps][&id=file.json | &world=1]`
Глубокий анализ сценариев или карт (read-only, `scope: read`):
- **Сценарии (`resource=scenarios`):**
  - **Один файл** (`&id=dungeon_gate.json`): `summary` (шаги/события), инвентарь компонентов `components` (`type`, `count`, `steps`, `status`: `known` | `legacy` | `unknown`), `unknown_component_types`, типобезопасные исходящие `references` с проверкой существования в БД (`exists`, `label`), агрегированные `dependencies` по таблицам (refs/unique/missing) и флаг `dependency_ok`, входящие ссылки `inbound` (кто ссылается на сценарий, включая `items.Data.use_scenario.name`), а также `reuse` — другие сценарии, использующие те же типы компонентов (для переиспользования логики вместо дублирования). Ответ содержит `baseHash` (SHA-256) для последующего `propose`.
  - **Весь каталог** (без `&id=`): сводка `mode=corpus` — `scanned` / `truncated` (до 200 файлов), невалидные JSON (`invalid` с `parse_error`), гистограмма `components` (в каких скольки сценариях встречается каждый тип), `reference_total` и отчёт `missing_references` (до 100 строк: `filename`, `path`, `target`, `id`) по всем ссылкам сценариев на ботов/предметы/миры/квесты — готовая карта битых зависимостей. Параметр `search` фильтрует файлы теми же полями, что и `list`.
- **Карты (`resource=maps`):** `?action=analyze&resource=maps&world=1[&target=guild_house:1|house:1|zone:1|spawn|overview][&structure_kind=castle_or_guild_hall|house|...][&x=...&y=...&radius=...][&compact=1]` (или `&path=main.map`, MCP-инструмент `analyze_map`).
  Возвращает структурный и семантический разбор карты: классификацию визуальных слоёв и тайлсетов, профиль рельефа (`terrain.elevation_profile`), каталог распознанных построек (`structures.items` с типом `castle_or_guild_hall` / `house` / `shop_or_service` / `dungeon_or_ruins` / `cave_or_underground` / `building`, координатами и привязанными объектами БД), именованные зоны, контекст точки (`point_check` + `nearest_standable_point`) и семантическую ASCII-миникарту выбранной области (`region_analysis.semantic_minimap`).
Для записей БД (боты, предметы, миры) используйте `references`.

### `GET api/ai.php?action=map_screenshot&world=<id>[&target=overview][&mode=hybrid|visual|schematic][&format=json|png]`
Серверный рендер PNG-скриншота карты или конкретной постройки/области (read-only, `scope: read`; MCP-инструмент `get_map_screenshot`):
- **Режимы (`mode`):**
  - `hybrid` (по умолчанию) — реальный визуальный рендер тайлсетов (встроенных RGBA и внешних `data/mapres/*.png`) и квадов карты + полупрозрачные контуры построек/домов/замков, подписи, маркеры спавна/объектов БД и линейка координат;
  - `visual` — чистый игровой визуальный вид без схематичных оверлеев;
  - `schematic` — структурно-коллизионная схема (земля, стены построек, интерьеры, пещеры, вода, смерть, дома, сервисы, спавны, объекты БД).
- **Область кадра (`target` или координаты):** `overview`, `spawn`, `guild_house:<id>`, `house:<id>`, `zone:<num|name>`, `structure:<id>`, `warehouse:<id>`, `aether:<id>`, либо `x`/`y`/`radius` (`radius_tiles`), `x0..y1`, `tx0..ty1`; опционально `highlight_x`/`highlight_y` (красный прицел на точке), `width` (256..1600), `height` (256..1200), `show_grid`, `show_structures`, `show_labels`, `show_objects`, `show_collision`.
- **Формат (`format`):** `json` (возвращает `image_base64`, `data_uri`, `viewport`, `coordinate_mapping` с точной формулой пересчёта пикселей изображения в мировые координаты MRPG и список видимых построек/зон) или `png` (отдаёт бинарный `Content-Type: image/png` + заголовок `X-Map-Viewport`). Через MCP `get_map_screenshot` изображение возвращается нативным мультимодальным блоком `type: "image"` (`image/png`) без дублирования base64 в тексте.

### `GET api/ai.php?action=similar&resource=<res>&name=<Name>[&limit=5]`
Поиск похожих **существующих** записей до создания новой (анти-дубли, read-only). Работает по любой таблице БД с человекочитаемой колонкой (`items`, `quests`, `bots_npc`, `worlds`, …) и по справочникам (`skills`, `attributes`, …). Скоринг — Dice-коэффициент по биграммам нормализованного текста (lowercase, «ё»→«е», без пунктуации) + бонус за вхождение; регистронезависим. Ответ: `candidates: [{id, label, score, score_percent}]` (по убыванию score), `scanned` (сколько строк просмотрено предфильтром), `threshold`. Ресурсы без колонки-названия (например, `quest_bots`) возвращают пустой список.
Дополнительно `validate`/`preview`/`propose` при `create` автоматически добавляют в `warnings` запись `similar_records_found` со списком похожих записей (score ≥ 62%) — агент обязан сверить их перед отправкой заявки.

### `POST api/ai.php?action=apply_patch`
**Verify-before-propose** (read-only, никаких записей в БД/SQLite/файлы; `scope: read`): прогоняет ровно ту же валидацию, что и `propose`, затем возвращает исходную запись/файл и объединённый результат.
Тело: `{ "resource": "items", "id": 142, "patch_json": { "Data": { "bonus": { "strength": 8 } } }, "expected_before": { "Name": "Old Sword" } }`
(для сценария: `{ "resource": "scenarios", "filename": "guard_intro.json", "patch_json": {"scenario_mode": "universal"}, "baseHash": "<sha256>" }`).
Ответ при успехе: `before`, `after`/`record`, `diff` (попольный для БД, `scenario_json_diff` для сценария), `changed_fields`, `noop`, `sha256_before`/`summary_after` (для сценариев), `normalized_operation`, `warnings`. При невалидном патче — те же `errors[]` / `code`, что вернул бы `propose`, со статусом `400|403|409`.
Порядок применения операции идентичен `propose`: сначала читается актуальная запись, затем `patch_json` (RFC 7396) накладывается поверх неё. Используйте этот шаг **между `get` и `propose`**: если `noop` или патч невалиден — исправьте до отправки.

### `POST api/ai.php?action=validate`
Полная проверка массива `operations` (БД-схемы, JSON-поля, enum, длины, уникальность, конфликт флагов квестов, проверка имён и структуры сценариев `steps` / `on_*`, проверка ссылок и `tempId` `2000000000..2000999999`, проверка `baseHash`). **Ничего не изменяет** и **не тратит cooldown**. При `create`-операциях дополнительно автоматически ищет похожие существующие записи и добавляет предупреждение `similar_records_found` (см. `action=similar`).

### `POST api/ai.php?action=preview`
Dry-run анализ: прогоняет всю валидацию + вычисляет постраничный и попольный `diff`, проверяет `before`, затрагиваемые связи и предупреждения (например, удаление используемого объекта или смену уже существующего сценария). Не записывает в БД/SQLite.

### `POST api/ai.php?action=propose`
Создаёт новую заявку в SQLite (`status: "pending"`, `version: 1`) либо **обновляет ожидающую заявку** того же гостя (`request_id` + `version`).

### Сопровождение заявки через `api/ai.php` / MCP `manage_change_request` и `api/change-requests.php`
Агент может вести весь жизненный цикл своих заявок прямо через единый AI API и MCP-инструмент `manage_change_request` (`list` | `get` | `reply` | `withdraw`):
- `GET api/ai.php?action=history` — список своих заявок (`requests[]`) и журнал аудита;
- `GET api/ai.php?action=request&id=cr_...[&include_messages=1]` — детальный статус заявки, её операции и сообщения обсуждения с администратором;
- `POST api/ai.php?action=request_reply` (`{ "id": "cr_...", "text": "..." }`, scope `create` или `update`) — ответить в тред своей заявки;
- `POST api/ai.php?action=request_withdraw` (`{ "id": "cr_..." }`, scope `update` или `delete`) — отозвать ещё не рассмотренную (`pending`) заявку.
Для веб-интерфейса также доступны аналогичные методы `api/change-requests.php` (`mine`, `get`, `messages`, `message`, `mark_read`, `withdraw`).

---

## 7. Сквозной сценарий работы AI-агента (End-to-End)

Ниже показана полная последовательность от первого подключения до отправки исправленного многошагового пакета и его доработки после замечания администратора.

### Шаг 1. Изучить контракт и получить сессию
1. Прочитать контракт без входа: `GET api/ai.php?action=openapi`.
2. Получить начальный `csrf`: `GET api/auth.php?action=status`.
3. Войти как гость: `POST api/auth.php?action=guest_login` (с заголовками `Origin` и `X-Editor-CSRF`), сохранить новый `csrf` из ответа.
4. Загрузить живые права и схемы: `GET api/ai.php?action=manifest`.

### Шаг 2. Изучить сценарии и похожие записи, затем прочитать объекты
1. **Анализ сценариев** (структура, зависимости, переиспользование компонентов):
   `GET api/ai.php?action=analyze&resource=scenarios` — сводка по всему каталогу (гистограмма компонентов, `missing_references` по ботам/предметам/мирам/квестам, невалидные JSON);
   `GET api/ai.php?action=analyze&resource=scenarios&id=guard_intro.json` — глубокий разбор одного файла (инвентарь `components` со статусами `known/legacy/unknown`, разрешённые `references`, `dependencies` с `missing`, входящие `inbound`, `reuse` — сценарии с теми же компонентами). Используйте найденные компоненты вместо создания новых с нуля.
2. **Поиск похожих** перед созданием предмета (анти-дубли):
   `GET api/ai.php?action=similar&resource=items&name=Ключ Стражника` — кандидаты с `score_percent`; если похожие существуют, дополните заявку или переиспользуйте их. (Валидация также добавит `similar_records_found` в `warnings` автоматически.)
3. Найти целевого квестового бота и сценарий с помощью `fields` и `search`:
   `GET api/ai.php?action=list&resource=quest_bots&search=Стражник&fields=ID,Name,QuestID,ScenarioData`
4. Получить детальную запись и входящие/исходящие связи:
   `GET api/ai.php?action=get&resource=quest_bots&id=5&include_references=1`
5. Прочитать связанный файл сценария и запомнить его `sha256` как будущий `baseHash`:
   `GET api/ai.php?action=get&resource=scenarios&id=guard_intro.json&include_references=1`
6. Проверить координаты спавна или портала на карте мира:
   `GET api/ai.php?action=get&resource=maps&world=1&x=1040&y=784`
   Например, агент обнаруживает: `guard_intro.json` ссылается на несуществующий `item_id: 99999`, а у квеста не хватает нового ключевого предмета.

### Шаг 3. Подготовить многошаговый пакет изменений с символическими ссылками `ref` / `$alias` (или `tempId`) и `baseHash`
Вместо ручного подбора чисел `2000000001..` можно указать `"ref": "guard_key"` на операции `create`/`copy` и ссылаться на неё как `"$guard_key"` в `data` или `patch_json` любых последующих операций пакета (включая вложенные JSON-объекты и сценарии). Сервер автоматически назначит детерминированный `tempId`, выполнит подстановку и вернёт карту `resolved_refs: { "guard_key": 2000000001 }`. Явные `tempId` по-прежнему полностью поддерживаются:
```json
{
  "operations": [
    {
      "resource": "items",
      "action": "create",
      "ref": "guard_key",
      "data": {
        "Name": "Ключ Стражника",
        "Description": "Открывает ворота цитадели.",
        "Type": "Quest",
        "Data": { "Icon": 14 }
      }
    },
    {
      "resource": "scenarios",
      "action": "update",
      "id": "guard_intro.json",
      "baseHash": "4e1243bd22c66e76c2ba9eddc1f91394e57f9f83...",
      "data": {
        "content": {
          "scenario_mode": "default",
          "steps": [
            {
              "id": "start",
              "msg": [{ "text": "Возьми ключ и открой ворота!", "delay": 0 }],
              "components": [
                { "type": "player_give_item", "item_id": "$guard_key", "value": 1 }
              ]
            }
          ]
        }
      }
    }
  ]
}
```
При валидации (`validate`), предпросмотре (`preview`) и отправке (`propose`) ошибки содержат самовосстанавливающие подсказки (`did_you_mean`, `allowed`, `expected_type`, `default`, `hint`), чтобы агент исправлял опечатки и схемы за одну итерацию. При одобрении заявки администратором (`approve`) движок автоматически создаст запись в `tw_items_list`, получит её реальный автоинкрементный `ID` (например, `142`) и подменит временный ID внутри `guard_intro.json` перед записью на диск — в рамках единой защищённой транзакции с откатом файла при сбое.

### Шаг 4. Проверить через `validate`
```http
POST /editor-studio/api/ai.php?action=validate
Content-Type: application/json
Origin: http://127.0.0.1:8080
X-Editor-CSRF: <новый_csrf>
```
Если `"valid": false` — исправить операции по точным кодам `errors[].index`, `errors[].code`, `errors[].field`. Если в `warnings` есть `similar_records_found` — проверьте `similar[]` и убедитесь, что объекты не дублируются.

### Шаг 5. Проверить патч серверно (`apply_patch`) и diff пакета (`preview`)
Сначала **read-only** проверка каждого точечного `patch_json` против актуальной записи/файла (ничего не записывается, cooldown не тратится):
```http
POST /editor-studio/api/ai.php?action=apply_patch
Content-Type: application/json
Origin: http://127.0.0.1:8080
X-Editor-CSRF: <новый_csrf>

{
  "resource": "scenarios",
  "filename": "guard_intro.json",
  "patch_json": { "steps": [ { "id": "start", "components": [ { "type": "quest_action", "action": "accept", "quest_id": 42 } ] } ] },
  "baseHash": "4e1243bd22c66e76c2ba9eddc1f91394e57f9f83..."
}
```
Ответ: `before`/`after`, `diff` по путям JSON, `changed_fields`, `noop`, `summary_after` и те же `errors[]`, что вернул бы `propose` (патч применяется сервером к актуальным данным — клиентский merge не нужен). Если `noop: true` или патч невалиден — исправьте до отправки.
Затем dry-run всего пакета (`analysis.items[].diff`, затронутые связи, `warnings`):
```http
POST /editor-studio/api/ai.php?action=preview
Content-Type: application/json
Origin: http://127.0.0.1:8080
X-Editor-CSRF: <новый_csrf>
```
Убедиться по `analysis.items[].diff` и `warnings`, что не задеты лишние поля и ссылки согласованы.

### Шаг 6. Отправить заявку через `propose`
```http
POST /editor-studio/api/ai.php?action=propose
Content-Type: application/json
Origin: http://127.0.0.1:8080
X-Editor-CSRF: <новый_csrf>

{
  "message": "Исправлена битая ссылка в guard_intro.json: создан предмет Ключ Стражника.",
  "idempotency_key": "ai-fix-guard-intro-2026-11-27",
  "operations": [ ... ]
}
```
Ответ содержит `request.id` (например, `cr_7c1d...`) и `request.version: 1`. Повтор того же HTTP-запроса с тем же `idempotency_key` вернёт ту же заявку с `"duplicate": true`.

### Шаг 7. Отследить ответ администратора и доработать черновик
1. Проверить сообщения: `GET api/change-requests.php?action=messages&id=cr_7c1d...`.
2. Если администратор попросил поправить текст или параметры (пока `status === "pending"`), отправить обновлённый пакет через тот же `propose`, передав `request_id` и текущий `version`:
```json
{
  "request_id": "cr_7c1d...",
  "version": 1,
  "message": "Уточнено описание предмета по замечанию администратора.",
  "operations": [ ... ]
}
```
Если за время подготовки заявку уже изменили, сервер вернёт `409` (`code: "version_conflict"`, `version: 2`); если заявку уже одобрили/отклонили — `409` (`code: "request_not_pending"`). После повторного `propose` (`revised: true`, `version: 2`) заявка готова к финальному одобрению администратором в `change-requests.html`.

---

## 8. Безопасное улучшение данных (`patch_json`, `copy`, `expected_before`, `confirm_dangerous`, `history`)

- **Частичное обновление JSON без потери данных (`patch_json`)**: при `type: "update"` (или `action: "update"`) передайте `patch_json: {"Data": {"bonus": {"strength": 8}}}` для таблиц БД или `patch_json: {"scenario_mode": "universal"}` для `scenarios` — сервер применит RFC 7396 JSON Merge Patch поверх актуальной записи в БД/диске, сохранив все нетронутые поля, шаги и триггеры.
- **Копирование существующих объектов (`type: "copy"`)**: укажите `source_id` (для БД) или `source_filename` + новый `filename` (для сценариев), а также опциональные `data` / `patch_json` для точечных отличий — сервер клонирует запись в новую операцию `create`.
- **Оптимистичная проверка записей БД (`expected_before`)**: передайте `expected_before: {"Name": "Old Sword"}` на `update`/`delete` — если запись уже была изменена в БД, операция отклонится с `409 version_conflict`.
- **Защита от опасных изменений (`confirm_dangerous`)**: удаление (`delete`), очистка непустых JSON-полей, удаление непустых секций сценария, изменение критических ресурсов (`worlds`, `dungeons`, `vouchers`) и пакеты > 10 операций помечаются флагом `requires_confirmation: true` и списком `dangerous_actions`. Чтобы отправить такую заявку в `propose`, необходимо явно передать `"confirm_dangerous": true` (иначе вернётся `400 confirmation_required`).
- **Журнал действий (`GET api/ai.php?action=history`)**: возвращает историю заявок и аудит-лог текущего актора без раскрытия системных путей и IP-адресов администратора.
