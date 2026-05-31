import { useStore } from '../store';
import { t } from '../i18n';
import { Languages, ChevronRight, CheckCircle2, AlertCircle } from 'lucide-react';

export default function LanguageSelector() {
  const { languages, translations, selectedLanguage, setSelectedLanguage } = useStore();

  const getStats = (langFile: string) => {
    const file = translations[langFile];
    if (!file) return { total: 0, translated: 0, untranslated: 0, percent: 0 };
    const total = file.entries.length;
    const translated = file.entries.filter(e =>
      e.translation.trim() !== '' && e.translation.trim() !== e.original.trim()
    ).length;
    const untranslated = total - translated;
    const percent = total > 0 ? Math.round((translated / total) * 100) : 0;
    return { total, translated, untranslated, percent };
  };

  return (
    <div className="h-full max-h-full min-h-0 bg-white rounded-2xl shadow-sm border border-slate-200/80 overflow-hidden flex flex-col">
      <div className="px-4 py-3 bg-gradient-to-r from-slate-50 to-white border-b border-slate-100">
        <div className="flex items-center gap-2">
          <Languages className="w-4 h-4 text-slate-500" />
          <h2 className="text-sm font-semibold text-slate-700">{t('sidebar.files')}</h2>
        </div>
      </div>

      <div className="px-3 py-2 min-h-0 flex-1 overflow-y-auto overscroll-contain language-list-scroll">
        <div className="space-y-1 pr-1">
          {languages.map(lang => {
            const stats = getStats(lang.file);
            const isComplete = stats.percent === 100;
            return (
              <button
                key={lang.file}
                onClick={() => setSelectedLanguage(lang.file)}
                className={`w-full flex items-center gap-3 px-3 py-2.5 rounded-xl text-left transition-all duration-200 ${
                  selectedLanguage === lang.file
                    ? 'bg-blue-50 border border-blue-200 shadow-sm'
                    : 'hover:bg-slate-50 border border-transparent'
                }`}
              >
                <div className={`w-8 h-8 rounded-lg flex items-center justify-center text-xs font-bold uppercase ${
                  selectedLanguage === lang.file
                    ? 'bg-blue-500 text-white'
                    : 'bg-slate-100 text-slate-500'
                }`}>
                  {lang.file}
                </div>
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-1.5">
                    <p className={`text-sm font-medium truncate ${
                      selectedLanguage === lang.file ? 'text-blue-700' : 'text-slate-700'
                    }`}>{lang.name}</p>
                    {isComplete ? (
                      <CheckCircle2 className="w-3.5 h-3.5 text-emerald-500 flex-shrink-0" />
                    ) : stats.untranslated > 0 ? (
                      <AlertCircle className="w-3.5 h-3.5 text-amber-500 flex-shrink-0" />
                    ) : null}
                  </div>
                  <div className="flex items-center gap-2 mt-1">
                    <div className="flex-1 h-1.5 bg-slate-100 rounded-full overflow-hidden">
                      <div
                        className={`h-full rounded-full transition-all duration-500 ${
                          isComplete ? 'bg-emerald-500' : stats.percent > 50 ? 'bg-blue-500' : 'bg-amber-500'
                        }`}
                        style={{ width: `${stats.percent}%` }}
                      />
                    </div>
                    <span className="text-xs text-slate-400 font-medium tabular-nums w-8">{stats.percent}%</span>
                  </div>
                  <div className="flex items-center gap-2 mt-0.5 text-[10px] text-slate-400">
                    <span>{stats.total} {t('sidebar.lines')}</span>
                    <span>•</span>
                    <span className="text-emerald-500">{stats.translated} ✓</span>
                    <span>•</span>
                    <span className="text-amber-500">{stats.untranslated} ✗</span>
                  </div>
                </div>
                {selectedLanguage === lang.file && (
                  <ChevronRight className="w-4 h-4 text-blue-400 flex-shrink-0" />
                )}
              </button>
            );
          })}
        </div>
      </div>
    </div>
  );
}
