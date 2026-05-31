import type { LanguageIndex, TopContributor } from '../types';

export interface LoadResult {
  languages: LanguageIndex[];
  files: Record<string, string>;
  errors: string[];
  versions?: Record<string, number>;
}

/**
 * Parse index.json content
 */
export function parseIndexJson(content: string): { languages: LanguageIndex[]; error?: string } {
  try {
    const parsed = JSON.parse(content);
    if (!parsed['language indices'] || !Array.isArray(parsed['language indices'])) {
      return { languages: [], error: 'index.json: missing or invalid "language indices" array' };
    }
    const languages: LanguageIndex[] = [];
    for (const item of parsed['language indices']) {
      if (!item.file || !item.name) {
        continue;
      }
      languages.push({ file: item.file, name: item.name });
    }
    return { languages };
  } catch (e) {
    return { languages: [], error: `index.json: ${(e as Error).message}` };
  }
}

/**
 * Load translation files from the Node.js backend.
 * Backend reads TRANSLATION_ROOT, for example /root/mmorpg/server_lang.
 */
export async function loadFromServerApi(): Promise<LoadResult> {
  const result: LoadResult = { languages: [], files: {}, errors: [] };

  try {
    const apiBase = (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');
    const response = await fetch(`${apiBase}/api/translations`);
    const payload = await response.json().catch(() => null);

    if (!response.ok) {
      result.errors.push(payload?.error || `Server error (${response.status})`);
      return result;
    }

    result.languages = Array.isArray(payload.languages) ? payload.languages : [];
    result.files = payload.files || {};
    result.errors = Array.isArray(payload.errors) ? payload.errors : [];
    result.versions = payload.versions || {};
    return result;
  } catch (e) {
    result.errors.push(`API connection error: ${(e as Error).message}. Start the backend with npm run server.`);
    return result;
  }
}


/**
 * Load only language list and file versions from the backend.
 * Used for cheap polling so large localization files are not fetched every few seconds.
 */
export async function loadServerManifest(): Promise<LoadResult> {
  const result: LoadResult = { languages: [], files: {}, errors: [] };

  try {
    const apiBase = (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');
    const response = await fetch(`${apiBase}/api/translations/manifest`);
    const payload = await response.json().catch(() => null);

    if (!response.ok) {
      result.errors.push(payload?.error || `Server error (${response.status})`);
      return result;
    }

    result.languages = Array.isArray(payload.languages) ? payload.languages : [];
    result.versions = payload.versions || {};
    return result;
  } catch (e) {
    result.errors.push(`API connection error: ${(e as Error).message}. Start the backend with npm run server.`);
    return result;
  }
}

/**
 * Save one translation file through the Node.js backend.
 */
export async function saveTranslationToServer(file: string, content: string): Promise<void> {
  const apiBase = (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');
  const response = await fetch(`${apiBase}/api/translations/${encodeURIComponent(file)}`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ content }),
  });

  if (!response.ok) {
    const payload = await response.json().catch(() => null);
    throw new Error(payload?.error || `Failed to save ${file}.txt (${response.status})`);
  }
}


/**
 * Apply changed translations by original source string. The backend rereads the
 * current file from disk, updates only keys that still exist, ignores missing
 * keys, and returns the synchronized file content.
 */
export async function applyTranslationChangesToServer(
  file: string,
  changes: Array<{ original: string; newTranslation: string }>
): Promise<{ content: string; applied: number; skipped: number; version?: number; appliedKeys?: string[] }> {
  const apiBase = (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');
  const response = await fetch(`${apiBase}/api/translations/${encodeURIComponent(file)}/apply`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ changes }),
  });

  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error || `Failed to apply changes to ${file}.txt (${response.status})`);
  }

  return {
    content: String(payload?.content ?? ''),
    applied: Number(payload?.applied) || 0,
    skipped: Number(payload?.skipped) || 0,
    version: typeof payload?.version === 'number' ? payload.version : undefined,
    appliedKeys: Array.isArray(payload?.appliedKeys) ? payload.appliedKeys.map(String) : [],
  };
}


export async function loadTopContributorsFromServer(): Promise<TopContributor[]> {
  const apiBase = (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');
  const response = await fetch(`${apiBase}/api/top-contributors`);
  if (!response.ok) return [];
  const payload = await response.json().catch(() => null);
  return Array.isArray(payload?.topContributors) ? payload.topContributors : [];
}

export async function saveTopContributorsToServer(topContributors: TopContributor[]): Promise<void> {
  const apiBase = (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');
  const response = await fetch(`${apiBase}/api/top-contributors`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ topContributors }),
  });
  if (!response.ok) {
    const payload = await response.json().catch(() => null);
    throw new Error(payload?.error || `Failed to save top contributors (${response.status})`);
  }
}
