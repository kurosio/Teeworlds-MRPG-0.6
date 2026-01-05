const modulesConfig = window.moduleRegistry || [];

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

const createSection = ({ id, title, description, tag }, isActive) => {
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
    `;
    const content = document.createElement('div');
    content.className = 'section-content';
    content.dataset.moduleContent = id;
    section.appendChild(content);
    return section;
};

const createLayout = () => {
    const appShell = document.createElement('div');
    appShell.className = 'app-shell';

    const sidebar = createSidebar(modulesConfig);
    const content = document.createElement('main');
    content.className = 'content';

    modulesConfig.forEach((section, index) => {
        content.appendChild(createSection(section, index === 0));
    });

    appShell.appendChild(sidebar);
    appShell.appendChild(content);

    return appShell;
};

const getModuleById = (id) => modulesConfig.find((module) => module.id === id);

const getModuleContainer = (id) => {
    const section = document.getElementById(id);
    return section?.querySelector('[data-module-content]');
};

const mountModule = (moduleId) => {
    const module = getModuleById(moduleId);
    const container = getModuleContainer(moduleId);
    if (!module || !container) {
        return;
    }
    module.mount(container, { route: module.route });
};

const unmountModule = (moduleId) => {
    const module = getModuleById(moduleId);
    const container = getModuleContainer(moduleId);
    if (!module || !container || !module.unmount) {
        return;
    }
    module.unmount(container);
};

const mountNavigation = () => {
    const sections = document.querySelectorAll('.section');
    const navButtons = document.querySelectorAll('.nav button');
    let activeModuleId = modulesConfig[0]?.id;

    const activateModule = (moduleId) => {
        if (activeModuleId === moduleId) {
            return;
        }
        if (activeModuleId) {
            unmountModule(activeModuleId);
        }
        activeModuleId = moduleId;
        mountModule(moduleId);
    };

    navButtons.forEach((button) => {
        button.addEventListener('click', () => {
            navButtons.forEach((item) => item.classList.remove('active'));
            button.classList.add('active');
            sections.forEach((section) => {
                section.classList.toggle('active', section.id === button.dataset.section);
            });
            activateModule(button.dataset.section);
        });
    });

    if (activeModuleId) {
        mountModule(activeModuleId);
    }
};

const bootstrap = () => {
    const root = document.getElementById('app');
    root.appendChild(createLayout());
    mountNavigation();
};

document.addEventListener('DOMContentLoaded', bootstrap);
