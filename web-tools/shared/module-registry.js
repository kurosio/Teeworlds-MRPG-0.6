(() => {
    const demoData = {
        quests: [
            { id: 'Q-001', name: 'Знакомство с деревней', status: 'active', notes: 'Начальный квест.' },
            { id: 'Q-014', name: 'Ритуал у озера', status: 'draft', notes: 'Подготовить реплики.' },
            { id: 'Q-203', name: 'Осада крепости', status: 'archived', notes: 'Закрыт после патча.' },
        ],
        items: [
            { id: 'I-112', name: 'Клинок странника', status: 'active', notes: 'Добавить редкий дроп.' },
            { id: 'I-221', name: 'Зелье скорости', status: 'active', notes: 'Баланс +5%.' },
        ],
        npcs: [
            { id: 'N-008', name: 'Староста Тор', status: 'active', notes: 'Обновить диалоги.' },
            { id: 'N-047', name: 'Бродячий торговец', status: 'draft', notes: 'Нужны товары.' },
        ],
        events: [
            { id: 'E-001', name: 'Городская ярмарка', status: 'active', notes: 'Периодичность 7 дней.' },
            { id: 'E-009', name: 'Нашествие слизней', status: 'draft', notes: 'Тестовые волны.' },
        ],
    };

    const tableListConfig = [
        { key: 'quests', label: 'quests' },
        { key: 'items', label: 'items' },
        { key: 'npcs', label: 'npcs' },
        { key: 'events', label: 'events' },
    ];

    const createStaticModule = (buildContent, onMount) => {
        return {
            mount(container) {
                container.innerHTML = buildContent();
                if (onMount) {
                    onMount(container);
                }
            },
            unmount(container) {
                container.innerHTML = '';
            },
        };
    };

    const createIframeModule = ({ src, globalKey }) => {
        let iframe = null;
        return {
            mount(container) {
                if (iframe) {
                    return;
                }
                container.innerHTML = '<div class="iframe-wrap"></div>';
                const wrapper = container.querySelector('.iframe-wrap');
                iframe = document.createElement('iframe');
                const url = new URL(src, window.location.href);
                url.searchParams.set('embedded', '1');
                iframe.src = url.toString();
                iframe.addEventListener('load', () => {
                    const api = iframe?.contentWindow?.[globalKey];
                    if (api?.mount) {
                        api.mount(iframe.contentDocument.body, { embedded: true });
                    }
                });
                wrapper.appendChild(iframe);
            },
            unmount(container) {
                if (iframe) {
                    iframe.remove();
                    iframe = null;
                }
                container.innerHTML = '';
            },
        };
    };

    const mountDatabaseEditor = (container) => {
        const tableList = container.querySelector('#table-list');
        const rowTableBody = container.querySelector('#row-table tbody');
        const rowForm = container.querySelector('#row-form');
        const rowId = container.querySelector('#row-id');
        const rowName = container.querySelector('#row-name');
        const rowStatus = container.querySelector('#row-status');
        const rowNotes = container.querySelector('#row-notes');

        const setFormData = (row) => {
            if (!row) {
                rowId.value = '';
                rowName.value = '';
                rowStatus.value = 'draft';
                rowNotes.value = '';
                return;
            }
            rowId.value = row.id;
            rowName.value = row.name;
            rowStatus.value = row.status;
            rowNotes.value = row.notes;
        };

        const renderRows = (tableKey) => {
            rowTableBody.innerHTML = '';
            demoData[tableKey].forEach((row, index) => {
                const tr = document.createElement('tr');
                tr.dataset.index = index;
                tr.innerHTML = `
                    <td>${row.id}</td>
                    <td>${row.name}</td>
                    <td>${row.status}</td>
                `;
                if (index === 0) {
                    tr.classList.add('active');
                }
                rowTableBody.appendChild(tr);
            });
            setFormData(demoData[tableKey][0]);
        };

        tableList.innerHTML = '';
        tableListConfig.forEach((table, index) => {
            const button = document.createElement('button');
            button.type = 'button';
            button.dataset.table = table.key;
            button.textContent = table.label;
            if (index === 0) {
                button.classList.add('active');
            }
            tableList.appendChild(button);
        });

        tableList.addEventListener('click', (event) => {
            const button = event.target.closest('button');
            if (!button) {
                return;
            }
            tableList.querySelectorAll('button').forEach((item) => item.classList.remove('active'));
            button.classList.add('active');
            renderRows(button.dataset.table);
        });

        rowTableBody.addEventListener('click', (event) => {
            const row = event.target.closest('tr');
            if (!row) {
                return;
            }
            rowTableBody.querySelectorAll('tr').forEach((item) => item.classList.remove('active'));
            row.classList.add('active');
            const tableKey = tableList.querySelector('button.active').dataset.table;
            const rowData = demoData[tableKey][Number(row.dataset.index)];
            setFormData(rowData);
        });

        rowForm.addEventListener('submit', (event) => {
            event.preventDefault();
        });

        renderRows(tableListConfig[0].key);
    };

    window.moduleRegistry = [
        {
            id: 'db-editor',
            label: '🗂️ Редактор БД',
            route: '/db-editor',
            title: 'Редактор БД',
            description: 'Выберите таблицу, строку и отредактируйте данные в форме.',
            tag: 'таблица → строка → форма',
            ...createStaticModule(
                () => `
                    <div class="card db-layout">
                        <div>
                            <h3>Таблицы</h3>
                            <div class="list" id="table-list"></div>
                        </div>
                        <div>
                            <h3>Строки</h3>
                            <table class="db-table" id="row-table">
                                <thead>
                                    <tr>
                                        <th>ID</th>
                                        <th>Название</th>
                                        <th>Статус</th>
                                    </tr>
                                </thead>
                                <tbody></tbody>
                            </table>
                        </div>
                        <div>
                            <h3>Форма редактирования</h3>
                            <form class="form-grid" id="row-form">
                                <div>
                                    <label for="row-id">ID</label>
                                    <input id="row-id" name="id" type="text" readonly>
                                </div>
                                <div>
                                    <label for="row-name">Название</label>
                                    <input id="row-name" name="name" type="text">
                                </div>
                                <div>
                                    <label for="row-status">Статус</label>
                                    <select id="row-status" name="status">
                                        <option value="draft">Черновик</option>
                                        <option value="active">Активен</option>
                                        <option value="archived">Архив</option>
                                    </select>
                                </div>
                                <div>
                                    <label for="row-notes">Примечание</label>
                                    <textarea id="row-notes" name="notes" rows="4"></textarea>
                                </div>
                                <button type="button" class="tag" id="save-row">Сохранить изменения</button>
                            </form>
                            <p class="notice">Данные демонстрационные. Подключите API для сохранения.</p>
                        </div>
                    </div>
                `,
                mountDatabaseEditor
            ),
        },
        {
            id: 'utilities',
            label: '🛠️ Утилиты',
            route: '/utilities',
            title: 'Утилиты',
            description: 'Быстрый доступ к вспомогательным инструментам.',
            ...createStaticModule(() => `
                <div class="grid three">
                    <div class="card">
                        <h3>Импорт/Экспорт</h3>
                        <p class="notice">Сюда можно добавить экспорт JSON, бэкапы или миграции.</p>
                    </div>
                    <div class="card">
                        <h3>Логи и мониторинг</h3>
                        <p class="notice">Панель для логов событий или аналитики.</p>
                    </div>
                    <div class="card">
                        <h3>Проверка контента</h3>
                        <p class="notice">Валидация сценариев, NPC и квестов.</p>
                    </div>
                </div>
            `),
        },
        {
            id: 'scenarios',
            label: '🧩 Сценарии',
            route: '/scenarios',
            title: 'Сценарии',
            description: 'Редактор сценариев, подключенный как встроенный модуль.',
            ...createIframeModule({
                src: '../scenario-editor/index.html',
                globalKey: 'ScenarioEditor',
            }),
        },
        {
            id: 'events',
            label: '🎯 События',
            route: '/events',
            title: 'События',
            description: 'Редактор событий внутри общего layout.',
            ...createIframeModule({
                src: '../event-editor/event-editor.php',
                globalKey: 'EventEditor',
            }),
        },
        {
            id: 'vouchers',
            label: '🎫 Voucher-коды',
            route: '/vouchers',
            title: 'Voucher-коды',
            description: 'Управление наградами и кодами активации.',
            ...createStaticModule(() => `
                <div class="card">
                    <div class="grid two">
                        <div>
                            <h3>Активные коды</h3>
                            <ul class="notice">
                                <li>SPRING-2024 — 100 золота</li>
                                <li>WELCOME-NEW — стартовый набор</li>
                            </ul>
                        </div>
                        <div>
                            <h3>Создать код</h3>
                            <form class="form-grid">
                                <div>
                                    <label for="voucher-name">Название</label>
                                    <input id="voucher-name" type="text" placeholder="VIP-APRIL">
                                </div>
                                <div>
                                    <label for="voucher-reward">Награда</label>
                                    <input id="voucher-reward" type="text" placeholder="300 золота">
                                </div>
                                <button type="button" class="tag">Сгенерировать</button>
                            </form>
                        </div>
                    </div>
                </div>
            `),
        },
    ];
})();
