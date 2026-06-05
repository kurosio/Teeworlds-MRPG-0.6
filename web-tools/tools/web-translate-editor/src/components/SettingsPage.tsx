import { useEffect, useState } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import { loadFromServerApi } from '../utils/fileLoader';
import {
  Settings, ShieldCheck, CheckCircle2, AlertTriangle, RotateCcw, Info,
  Lock, Upload, FileWarning, FileCheck, Server, Trophy, Gift, Save,
} from 'lucide-react';

export default function SettingsPage() {
  const {
    isAdmin,
    resetData,
    loadFiles,
    languages,
    eventSettings,
    saveEventSettings,
    resetTopContributors,
  } = useStore();
  const [saved, setSaved] = useState(false);
  const [showResetConfirm, setShowResetConfirm] = useState(false);
  const [showTopResetConfirm, setShowTopResetConfirm] = useState(false);
  const [loading, setLoading] = useState(false);
  const [savingRewards, setSavingRewards] = useState(false);
  const [resettingTop, setResettingTop] = useState(false);
  const [loadResult, setLoadResult] = useState<{ message: string; type: 'success' | 'error' | 'info' } | null>(null);
  const [rewardEnabled, setRewardEnabled] = useState(eventSettings.translationRewards.enabled);
  const [rewardEndDate, setRewardEndDate] = useState(eventSettings.translationRewards.endDate || '');
  const [rewardText, setRewardText] = useState(eventSettings.translationRewards.rewards || '');

  useEffect(() => {
    setRewardEnabled(eventSettings.translationRewards.enabled);
    setRewardEndDate(eventSettings.translationRewards.endDate || '');
    setRewardText(eventSettings.translationRewards.rewards || '');
  }, [eventSettings]);

  const handleReset = () => {
    resetData();
    setShowResetConfirm(false);
  };

  const handleLoadFromServer = async () => {
    setLoading(true);
    setLoadResult(null);
    try {
      const result = await loadFromServerApi();
      if (result.errors.length > 0 && result.languages.length === 0) {
        setLoadResult({ message: result.errors.join('\n'), type: 'error' });
      } else {
        await loadFiles(result);
        const msg = `Loaded ${result.languages.length} language(s) from server${result.errors.length ? `. Warnings: ${result.errors.join('; ')}` : ''}`;
        setLoadResult({ message: msg, type: result.errors.length ? 'info' : 'success' });
      }
    } finally {
      setLoading(false);
    }
  };

  const handleSaveRewards = async () => {
    setSavingRewards(true);
    try {
      await saveEventSettings({
        ...eventSettings,
        translationRewards: {
          enabled: rewardEnabled,
          endDate: rewardEndDate,
          rewards: rewardText,
          updatedAt: new Date().toISOString(),
        },
      });
      setSaved(true);
      setTimeout(() => setSaved(false), 3000);
    } finally {
      setSavingRewards(false);
    }
  };

  const handleResetTop = async () => {
    setResettingTop(true);
    try {
      await resetTopContributors();
      setShowTopResetConfirm(false);
      setSaved(true);
      setTimeout(() => setSaved(false), 3000);
    } finally {
      setResettingTop(false);
    }
  };

  if (!isAdmin) return null;

  return (
    <div className="max-w-3xl mx-auto space-y-6">
      <div className="theme-panel rounded-2xl shadow-sm border p-5">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 theme-accent-box rounded-xl flex items-center justify-center">
            <Settings className="w-5 h-5 text-white" />
          </div>
          <div>
            <h2 className="text-lg font-semibold text-slate-800">{t('settings.title')}</h2>
            <p className="text-sm text-slate-400">{t('settings.subtitle')}</p>
          </div>
        </div>
      </div>

      <div className="theme-panel rounded-2xl shadow-sm border p-5">
        <div className="flex items-center gap-2 mb-4">
          <Upload className="w-5 h-5 text-purple-500" />
          <h3 className="text-sm font-semibold text-slate-700">{t('settings.loadTitle')}</h3>
          {languages.length > 0 && (
            <span className="text-xs text-slate-400 ml-auto">{languages.length} language(s) loaded</span>
          )}
        </div>

        <div className="space-y-3">
          {loadResult && (
            <div className={`rounded-xl p-3 flex items-start gap-2 ${
              loadResult.type === 'success' ? 'bg-emerald-50 border border-emerald-200' :
              loadResult.type === 'error' ? 'bg-red-50 border border-red-200' :
              'bg-blue-50 border border-blue-200'
            }`}>
              {loadResult.type === 'success' ? <FileCheck className="w-4 h-4 text-emerald-500 flex-shrink-0 mt-0.5" /> :
               loadResult.type === 'error' ? <FileWarning className="w-4 h-4 text-red-500 flex-shrink-0 mt-0.5" /> :
               <Info className="w-4 h-4 text-blue-500 flex-shrink-0 mt-0.5" />}
              <p className={`text-xs whitespace-pre-line ${
                loadResult.type === 'success' ? 'text-emerald-700' :
                loadResult.type === 'error' ? 'text-red-700' : 'text-blue-700'
              }`}>{loadResult.message}</p>
            </div>
          )}

          <button
            onClick={handleLoadFromServer}
            disabled={loading}
            className="w-full flex items-center gap-3 p-4 bg-gradient-to-br from-slate-50 to-zinc-50 border border-slate-200 rounded-xl hover:border-slate-300 hover:shadow-md transition-all disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <Server className="w-6 h-6 text-slate-600 flex-shrink-0" />
            <div className="text-left">
              <span className="block text-xs font-semibold text-slate-700">{t('settings.loadServer')}</span>
              <span className="block text-[10px] text-slate-500 leading-tight">{t('settings.loadServerDesc')}</span>
            </div>
          </button>
        </div>
      </div>

      <div className="theme-panel rounded-2xl shadow-sm border p-5">
        <div className="flex items-center gap-2 mb-4">
          <Gift className="w-5 h-5 text-emerald-500" />
          <h3 className="text-sm font-semibold text-slate-700">{t('settings.rewardsTitle')}</h3>
        </div>
        <div className="space-y-4">
          <label className="flex items-center justify-between gap-3 rounded-xl border border-slate-200 bg-slate-50/70 px-4 py-3 cursor-pointer">
            <div>
              <p className="text-sm font-semibold text-slate-700">{t('settings.rewardsEnable')}</p>
              <p className="text-xs text-slate-500">{t('settings.rewardsEnableDesc')}</p>
            </div>
            <input
              type="checkbox"
              checked={rewardEnabled}
              onChange={(e) => setRewardEnabled(e.target.checked)}
              className="h-5 w-5 rounded border-slate-300 text-blue-600 focus:ring-blue-500"
            />
          </label>
          <div>
            <label className="block text-xs font-semibold text-slate-500 mb-1.5">{t('settings.rewardsEndDate')}</label>
            <input
              type="date"
              value={rewardEndDate}
              onChange={(e) => setRewardEndDate(e.target.value)}
              className="w-full px-3.5 py-2.5 border border-slate-200 bg-white rounded-xl text-sm text-slate-800 focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400"
            />
          </div>
          <div>
            <label className="block text-xs font-semibold text-slate-500 mb-1.5">{t('settings.rewardsText')}</label>
            <textarea
              value={rewardText}
              onChange={(e) => setRewardText(e.target.value)}
              rows={5}
              placeholder={t('settings.rewardsPlaceholder')}
              className="w-full px-3.5 py-2.5 border border-slate-200 bg-white rounded-xl text-sm text-slate-800 resize-y focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400"
            />
          </div>
          <div className="flex justify-end">
            <button onClick={handleSaveRewards} disabled={savingRewards} className="flex items-center gap-1.5 px-5 py-2.5 theme-primary-button text-white rounded-xl text-sm font-medium shadow-lg transition-all disabled:opacity-60">
              <Save className="w-4 h-4" />{savingRewards ? t('settings.saving') : t('settings.saveRewards')}
            </button>
          </div>
        </div>
      </div>

      <div className="theme-panel rounded-2xl shadow-sm border p-5">
        <div className="flex items-center gap-2 mb-4">
          <Trophy className="w-5 h-5 text-amber-500" />
          <h3 className="text-sm font-semibold text-slate-700">{t('settings.topTitle')}</h3>
        </div>
        <div className="bg-amber-50 border border-amber-200 rounded-xl p-4 mb-4">
          <p className="text-sm text-amber-800 font-medium">{t('settings.resetTopHint')}</p>
        </div>
        <button onClick={() => setShowTopResetConfirm(true)} className="flex items-center gap-1.5 px-4 py-2 text-sm text-red-500 hover:bg-red-50 border border-red-200 rounded-xl transition-all">
          <RotateCcw className="w-4 h-4" />{t('settings.resetTop')}
        </button>
      </div>

      <div className="theme-panel rounded-2xl shadow-sm border p-5">
        <div className="flex items-center gap-2 mb-4">
          <ShieldCheck className="w-5 h-5 text-emerald-500" />
          <h3 className="text-sm font-semibold text-slate-700">{t('settings.adminTitle')}</h3>
        </div>
        <div className="bg-slate-50 border border-slate-200 rounded-xl p-4">
          <div className="flex items-start gap-3">
            <Lock className="w-5 h-5 text-slate-400 flex-shrink-0 mt-0.5" />
            <div className="text-sm text-slate-600">
              <p className="font-medium text-slate-700 mb-1">{t('settings.adminLogin')}: <code className="bg-slate-200 px-1.5 py-0.5 rounded text-xs">***</code></p>
              <p className="font-medium text-slate-700 mb-2">{t('settings.adminPassword')}: <code className="bg-slate-200 px-1.5 py-0.5 rounded text-xs">********</code></p>
              <p className="text-xs text-slate-500">{t('settings.adminHint')}</p>
            </div>
          </div>
        </div>
      </div>

      <div className="flex items-center justify-between">
        <button onClick={() => setShowResetConfirm(true)} className="flex items-center gap-1.5 px-4 py-2 text-sm text-red-500 hover:bg-red-50 border border-red-200 rounded-xl transition-all">
          <RotateCcw className="w-4 h-4" />{t('settings.resetData')}
        </button>
      </div>

      {saved && (
        <div className="bg-emerald-50 border border-emerald-200 rounded-xl p-3 flex items-center gap-2">
          <CheckCircle2 className="w-4 h-4 text-emerald-500" />
          <span className="text-sm text-emerald-600 font-medium">{t('settings.saved')}</span>
        </div>
      )}

      {showTopResetConfirm && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-sm" onClick={() => setShowTopResetConfirm(false)}>
          <div className="bg-white rounded-2xl shadow-2xl border border-slate-200 w-full max-w-md mx-4 p-6" onClick={(e) => e.stopPropagation()}>
            <div className="flex items-center gap-3 mb-4">
              <div className="w-10 h-10 bg-red-100 rounded-xl flex items-center justify-center"><AlertTriangle className="w-5 h-5 text-red-500" /></div>
              <div><h3 className="text-lg font-semibold text-slate-800">{t('settings.resetTop')}</h3><p className="text-sm text-slate-400">{t('settings.resetCannotUndo')}</p></div>
            </div>
            <p className="text-sm text-slate-600 mb-5">{t('settings.resetTopDesc')}</p>
            <div className="flex justify-end gap-2">
              <button onClick={() => setShowTopResetConfirm(false)} className="px-4 py-2 text-sm text-slate-600 hover:bg-slate-100 rounded-xl transition-colors">{t('settings.cancel')}</button>
              <button onClick={handleResetTop} disabled={resettingTop} className="px-4 py-2 bg-red-500 hover:bg-red-600 text-white rounded-xl text-sm font-medium transition-colors disabled:opacity-60">{t('settings.resetConfirm')}</button>
            </div>
          </div>
        </div>
      )}

      {showResetConfirm && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-sm" onClick={() => setShowResetConfirm(false)}>
          <div className="bg-white rounded-2xl shadow-2xl border border-slate-200 w-full max-w-md mx-4 p-6" onClick={(e) => e.stopPropagation()}>
            <div className="flex items-center gap-3 mb-4">
              <div className="w-10 h-10 bg-red-100 rounded-xl flex items-center justify-center"><AlertTriangle className="w-5 h-5 text-red-500" /></div>
              <div><h3 className="text-lg font-semibold text-slate-800">{t('settings.resetTitle')}</h3><p className="text-sm text-slate-400">{t('settings.resetCannotUndo')}</p></div>
            </div>
            <p className="text-sm text-slate-600 mb-5">{t('settings.resetDesc')}</p>
            <div className="flex justify-end gap-2">
              <button onClick={() => setShowResetConfirm(false)} className="px-4 py-2 text-sm text-slate-600 hover:bg-slate-100 rounded-xl transition-colors">{t('settings.cancel')}</button>
              <button onClick={handleReset} className="px-4 py-2 bg-red-500 hover:bg-red-600 text-white rounded-xl text-sm font-medium transition-colors">{t('settings.resetConfirm')}</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
