import type { LanguageIndex, TopContributor, ChangeRequest, GlobalEventSettings } from '../types';

export interface LoadResult {
  languages: LanguageIndex[];
  files: Record<string, string>;
  errors: string[];
  versions?: Record<string, number>;
  changeRequests?: ChangeRequest[];
  topContributors?: TopContributor[];
  eventSettings?: GlobalEventSettings;
  loadedAt?: string;
}

export interface GlobalRequestsResult {
  changeRequests: ChangeRequest[];
  topContributors: TopContributor[];
  eventSettings?: GlobalEventSettings;
}


function apiBaseUrl(): string {
  return (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');
}

async function fetchJsonWithTimeout(url: string, init: RequestInit = {}, timeoutMs = 45000): Promise<Response> {
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, {
      cache: 'no-store',
      ...init,
      signal: init.signal || controller.signal,
      headers: {
        'Accept': 'application/json',
        ...(init.headers || {}),
      },
    });
  } finally {
    window.clearTimeout(timeout);
  }
}

export interface ApproveChangeRequestResult {
  approved: boolean;
  reason?: string;
  changeRequests: ChangeRequest[];
  topContributors: TopContributor[];
  eventSettings?: GlobalEventSettings;
  applyResult: {
    content: string;
    applied: number;
    skipped: number;
    version?: number;
    appliedKeys?: string[];
  };
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
    const apiBase = apiBaseUrl();
    const response = await fetchJsonWithTimeout(`${apiBase}/api/bootstrap`);
    const payload = await response.json().catch(() => null);

    if (!response.ok) {
      result.errors.push(payload?.error || `Server error (${response.status})`);
      return result;
    }

    result.languages = Array.isArray(payload.languages) ? payload.languages : [];
    result.files = payload.files || {};
    result.errors = Array.isArray(payload.errors) ? payload.errors : [];
    result.versions = payload.versions || {};
    result.changeRequests = Array.isArray(payload.changeRequests) ? payload.changeRequests : undefined;
    result.topContributors = Array.isArray(payload.topContributors) ? payload.topContributors : undefined;
    result.eventSettings = payload.eventSettings || undefined;
    result.loadedAt = payload.loadedAt ? String(payload.loadedAt) : undefined;
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
    const apiBase = apiBaseUrl();
    const response = await fetchJsonWithTimeout(`${apiBase}/api/translations/manifest`, {}, 20000);
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
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/translations/${encodeURIComponent(file)}`, {
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
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/translations/${encodeURIComponent(file)}/apply`, {
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
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/top-contributors`);
  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error || `Failed to load global top contributors (${response.status})`);
  }
  if (!Array.isArray(payload?.topContributors)) {
    throw new Error('Invalid global top contributors response');
  }
  return payload.topContributors;
}

export async function saveTopContributorsToServer(_topContributors: TopContributor[]): Promise<void> {
  throw new Error('Top contributors are global and are recalculated from approved requests on the server.');
}


export async function loadChangeRequestsFromServer(): Promise<GlobalRequestsResult> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/change-requests`, {}, 30000);
  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error || `Failed to load global change requests (${response.status})`);
  }
  if (!Array.isArray(payload?.changeRequests)) {
    throw new Error('Invalid global change requests response');
  }
  return {
    changeRequests: payload.changeRequests,
    topContributors: Array.isArray(payload?.topContributors) ? payload.topContributors : [],
    eventSettings: payload?.eventSettings || undefined,
  };
}

export async function createChangeRequestOnServer(request: ChangeRequest): Promise<GlobalRequestsResult> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/change-requests`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(request),
  });
  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error || `Failed to create change request (${response.status})`);
  }
  if (!Array.isArray(payload?.changeRequests)) {
    throw new Error('Invalid create change request response');
  }
  return {
    changeRequests: payload.changeRequests,
    topContributors: Array.isArray(payload?.topContributors) ? payload.topContributors : [],
    eventSettings: payload?.eventSettings || undefined,
  };
}

export async function updateChangeRequestOnServer(requestId: string, patch: Partial<ChangeRequest>): Promise<GlobalRequestsResult> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/change-requests/${encodeURIComponent(requestId)}`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(patch),
  });
  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error || `Failed to update change request (${response.status})`);
  }
  if (!Array.isArray(payload?.changeRequests)) {
    throw new Error('Invalid update change request response');
  }
  return {
    changeRequests: payload.changeRequests,
    topContributors: Array.isArray(payload?.topContributors) ? payload.topContributors : [],
    eventSettings: payload?.eventSettings || undefined,
  };
}

export async function approveChangeRequestOnServer(
  requestId: string,
  entries: ChangeRequest['entries']
): Promise<ApproveChangeRequestResult> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/change-requests/${encodeURIComponent(requestId)}/approve`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ entries }),
  });
  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error || `Failed to approve change request (${response.status})`);
  }

  if (!Array.isArray(payload?.changeRequests)) {
    throw new Error('Invalid approve request response');
  }
  if (!Array.isArray(payload?.topContributors)) {
    throw new Error('Invalid approve top contributors response');
  }

  return {
    approved: Boolean(payload?.approved),
    reason: payload?.reason ? String(payload.reason) : undefined,
    changeRequests: payload.changeRequests,
    topContributors: payload.topContributors,
    eventSettings: payload?.eventSettings || undefined,
    applyResult: {
      content: String(payload?.applyResult?.content ?? ''),
      applied: Number(payload?.applyResult?.applied) || 0,
      skipped: Number(payload?.applyResult?.skipped) || 0,
      version: typeof payload?.applyResult?.version === 'number' ? payload.applyResult.version : undefined,
      appliedKeys: Array.isArray(payload?.applyResult?.appliedKeys) ? payload.applyResult.appliedKeys.map(String) : [],
    },
  };
}


export async function deleteChangeRequestOnServer(requestId: string): Promise<GlobalRequestsResult> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/change-requests/${encodeURIComponent(requestId)}`, {
    method: 'DELETE',
  });
  const payload = await response.json().catch(() => null);
  if (!response.ok) {
    throw new Error(payload?.error || `Failed to delete change request (${response.status})`);
  }
  if (!Array.isArray(payload?.changeRequests)) {
    throw new Error('Invalid delete change request response');
  }
  return {
    changeRequests: payload.changeRequests,
    topContributors: Array.isArray(payload?.topContributors) ? payload.topContributors : [],
    eventSettings: payload?.eventSettings || undefined,
  };
}


export async function loadEventSettingsFromServer(): Promise<GlobalEventSettings> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/event-settings`, {}, 20000);
  const payload = await response.json().catch(() => null);
  if (!response.ok) throw new Error(payload?.error || `Failed to load event settings (${response.status})`);
  return payload.eventSettings;
}

export async function saveEventSettingsToServer(eventSettings: GlobalEventSettings): Promise<{ eventSettings: GlobalEventSettings; topContributors: TopContributor[] }> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/event-settings`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ eventSettings }),
  });
  const payload = await response.json().catch(() => null);
  if (!response.ok) throw new Error(payload?.error || `Failed to save event settings (${response.status})`);
  return { eventSettings: payload.eventSettings, topContributors: Array.isArray(payload.topContributors) ? payload.topContributors : [] };
}

export async function resetTopContributorsOnServer(): Promise<{ eventSettings: GlobalEventSettings; topContributors: TopContributor[] }> {
  const apiBase = apiBaseUrl();
  const response = await fetchJsonWithTimeout(`${apiBase}/api/top-contributors/reset`, { method: 'POST' }, 20000);
  const payload = await response.json().catch(() => null);
  if (!response.ok) throw new Error(payload?.error || `Failed to reset top contributors (${response.status})`);
  return { eventSettings: payload.eventSettings, topContributors: Array.isArray(payload.topContributors) ? payload.topContributors : [] };
}
