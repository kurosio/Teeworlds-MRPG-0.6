import { useMemo } from 'react';
import { useStore } from '../store';
import { Gift, CalendarDays, Sparkles } from 'lucide-react';
import { t } from '../i18n';

function formatDate(value: string) {
  if (!value) return '';
  const date = new Date(`${value}T23:59:59`);
  if (Number.isNaN(date.getTime())) return value;
  return new Intl.DateTimeFormat(undefined, { year: 'numeric', month: 'short', day: 'numeric' }).format(date);
}

export default function TranslationRewardsPanel() {
  const { eventSettings } = useStore();
  const rewards = eventSettings.translationRewards;

  const progress = useMemo(() => {
    if (!rewards.endDate) return { percent: 0, label: t('event.noEndDate') };
    const end = new Date(`${rewards.endDate}T23:59:59`).getTime();
    const now = Date.now();
    if (Number.isNaN(end)) return { percent: 0, label: rewards.endDate };
    const start = eventSettings.topResetAt ? new Date(eventSettings.topResetAt).getTime() : now;
    const total = Math.max(1, end - start);
    const elapsed = Math.min(total, Math.max(0, now - start));
    const daysLeft = Math.max(0, Math.ceil((end - now) / 86400000));
    return {
      percent: Math.round((elapsed / total) * 100),
      label: daysLeft > 0 ? `${daysLeft} ${t('event.daysLeft')}` : t('event.finished'),
    };
  }, [eventSettings.topResetAt, rewards.endDate]);

  if (!rewards.enabled) return null;

  return (
    <div className="theme-panel rounded-2xl shadow-sm border overflow-hidden">
      <div className="px-4 py-3 theme-section-header border-b">
        <div className="flex items-center gap-2">
          <Gift className="w-4 h-4 text-emerald-500" />
          <h2 className="text-sm font-semibold text-slate-800">{t('event.title')}</h2>
        </div>
        <p className="text-[10px] text-slate-500 mt-0.5">{t('event.subtitle')}</p>
      </div>
      <div className="p-3 space-y-3">
        <div className="flex items-center justify-between gap-2 text-xs">
          <span className="flex items-center gap-1.5 text-slate-500"><CalendarDays className="w-3.5 h-3.5" />{formatDate(rewards.endDate)}</span>
          <span className="font-semibold theme-text-accent">{progress.label}</span>
        </div>
        <div className="h-2 rounded-full bg-slate-100 overflow-hidden border border-slate-200/70">
          <div className="h-full theme-progress-bar rounded-full" style={{ width: `${progress.percent}%` }} />
        </div>
        {rewards.rewards.trim() && (
          <div className="rounded-xl border border-emerald-200/70 bg-emerald-50/70 px-3 py-2">
            <div className="flex items-center gap-1.5 text-[11px] font-semibold text-emerald-700 mb-1">
              <Sparkles className="w-3.5 h-3.5" />{t('event.rewards')}
            </div>
            <p className="text-xs text-slate-700 whitespace-pre-wrap leading-relaxed">{rewards.rewards}</p>
          </div>
        )}
      </div>
    </div>
  );
}
