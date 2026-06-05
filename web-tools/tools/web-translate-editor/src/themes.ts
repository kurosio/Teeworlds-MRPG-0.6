export const UI_THEME_OPTIONS = [
  { id: 'ocean', name: 'Ocean Blue', swatch: 'linear-gradient(135deg, #2563eb, #06b6d4)' },
  { id: 'violet', name: 'Violet', swatch: 'linear-gradient(135deg, #7c3aed, #ec4899)' },
  { id: 'emerald', name: 'Emerald', swatch: 'linear-gradient(135deg, #059669, #10b981)' },
  { id: 'sunset', name: 'Sunset', swatch: 'linear-gradient(135deg, #f97316, #e11d48)' },
  { id: 'rose', name: 'Rose', swatch: 'linear-gradient(135deg, #e11d48, #f472b6)' },
  { id: 'amber', name: 'Amber', swatch: 'linear-gradient(135deg, #f59e0b, #f97316)' },
  { id: 'cyan', name: 'Cyan', swatch: 'linear-gradient(135deg, #0891b2, #22d3ee)' },
  { id: 'indigo', name: 'Indigo', swatch: 'linear-gradient(135deg, #4f46e5, #8b5cf6)' },
  { id: 'slate', name: 'Slate', swatch: 'linear-gradient(135deg, #0f172a, #475569)' },
  { id: 'black', name: 'Black', swatch: 'linear-gradient(135deg, #020617, #18181b)' },
  { id: 'forest', name: 'Forest', swatch: 'linear-gradient(135deg, #166534, #65a30d)' },
  { id: 'grape', name: 'Grape', swatch: 'linear-gradient(135deg, #6d28d9, #9333ea)' },
  { id: 'fire', name: 'Fire', swatch: 'linear-gradient(135deg, #dc2626, #f59e0b)' },
  { id: 'mint', name: 'Mint', swatch: 'linear-gradient(135deg, #0d9488, #5eead4)' },
  { id: 'sky', name: 'Sky', swatch: 'linear-gradient(135deg, #0284c7, #60a5fa)' },
  { id: 'mono', name: 'Mono', swatch: 'linear-gradient(135deg, #334155, #94a3b8)' },
] as const;

export type UIThemeId = typeof UI_THEME_OPTIONS[number]['id'];

const VALID_THEME_IDS = new Set<string>(UI_THEME_OPTIONS.map(theme => theme.id));

export function normalizeThemeId(value: unknown): UIThemeId {
  return typeof value === 'string' && VALID_THEME_IDS.has(value)
    ? value as UIThemeId
    : 'ocean';
}

export function applyUITheme(themeId: UIThemeId) {
  if (typeof document === 'undefined') return;
  document.documentElement.dataset.theme = normalizeThemeId(themeId);
}
