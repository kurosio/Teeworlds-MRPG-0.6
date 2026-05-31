import { useState, useRef } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import { loadFromDirectory, loadFromUrlPath, loadFromFileList, loadFromServerApi } from '../utils/fileLoader';
import {
  Settings, FolderOpen, ShieldCheck, Save, CheckCircle2,
  AlertTriangle, RotateCcw, Info, Lock, Upload, FolderSync,
  Globe, FileWarning, FileCheck, Server,
} from 'lucide-react';

export default function SettingsPage() {
  const { settings, updateSettings, isAdmin, resetData, loadFiles, languages } = useStore();
  const [path, setPath] = useState(settings.translationPath);
  const [saved, setSaved] = useState(false);
  const [showResetConfirm, setShowResetConfirm] = useState(false);
  const [loading, setLoading] = useState(false);
  const [loadResult, setLoadResult] = useState<{ message: string; type: 'success' | 'error' | 'info' } | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const handleSave = () => {
    updateSettings({ translationPath: path });
    setSaved(true);
    setTimeout(() => setSaved(false), 3000);
  };

  const handleReset = () => {
    resetData();
    setPath('/translations');
    setShowResetConfirm(false);
  };

  const handleLoadFromDirectory = async () => {
    setLoading(true);
    setLoadResult(null);
    try {
      const result = await loadFromDirectory();
      if (result.errors.length > 0 && result.languages.length === 0) {
        setLoadResult({ message: result.errors.join('\n'), type: 'error' });
      } else {
        loadFiles(result);
        const msg = `Loaded ${result.languages.length} language(s)${result.errors.length ? `. Warnings: ${result.errors.join('; ')}` : ''}`;
        setLoadResult({ message: msg, type: result.errors.length ? 'info' : 'success' });
      }
    } finally {
      setLoading(false);
    }
  };

  const handleLoadFromUrl = async () => {
    setLoading(true);
    setLoadResult(null);
    try {
      const result = await loadFromUrlPath(path);
      if (result.errors.length > 0 && result.languages.length === 0) {
        setLoadResult({ message: result.errors.join('\n'), type: 'error' });
      } else {
        loadFiles(result);
        const msg = `Loaded ${result.languages.length} language(s)${result.errors.length ? `. Warnings: ${result.errors.join('; ')}` : ''}`;
        setLoadResult({ message: msg, type: result.errors.length ? 'info' : 'success' });
      }
    } finally {
      setLoading(false);
    }
  };

  const handleLoadFromServer = async () => {
    setLoading(true);
    setLoadResult(null);
    try {
      const result = await loadFromServerApi();
      if (result.errors.length > 0 && result.languages.length === 0) {
        setLoadResult({ message: result.errors.join('\n'), type: 'error' });
      } else {
        loadFiles(result);
        const msg = `Loaded ${result.languages.length} language(s) from server${result.errors.length ? `. Warnings: ${result.errors.join('; ')}` : ''}`;
        setLoadResult({ message: msg, type: result.errors.length ? 'info' : 'success' });
      }
    } finally {
      setLoading(false);
    }
  };

  const handleFileUpload = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    if (!files || files.length === 0) return;
    setLoading(true);
    setLoadResult(null);
    try {
      const result = await loadFromFileList(files);
      if (result.errors.length > 0 && result.languages.length === 0) {
        setLoadResult({ message: result.errors.join('\n'), type: 'error' });
      } else {
        loadFiles(result);
        const msg = `Loaded ${result.languages.length} language(s)${result.errors.length ? `. Warnings: ${result.errors.join('; ')}` : ''}`;
        setLoadResult({ message: msg, type: result.errors.length ? 'info' : 'success' });
      }
    } finally {
      setLoading(false);
    }
    // Reset input to allow re-upload
    if (fileInputRef.current) fileInputRef.current.value = '';
  };

  return (
    <div className="max-w-3xl mx-auto space-y-6">
      {/* Header */}
      <div className="bg-white rounded-2xl shadow-sm border border-slate-200/80 p-5">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 bg-gradient-to-br from-slate-600 to-slate-700 rounded-xl flex items-center justify-center">
            <Settings className="w-5 h-5 text-white" />
          </div>
          <div>
            <h2 className="text-lg font-semibold text-slate-800">{t('settings.title')}</h2>
            <p className="text-sm text-slate-400">{t('settings.subtitle')}</p>
          </div>
        </div>
      </div>

      {!isAdmin && (
        <div className="bg-amber-50 border border-amber-200 rounded-xl p-4 flex items-start gap-3">
          <AlertTriangle className="w-5 h-5 text-amber-500 flex-shrink-0 mt-0.5" />
          <div>
            <p className="text-sm font-medium text-amber-700">{t('settings.limitedAccess')}</p>
            <p className="text-xs text-amber-600 mt-0.5">{t('settings.limitedAccessDesc')}</p>
          </div>
        </div>
      )}

      {/* Path setting */}
      <div className="bg-white rounded-2xl shadow-sm border border-slate-200/80 p-5">
        <div className="flex items-center gap-2 mb-4">
          <FolderOpen className="w-5 h-5 text-blue-500" />
          <h3 className="text-sm font-semibold text-slate-700">{t('settings.pathTitle')}</h3>
        </div>
        <div className="space-y-3">
          <div>
            <label className="block text-xs font-medium text-slate-500 mb-1.5">{t('settings.pathLabel')}</label>
            <input
              type="text"
              value={path}
              onChange={(e) => setPath(e.target.value)}
              disabled={!isAdmin}
              placeholder="/path/to/translations"
              className={`w-full px-3.5 py-2.5 border rounded-xl text-sm font-mono transition-all focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400 ${
                isAdmin ? 'border-slate-200 bg-white hover:border-slate-300 text-slate-800' : 'border-slate-100 bg-slate-50 text-slate-500 cursor-not-allowed'
              }`}
            />
          </div>
          <div className="bg-blue-50 border border-blue-100 rounded-xl p-3">
            <div className="flex items-start gap-2">
              <Info className="w-4 h-4 text-blue-400 flex-shrink-0 mt-0.5" />
              <div className="text-xs text-blue-600 space-y-1">
                <p className="font-medium">{t('settings.pathHint')}</p>
                <pre className="bg-blue-100/50 rounded-lg p-2 font-mono text-[11px] overflow-x-auto">
{`${path}/
├── index.json      (${t('settings.pathIndex')})
├── ru.txt          (${t('settings.pathRu')})
└── cn.txt          (${t('settings.pathCn')})`}
                </pre>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* File loading */}
      <div className="bg-white rounded-2xl shadow-sm border border-slate-200/80 p-5">
        <div className="flex items-center gap-2 mb-4">
          <Upload className="w-5 h-5 text-purple-500" />
          <h3 className="text-sm font-semibold text-slate-700">Load translation files</h3>
          {languages.length > 0 && (
            <span className="text-xs text-slate-400 ml-auto">{languages.length} language(s) loaded</span>
          )}
        </div>

        <div className="space-y-3">
          {/* Load feedback */}
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

          <div className="grid grid-cols-1 md:grid-cols-4 gap-3">
            {/* Load from server API */}
            <button
              onClick={handleLoadFromServer}
              disabled={loading || !isAdmin}
              className="flex flex-col items-center gap-2 p-4 bg-gradient-to-br from-slate-50 to-zinc-50 border border-slate-200 rounded-xl hover:border-slate-300 hover:shadow-md transition-all disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <Server className="w-6 h-6 text-slate-600" />
              <span className="text-xs font-semibold text-slate-700">Load from Server</span>
              <span className="text-[10px] text-slate-500 leading-tight text-center">Read TRANSLATION_ROOT via backend API</span>
            </button>

            {/* Load from directory (File System Access API) */}
            <button
              onClick={handleLoadFromDirectory}
              disabled={loading || !isAdmin}
              className="flex flex-col items-center gap-2 p-4 bg-gradient-to-br from-purple-50 to-fuchsia-50 border border-purple-200 rounded-xl hover:border-purple-300 hover:shadow-md transition-all disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <FolderSync className="w-6 h-6 text-purple-500" />
              <span className="text-xs font-semibold text-purple-700">Pick Directory</span>
              <span className="text-[10px] text-purple-500 leading-tight text-center">Select a folder with index.json + .txt files</span>
            </button>

            {/* Load from URL */}
            <button
              onClick={handleLoadFromUrl}
              disabled={loading || !isAdmin}
              className="flex flex-col items-center gap-2 p-4 bg-gradient-to-br from-blue-50 to-cyan-50 border border-blue-200 rounded-xl hover:border-blue-300 hover:shadow-md transition-all disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <Globe className="w-6 h-6 text-blue-500" />
              <span className="text-xs font-semibold text-blue-700">Load via URL</span>
              <span className="text-[10px] text-blue-500 leading-tight text-center">Fetch from public/{path}/</span>
            </button>

            {/* Manual upload */}
            <button
              onClick={() => fileInputRef.current?.click()}
              disabled={loading || !isAdmin}
              className="flex flex-col items-center gap-2 p-4 bg-gradient-to-br from-emerald-50 to-green-50 border border-emerald-200 rounded-xl hover:border-emerald-300 hover:shadow-md transition-all disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <Upload className="w-6 h-6 text-emerald-500" />
              <span className="text-xs font-semibold text-emerald-700">Upload Files</span>
              <span className="text-[10px] text-emerald-500 leading-tight text-center">Select index.json + .txt files</span>
            </button>
            <input
              ref={fileInputRef}
              type="file"
              multiple
              accept=".json,.txt"
              onChange={handleFileUpload}
              className="hidden"
            />
          </div>

          <p className="text-[10px] text-slate-400 flex items-center gap-1">
            <Info className="w-3 h-3" />
            {loading ? 'Loading...' : 'Use Load from Server for /root/mmorpg/server_lang. Start backend with TRANSLATION_ROOT=/root/mmorpg/server_lang npm run server'}
          </p>
        </div>
      </div>

      {/* Admin credentials - read only */}
      <div className="bg-white rounded-2xl shadow-sm border border-slate-200/80 p-5">
        <div className="flex items-center gap-2 mb-4">
          <ShieldCheck className="w-5 h-5 text-emerald-500" />
          <h3 className="text-sm font-semibold text-slate-700">{t('settings.adminTitle')}</h3>
        </div>
        <div className="bg-slate-50 border border-slate-200 rounded-xl p-4">
          <div className="flex items-start gap-3">
            <Lock className="w-5 h-5 text-slate-400 flex-shrink-0 mt-0.5" />
            <div className="text-sm text-slate-600">
              <p className="font-medium text-slate-700 mb-1">
                {t('settings.adminLogin')}: <code className="bg-slate-200 px-1.5 py-0.5 rounded text-xs">***</code>
              </p>
              <p className="font-medium text-slate-700 mb-2">
                {t('settings.adminPassword')}: <code className="bg-slate-200 px-1.5 py-0.5 rounded text-xs">********</code>
              </p>
              <p className="text-xs text-slate-500">{t('settings.adminHint')}</p>
              <p className="text-xs text-amber-600 mt-2 flex items-center gap-1">
                <AlertTriangle className="w-3 h-3" />
                Credentials are configured via .env file at build time
              </p>
            </div>
          </div>
        </div>
      </div>

      {/* Actions */}
      {isAdmin && (
        <div className="flex items-center justify-between">
          <button onClick={() => setShowResetConfirm(true)} className="flex items-center gap-1.5 px-4 py-2 text-sm text-red-500 hover:bg-red-50 border border-red-200 rounded-xl transition-all">
            <RotateCcw className="w-4 h-4" />{t('settings.resetData')}
          </button>
          <button onClick={handleSave} className="flex items-center gap-1.5 px-6 py-2.5 bg-gradient-to-r from-blue-500 to-blue-600 hover:from-blue-600 hover:to-blue-700 text-white rounded-xl text-sm font-medium shadow-lg shadow-blue-500/25 transition-all">
            <Save className="w-4 h-4" />{t('settings.save')}
          </button>
        </div>
      )}

      {saved && (
        <div className="bg-emerald-50 border border-emerald-200 rounded-xl p-3 flex items-center gap-2">
          <CheckCircle2 className="w-4 h-4 text-emerald-500" />
          <span className="text-sm text-emerald-600 font-medium">{t('settings.saved')}</span>
        </div>
      )}

      {/* Reset modal */}
      {showResetConfirm && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-sm" onClick={() => setShowResetConfirm(false)}>
          <div className="bg-white rounded-2xl shadow-2xl border border-slate-200 w-full max-w-md mx-4 p-6" onClick={(e) => e.stopPropagation()}>
            <div className="flex items-center gap-3 mb-4">
              <div className="w-10 h-10 bg-red-100 rounded-xl flex items-center justify-center">
                <AlertTriangle className="w-5 h-5 text-red-500" />
              </div>
              <div>
                <h3 className="text-lg font-semibold text-slate-800">{t('settings.resetTitle')}</h3>
                <p className="text-sm text-slate-400">{t('settings.resetCannotUndo')}</p>
              </div>
            </div>
            <p className="text-sm text-slate-600 mb-5">{t('settings.resetDesc')}</p>
            <div className="flex justify-end gap-2">
              <button onClick={() => setShowResetConfirm(false)} className="px-4 py-2 text-sm text-slate-600 hover:bg-slate-100 rounded-xl transition-colors">
                {t('settings.cancel')}
              </button>
              <button onClick={handleReset} className="px-4 py-2 bg-red-500 hover:bg-red-600 text-white rounded-xl text-sm font-medium transition-colors">
                {t('settings.resetConfirm')}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
