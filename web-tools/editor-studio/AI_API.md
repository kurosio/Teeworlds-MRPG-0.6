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

1. **Единая точка подключения (`/ai`)**:
   - `GET /ai` (или `/ai/`) — публичная страница подключения для браузера, LLM и веб-агентов (без секретов и локальных путей сервера), содержащая встроенный JSON-манифест, промпт-заготовку, конфиг MCP и проверку токена.
   - `GET /ai?format=json` (или `Accept: application/json`) — машиночитаемый JSON онбординга.
   - `GET /.well-known/ai-plugin.json` (алиас `api/ai.php?action=discovery`) — стандартный дескриптор автообнаружения.
2. **OpenAPI 3.1.0**:
   - `GET api/ai.php?action=openapi` — публичная спецификация `3.1.0` (версия API `2.0.0`), работает на любом хостинге (**рекомендуемый основной URL**).
   - `GET /openapi.json` — тот же документ, но только при активном rewrite (Docker / `router.php`); без него — `403`.
3. **MCP Server (JSON-RPC 2.0)**:
   - `POST /mcp` (или `api/mcp.php`) — 14 типизированных инструментов (`get_manifest`, `get_schema`, `list_records`, `get_record`, `get_references`, `analyze_scenario`, `find_similar`, `validate_operations`, `preview_operations`, `apply_patch`, `propose_changes`, `copy_record`, `patch_json_record`, `get_history`), динамические `resources/list`, `resources/read` (`mrpg://manifest`, `mrpg://openapi`, `mrpg://schema/<resource>`) и `prompts/*`.
   - `node tools/mcp-stdio.mjs --url https://... --token esai_...` — stdio-мост для Claude Desktop, Codex CLI, Cursor, Windsurf и локальных клиентов (по умолчанию ходит на `/api/mcp.php`, работает без rewrite; `--url .../mcp` — алиас).

### Живой `manifest` (v2.0)
```http
GET /editor-studio/api/ai.php?action=manifest
```
После входа (Bearer-токеном или гостевой сессией) возвращает актуальное состояние:
- `capabilities` и `discovery_urls`: поддерживаемые возможности (`model_agnostic`, `auto_resource_discovery`, `mcp_jsonrpc_2_0`, `bearer_tokens`, `scoped_rbac`, `patch_json_merge`, `copy_records`, `dangerous_confirmation_gate`, `optimistic_concurrency`, `idempotent_propose`, `scenario_analysis`, `scenario_corpus_dependency_check`, `similar_records_search`, `similar_records_create_warning`, `verify_before_propose`). Поле `recommended_workflow` внутри manifest фиксирует безопасный цикл `analyze → similar → get → apply_patch → validate → preview → propose`.
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
Администратор выпускает токен в разделе **Guest users** (`guests-editor.html` -> `New token`), привязывая его к гостевому аккаунту и явному набору прав (`read`, `create`, `update`, `delete`):
```http
GET /editor-studio/api/ai.php?action=manifest
Authorization: Bearer esai_<ваш_токен>
```
Токены работают как в REST API (`api/ai.php`, `api/change-requests.php`, `api/auth.php?action=status`), так и в MCP (`/mcp`, `tools/mcp-stdio.mjs`), не требуют cookie и `X-Editor-CSRF`, хранятся в SQLite в виде `sha256`-хеша и могут быть мгновенно отозваны.

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
- **Фильтрация:**
  - `search=...` (поиск подстроки по индексным/поисковым колонкам или метаданным сценария);
  - `filter={"Col":"Val","Status":["A","B"],"Deleted":null}` (точные совпадения, `IS NULL` и `IN (...)`; для `scenarios` поддерживаются `scenario_mode` и `valid_json`);
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

### `GET api/ai.php?action=get`
- **БД:** `?action=get&resource=quests&id=12&fields=ID,Name,WorldID,ScenarioData&include_references=1`
- **Сценарий:** `?action=get&resource=scenarios&id=dungeon_gate.json&include_references=1` (или `&summary_only=1`)
- **Карта DDNet v4:** `?action=get&resource=maps&world=1&include_entities=1&x=1040&y=784`
  Возвращает размеры карты в тайлах и пикселях (`width_tiles * 32`), статистику тайлов `game_layer` и `switch_layer`, координаты сущностей и результат проверки `point_check` (`in_bounds`, `solid`, `death`, `unhookable`, `walkable`).

### `GET api/ai.php?action=references`
- По конкретной записи или файлу сценария: `?action=references&resource=items&id=10` (исходящие и входящие ссылки только по разрешённым гостю таблицам и сценариям).
- По справочнику: `?action=references&source=attributes&search=Str&limit=50`.
- Без параметров: полный граф зависимостей схемы между доступными ресурсами.

### `GET api/ai.php?action=analyze[&resource=scenarios][&id=file.json]`
Глубокий анализ сценариев (read-only, `scope: read`):
- **Один файл** (`&id=dungeon_gate.json`): `summary` (шаги/события), инвентарь компонентов `components` (`type`, `count`, `steps`, `status`: `known` | `legacy` | `unknown`), `unknown_component_types`, типобезопасные исходящие `references` с проверкой существования в БД (`exists`, `label`), агрегированные `dependencies` по таблицам (refs/unique/missing) и флаг `dependency_ok`, входящие ссылки `inbound` (кто ссылается на сценарий, включая `items.Data.use_scenario.name`), а также `reuse` — другие сценарии, использующие те же типы компонентов (для переиспользования логики вместо дублирования). Ответ содержит `baseHash` (SHA-256) для последующего `propose`.
- **Весь каталог** (без `&id=`): сводка `mode=corpus` — `scanned` / `truncated` (до 200 файлов), невалидные JSON (`invalid` с `parse_error`), гистограмма `components` (в каких скольки сценариях встречается каждый тип), `reference_total` и отчёт `missing_references` (до 100 строк: `filename`, `path`, `target`, `id`) по всем ссылкам сценариев на ботов/предметы/миры/квесты — готовая карта битых зависимостей. Параметр `search` фильтрует файлы теми же полями, что и `list`.
Для записей БД (боты, предметы, миры) используйте `references` — `analyze` сценариев опирается на то же разрешение ссылок.

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

### Сопровождение заявки через `api/change-requests.php`
- `GET ?action=mine&limit=20&offset=0` — список своих заявок и непрочитанных ответов;
- `GET ?action=get&id=cr_...` и `GET ?action=messages&id=cr_...` — детальный просмотр и переписка с администратором;
- `POST ?action=message` (`{ "id": "cr_...", "body": "..." }`) — ответ в треде заявки;
- `POST ?action=mark_read` (`{ "id": "cr_..." }`) — отметить ответы прочитанными;
- `POST ?action=withdraw` (`{ "id": "cr_..." }`) — отозвать ещё не рассмотренную заявку.

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

### Шаг 3. Подготовить многошаговый пакет изменений с `tempId` и `baseHash`
```json
{
  "operations": [
    {
      "resource": "items",
      "action": "create",
      "tempId": 2000000101,
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
                { "type": "player_give_item", "item_id": 2000000101, "value": 1 }
              ]
            }
          ]
        }
      }
    }
  ]
}
```
При одобрении заявки администратором (`approve`) движок автоматически создаст запись в `tw_items_list`, получит её реальный автоинкрементный `ID` (например, `142`) и подменит `2000000101 -> 142` внутри `guard_intro.json` перед записью на диск — в рамках единой защищённой транзакции с откатом файла при сбое.

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
