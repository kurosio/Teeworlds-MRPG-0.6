import { useMemo } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import { Trophy, Medal, Award, User } from 'lucide-react';

function isLeaderboardSystemAuthor(author: string): boolean {
  return String(author || '').trim().toLowerCase() === 'administrator';
}

function formatCount(value: number): string {
  return new Intl.NumberFormat().format(value);
}

export default function TopContributors() {
  const { topContributors } = useStore();

  const topList = useMemo(() => {
    return [...topContributors]
      .filter(item => !isLeaderboardSystemAuthor(item.author))
      .sort((a, b) => b.count - a.count)
      .slice(0, 5);
  }, [topContributors]);

  if (topList.length === 0) return null;

  const getIcon = (i: number) => {
    if (i === 0) return <Trophy className="w-4 h-4 text-amber-500" />;
    if (i === 1) return <Medal className="w-4 h-4 text-slate-400" />;
    if (i === 2) return <Award className="w-4 h-4 text-amber-700" />;
    return <User className="w-3.5 h-3.5 text-slate-400" />;
  };

  const getBg = (i: number) => {
    if (i === 0) return 'bg-gradient-to-r from-amber-50 to-yellow-50 border-amber-200';
    if (i === 1) return 'bg-gradient-to-r from-slate-50 to-gray-50 border-slate-200';
    if (i === 2) return 'bg-gradient-to-r from-orange-50 to-amber-50 border-orange-200';
    return 'bg-white border-slate-100';
  };

  const getTextColor = (i: number) => {
    if (i === 0) return 'text-amber-800';
    if (i === 1) return 'text-slate-700';
    if (i === 2) return 'text-orange-800';
    return 'text-slate-600';
  };

  const getBadge = (i: number) => {
    if (i === 0) return 'bg-amber-200 text-amber-800';
    if (i === 1) return 'bg-slate-200 text-slate-700';
    if (i === 2) return 'bg-orange-200 text-orange-800';
    return 'bg-slate-100 text-slate-600';
  };

  return (
    <div className="theme-panel rounded-2xl shadow-sm border overflow-hidden">
      <div className="px-4 py-3 theme-section-header border-b">
        <div className="flex items-center gap-2">
          <Trophy className="w-4 h-4 text-amber-500" />
          <h2 className="text-sm font-semibold text-amber-800">{t('top.title')}</h2>
        </div>
        <p className="text-[10px] text-amber-600 mt-0.5">{t('top.subtitle')}</p>
      </div>
      <div className="px-3 py-2 max-h-[220px] overflow-y-auto overscroll-contain language-list-scroll">
        <div className="space-y-1">
          {topList.map((item, index) => (
            <div key={item.author} className={`flex items-center gap-2.5 px-2.5 py-2 rounded-lg border ${getBg(index)}`}>
              <div className="flex items-center justify-center w-5">{getIcon(index)}</div>
              <div className="flex-1 min-w-0">
                <p className={`text-sm font-medium truncate ${getTextColor(index)}`}>{item.author}</p>
              </div>
              <div className={`text-[11px] font-bold tabular-nums px-2 py-0.5 rounded-full ${getBadge(index)}`} title={`${formatCount(item.count)} ${t('top.unit')}`}>
                {formatCount(item.count)} <span className="font-semibold opacity-75">{t('top.unit')}</span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
