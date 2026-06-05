import { useMemo, useState, useEffect } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import { Languages, ChevronRight, CheckCircle2, AlertCircle, Loader2, Search, X } from 'lucide-react';

function clean(value: string) {
  return value.trim().toLowerCase();
}

export default function LanguageSelector() {
  const { languages, translations, selectedLanguage, setSelectedLanguage, filesLoading, fileLoadStatus } = useStore();
  const [query, setQuery] = useState('');

  const statsByLanguage = useMemo(() => {
    const result: Record<string, { total: number; translated: number; untranslated: number; percent: number }> = {};
    for (const lang of languages) {
      const file = translations[lang.file];
      if (!file) {
        result[lang.file] = { total: 0, translated: 0, untranslated: 0, percent: 0 };
        continue;
      }
      let translated = 0;
      for (const entry of file.entries) {
        const translation = entry.translation.trim();
        if (translation !== '' && translation !== entry.original.trim()) translated++;
      }
      const total = file.entries.length;
      result[lang.file] = {
        total,
        translated,
        untranslated: total - translated,
        percent: total > 0 ? Math.round((translated / total) * 100) : 0,
      };
    }
    return result;
  }, [languages, translations]);

  const filteredLanguages = useMemo(() => {
    const value = clean(query);
    if (!value) return languages;
    return languages.filter(lang =>
      clean(lang.name).includes(value) || clean(lang.file).includes(value)
    );
  }, [languages, query]);

  const selectedIndex = useMemo(() => {
    return languages.findIndex(lang => lang.file === selectedLanguage);
  }, [languages, selectedLanguage]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (!event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) return;
      if (event.key !== 'ArrowDown' && event.key !== 'ArrowUp') return;
      if (languages.length === 0) return;
      event.preventDefault();
      const currentIndex = selectedIndex >= 0 ? selectedIndex : 0;
      const delta = event.key === 'ArrowDown' ? 1 : -1;
      const nextIndex = (currentIndex + delta + languages.length) % languages.length;
      setSelectedLanguage(languages[nextIndex].file);
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [languages, selectedIndex, setSelectedLanguage]);

  const selectedStats = selectedLanguage ? statsByLanguage[selectedLanguage] : null;

  const renderLanguageButton = (lang: typeof languages[number], compact = false) => {
    const stats = statsByLanguage[lang.file] || { total: 0, translated: 0, untranslated: 0, percent: 0 };
    const loading = filesLoading && fileLoadStatus[lang.file] === 'loading';
    const isComplete = !loading && stats.percent === 100;
    const isSelected = selectedLanguage === lang.file;

    return (
      <button
        key={lang.file}
        type="button"
        onClick={() => setSelectedLanguage(lang.file)}
        className={`w-full flex items-center gap-2.5 rounded-xl text-left transition-colors ${
          compact ? 'px-2.5 py-2' : 'px-3 py-2.5'
        } ${
          isSelected
            ? 'theme-list-active border shadow-sm'
            : 'theme-list-button border border-transparent'
        }`}
      >
        <div className={`w-8 h-8 rounded-lg flex items-center justify-center text-xs font-bold uppercase shrink-0 ${
          isSelected ? 'theme-accent-box text-white' : 'theme-mini-badge'
        }`}>
          {lang.file}
        </div>
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-1.5">
            <p className={`text-sm font-medium truncate ${isSelected ? 'theme-text-accent' : 'theme-title'}`}>{lang.name}</p>
            {loading ? (
              <Loader2 className="w-3.5 h-3.5 text-blue-500 flex-shrink-0 animate-spin" />
            ) : isComplete ? (
              <CheckCircle2 className="w-3.5 h-3.5 text-emerald-500 flex-shrink-0" />
            ) : stats.untranslated > 0 ? (
              <AlertCircle className="w-3.5 h-3.5 text-amber-500 flex-shrink-0" />
            ) : null}
          </div>
          <div className="flex items-center gap-2 mt-1">
            <div className="flex-1 h-1.5 bg-slate-100/80 rounded-full overflow-hidden">
              <div
                className={`h-full rounded-full ${isComplete ? 'bg-emerald-500' : stats.percent > 50 ? 'theme-progress-bar' : 'bg-amber-500'}`}
                style={{ width: loading ? '35%' : `${stats.percent}%` }}
              />
            </div>
            <span className="theme-muted text-xs font-medium tabular-nums w-12 text-right">{loading ? t('sidebar.loading') : `${stats.percent}%`}</span>
          </div>
          {!compact && (
            <div className="flex items-center gap-2 mt-0.5 text-[10px] theme-muted">
              <span>{loading ? t('sidebar.loadingFile') : `${stats.total} ${t('sidebar.lines')}`}</span>
              <span>•</span>
              <span className="text-emerald-500">{stats.translated} ✓</span>
              <span>•</span>
              <span className="text-amber-500">{stats.untranslated} ✗</span>
            </div>
          )}
        </div>
        {isSelected && <ChevronRight className="w-4 h-4 text-blue-400 flex-shrink-0" />}
      </button>
    );
  };

  return (
    <div className="h-full max-h-full min-h-0 theme-panel rounded-2xl shadow-sm border overflow-hidden flex flex-col">
      <div className="px-4 py-3 theme-section-header border-b shrink-0">
        <div className="flex items-center justify-between gap-2">
          <div className="flex items-center gap-2 min-w-0">
            <Languages className="w-4 h-4 text-slate-500 shrink-0" />
            <h2 className="text-sm font-semibold text-slate-700 truncate">{t('sidebar.files')}</h2>
          </div>
          <span className="theme-chip px-2 py-0.5 rounded-full text-[10px] font-semibold">{languages.length}</span>
        </div>
        {selectedLanguage && selectedStats && (
          <div className="mt-2 rounded-xl theme-soft-panel border px-2.5 py-2">
            <div className="flex items-center justify-between gap-2 text-[11px]">
              <span className="theme-muted truncate">{t('sidebar.current')}</span>
              <span className="font-semibold theme-text-accent uppercase">{selectedLanguage}</span>
            </div>
            <div className="mt-1.5 h-1.5 rounded-full bg-slate-100/80 overflow-hidden">
              <div className="h-full theme-progress-bar rounded-full" style={{ width: `${selectedStats.percent}%` }} />
            </div>
          </div>
        )}
        <div className="relative mt-2">
          <Search className="theme-muted absolute left-2.5 top-1/2 -translate-y-1/2 h-3.5 w-3.5" />
          <input
            type="text"
            value={query}
            onChange={(event) => setQuery(event.currentTarget.value)}
            placeholder={t('sidebar.searchLanguages')}
            className="theme-input w-full rounded-xl border py-1.5 pl-8 pr-7 text-xs outline-none"
          />
          {query && (
            <button
              type="button"
              onClick={() => setQuery('')}
              className="absolute right-2 top-1/2 -translate-y-1/2 theme-muted hover:opacity-80"
              title={t('editor.clear')}
            >
              <X className="h-3.5 w-3.5" />
            </button>
          )}
        </div>
      </div>

      <div className="px-3 py-2 min-h-0 flex-1 overflow-y-auto overscroll-contain language-list-scroll">
        {filteredLanguages.length === 0 ? (
          <div className="px-3 py-8 text-center text-xs theme-muted">{t('sidebar.noLanguages')}</div>
        ) : (
          <div className="space-y-1 pr-1">
            {filteredLanguages.map(lang => renderLanguageButton(lang, languages.length > 12))}
          </div>
        )}
      </div>
      <div className="border-t theme-section-header px-3 py-2 text-[10px] theme-muted shrink-0">
        {t('sidebar.languageHotkeys')}
      </div>
    </div>
  );
}
