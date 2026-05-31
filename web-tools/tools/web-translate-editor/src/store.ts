import { create } from 'zustand';
import { v4 as uuidv4 } from 'uuid';
import type { AppState, Settings, LanguageIndex, TranslationFile, TranslationEntry, ChangeRequest, ChangeRequestEntry, UILanguage, AdminConfig } from './types';
import { setUILanguage as setI18nLanguage } from './i18n';
import { sha256 } from './utils/crypto';
import type { LoadResult } from './utils/fileLoader';
import { saveTranslationToServer } from './utils/fileLoader';

const DEFAULT_SETTINGS: Settings = {
  translationPath: '/translations',
};

// Admin credentials from .env (read at build time)
const ADMIN_CONFIG: AdminConfig = {
  username: import.meta.env.VITE_ADMIN_USERNAME || 'admin',
  passwordHash: import.meta.env.VITE_ADMIN_PASSWORD_HASH || '8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918', // default: "admin"
};

// Sample index.json
const SAMPLE_INDEX = {
  "language indices": [
    { file: "ru", name: "Русский" },
    { file: "cn", name: "中文" },
  ]
};

// Sample translation files
const SAMPLE_RU = `Open
== Открыть

Close
== Закрыть

Save
== Сохранить

File
== Файл

Edit
== Редактировать

Settings
== Настройки

Help
== Помощь

Search
== Поиск

Cancel
== Отмена

Apply
== Применить

Delete
== Удалить

Create
== Создать

$dialog_quest590
Leia's egg is irreplaceable. I cannot well express to you the depth of my gratitude.
== Яйцо Леи незаменимо. Я не могу выразить вам всю глубину моей благодарности.

- Required: {} ({} | {})\\n
== - Требуется: {} ({} | {})\\n

- /group Get all sub commands
== - /group Получить все подкоманды

Enchantable
== Зачаровываемый

Welcome to our application
==

Loading resources...
==

Connection established
==

Authentication required
==

Permission denied
==`;

const SAMPLE_CN = `Open
== 打开

Close
== 关闭

Save
== 保存

File
== 文件

Edit
== 编辑

Settings
== 设置

Help
== 帮助

Search
== 搜索

Cancel
== 取消

Apply
== 应用

Delete
== 删除

Create
== 创建

$dialog_quest590
Leia's egg is irreplaceable. I cannot well express to you the depth of my gratitude.
== 莱娅的蛋是不可替代的。我无法向你表达我感激之深。

- Required: {} ({} | {})\\n
== - 需要: {} ({} | {})\\n

- /group Get all sub commands
== - /group 获取所有子命令

Enchantable
== 可附魔

Welcome to our application
== 欢迎使用我们的应用程序

Loading resources...
==

Connection established
== 连接已建立

Authentication required
== 需要认证

Permission denied
==`;

function parseTranslationFile(content: string, lang: LanguageIndex): TranslationFile {
  const lines = content.split('\n');
  const entries: TranslationEntry[] = [];
  let i = 0;
  let entryIndex = 0;

  while (i < lines.length) {
    const line = lines[i].trim();

    // Skip empty lines
    if (line === '') {
      i++;
      continue;
    }

    // Skip lines starting with $ (hash identifiers)
    if (line.startsWith('$')) {
      i++;
      continue;
    }

    // Check if the next non-empty line starts with ==
    let j = i + 1;
    while (j < lines.length && lines[j].trim() === '') {
      j++;
    }

    if (j < lines.length && lines[j].trim().startsWith('==')) {
      const original = line;
      const translationLine = lines[j].trim();
      const translation = translationLine.substring(2).trim();

      entries.push({
        id: uuidv4(),
        original,
        translation,
        lineIndex: entryIndex,
      });
      entryIndex++;
      i = j + 1;
    } else {
      i++;
    }
  }

  return {
    language: lang,
    entries,
    rawContent: content,
  };
}

// rebuildTranslationContent removed - inline rebuild used in approveRequest

const SAMPLE_FILES: Record<string, string> = {
  ru: SAMPLE_RU,
  cn: SAMPLE_CN,
};

function loadInitialState(): Partial<AppState> {
  const stored = localStorage.getItem('translationEditor');
  if (stored) {
    try {
      const parsed = JSON.parse(stored);
      // Re-parse translation files
      const translations: Record<string, TranslationFile> = {};
      const langs = parsed.languages || SAMPLE_INDEX["language indices"];
      const rawFiles = parsed.rawFiles || SAMPLE_FILES;
      for (const lang of langs) {
        if (rawFiles[lang.file]) {
          translations[lang.file] = parseTranslationFile(rawFiles[lang.file], lang);
        }
      }
      return {
        settings: parsed.settings || DEFAULT_SETTINGS,
        languages: langs,
        translations,
        changeRequests: parsed.changeRequests || [],
        isAdmin: false,
      };
    } catch {
      // fallback
    }
  }

  // Fallback: use demo data
  const translations: Record<string, TranslationFile> = {};
  for (const lang of SAMPLE_INDEX["language indices"]) {
    if (SAMPLE_FILES[lang.file]) {
      translations[lang.file] = parseTranslationFile(SAMPLE_FILES[lang.file], lang);
    }
  }

  return {
    settings: DEFAULT_SETTINGS,
    languages: SAMPLE_INDEX["language indices"],
    translations,
    changeRequests: [],
    isAdmin: false,
  };
}

function saveState(state: AppState) {
  const rawFiles: Record<string, string> = {};
  for (const [key, tf] of Object.entries(state.translations)) {
    rawFiles[key] = tf.rawContent;
  }
  localStorage.setItem('translationEditor', JSON.stringify({
    settings: state.settings,
    languages: state.languages,
    changeRequests: state.changeRequests,
    rawFiles,
  }));
}

interface StoreActions {
  setActiveTab: (tab: AppState['activeTab']) => void;
  setSelectedLanguage: (lang: string | null) => void;
  setSearchQuery: (query: string) => void;
  setFilterMode: (mode: AppState['filterMode']) => void;
  setRequestFilterTag: (tag: string | null) => void;
  setUiLanguage: (lang: UILanguage) => void;
  updateSettings: (settings: Settings) => void;
  login: (username: string, password: string) => Promise<boolean>;
  logout: () => void;
  submitChangeRequest: (name: string, author: string, languageFile: string, changes: ChangeRequestEntry[]) => void;
  approveRequest: (requestId: string) => Promise<void>;
  rejectRequest: (requestId: string) => void;
  deleteRequest: (requestId: string) => void;
  resetData: () => void;
  loadFiles: (result: LoadResult) => void;
}

const initial = loadInitialState();

export const useStore = create<AppState & StoreActions>((set, get) => ({
  settings: initial.settings || DEFAULT_SETTINGS,
  languages: initial.languages || [],
  translations: initial.translations || {},
  changeRequests: initial.changeRequests || [],
  isAdmin: false,
  activeTab: 'editor',
  selectedLanguage: null,
  searchQuery: '',
  uiLanguage: 'en' as UILanguage,
  filterMode: 'all',
  requestFilterTag: null,

  setActiveTab: (tab) => set({ activeTab: tab }),
  setSelectedLanguage: (lang) => set({ selectedLanguage: lang, searchQuery: '', filterMode: 'all' }),
  setSearchQuery: (query) => set({ searchQuery: query }),
  setFilterMode: (mode) => set({ filterMode: mode }),
  setRequestFilterTag: (tag) => set({ requestFilterTag: tag }),
  setUiLanguage: (lang) => {
    setI18nLanguage(lang);
    set({ uiLanguage: lang });
  },

  updateSettings: (settings) => {
    set({ settings });
    const state = get();
    saveState(state as AppState);
  },

  login: async (username, password) => {
    const passwordHash = await sha256(password);
    if (username === ADMIN_CONFIG.username && passwordHash === ADMIN_CONFIG.passwordHash) {
      set({ isAdmin: true });
      return true;
    }
    return false;
  },

  logout: () => set({ isAdmin: false }),

  submitChangeRequest: (name, author, languageFile, changes) => {
    const state = get();
    const lang = state.languages.find(l => l.file === languageFile);
    const request: ChangeRequest = {
      id: uuidv4(),
      name,
      author,
      languageFile,
      languageName: lang?.name || languageFile,
      entries: changes,
      status: 'pending',
      createdAt: new Date().toISOString(),
    };
    const newRequests = [...state.changeRequests, request];
    set({ changeRequests: newRequests });
    saveState({ ...state, changeRequests: newRequests } as AppState);
  },

  approveRequest: async (requestId) => {
    const state = get();
    const request = state.changeRequests.find(r => r.id === requestId);
    if (!request) return;

    // Apply changes to the translation file
    const file = state.translations[request.languageFile];
    if (!file) return;

    // Rebuild raw content
    const lines = file.rawContent.split('\n');
    const resultLines: string[] = [];
    let i = 0;

    while (i < lines.length) {
      const line = lines[i].trim();

      if (line === '' || line.startsWith('$')) {
        resultLines.push(lines[i]);
        i++;
        continue;
      }

      let j = i + 1;
      while (j < lines.length && lines[j].trim() === '') {
        j++;
      }

      if (j < lines.length && lines[j].trim().startsWith('==')) {
        const original = line;
        const change = request.entries.find(c => c.original === original);
        resultLines.push(lines[i]);
        if (change) {
          resultLines.push(`== ${change.newTranslation}`);
        } else {
          resultLines.push(lines[j]);
        }
        i = j + 1;
      } else {
        resultLines.push(lines[i]);
        i++;
      }
    }

    const newRawContent = resultLines.join('\n');

    await saveTranslationToServer(request.languageFile, newRawContent);

    const lang = state.languages.find(l => l.file === request.languageFile)!;
    const newFile = parseTranslationFile(newRawContent, lang);

    const newTranslations = {
      ...state.translations,
      [request.languageFile]: newFile,
    };

    const newRequests = state.changeRequests.map(r =>
      r.id === requestId ? { ...r, status: 'approved' as const, resolvedAt: new Date().toISOString() } : r
    );

    set({ translations: newTranslations, changeRequests: newRequests });
    saveState({ ...state, translations: newTranslations, changeRequests: newRequests } as AppState);
  },

  rejectRequest: (requestId) => {
    const state = get();
    const newRequests = state.changeRequests.map(r =>
      r.id === requestId ? { ...r, status: 'rejected' as const, resolvedAt: new Date().toISOString() } : r
    );
    set({ changeRequests: newRequests });
    saveState({ ...state, changeRequests: newRequests } as AppState);
  },

  deleteRequest: (requestId) => {
    const state = get();
    const newRequests = state.changeRequests.filter(r => r.id !== requestId);
    set({ changeRequests: newRequests });
    saveState({ ...state, changeRequests: newRequests } as AppState);
  },

  resetData: () => {
    localStorage.removeItem('translationEditor');
    const fresh = loadInitialState();
    set({
      settings: fresh.settings,
      languages: fresh.languages,
      translations: fresh.translations,
      changeRequests: [],
    });
  },

  loadFiles: (result: LoadResult) => {
    const translations: Record<string, TranslationFile> = {};
    for (const lang of result.languages) {
      const content = result.files[lang.file] || '';
      translations[lang.file] = parseTranslationFile(content, lang);
    }
    const state = get();
    set({ languages: result.languages, translations, selectedLanguage: null });
    saveState({ ...state, languages: result.languages, translations } as AppState);
  },
}));
