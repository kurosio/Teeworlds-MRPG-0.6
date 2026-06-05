import { create } from 'zustand';
import { v4 as uuidv4 } from 'uuid';
import type { AppState, Settings, LanguageIndex, TranslationFile, TranslationEntry, ChangeRequest, ChangeRequestEntry, UILanguage, UIThemeId, AdminConfig, TopContributor, GlobalEventSettings } from './types';
import { setUILanguage as setI18nLanguage } from './i18n';
import { sha256 } from './utils/crypto';
import type { LoadResult } from './utils/fileLoader';
import { applyUITheme, normalizeThemeId } from './themes';
import { loadTopContributorsFromServer, loadChangeRequestsFromServer, createChangeRequestOnServer, updateChangeRequestOnServer, deleteChangeRequestOnServer, approveChangeRequestOnServer, loadEventSettingsFromServer, saveEventSettingsToServer, resetTopContributorsOnServer } from './utils/fileLoader';

const DEFAULT_SETTINGS: Settings = {
  translationPath: '/translations',
};

const DEFAULT_EVENT_SETTINGS: GlobalEventSettings = {
  translationRewards: {
    enabled: false,
    endDate: '',
    rewards: '',
  },
};

// Admin credentials from .env (read at build time)
const ADMIN_CONFIG: AdminConfig = {
  username: import.meta.env.VITE_ADMIN_USERNAME || 'admin',
  passwordHash: import.meta.env.VITE_ADMIN_PASSWORD_HASH || '8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918', // default: "admin"
};


const ADMIN_SESSION_STORAGE_KEY = 'translationEditorAdminSession';

type StoredAdminSession = {
  username?: string;
  passwordHash?: string;
  createdAt?: string;
};

function isValidStoredAdminSession(): boolean {
  try {
    const raw = localStorage.getItem(ADMIN_SESSION_STORAGE_KEY);
    if (!raw) return false;
    const session = JSON.parse(raw) as StoredAdminSession;
    return session.username === ADMIN_CONFIG.username && session.passwordHash === ADMIN_CONFIG.passwordHash;
  } catch {
    localStorage.removeItem(ADMIN_SESSION_STORAGE_KEY);
    return false;
  }
}

function saveAdminSession(username: string, passwordHash: string) {
  localStorage.setItem(ADMIN_SESSION_STORAGE_KEY, JSON.stringify({
    username,
    passwordHash,
    createdAt: new Date().toISOString(),
  }));
}

function clearAdminSession() {
  localStorage.removeItem(ADMIN_SESSION_STORAGE_KEY);
}

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

function waitForUiFrame(): Promise<void> {
  return new Promise(resolve => {
    if (typeof window !== 'undefined' && 'requestAnimationFrame' in window) {
      window.requestAnimationFrame(() => resolve());
      return;
    }
    setTimeout(resolve, 0);
  });
}

function makeLoadingStatus(languages: LanguageIndex[], status: 'loading' | 'loaded' | 'error' = 'loading'): Record<string, 'loading' | 'loaded' | 'error'> {
  return Object.fromEntries(normalizeLocalizationLanguages(languages).map(lang => [lang.file, status]));
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

function isLeaderboardSystemAuthor(author: string): boolean {
  return String(author || '').trim().toLowerCase() === 'administrator';
}

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
    if (isLeaderboardSystemAuthor(request.author)) continue;
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
        eventSettings: parsed.eventSettings || DEFAULT_EVENT_SETTINGS,
        fileVersions: parsed.fileVersions || {},
        isAdmin: isValidStoredAdminSession(),
        uiLanguage: parsed.uiLanguage || 'en',
        uiTheme: normalizeThemeId(parsed.uiTheme),
      filesLoading: false,
      fileLoadStatus: {},
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
    eventSettings: DEFAULT_EVENT_SETTINGS,
    fileVersions: {},
    isAdmin: isValidStoredAdminSession(),
    uiLanguage: 'en',
    uiTheme: 'ocean',
    filesLoading: false,
    fileLoadStatus: {},
  };
}


function saveState(state: AppState) {
  // Do not persist raw localization files in localStorage. Large projects can contain
  // thousands of rows and serializing them on every sync/edit causes UI freezes.
  localStorage.setItem('translationEditor', JSON.stringify({
    settings: state.settings,
    languages: state.languages,
    fileVersions: state.fileVersions,
    uiLanguage: state.uiLanguage,
    uiTheme: state.uiTheme,
    eventSettings: state.eventSettings,
  }));
}

interface StoreActions {
  setActiveTab: (tab: AppState['activeTab']) => void;
  setSelectedLanguage: (lang: string | null) => void;
  setSearchQuery: (query: string) => void;
  setFilterMode: (mode: AppState['filterMode']) => void;
  setRequestFilterTag: (tag: string | null) => void;
  setUiLanguage: (lang: UILanguage) => void;
  setUiTheme: (theme: UIThemeId) => void;
  updateSettings: (settings: Settings) => void;
  login: (username: string, password: string) => Promise<boolean>;
  logout: () => void;
  submitChangeRequest: (name: string, author: string, languageFile: string, changes: ChangeRequestEntry[]) => Promise<ChangeRequest | null>;
  approveRequest: (requestId: string, editedEntries?: ChangeRequestEntry[]) => Promise<void>;
  rejectRequest: (requestId: string) => Promise<void>;
  deleteRequest: (requestId: string) => Promise<void>;
  resetData: () => void;
  beginFileLoading: (result: LoadResult) => void;
  loadFiles: (result: LoadResult) => Promise<void>;
  refreshFiles: (result: LoadResult) => void;
  forceSyncFiles: (result: LoadResult) => void;
  loadTopContributors: () => Promise<void>;
  loadChangeRequests: () => Promise<void>;
  loadEventSettings: () => Promise<void>;
  saveEventSettings: (settings: GlobalEventSettings) => Promise<void>;
  resetTopContributors: () => Promise<void>;
  hasServerChanges: (result: LoadResult) => boolean;
}

const initial = loadInitialState();
setI18nLanguage((initial.uiLanguage || 'en') as UILanguage);
applyUITheme(normalizeThemeId(initial.uiTheme));

export const useStore = create<AppState & StoreActions>((set, get) => ({
  settings: initial.settings || DEFAULT_SETTINGS,
  languages: initial.languages || [],
  translations: initial.translations || {},
  fileVersions: initial.fileVersions || {},
  changeRequests: initial.changeRequests || [],
  topContributors: initial.topContributors || [],
  eventSettings: initial.eventSettings || DEFAULT_EVENT_SETTINGS,
  isAdmin: initial.isAdmin || false,
  activeTab: 'editor',
  selectedLanguage: null,
  searchQuery: '',
  uiLanguage: (initial.uiLanguage || 'en') as UILanguage,
  uiTheme: normalizeThemeId(initial.uiTheme),
  filesLoading: false,
  fileLoadStatus: {},
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
    saveState({ ...get(), uiLanguage: lang } as AppState);
  },

  setUiTheme: (theme) => {
    const nextTheme = normalizeThemeId(theme);
    applyUITheme(nextTheme);
    set({ uiTheme: nextTheme });
    saveState({ ...get(), uiTheme: nextTheme } as AppState);
  },

  updateSettings: (settings) => {
    set({ settings });
    const state = get();
    saveState(state as AppState);
  },

  login: async (username, password) => {
    const passwordHash = await sha256(password);
    if (username === ADMIN_CONFIG.username && passwordHash === ADMIN_CONFIG.passwordHash) {
      saveAdminSession(username, passwordHash);
      set({ isAdmin: true });
      return true;
    }
    return false;
  },

  logout: () => {
    clearAdminSession();
    set({ isAdmin: false });
  },

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
      const result = await createChangeRequestOnServer(request);
      set({
        changeRequests: result.changeRequests.length > 0 ? result.changeRequests : [request, ...state.changeRequests],
        ...(result.topContributors.length > 0 ? { topContributors: result.topContributors } : {}),
        ...(result.eventSettings ? { eventSettings: result.eventSettings } : {}),
      });
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

    // Approval is performed by the backend as a single global operation:
    // 1) reread the current translation file from disk,
    // 2) apply only keys that still exist,
    // 3) update the global request history,
    // 4) recompute and save the global top list.
    const serverResult = await approveChangeRequestOnServer(requestId, entriesToApply);
    const applyResult = serverResult.applyResult;

    if (applyResult.content) {
      const newFile = parseTranslationFile(applyResult.content, lang);
      const newTranslations = {
        ...state.translations,
        [request.languageFile]: newFile,
      };
      const newFileVersions = {
        ...state.fileVersions,
        [request.languageFile]: applyResult.version || Date.now(),
      };
      set({ translations: newTranslations, fileVersions: newFileVersions });
      saveState({ ...state, translations: newTranslations, fileVersions: newFileVersions } as AppState);
    }

    if (serverResult.changeRequests.length > 0) {
      set({ changeRequests: serverResult.changeRequests });
    }
    if (serverResult.topContributors.length > 0 || serverResult.approved) {
      set({ topContributors: serverResult.topContributors, ...(serverResult.eventSettings ? { eventSettings: serverResult.eventSettings } : {}) });
    }
  },

  rejectRequest: async (requestId) => {
    const state = get();
    const resolvedAt = new Date().toISOString();
    const optimisticRequests = state.changeRequests.map(r =>
      r.id === requestId ? { ...r, status: 'rejected' as const, resolvedAt } : r
    );
    set({ changeRequests: optimisticRequests });
    try {
      const result = await updateChangeRequestOnServer(requestId, { status: 'rejected', resolvedAt });
      set({
        changeRequests: result.changeRequests,
        topContributors: result.topContributors.length > 0 ? result.topContributors : buildTopContributorsFromRequests(result.changeRequests),
        ...(result.eventSettings ? { eventSettings: result.eventSettings } : {}),
      });
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
      const result = await deleteChangeRequestOnServer(requestId);
      set({
        changeRequests: result.changeRequests,
        topContributors: result.topContributors.length > 0 ? result.topContributors : buildTopContributorsFromRequests(result.changeRequests),
        ...(result.eventSettings ? { eventSettings: result.eventSettings } : {}),
      });
    } catch (error) {
      console.error(error);
      set({ changeRequests: state.changeRequests });
      throw error;
    }
  },

  resetData: () => {
    const current = get();
    localStorage.removeItem('translationEditor');
    const fresh = loadInitialState();
    set({
      settings: fresh.settings,
      languages: fresh.languages,
      translations: fresh.translations,
      changeRequests: [],
      uiLanguage: current.uiLanguage,
      uiTheme: current.uiTheme,
      filesLoading: false,
      fileLoadStatus: {},
    });
    saveState({ ...current, settings: fresh.settings || DEFAULT_SETTINGS, languages: fresh.languages || [], translations: fresh.translations || {}, changeRequests: [] } as AppState);
  },

  beginFileLoading: (result: LoadResult) => {
    const languages = normalizeLocalizationLanguages(result.languages);
    if (languages.length === 0) return;
    set({
      languages,
      filesLoading: true,
      fileLoadStatus: makeLoadingStatus(languages, 'loading'),
      selectedLanguage: get().selectedLanguage && languages.some(lang => lang.file === get().selectedLanguage) ? get().selectedLanguage : languages[0]?.file || null,
    });
  },

  loadFiles: async (result: LoadResult) => {
    const translations: Record<string, TranslationFile> = {};
    const languages = normalizeLocalizationLanguages(result.languages);
    const state = get();
    const fileVersions = result.versions || state.fileVersions || {};

    if (languages.length === 0) {
      set({ filesLoading: false, fileLoadStatus: {} });
      return;
    }

    set({
      languages,
      filesLoading: true,
      fileLoadStatus: makeLoadingStatus(languages, 'loading'),
      ...(Array.isArray(result.changeRequests) ? { changeRequests: result.changeRequests } : {}),
      ...(Array.isArray(result.topContributors) ? { topContributors: result.topContributors } : {}),
      ...(result.eventSettings ? { eventSettings: result.eventSettings } : {}),
    });

    const batchSize = languages.length > 16 ? 6 : 4;
    for (let start = 0; start < languages.length; start += batchSize) {
      await waitForUiFrame();
      const batch = languages.slice(start, start + batchSize);
      const batchTranslations: Record<string, TranslationFile> = {};
      const batchStatus: Record<string, 'loaded'> = {};

      for (const lang of batch) {
        const content = result.files[lang.file] || '';
        const parsed = parseTranslationFile(content, lang);
        translations[lang.file] = parsed;
        batchTranslations[lang.file] = parsed;
        batchStatus[lang.file] = 'loaded';
      }

      set(current => ({
        translations: { ...current.translations, ...batchTranslations },
        fileLoadStatus: { ...current.fileLoadStatus, ...batchStatus },
      }));
    }

    const selectedLanguage = state.selectedLanguage && languages.some(lang => lang.file === state.selectedLanguage)
      ? state.selectedLanguage
      : languages[0]?.file || null;

    const finalStatus = makeLoadingStatus(languages, 'loaded');
    const finalStatePatch = {
      languages,
      translations,
      fileVersions,
      selectedLanguage,
      filesLoading: false,
      fileLoadStatus: finalStatus,
      ...(Array.isArray(result.changeRequests) ? { changeRequests: result.changeRequests } : {}),
      ...(Array.isArray(result.topContributors) ? { topContributors: result.topContributors } : {}),
      ...(result.eventSettings ? { eventSettings: result.eventSettings } : {}),
    };

    set(finalStatePatch);
    saveState({ ...state, ...finalStatePatch } as AppState);
  },

  hasServerChanges: (result: LoadResult) => {
    const languages = normalizeLocalizationLanguages(result.languages);
    if (languages.length === 0) return false;
    const state = get();
    const incomingVersions = result.versions || {};
    return getVersionSignature(languages, incomingVersions) !== getVersionSignature(state.languages, state.fileVersions || {});
  },

  refreshFiles: async (result: LoadResult) => {
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
      : languages[0]?.file || null;

    set({ languages, translations, fileVersions: incomingVersions, selectedLanguage, filesLoading: false, fileLoadStatus: makeLoadingStatus(languages, 'loaded') });
    saveState({ ...state, languages, translations, fileVersions: incomingVersions, selectedLanguage, filesLoading: false, fileLoadStatus: makeLoadingStatus(languages, 'loaded') } as AppState);
  },


  forceSyncFiles: async (result: LoadResult) => {
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
      : languages[0]?.file || null;
    const fileVersions = result.versions || state.fileVersions || {};

    set({ languages, translations, fileVersions, selectedLanguage, filesLoading: false, fileLoadStatus: makeLoadingStatus(languages, 'loaded') });
    saveState({ ...state, languages, translations, fileVersions, selectedLanguage, filesLoading: false, fileLoadStatus: makeLoadingStatus(languages, 'loaded') } as AppState);
  },

  loadTopContributors: async () => {
    try {
      const topContributors = await loadTopContributorsFromServer();
      set({ topContributors });
    } catch (error) {
      console.error(error);
      // Keep the current global top in memory during temporary connection loss.
    }
  },

  loadChangeRequests: async () => {
    try {
      const { changeRequests, topContributors, eventSettings } = await loadChangeRequestsFromServer();
      set({
        changeRequests,
        topContributors: topContributors.length > 0 ? topContributors : buildTopContributorsFromRequests(changeRequests),
        ...(eventSettings ? { eventSettings } : {}),
      });
    } catch (error) {
      console.error(error);
      // Do not clear the currently displayed global requests/top during a
      // temporary connection loss or a failed server response.
    }
  },

  loadEventSettings: async () => {
    try {
      const eventSettings = await loadEventSettingsFromServer();
      set({ eventSettings: eventSettings || DEFAULT_EVENT_SETTINGS });
      saveState({ ...get(), eventSettings: eventSettings || DEFAULT_EVENT_SETTINGS } as AppState);
    } catch (error) {
      console.error(error);
    }
  },

  saveEventSettings: async (eventSettings) => {
    const result = await saveEventSettingsToServer(eventSettings);
    set({ eventSettings: result.eventSettings || eventSettings, topContributors: result.topContributors });
    saveState({ ...get(), eventSettings: result.eventSettings || eventSettings, topContributors: result.topContributors } as AppState);
  },

  resetTopContributors: async () => {
    const result = await resetTopContributorsOnServer();
    set({ topContributors: result.topContributors, eventSettings: result.eventSettings });
    saveState({ ...get(), topContributors: result.topContributors, eventSettings: result.eventSettings } as AppState);
  },
}));
