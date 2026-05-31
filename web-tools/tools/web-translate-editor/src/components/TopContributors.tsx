import { useMemo } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import { Trophy, Medal, Award, User } from 'lucide-react';

export default function TopContributors() {
  const { changeRequests } = useStore();

  const topList = useMemo(() => {
    const approvedRequests = changeRequests.filter(r => r.status === 'approved');
    const authorStats: Record<string, number> = {};
    for (const request of approvedRequests) {
      authorStats[request.author] = (authorStats[request.author] || 0) + request.entries.length;
    }
    return Object.entries(authorStats)
      .map(([author, count]) => ({ author, count }))
      .sort((a, b) => b.count - a.count)
      .slice(0, 10);
  }, [changeRequests]);

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
    <div className="bg-white rounded-2xl shadow-sm border border-slate-200/80 overflow-hidden mt-4">
      <div className="px-4 py-3 bg-gradient-to-r from-amber-50 to-yellow-50 border-b border-amber-100">
        <div className="flex items-center gap-2">
          <Trophy className="w-4 h-4 text-amber-500" />
          <h2 className="text-sm font-semibold text-amber-800">{t('top.title')}</h2>
        </div>
        <p className="text-[10px] text-amber-600 mt-0.5">{t('top.subtitle')}</p>
      </div>
      <div className="px-3 py-2">
        <div className="space-y-1">
          {topList.map((item, index) => (
            <div key={item.author} className={`flex items-center gap-2.5 px-2.5 py-2 rounded-lg border ${getBg(index)}`}>
              <div className="flex items-center justify-center w-5">{getIcon(index)}</div>
              <div className="flex-1 min-w-0">
                <p className={`text-sm font-medium truncate ${getTextColor(index)}`}>{item.author}</p>
              </div>
              <div className={`text-xs font-bold tabular-nums px-2 py-0.5 rounded-full ${getBadge(index)}`}>
                {item.count}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
