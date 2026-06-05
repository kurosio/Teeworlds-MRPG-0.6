import { useMemo } from 'react';
import { AlertCircle, ChevronRight, Gift, Languages, Trophy } from 'lucide-react';
import { useStore } from '../store';
import { t } from '../i18n';
import LanguageSelector from './LanguageSelector';
import TopContributors from './TopContributors';
import TranslationRewardsPanel from './TranslationRewardsPanel';

function isLeaderboardSystemAuthor(author: string): boolean {
  return String(author || '').trim().toLowerCase() === 'administrator';
}

function getLanguageStats(languages: ReturnType<typeof useStore.getState>['languages'], translations: ReturnType<typeof useStore.getState>['translations']) {
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
      untranslated: Math.max(0, total - translated),
      percent: total > 0 ? Math.round((translated / total) * 100) : 0,
    };
  }
  return result;
}

function getEventProgress(endDate: string, startAt?: string) {
  if (!endDate) return { percent: 0, label: t('event.noEndDate') };
  const end = new Date(`${endDate}T23:59:59`).getTime();
  if (Number.isNaN(end)) return { percent: 0, label: endDate };
  const now = Date.now();
  const start = startAt ? new Date(startAt).getTime() : now;
  const safeStart = Number.isNaN(start) ? now : start;
  const total = Math.max(1, end - safeStart);
  const elapsed = Math.min(total, Math.max(0, now - safeStart));
  const daysLeft = Math.max(0, Math.ceil((end - now) / 86400000));
  return {
    percent: Math.round((elapsed / total) * 100),
    label: daysLeft > 0 ? `${daysLeft}d` : t('event.finished'),
  };
}

export default function SidebarDock() {
  const { languages, translations, selectedLanguage, setSelectedLanguage, topContributors, eventSettings } = useStore();

  const statsByLanguage = useMemo(() => getLanguageStats(languages, translations), [languages, translations]);
  const selectedStats = selectedLanguage ? statsByLanguage[selectedLanguage] : null;
  const selectedMeta = useMemo(() => languages.find(lang => lang.file === selectedLanguage), [languages, selectedLanguage]);
  const topUser = useMemo(() => [...topContributors]
    .filter(item => !isLeaderboardSystemAuthor(item.author))
    .sort((a, b) => b.count - a.count)[0], [topContributors]);
  const eventRewards = eventSettings.translationRewards;
  const eventProgress = useMemo(() => getEventProgress(eventRewards.endDate, eventSettings.topResetAt), [eventRewards.endDate, eventSettings.topResetAt]);

  const previewLanguages = languages.slice(0, 8);

  return (
    <aside className="sidebar-dock group/sidebar relative z-20 h-[calc(100vh-3rem)] max-h-[calc(100vh-3rem)] w-[76px] shrink-0">
      <div className="sidebar-rail theme-panel border rounded-[1.4rem] h-full overflow-hidden flex flex-col items-center py-3 gap-3">
        <div className="sidebar-rail-brand theme-accent-box text-white shadow-sm">
          <Languages className="h-5 w-5" />
        </div>

        {eventRewards.enabled && (
          <div className="sidebar-rail-card" title={t('event.title')}>
            <Gift className="h-4 w-4 theme-text-accent" />
            <div className="sidebar-rail-progress">
              <div style={{ height: `${eventProgress.percent}%` }} />
            </div>
            <span>{eventProgress.label}</span>
          </div>
        )}

        {topUser && (
          <div className="sidebar-rail-card" title={`${topUser.author}: ${topUser.count} ${t('top.unit')}`}>
            <Trophy className="h-4 w-4 text-amber-500" />
            <strong>#{1}</strong>
            <span>{new Intl.NumberFormat().format(topUser.count)}</span>
          </div>
        )}

        <div className="sidebar-rail-divider" />

        <div className="sidebar-rail-languages min-h-0 flex-1 overflow-hidden w-full px-2 space-y-2">
          {previewLanguages.map(lang => {
            const stats = statsByLanguage[lang.file] || { percent: 0, untranslated: 0 };
            const isSelected = selectedLanguage === lang.file;
            return (
              <button
                key={lang.file}
                type="button"
                onClick={() => setSelectedLanguage(lang.file)}
                className={`sidebar-rail-language ${isSelected ? 'is-selected' : ''}`}
                title={`${lang.name} · ${stats.percent}%`}
              >
                <span>{lang.file}</span>
                <i><b style={{ width: `${stats.percent}%` }} /></i>
                {stats.untranslated > 0 && <em />}
              </button>
            );
          })}
        </div>

        {selectedMeta && selectedStats && (
          <div className="sidebar-rail-current" title={`${selectedMeta.name}: ${selectedStats.percent}%`}>
            <span>{selectedMeta.file}</span>
            <b>{selectedStats.percent}%</b>
          </div>
        )}
      </div>

      <div className="sidebar-expanded theme-panel border rounded-[1.6rem] shadow-2xl overflow-hidden">
        <div className="sidebar-expanded-head theme-section-header border-b">
          <div className="flex items-center gap-3 min-w-0">
            <div className="h-10 w-10 rounded-2xl theme-accent-box text-white flex items-center justify-center shadow-sm">
              <Languages className="h-5 w-5" />
            </div>
            <div className="min-w-0">
              <div className="text-sm font-bold theme-title truncate">{t('sidebar.files')}</div>
              <div className="text-[11px] theme-muted truncate">
                {selectedMeta ? `${selectedMeta.name} · ${selectedStats?.percent ?? 0}%` : t('editor.selectFile')}
              </div>
            </div>
          </div>
          <ChevronRight className="h-4 w-4 theme-muted" />
        </div>

        <div className="sidebar-expanded-scroll">
          {eventRewards.enabled && (
            <div className="sidebar-block-wrap">
              <TranslationRewardsPanel />
            </div>
          )}

          <div className="sidebar-block-wrap">
            <TopContributors />
          </div>

          {languages.length > 0 ? (
            <div className="sidebar-language-zone">
              <LanguageSelector />
            </div>
          ) : (
            <div className="theme-soft-panel border rounded-2xl p-4 text-xs theme-muted flex items-center gap-2">
              <AlertCircle className="h-4 w-4" />{t('sidebar.loadingFile')}
            </div>
          )}
        </div>
      </div>
    </aside>
  );
}
