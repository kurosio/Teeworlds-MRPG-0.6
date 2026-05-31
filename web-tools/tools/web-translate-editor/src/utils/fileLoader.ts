import type { LanguageIndex } from '../types';

export interface LoadResult {
  languages: LanguageIndex[];
  files: Record<string, string>;
  errors: string[];
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
 * Load files from a directory handle (File System Access API)
 */
export async function loadFromDirectory(): Promise<LoadResult> {
  const result: LoadResult = { languages: [], files: {}, errors: [] };

  try {
    // @ts-expect-error File System Access API
    const dirHandle = await window.showDirectoryPicker({ mode: 'read' });
    let indexContent = '';

    // First pass: find index.json
    for await (const [name, handle] of (dirHandle as any).entries()) {
      if (name === 'index.json' && handle.kind === 'file') {
        const file = await handle.getFile();
        indexContent = await file.text();
      }
    }

    if (!indexContent) {
      result.errors.push('index.json not found in the selected directory');
      return result;
    }

    const parsed = parseIndexJson(indexContent);
    if (parsed.error) {
      result.errors.push(parsed.error);
      return result;
    }
    result.languages = parsed.languages;

    // Second pass: load translation files
    const fileMap = new Map<string, any>();
    for await (const [name, handle] of (dirHandle as any).entries()) {
      if (handle.kind === 'file') {
        fileMap.set(name, handle);
      }
    }

    for (const lang of result.languages) {
      const fileName = `${lang.file}.txt`;
      const handle = fileMap.get(fileName);
      if (handle) {
        const file = await handle.getFile();
        result.files[lang.file] = await file.text();
      } else {
        result.errors.push(`File not found: ${fileName}`);
        result.files[lang.file] = '';
      }
    }

    return result;
  } catch (e) {
    if ((e as Error).name !== 'AbortError') {
      result.errors.push(`Directory picker: ${(e as Error).message}`);
    }
    return result;
  }
}

/**
 * Load files by fetching from a URL path (for files in public/ folder)
 */
export async function loadFromUrlPath(basePath: string): Promise<LoadResult> {
  const result: LoadResult = { languages: [], files: {}, errors: [] };

  // Normalize path
  const normalized = basePath.replace(/\/$/, '');

  try {
    // Fetch index.json
    const indexResponse = await fetch(`${normalized}/index.json`);
    if (!indexResponse.ok) {
      result.errors.push(`Failed to load index.json from ${normalized}/index.json (${indexResponse.status})`);
      return result;
    }

    const indexContent = await indexResponse.text();
    const parsed = parseIndexJson(indexContent);
    if (parsed.error) {
      result.errors.push(parsed.error);
      return result;
    }
    result.languages = parsed.languages;

    // Fetch all translation files
    for (const lang of result.languages) {
      const filePath = `${normalized}/${lang.file}.txt`;
      try {
        const response = await fetch(filePath);
        if (response.ok) {
          result.files[lang.file] = await response.text();
        } else {
          result.errors.push(`Failed to load ${filePath} (${response.status})`);
          result.files[lang.file] = '';
        }
      } catch {
        result.errors.push(`Network error loading ${filePath}`);
        result.files[lang.file] = '';
      }
    }

    return result;
  } catch (e) {
    result.errors.push(`Fetch error: ${(e as Error).message}`);
    return result;
  }
}

/**
 * Load files from FileList (manual upload via input[type=file])
 */
export async function loadFromFileList(files: FileList | File[]): Promise<LoadResult> {
  const result: LoadResult = { languages: [], files: {}, errors: [] };
  const fileMap = new Map<string, string>();

  // Read all files
  for (const file of Array.from(files)) {
    const content = await file.text();
    fileMap.set(file.name, content);
  }

  // Parse index.json
  const indexContent = fileMap.get('index.json');
  if (!indexContent) {
    result.errors.push('index.json not found in uploaded files');
    return result;
  }

  const parsed = parseIndexJson(indexContent);
  if (parsed.error) {
    result.errors.push(parsed.error);
    return result;
  }
  result.languages = parsed.languages;

  // Read translation files
  for (const lang of result.languages) {
    const fileName = `${lang.file}.txt`;
    const content = fileMap.get(fileName);
    if (content !== undefined) {
      result.files[lang.file] = content;
    } else {
      result.errors.push(`File not found in upload: ${fileName}`);
      result.files[lang.file] = '';
    }
  }

  return result;
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
