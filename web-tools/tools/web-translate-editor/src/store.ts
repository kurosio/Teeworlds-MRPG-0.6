import { create } from 'zustand';
import { v4 as uuidv4 } from 'uuid';
import type { AppState, Settings, LanguageIndex, TranslationFile, TranslationEntry, ChangeRequest, ChangeRequestEntry, UILanguage, AdminConfig, TopContributor } from './types';
import { setUILanguage as setI18nLanguage } from './i18n';
import { sha256 } from './utils/crypto';
import type { LoadResult } from './utils/fileLoader';
import { applyTranslationChangesToServer, loadTopContributorsFromServer, saveTopContributorsToServer, loadChangeRequestsFromServer, createChangeRequestOnServer, updateChangeRequestOnServer, deleteChangeRequestOnServer } from './utils/fileLoader';

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


function isEnglishLanguage(lang: LanguageIndex): boolean {
  const file = String(lang?.file || '').trim().toLowerCase();
  const name = String(lang?.name || '').trim().toLowerCase();
  return file === 'en' || file === 'eng' || name === 'english';
}

function normalizeLocalizationLanguages(languages: LanguageIndex[]): LanguageIndex[] {
  return languages.filter(lang => lang?.file && !isEnglishLanguage(lang));
}

function makeEntryId(langFile: string, entryIndex: number, original: string): string {
  // Keep ids stable enough for local editing within one loaded file.
  // Index is retained to disambiguate duplicate source lines.
  return `${langFile}:${entryIndex}:${original}`;
}

function getLanguageSignature(languages: LanguageIndex[]): string {
  return languages.map(lang => `${lang.file}:${lang.name}`).join('|');
}

function getVersionSignature(languages: LanguageIndex[], versions: Record<string, number>): string {
  return `${versions.__index || 0}|${getLanguageSignature(languages)}|${languages.map(lang => `${lang.file}:${versions[lang.file] || 0}`).join('|')}`;
}

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
        id: makeEntryId(lang.file, entryIndex, original),
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

function getTranslatedTextAmount(entries: ChangeRequestEntry[]): number {
  return entries.reduce((total, entry) => {
    const text = String(entry.newTranslation ?? '').trim();
    return total + text.length;
  }, 0);
}

function buildTopContributorsFromRequests(changeRequests: ChangeRequest[]): TopContributor[] {
  const authorStats = new Map<string, number>();
  for (const request of changeRequests) {
    if (request.status !== 'approved') continue;
    const translatedTextAmount = getTranslatedTextAmount(request.entries);
    if (translatedTextAmount <= 0) continue;
    authorStats.set(request.author, (authorStats.get(request.author) || 0) + translatedTextAmount);
  }
  return Array.from(authorStats.entries())
    .map(([author, count]) => ({ author, count }))
    .sort((a, b) => b.count - a.count)
    .slice(0, 100);
}

function loadInitialState(): Partial<AppState> {
  const stored = localStorage.getItem('translationEditor');
  if (stored) {
    try {
      const parsed = JSON.parse(stored);
      // Re-parse translation files
      const translations: Record<string, TranslationFile> = {};
      const langs = normalizeLocalizationLanguages(parsed.languages || SAMPLE_INDEX["language indices"]);
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
        changeRequests: [],
        topContributors: [],
        fileVersions: parsed.fileVersions || {},
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
    languages: normalizeLocalizationLanguages(SAMPLE_INDEX["language indices"]),
    translations,
    changeRequests: [],
    topContributors: [],
    fileVersions: {},
    isAdmin: false,
  };
}

function saveState(state: AppState) {
  // Do not persist raw localization files in localStorage. Large projects can contain
  // thousands of rows and serializing them on every sync/edit causes UI freezes.
  localStorage.setItem('translationEditor', JSON.stringify({
    settings: state.settings,
    languages: state.languages,
    fileVersions: state.fileVersions,
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
  submitChangeRequest: (name: string, author: string, languageFile: string, changes: ChangeRequestEntry[]) => Promise<ChangeRequest | null>;
  approveRequest: (requestId: string, editedEntries?: ChangeRequestEntry[]) => Promise<void>;
  rejectRequest: (requestId: string) => Promise<void>;
  deleteRequest: (requestId: string) => Promise<void>;
  resetData: () => void;
  loadFiles: (result: LoadResult) => void;
  refreshFiles: (result: LoadResult) => void;
  forceSyncFiles: (result: LoadResult) => void;
  loadTopContributors: () => Promise<void>;
  loadChangeRequests: () => Promise<void>;
  hasServerChanges: (result: LoadResult) => boolean;
}

const initial = loadInitialState();

export const useStore = create<AppState & StoreActions>((set, get) => ({
  settings: initial.settings || DEFAULT_SETTINGS,
  languages: initial.languages || [],
  translations: initial.translations || {},
  fileVersions: initial.fileVersions || {},
  changeRequests: initial.changeRequests || [],
  topContributors: initial.topContributors || [],
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

  submitChangeRequest: async (name, author, languageFile, changes) => {
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

    try {
      const changeRequests = await createChangeRequestOnServer(request);
      set({ changeRequests: changeRequests.length > 0 ? changeRequests : [request, ...state.changeRequests] });
      return request;
    } catch (error) {
      console.error(error);
      // Do not silently save requests to localStorage: requests must be global.
      throw error;
    }
  },

  approveRequest: async (requestId, editedEntries) => {
    const state = get();
    const request = state.changeRequests.find(r => r.id === requestId);
    if (!request) return;

    const lang = state.languages.find(l => l.file === request.languageFile);
    if (!lang) return;

    const entriesToApply = Array.isArray(editedEntries) && editedEntries.length > 0
      ? editedEntries.map(entry => ({
        ...entry,
        newTranslation: String(entry.newTranslation ?? ''),
      }))
      : request.entries;

    // Apply by original source-string keys against the latest file on disk.
    // Missing keys are skipped by the backend, so external additions/removals
    // cannot overwrite the wrong rows.
    const applyResult = await applyTranslationChangesToServer(
      request.languageFile,
      entriesToApply.map(entry => ({
        original: entry.original,
        newTranslation: entry.newTranslation,
      }))
    );

    // If none of the changed keys exists in the latest file, do not mark the
    // request as approved and keep it pending for review.
    if (applyResult.applied === 0) {
      const syncedFile = parseTranslationFile(applyResult.content, lang);
      const syncedTranslations = {
        ...state.translations,
        [request.languageFile]: syncedFile,
      };
      const syncedVersions = {
        ...state.fileVersions,
        [request.languageFile]: applyResult.version || Date.now(),
      };
      set({ translations: syncedTranslations, fileVersions: syncedVersions });
      saveState({ ...state, translations: syncedTranslations, fileVersions: syncedVersions } as AppState);
      return;
    }

    const newFile = parseTranslationFile(applyResult.content, lang);

    const newTranslations = {
      ...state.translations,
      [request.languageFile]: newFile,
    };

    const newRequests = state.changeRequests.map(r =>
      r.id === requestId ? { ...r, entries: entriesToApply, status: 'approved' as const, resolvedAt: new Date().toISOString() } : r
    );

    const appliedKeys = new Set(applyResult.appliedKeys || []);
    const acceptedEntries = appliedKeys.size > 0
      ? entriesToApply.filter(entry => appliedKeys.has(entry.original))
      : entriesToApply.slice(0, applyResult.applied);
    const translatedTextAmount = getTranslatedTextAmount(acceptedEntries);

    const topMap = new Map<string, number>();
    for (const contributor of state.topContributors) {
      topMap.set(contributor.author, contributor.count);
    }
    if (translatedTextAmount > 0) {
      topMap.set(request.author, (topMap.get(request.author) || 0) + translatedTextAmount);
    }
    const newTopContributors: TopContributor[] = Array.from(topMap.entries())
      .map(([author, count]) => ({ author, count }))
      .filter(item => item.count > 0)
      .sort((a, b) => b.count - a.count)
      .slice(0, 100);

    const newFileVersions = {
      ...state.fileVersions,
      [request.languageFile]: applyResult.version || Date.now(),
    };

    set({ translations: newTranslations, changeRequests: newRequests, topContributors: newTopContributors, fileVersions: newFileVersions });
    saveState({ ...state, translations: newTranslations, fileVersions: newFileVersions } as AppState);
    updateChangeRequestOnServer(requestId, { entries: entriesToApply, status: 'approved', resolvedAt: new Date().toISOString() }).then(serverRequests => {
      if (serverRequests.length > 0) set({ changeRequests: serverRequests });
    }).catch(console.error);
    saveTopContributorsToServer(newTopContributors).catch(console.error);
  },

  rejectRequest: async (requestId) => {
    const state = get();
    const resolvedAt = new Date().toISOString();
    const optimisticRequests = state.changeRequests.map(r =>
      r.id === requestId ? { ...r, status: 'rejected' as const, resolvedAt } : r
    );
    set({ changeRequests: optimisticRequests });
    try {
      const changeRequests = await updateChangeRequestOnServer(requestId, { status: 'rejected', resolvedAt });
      if (changeRequests.length > 0) set({ changeRequests });
    } catch (error) {
      console.error(error);
      set({ changeRequests: state.changeRequests });
      throw error;
    }
  },

  deleteRequest: async (requestId) => {
    const state = get();
    const optimisticRequests = state.changeRequests.filter(r => r.id !== requestId);
    set({ changeRequests: optimisticRequests });
    try {
      const changeRequests = await deleteChangeRequestOnServer(requestId);
      set({ changeRequests });
    } catch (error) {
      console.error(error);
      set({ changeRequests: state.changeRequests });
      throw error;
    }
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
    const languages = normalizeLocalizationLanguages(result.languages);
    for (const lang of languages) {
      const content = result.files[lang.file] || '';
      translations[lang.file] = parseTranslationFile(content, lang);
    }
    const state = get();
    const fileVersions = result.versions || state.fileVersions || {};
    set({ languages, translations, fileVersions, selectedLanguage: null });
    saveState({ ...state, languages, translations, fileVersions } as AppState);
  },

  hasServerChanges: (result: LoadResult) => {
    const languages = normalizeLocalizationLanguages(result.languages);
    if (languages.length === 0) return false;
    const state = get();
    const incomingVersions = result.versions || {};
    return getVersionSignature(languages, incomingVersions) !== getVersionSignature(state.languages, state.fileVersions || {});
  },

  refreshFiles: (result: LoadResult) => {
    const languages = normalizeLocalizationLanguages(result.languages);
    if (languages.length === 0) return;
    const state = get();
    const incomingVersions = result.versions || {};
    if (getVersionSignature(languages, incomingVersions) === getVersionSignature(state.languages, state.fileVersions || {})) return;

    const translations: Record<string, TranslationFile> = {};
    for (const lang of languages) {
      const content = result.files[lang.file] || '';
      translations[lang.file] = parseTranslationFile(content, lang);
    }

    const selectedLanguage = state.selectedLanguage && languages.some(lang => lang.file === state.selectedLanguage)
      ? state.selectedLanguage
      : null;

    set({ languages, translations, fileVersions: incomingVersions, selectedLanguage });
    saveState({ ...state, languages, translations, fileVersions: incomingVersions, selectedLanguage } as AppState);
  },


  forceSyncFiles: (result: LoadResult) => {
    const languages = normalizeLocalizationLanguages(result.languages);
    if (languages.length === 0) return;
    const state = get();
    const translations: Record<string, TranslationFile> = {};
    for (const lang of languages) {
      const content = result.files[lang.file] || '';
      translations[lang.file] = parseTranslationFile(content, lang);
    }

    const selectedLanguage = state.selectedLanguage && languages.some(lang => lang.file === state.selectedLanguage)
      ? state.selectedLanguage
      : null;
    const fileVersions = result.versions || state.fileVersions || {};

    set({ languages, translations, fileVersions, selectedLanguage });
    saveState({ ...state, languages, translations, fileVersions, selectedLanguage } as AppState);
  },

  loadTopContributors: async () => {
    try {
      const topContributors = await loadTopContributorsFromServer();
      if (topContributors.length === 0) return;
      set({ topContributors });
    } catch {
      // Server-side top file is optional.
    }
  },

  loadChangeRequests: async () => {
    try {
      const changeRequests = await loadChangeRequestsFromServer();
      const topContributors = buildTopContributorsFromRequests(changeRequests);
      set({ changeRequests, topContributors: topContributors.length > 0 ? topContributors : get().topContributors });
      if (topContributors.length > 0) {
        saveTopContributorsToServer(topContributors).catch(console.error);
      }
    } catch (error) {
      console.error(error);
    }
  },
}));
