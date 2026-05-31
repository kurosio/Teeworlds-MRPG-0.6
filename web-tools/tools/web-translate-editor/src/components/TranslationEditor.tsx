import { useState, useMemo, useCallback } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import type { ChangeRequestEntry } from '../types';
import { Search, Filter, Send, CheckCircle2, AlertCircle, X, FileText, ArrowRight } from 'lucide-react';

export default function TranslationEditor() {
  const { selectedLanguage, translations, languages, searchQuery, setSearchQuery, filterMode, setFilterMode, isAdmin, submitChangeRequest, approveRequest } = useStore();
  const [editedTranslations, setEditedTranslations] = useState<Record<string, string>>({});
  const [showSubmitModal, setShowSubmitModal] = useState(false);
  const [requestName, setRequestName] = useState('');
  const [requestAuthor, setRequestAuthor] = useState('');
  const [submitSuccess, setSubmitSuccess] = useState(false);

  const file = selectedLanguage ? translations[selectedLanguage] : null;
  const lang = languages.find(l => l.file === selectedLanguage);

  const isTranslated = useCallback((original: string, translation: string) => {
    return translation.trim() !== '' && translation.trim() !== original.trim();
  }, []);

  const filteredEntries = useMemo(() => {
    if (!file) return [];
    let entries = file.entries;
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase();
      entries = entries.filter(e =>
        e.original.toLowerCase().includes(q) ||
        e.translation.toLowerCase().includes(q) ||
        (editedTranslations[e.id] || '').toLowerCase().includes(q)
      );
    }
    if (filterMode === 'translated') {
      entries = entries.filter(e => {
        const current = editedTranslations[e.id] !== undefined ? editedTranslations[e.id] : e.translation;
        return isTranslated(e.original, current);
      });
    } else if (filterMode === 'untranslated') {
      entries = entries.filter(e => {
        const current = editedTranslations[e.id] !== undefined ? editedTranslations[e.id] : e.translation;
        return !isTranslated(e.original, current);
      });
    }
    return entries;
  }, [file, searchQuery, filterMode, editedTranslations, isTranslated]);

  const changedEntries = useMemo(() => {
    if (!file) return [];
    return Object.entries(editedTranslations)
      .filter(([id, val]) => {
        const entry = file.entries.find(e => e.id === id);
        return entry && entry.translation !== val;
      })
      .map(([id, val]) => {
        const entry = file.entries.find(e => e.id === id)!;
        return { entryId: id, original: entry.original, oldTranslation: entry.translation, newTranslation: val } as ChangeRequestEntry;
      });
  }, [editedTranslations, file]);

  const handleTranslationChange = useCallback((entryId: string, value: string) => {
    setEditedTranslations(prev => ({ ...prev, [entryId]: value }));
  }, []);

  const handleSubmit = () => {
    if (!requestName.trim() || !requestAuthor.trim() || changedEntries.length === 0 || !selectedLanguage) return;
    const authorName = isAdmin ? 'Administrator' : requestAuthor.trim();
    if (isAdmin) {
      submitChangeRequest(requestName, authorName, selectedLanguage, changedEntries);
      const state = useStore.getState();
      const lastReq = state.changeRequests[state.changeRequests.length - 1];
      if (lastReq) approveRequest(lastReq.id);
    } else {
      submitChangeRequest(requestName, authorName, selectedLanguage, changedEntries);
    }
    setEditedTranslations({});
    setRequestName('');
    setRequestAuthor('');
    setShowSubmitModal(false);
    setSubmitSuccess(true);
    setTimeout(() => setSubmitSuccess(false), 3000);
  };

  const stats = useMemo(() => {
    if (!file) return { total: 0, translated: 0, untranslated: 0 };
    const total = file.entries.length;
    const translated = file.entries.filter(e => {
      const current = editedTranslations[e.id] !== undefined ? editedTranslations[e.id] : e.translation;
      return isTranslated(e.original, current);
    }).length;
    return { total, translated, untranslated: total - translated };
  }, [file, editedTranslations, isTranslated]);

  if (!selectedLanguage || !file) {
    return (
      <div className="flex-1 flex items-center justify-center bg-gradient-to-br from-slate-50 to-blue-50/30 rounded-2xl border border-slate-200/60 border-dashed">
        <div className="text-center px-8">
          <div className="w-20 h-20 bg-gradient-to-br from-blue-100 to-indigo-100 rounded-2xl flex items-center justify-center mx-auto mb-4">
            <FileText className="w-10 h-10 text-blue-400" />
          </div>
          <h3 className="text-lg font-semibold text-slate-600 mb-1">{t('editor.selectFile')}</h3>
          <p className="text-sm text-slate-400 max-w-xs">{t('editor.selectFileDesc')}</p>
        </div>
      </div>
    );
  }

  return (
    <div className="flex-1 flex flex-col min-h-0">
      {/* Toolbar */}
      <div className="bg-white rounded-t-2xl border border-slate-200/80 border-b-0 px-4 py-3">
        <div className="flex items-center justify-between gap-4 flex-wrap">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 bg-gradient-to-br from-blue-500 to-indigo-500 rounded-lg flex items-center justify-center text-white text-xs font-bold uppercase">
              {selectedLanguage}
            </div>
            <div>
              <h2 className="text-base font-semibold text-slate-800">{lang?.name}</h2>
              <p className="text-xs text-slate-400">{lang?.file}.txt</p>
            </div>
          </div>
          <div className="flex items-center gap-2 text-xs">
            <span className="px-2.5 py-1 bg-slate-100 rounded-full text-slate-500 font-medium">{stats.total} {t('editor.total')}</span>
            <span className="px-2.5 py-1 bg-emerald-50 text-emerald-600 rounded-full font-medium">{stats.translated} {t('editor.translated')}</span>
            <span className="px-2.5 py-1 bg-amber-50 text-amber-600 rounded-full font-medium">{stats.untranslated} {t('editor.untranslated')}</span>
          </div>
        </div>
        <div className="flex items-center gap-3 mt-3">
          <div className="flex-1 relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-slate-400" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder={t('editor.searchPlaceholder')}
              className="w-full pl-9 pr-8 py-2 bg-slate-50 border border-slate-200 rounded-xl text-sm focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400 transition-all placeholder-slate-400"
            />
            {searchQuery && (
              <button onClick={() => setSearchQuery('')} className="absolute right-2.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-slate-600">
                <X className="w-4 h-4" />
              </button>
            )}
          </div>
          <div className="flex items-center gap-1 bg-slate-50 rounded-xl border border-slate-200 p-0.5">
            <Filter className="w-4 h-4 text-slate-400 ml-2" />
            {(['all', 'translated', 'untranslated'] as const).map(mode => (
              <button
                key={mode}
                onClick={() => setFilterMode(mode)}
                className={`px-3 py-1.5 rounded-lg text-xs font-medium transition-all ${
                  filterMode === mode ? 'bg-white text-blue-600 shadow-sm' : 'text-slate-500 hover:text-slate-700'
                }`}
              >
                {mode === 'all' ? t('editor.filterAll') : mode === 'translated' ? t('editor.filterTranslated') : t('editor.filterUntranslated')}
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Entries */}
      <div className="flex-1 bg-white border-x border-slate-200/80 overflow-y-auto">
        <div className="divide-y divide-slate-100">
          {filteredEntries.map((entry) => {
            const currentValue = editedTranslations[entry.id] !== undefined ? editedTranslations[entry.id] : entry.translation;
            const isChanged = editedTranslations[entry.id] !== undefined && editedTranslations[entry.id] !== entry.translation;
            const isUntranslated = !isTranslated(entry.original, currentValue);

            return (
              <div key={entry.id} className={`px-4 py-3 transition-colors ${isChanged ? 'bg-blue-50/50' : isUntranslated ? 'bg-amber-50/30' : 'hover:bg-slate-50/50'}`}>
                <div className="flex items-start gap-3">
                  <div className="mt-1 flex-shrink-0">
                    {isChanged ? (
                      <div className="w-2 h-2 rounded-full bg-blue-500 ring-4 ring-blue-100" />
                    ) : isUntranslated ? (
                      <AlertCircle className="w-4 h-4 text-amber-400" />
                    ) : (
                      <CheckCircle2 className="w-4 h-4 text-emerald-400" />
                    )}
                  </div>
                  <div className="flex-1 min-w-0 space-y-2">
                    <div className="flex items-start gap-2">
                      <span className="text-[10px] font-bold uppercase text-slate-400 bg-slate-100 px-1.5 py-0.5 rounded flex-shrink-0 mt-0.5">EN</span>
                      <p className="text-sm text-slate-700 font-mono break-all">{entry.original}</p>
                    </div>
                    <div className="flex items-center gap-2">
                      <span className="text-[10px] font-bold uppercase text-blue-400 bg-blue-50 px-1.5 py-0.5 rounded flex-shrink-0">{selectedLanguage}</span>
                      <ArrowRight className="w-3 h-3 text-slate-300 flex-shrink-0" />
                      <input
                        type="text"
                        value={currentValue}
                        onChange={(e) => handleTranslationChange(entry.id, e.target.value)}
                        placeholder={t('editor.enterTranslation')}
                        className={`flex-1 px-3 py-1.5 border rounded-lg text-sm font-mono transition-all focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400 ${
                          isUntranslated ? 'border-amber-200 bg-amber-50/50 placeholder-amber-300'
                          : isChanged ? 'border-blue-300 bg-blue-50/30'
                          : 'border-slate-200 bg-white hover:border-slate-300'
                        }`}
                      />
                    </div>
                    {currentValue.trim() !== '' && currentValue.trim() === entry.original.trim() && (
                      <p className="text-[10px] text-amber-500 ml-14">{t('editor.matchesOriginal')}</p>
                    )}
                  </div>
                </div>
              </div>
            );
          })}
          {filteredEntries.length === 0 && (
            <div className="py-16 text-center">
              <Search className="w-8 h-8 text-slate-300 mx-auto mb-2" />
              <p className="text-sm text-slate-400">{t('editor.nothingFound')}</p>
            </div>
          )}
        </div>
      </div>

      {/* Bottom bar */}
      <div className="bg-white rounded-b-2xl border border-slate-200/80 border-t-0 px-4 py-3">
        <div className="flex items-center justify-between">
          <div className="text-xs text-slate-400">
            {filteredEntries.length} {t('editor.entriesOf')} {file.entries.length} {t('editor.entries')}
            {changedEntries.length > 0 && (
              <span className="ml-2 text-blue-500 font-medium">• {changedEntries.length} {t('editor.changed')}</span>
            )}
          </div>
          <div className="flex items-center gap-2">
            {changedEntries.length > 0 && (
              <button onClick={() => setEditedTranslations({})} className="px-3 py-1.5 text-xs text-slate-500 hover:text-slate-700 hover:bg-slate-100 rounded-lg transition-all">
                {t('editor.reset')}
              </button>
            )}
            <button
              onClick={() => setShowSubmitModal(true)}
              disabled={changedEntries.length === 0}
              className={`flex items-center gap-1.5 px-4 py-2 rounded-xl text-sm font-medium transition-all ${
                changedEntries.length > 0
                  ? isAdmin
                    ? 'bg-gradient-to-r from-emerald-500 to-emerald-600 hover:from-emerald-600 hover:to-emerald-700 text-white shadow-lg shadow-emerald-500/25'
                    : 'bg-gradient-to-r from-blue-500 to-blue-600 hover:from-blue-600 hover:to-blue-700 text-white shadow-lg shadow-blue-500/25'
                  : 'bg-slate-100 text-slate-400 cursor-not-allowed'
              }`}
            >
              <Send className="w-3.5 h-3.5" />
              {isAdmin ? t('editor.applyChanges') : t('editor.submitRequest')}
            </button>
          </div>
        </div>
        {submitSuccess && (
          <div className="mt-2 p-2 bg-emerald-50 border border-emerald-200 rounded-lg flex items-center gap-2">
            <CheckCircle2 className="w-4 h-4 text-emerald-500" />
            <span className="text-xs text-emerald-600 font-medium">{isAdmin ? t('editor.changesApplied') : t('editor.requestSent')}</span>
          </div>
        )}
      </div>

      {/* Submit modal */}
      {showSubmitModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 backdrop-blur-sm" onClick={() => setShowSubmitModal(false)}>
          <div className="bg-white rounded-2xl shadow-2xl border border-slate-200 w-full max-w-lg mx-4 overflow-hidden" onClick={(e) => e.stopPropagation()}>
            <div className="px-6 py-4 border-b border-slate-100 bg-gradient-to-r from-slate-50 to-white">
              <h3 className="text-lg font-semibold text-slate-800">{isAdmin ? t('modal.titleAdmin') : t('modal.titleGuest')}</h3>
              <p className="text-sm text-slate-400 mt-0.5">{changedEntries.length} {t('modal.changesFor')} {lang?.name} ({selectedLanguage})</p>
            </div>
            <div className="px-6 py-4 space-y-4">
              <div>
                <label className="block text-sm font-medium text-slate-700 mb-1.5">{t('modal.authorLabel')} <span className="text-red-400">*</span></label>
                <input
                  type="text"
                  value={requestAuthor}
                  onChange={(e) => setRequestAuthor(e.target.value)}
                  placeholder={t('modal.authorPlaceholder')}
                  className="w-full px-3 py-2.5 border border-slate-200 rounded-xl text-sm focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400 transition-all placeholder-slate-400"
                  autoFocus
                />
              </div>
              <div>
                <label className="block text-sm font-medium text-slate-700 mb-1.5">{t('modal.nameLabel')} <span className="text-red-400">*</span></label>
                <input
                  type="text"
                  value={requestName}
                  onChange={(e) => setRequestName(e.target.value)}
                  placeholder={t('modal.namePlaceholder')}
                  className="w-full px-3 py-2.5 border border-slate-200 rounded-xl text-sm focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400 transition-all placeholder-slate-400"
                />
              </div>
              <div>
                <p className="text-xs font-medium text-slate-500 mb-2">{t('modal.preview')}</p>
                <div className="max-h-60 overflow-y-auto space-y-2 bg-slate-50 rounded-xl p-3">
                  {changedEntries.map((entry, i) => (
                    <div key={i} className="bg-white rounded-lg p-2.5 border border-slate-100 text-xs">
                      <p className="text-slate-500 font-mono mb-1 truncate">{entry.original}</p>
                      <div className="flex items-center gap-2">
                        <span className="text-red-400 line-through truncate flex-1 font-mono">{entry.oldTranslation || t('modal.empty')}</span>
                        <ArrowRight className="w-3 h-3 text-slate-300 flex-shrink-0" />
                        <span className="text-emerald-600 font-medium truncate flex-1 font-mono">{entry.newTranslation || t('modal.empty')}</span>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
            <div className="px-6 py-3 bg-slate-50 border-t border-slate-100 flex justify-end gap-2">
              <button onClick={() => setShowSubmitModal(false)} className="px-4 py-2 text-sm text-slate-600 hover:bg-slate-200/60 rounded-xl transition-colors">
                {t('modal.cancel')}
              </button>
              <button
                onClick={handleSubmit}
                disabled={!requestName.trim() || !requestAuthor.trim()}
                className={`flex items-center gap-1.5 px-5 py-2 rounded-xl text-sm font-medium transition-all ${
                  requestName.trim() && requestAuthor.trim()
                    ? isAdmin
                      ? 'bg-emerald-500 hover:bg-emerald-600 text-white shadow-lg shadow-emerald-500/25'
                      : 'bg-blue-500 hover:bg-blue-600 text-white shadow-lg shadow-blue-500/25'
                    : 'bg-slate-200 text-slate-400 cursor-not-allowed'
                }`}
              >
                <Send className="w-3.5 h-3.5" />
                {isAdmin ? t('modal.apply') : t('modal.submit')}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
