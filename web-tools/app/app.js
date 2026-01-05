const sectionsConfig = [
    {
        id: 'db-editor',
        label: '🗂️ Редактор БД',
        title: 'Редактор БД',
        description: 'Выберите таблицу, строку и отредактируйте данные в форме.',
        tag: 'таблица → строка → форма',
        buildContent: () => `
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
    },
    {
        id: 'utilities',
        label: '🛠️ Утилиты',
        title: 'Утилиты',
        description: 'Быстрый доступ к вспомогательным инструментам.',
        buildContent: () => `
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
        `,
    },
    {
        id: 'scenarios',
        label: '🧩 Сценарии',
        title: 'Сценарии',
        description: 'Редактор сценариев как подстраница внутри общего layout.',
        buildContent: () => createEmbeddedModule('Scenario Editor', 'Подключите модуль сценариев как встроенный компонент.'),
    },
    {
        id: 'events',
        label: '🎯 События',
        title: 'События',
        description: 'Редактор событий внутри общего layout.',
        buildContent: () => createEmbeddedModule('Event Editor', 'Подключите модуль событий как встроенный компонент.'),
    },
    {
        id: 'vouchers',
        label: '🎫 Voucher-коды',
        title: 'Voucher-коды',
        description: 'Управление наградами и кодами активации.',
        buildContent: () => `
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
        `,
    },
];

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

const createEmbeddedModule = (title, description) => `
    <div class="module-shell">
        <div>
            <span class="tag">встраиваемый модуль</span>
            <h3>${title}</h3>
            <p class="notice">${description}</p>
            <p class="notice">Здесь будет интеграция без iframe с общими стилями и навигацией.</p>
        </div>
    </div>
`;

const createSidebar = (sections) => {
    const sidebar = document.createElement('aside');
    sidebar.className = 'sidebar';
    sidebar.innerHTML = `
        <div class="brand">
            <h1>MRPG Web Tools</h1>
            <p>Единый вход для редакторов и служебных утилит.</p>
        </div>
        <nav class="nav" aria-label="Разделы"></nav>
    `;

    const nav = sidebar.querySelector('.nav');
    sections.forEach((section, index) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.dataset.section = section.id;
        button.textContent = section.label;
        if (index === 0) {
            button.classList.add('active');
        }
        nav.appendChild(button);
    });

    return sidebar;
};

const createSection = ({ id, title, description, tag, buildContent }, isActive) => {
    const section = document.createElement('section');
    section.id = id;
    section.className = `section${isActive ? ' active' : ''}`;
    const tagMarkup = tag ? `<span class="tag">${tag}</span>` : '';
    section.innerHTML = `
        <div class="section-header">
            <div>
                <h2>${title}</h2>
                <p>${description}</p>
            </div>
            ${tagMarkup}
        </div>
        ${buildContent()}
    `;
    return section;
};

const createLayout = () => {
    const appShell = document.createElement('div');
    appShell.className = 'app-shell';

    const sidebar = createSidebar(sectionsConfig);
    const content = document.createElement('main');
    content.className = 'content';

    sectionsConfig.forEach((section, index) => {
        content.appendChild(createSection(section, index === 0));
    });

    appShell.appendChild(sidebar);
    appShell.appendChild(content);

    return appShell;
};

const renderTableList = (tableList) => {
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
};

const mountDatabaseEditor = () => {
    const tableList = document.getElementById('table-list');
    const rowTableBody = document.querySelector('#row-table tbody');
    const rowForm = document.getElementById('row-form');
    const rowId = document.getElementById('row-id');
    const rowName = document.getElementById('row-name');
    const rowStatus = document.getElementById('row-status');
    const rowNotes = document.getElementById('row-notes');

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

    renderTableList(tableList);

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

const mountNavigation = () => {
    const sections = document.querySelectorAll('.section');
    const navButtons = document.querySelectorAll('.nav button');

    navButtons.forEach((button) => {
        button.addEventListener('click', () => {
            navButtons.forEach((item) => item.classList.remove('active'));
            button.classList.add('active');
            sections.forEach((section) => {
                section.classList.toggle('active', section.id === button.dataset.section);
            });
        });
    });
};

const bootstrap = () => {
    const root = document.getElementById('app');
    root.appendChild(createLayout());
    mountNavigation();
    mountDatabaseEditor();
};

document.addEventListener('DOMContentLoaded', bootstrap);
