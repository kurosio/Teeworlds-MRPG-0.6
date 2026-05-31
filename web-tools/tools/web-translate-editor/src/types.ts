import type { UILanguage } from './i18n';
export type { UILanguage };

export interface LanguageIndex {
  file: string;
  name: string;
}

export interface LanguageIndices {
  "language indices": LanguageIndex[];
}

export interface TranslationEntry {
  id: string;
  original: string;
  translation: string;
  lineIndex: number;
}

export interface TranslationFile {
  language: LanguageIndex;
  entries: TranslationEntry[];
  rawContent: string;
}

export interface TopContributor {
  author: string;
  count: number;
}

export interface ChangeRequestEntry {
  entryId: string;
  original: string;
  oldTranslation: string;
  newTranslation: string;
}

export interface ChangeRequest {
  id: string;
  name: string;
  author: string;
  languageFile: string;
  languageName: string;
  entries: ChangeRequestEntry[];
  status: 'pending' | 'approved' | 'rejected';
  createdAt: string;
  resolvedAt?: string;
}

export interface Settings {
  translationPath: string;
}

// Admin credentials from environment (read-only)
export interface AdminConfig {
  username: string;
  passwordHash: string;
}

export interface AppState {
  settings: Settings;
  languages: LanguageIndex[];
  translations: Record<string, TranslationFile>;
  fileVersions: Record<string, number>;
  changeRequests: ChangeRequest[];
  topContributors: TopContributor[];
  isAdmin: boolean;
  activeTab: 'editor' | 'requests' | 'settings';
  selectedLanguage: string | null;
  searchQuery: string;
  filterMode: 'all' | 'translated' | 'untranslated' | 'changed';
  requestFilterTag: string | null;
  uiLanguage: UILanguage;
}
