import { memo, useState, useMemo, useCallback, useEffect, useDeferredValue } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import type { ChangeRequestEntry, TranslationEntry } from '../types';
import { loadFromServerApi } from '../utils/fileLoader';
import { Search, Filter, Send, CheckCircle2, AlertCircle, X, FileText, ArrowRight, ChevronLeft, ChevronRight, RefreshCw, Copy, RotateCcw, Eraser } from 'lucide-react';

const PAGE_SIZE_OPTIONS = [50, 100, 250, 500];

function normalizeTranslationText(value: string) {
  return String(value ?? '')
    .replace(/\u00a0/g, ' ')
    .replace(/[\u200B-\u200D\uFEFF]/g, '')
    .trim();
}

function isTranslatedValue(original: string, translation: string) {
  const cleanOriginal = normalizeTranslationText(original);
  const cleanTranslation = normalizeTranslationText(translation);
  return cleanTranslation !== '' && cleanTranslation !== cleanOriginal;
}


interface TranslationRowProps {
  entry: TranslationEntry;
  langCode: string;
  editedValue: string | undefined;
  onChange: (original: string, value: string) => void;
}

const TranslationRow = memo(function TranslationRow({ entry, langCode, editedValue, onChange }: TranslationRowProps) {
  const externalValue = editedValue !== undefined ? editedValue : entry.translation;
  const [draftValue, setDraftValue] = useState(externalValue);

  useEffect(() => {
    setDraftValue(externalValue);
  }, [externalValue, entry.id, entry.original]);

  const isChanged = editedValue !== undefined && editedValue !== entry.translation;
  const isUntranslated = !isTranslatedValue(entry.original, draftValue);
  const isSameAsOriginal = draftValue.trim() !== '' && draftValue.trim() === entry.original.trim();
  const rows = Math.min(3, Math.max(1, Math.ceil((draftValue.length || 1) / 150)));

  const updateValue = useCallback((value: string) => {
    setDraftValue(value);
    onChange(entry.original, value);
  }, [entry.original, onChange]);

  const setFromOriginal = () => updateValue(entry.original);
  const clearValue = () => updateValue('');
  const revertValue = () => updateValue(entry.translation);

  return (
    <article className={`translation-row group mx-2 my-1.5 rounded-lg border px-3 py-2 ${
      isChanged
        ? 'border-blue-300 bg-blue-50/60'
        : isUntranslated
          ? 'border-amber-200 bg-amber-50/35'
          : 'border-slate-200 bg-white hover:border-blue-200'
    }`}>
      <div className="grid grid-cols-[20px_minmax(0,1fr)] gap-2">
        <div className="pt-1.5">
          {isChanged ? (
            <div className="h-2.5 w-2.5 rounded-full bg-blue-500 ring-2 ring-blue-100" />
          ) : isUntranslated ? (
            <AlertCircle className="h-3.5 w-3.5 text-amber-500" />
          ) : (
            <CheckCircle2 className="h-3.5 w-3.5 text-emerald-500" />
          )}
        </div>

        <div className="min-w-0">
          <div className="mb-1.5 flex items-center justify-between gap-2">
            <div className="min-w-0 flex items-center gap-2">
              <span className="shrink-0 rounded-md bg-slate-100 px-1.5 py-0.5 text-[10px] font-bold uppercase tracking-wide text-slate-500">EN</span>
              <p className="truncate font-mono text-[12px] leading-5 text-slate-700" title={entry.original}>{entry.original}</p>
            </div>
            <div className="flex shrink-0 items-center gap-1">
              {isChanged && <span className="rounded-full bg-blue-100 px-2 py-0.5 text-[10px] font-semibold text-blue-700">{t('editor.changed')}</span>}
              {isSameAsOriginal && <span className="rounded-full bg-amber-100 px-2 py-0.5 text-[10px] font-semibold text-amber-700">{t('editor.matchesOriginal')}</span>}
              <button
                type="button"
                onClick={setFromOriginal}
                className="inline-flex items-center gap-1 rounded-md border border-slate-200 bg-white px-2 py-1 text-[11px] font-medium text-blue-600 hover:bg-blue-50"
                title={t('editor.copyOriginal')}
              >
                <Copy className="h-3 w-3" />
                {t('editor.copyOriginal')}
              </button>
            </div>
          </div>

          <div className="rounded-lg border border-slate-200 bg-white p-2">
            <div className="mb-1.5 flex items-center justify-between gap-2">
              <span className="text-[10px] font-bold uppercase tracking-[0.16em] text-blue-500">{langCode} • {t('editor.translation')}</span>
              <div className="flex items-center gap-1.5">
                <span className="text-[10px] text-slate-400">{draftValue.length} {t('editor.characters')}</span>
                <button
                  type="button"
                  onClick={clearValue}
                  className="inline-flex items-center gap-1 rounded-md px-1.5 py-0.5 text-[10px] font-medium text-slate-500 hover:bg-slate-100 hover:text-slate-700"
                  title={t('editor.clear')}
                >
                  <Eraser className="h-3 w-3" />
                  {t('editor.clear')}
                </button>
                {isChanged && (
                  <button
                    type="button"
                    onClick={revertValue}
                    className="inline-flex items-center gap-1 rounded-md px-1.5 py-0.5 text-[10px] font-medium text-slate-500 hover:bg-slate-100 hover:text-slate-700"
                    title={t('editor.revert')}
                  >
                    <RotateCcw className="h-3 w-3" />
                    {t('editor.revert')}
                  </button>
                )}
              </div>
            </div>
            <textarea
              data-original={entry.original}
              value={draftValue}
              rows={rows}
              onChange={(e) => updateValue(e.currentTarget.value)}
              placeholder={t('editor.enterTranslation')}
              spellCheck={false}
              className={`block w-full resize-none overflow-hidden rounded-md border px-2.5 py-1.5 font-mono text-[13px] leading-5 text-slate-900 outline-none placeholder:text-slate-300 focus:border-blue-400 focus:ring-2 focus:ring-blue-500/10 ${
                isUntranslated
                  ? 'border-amber-200 bg-amber-50/20'
                  : isChanged
                    ? 'border-blue-300 bg-blue-50/20'
                    : 'border-slate-200 bg-white hover:border-slate-300'
              }`}
            />
          </div>
        </div>
      </div>
    </article>
  );
});

interface PaginationProps {
  page: number;
  totalPages: number;
  pageSize: number;
  filteredCount: number;
  totalCount: number;
  changedCount: number;
  onPageChange: (page: number) => void;
  onPageSizeChange: (pageSize: number) => void;
  compact?: boolean;
}

function PaginationBar({ page, totalPages, pageSize, filteredCount, totalCount, changedCount, onPageChange, onPageSizeChange, compact = false }: PaginationProps) {
  return (
    <div className="flex items-center justify-between gap-3 flex-wrap">
      <div className="text-xs text-slate-400">
        {filteredCount} {t('editor.entriesOf')} {totalCount} {t('editor.entries')}
        {changedCount > 0 && (
          <span className="ml-2 text-blue-500 font-medium">• {changedCount} {t('editor.changed')}</span>
        )}
      </div>
      <div className="flex items-center gap-2">
        <select
          value={pageSize}
          onChange={(e) => onPageSizeChange(Number(e.target.value))}
          className="px-2 py-1.5 bg-slate-50 border border-slate-200 rounded-lg text-xs text-slate-600 focus:outline-none focus:ring-2 focus:ring-blue-500/20"
          title="Rows per page"
        >
          {PAGE_SIZE_OPTIONS.map(size => (
            <option key={size} value={size}>{size}/page</option>
          ))}
        </select>
        <button
          type="button"
          onClick={() => onPageChange(page - 1)}
          disabled={page <= 1}
          className="p-1.5 rounded-lg border border-slate-200 text-slate-500 disabled:opacity-40 disabled:cursor-not-allowed hover:bg-slate-50"
        >
          <ChevronLeft className="w-4 h-4" />
        </button>
        <span className="text-xs text-slate-500 tabular-nums min-w-[88px] text-center">
          {page} / {totalPages}
        </span>
        <button
          type="button"
          onClick={() => onPageChange(page + 1)}
          disabled={page >= totalPages}
          className="p-1.5 rounded-lg border border-slate-200 text-slate-500 disabled:opacity-40 disabled:cursor-not-allowed hover:bg-slate-50"
        >
          <ChevronRight className="w-4 h-4" />
        </button>
        {!compact && totalPages > 1 && (
          <input
            type="number"
            min={1}
            max={totalPages}
            value={page}
            onChange={(e) => onPageChange(Number(e.target.value) || 1)}
            className="w-16 px-2 py-1.5 bg-slate-50 border border-slate-200 rounded-lg text-xs text-slate-600 focus:outline-none focus:ring-2 focus:ring-blue-500/20"
            title="Go to page"
          />
        )}
      </div>
    </div>
  );
}

export default function TranslationEditor() {
  const { selectedLanguage, translations, languages, searchQuery, setSearchQuery, filterMode, setFilterMode, isAdmin, submitChangeRequest, approveRequest, forceSyncFiles } = useStore();
  const [editedTranslations, setEditedTranslations] = useState<Record<string, string>>({});
  const [showSubmitModal, setShowSubmitModal] = useState(false);
  const [requestName, setRequestName] = useState('');
  const [requestAuthor, setRequestAuthor] = useState('');
  const [submitSuccess, setSubmitSuccess] = useState(false);
  const [syncStatus, setSyncStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [pageSize, setPageSize] = useState(50);
  const [page, setPage] = useState(1);

  const file = selectedLanguage ? translations[selectedLanguage] : null;
  const lang = languages.find(l => l.file === selectedLanguage);

  const deferredSearchQuery = useDeferredValue(searchQuery);

  const entryMap = useMemo(() => {
    const map = new Map<string, TranslationEntry>();
    if (file) {
      // User edits are keyed by the original source string, so changes can be
      // applied safely after the file is reloaded and row ids/indexes change.
      for (const entry of file.entries) {
        if (!map.has(entry.original)) map.set(entry.original, entry);
      }
    }
    return map;
  }, [file]);

  const filteredEntries = useMemo(() => {
    if (!file) return [];

    const getEffectiveTranslation = (entry: TranslationEntry) =>
      editedTranslations[entry.original] ?? entry.translation;

    const hasUserChange = (entry: TranslationEntry) =>
      editedTranslations[entry.original] !== undefined && editedTranslations[entry.original] !== entry.translation;

    const getSavedStatusPriority = (entry: TranslationEntry) => {
      if (hasUserChange(entry)) return 0;
      if (!isTranslatedValue(entry.original, entry.translation)) return 1;
      return 2;
    };

    const query = deferredSearchQuery.trim().toLowerCase();
    const entries = file.entries
      .map((entry, index) => ({ entry, index }))
      .filter(({ entry }) => {
        const effectiveTranslation = getEffectiveTranslation(entry);
        if (query && !(
          entry.original.toLowerCase().includes(query) ||
          effectiveTranslation.toLowerCase().includes(query)
        )) {
          return false;
        }

        if (filterMode === 'changed') {
          return hasUserChange(entry);
        }

        // Keep Translated/Untranslated membership based on the saved file state.
        // This prevents the row from disappearing while the user is typing under
        // an active filter. The separate Changed filter shows live unsaved edits.
        if (filterMode === 'translated') {
          return isTranslatedValue(entry.original, entry.translation);
        }
        if (filterMode === 'untranslated') {
          return !isTranslatedValue(entry.original, entry.translation);
        }
        return true;
      })
      .sort((a, b) => {
        const priorityDelta = getSavedStatusPriority(a.entry) - getSavedStatusPriority(b.entry);
        return priorityDelta || a.index - b.index;
      })
      .map(({ entry }) => entry);

    return entries;
  }, [file, deferredSearchQuery, filterMode, editedTranslations]);

  const totalPages = Math.max(1, Math.ceil(filteredEntries.length / pageSize));

  useEffect(() => {
    setPage(1);
  }, [selectedLanguage, deferredSearchQuery, filterMode, pageSize]);

  useEffect(() => {
    if (page > totalPages) setPage(totalPages);
  }, [page, totalPages]);

  const goToPage = useCallback((nextPage: number) => {
    const clampedPage = Math.min(Math.max(1, nextPage), totalPages);
    setPage(clampedPage);
  }, [totalPages]);

  const pagedEntries = useMemo(() => {
    const start = (page - 1) * pageSize;
    return filteredEntries.slice(start, start + pageSize);
  }, [filteredEntries, page, pageSize]);

  const changedEntries = useMemo(() => {
    return Object.entries(editedTranslations).flatMap(([original, val]) => {
      const entry = entryMap.get(original);
      if (!entry || entry.translation === val) return [];
      return [{ entryId: original, original, oldTranslation: entry.translation, newTranslation: val } as ChangeRequestEntry];
    });
  }, [editedTranslations, entryMap]);

  const handleTranslationChange = useCallback((original: string, value: string) => {
    const savedValue = entryMap.get(original)?.translation;
    setEditedTranslations(prev => {
      if (savedValue !== undefined && value === savedValue) {
        const next = { ...prev };
        delete next[original];
        return next;
      }
      return { ...prev, [original]: value };
    });
  }, [entryMap]);

  const handleSubmit = () => {
    if (!requestName.trim() || (!isAdmin && !requestAuthor.trim()) || changedEntries.length === 0 || !selectedLanguage) return;
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

  const handleForceSync = useCallback(async () => {
    if (syncStatus === 'loading') return;
    setSyncStatus('loading');
    try {
      const result = await loadFromServerApi();
      if (result.languages.length === 0) throw new Error(result.errors[0] || 'No localization files loaded');
      forceSyncFiles(result);
      setEditedTranslations({});
      setShowSubmitModal(false);
      setSyncStatus('success');
      setTimeout(() => setSyncStatus('idle'), 3000);
    } catch {
      setSyncStatus('error');
      setTimeout(() => setSyncStatus('idle'), 4000);
    }
  }, [forceSyncFiles, syncStatus]);

  const openSubmitModal = useCallback(() => setShowSubmitModal(true), []);


  const stats = useMemo(() => {
    if (!file) return { total: 0, translated: 0, untranslated: 0 };
    const total = file.entries.length;
    const translated = file.entries.filter(e => isTranslatedValue(e.original, e.translation)).length;
    return { total, translated, untranslated: total - translated };
  }, [file]);

  useEffect(() => {
    // Drop drafts for rows that disappeared after an external file sync.
    setEditedTranslations(prev => {
      let changed = false;
      const next: Record<string, string> = {};
      for (const [original, value] of Object.entries(prev)) {
        if (entryMap.has(original)) next[original] = value;
        else changed = true;
      }
      return changed ? next : prev;
    });
  }, [entryMap]);

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

  const syncButton = (
    <button
      type="button"
      onClick={handleForceSync}
      disabled={syncStatus === 'loading'}
      className={`flex items-center gap-1.5 px-4 py-2 rounded-xl text-sm font-medium transition-all ${
        syncStatus === 'loading'
          ? 'bg-slate-100 text-slate-400 cursor-wait'
          : 'bg-white text-blue-600 border border-blue-200 hover:bg-blue-50 hover:border-blue-300 shadow-sm'
      }`}
      title={t('editor.sync')}
    >
      <RefreshCw className={`w-3.5 h-3.5 ${syncStatus === 'loading' ? 'animate-spin' : ''}`} />
      {syncStatus === 'loading' ? t('editor.syncing') : t('editor.sync')}
    </button>
  );

  const submitButton = (
    <button
      onClick={openSubmitModal}
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
      {changedEntries.length > 0 && <span className="ml-1 rounded bg-white/20 px-1.5 py-0.5 text-[10px]">{changedEntries.length}</span>}
    </button>
  );

  return (
    <div className="flex-1 flex flex-col min-h-0 relative">
      {/* Toolbar */}
      <div className="bg-white rounded-t-2xl border border-slate-200/80 border-b-0 px-4 py-3 sticky top-0 z-20 shadow-sm">
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
          <div className="flex items-center gap-2 text-xs flex-wrap justify-end">
            <span className="px-2.5 py-1 bg-slate-100 rounded-full text-slate-500 font-medium">{stats.total} {t('editor.total')}</span>
            <span className="px-2.5 py-1 bg-emerald-50 text-emerald-600 rounded-full font-medium">{stats.translated} {t('editor.translated')}</span>
            <span className="px-2.5 py-1 bg-amber-50 text-amber-600 rounded-full font-medium">{stats.untranslated} {t('editor.untranslated')}</span>
            {syncButton}
            {submitButton}
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
            {(['all', 'translated', 'untranslated', 'changed'] as const).map(mode => (
              <button
                key={mode}
                type="button"
                onClick={() => setFilterMode(mode)}
                className={`px-3 py-1.5 rounded-lg text-xs font-medium transition-all ${
                  filterMode === mode ? 'bg-white text-blue-600 shadow-sm' : 'text-slate-500 hover:text-slate-700'
                }`}
              >
                {mode === 'all'
                  ? t('editor.filterAll')
                  : mode === 'translated'
                    ? t('editor.filterTranslated')
                    : mode === 'untranslated'
                      ? t('editor.filterUntranslated')
                      : `${t('editor.filterChanged')}${changedEntries.length > 0 ? ` (${changedEntries.length})` : ''}`}
              </button>
            ))}
          </div>
        </div>
        <div className="mt-3 rounded-xl border border-slate-100 bg-slate-50/70 px-3 py-2">
          <PaginationBar
            page={page}
            totalPages={totalPages}
            pageSize={pageSize}
            filteredCount={filteredEntries.length}
            totalCount={file.entries.length}
            changedCount={changedEntries.length}
            onPageChange={goToPage}
            onPageSizeChange={setPageSize}
          />
        </div>
      </div>

      {/* Entries */}
      <div className="editor-entries-scroll flex-1 min-h-0 bg-white border-x border-slate-200/80 overflow-y-auto overscroll-contain">
        <div className="divide-y divide-slate-100">
          {pagedEntries.map((entry) => (
            <TranslationRow
              key={entry.id}
              entry={entry}
              langCode={selectedLanguage}
              editedValue={editedTranslations[entry.original]}
              onChange={handleTranslationChange}
            />
          ))}
        </div>
        {filteredEntries.length === 0 && (
          <div className="py-16 text-center">
            <Search className="w-8 h-8 text-slate-300 mx-auto mb-2" />
            <p className="text-sm text-slate-400">{t('editor.nothingFound')}</p>
          </div>
        )}
      </div>

      {/* Bottom bar */}
      <div className="bg-white rounded-b-2xl border border-slate-200/80 border-t-0 px-4 py-3">
        <div className="flex items-center justify-between gap-3 flex-wrap">
          <PaginationBar
            page={page}
            totalPages={totalPages}
            pageSize={pageSize}
            filteredCount={filteredEntries.length}
            totalCount={file.entries.length}
            changedCount={changedEntries.length}
            onPageChange={goToPage}
            onPageSizeChange={setPageSize}
            compact
          />
          <div className="flex items-center gap-2 ml-auto">
            {changedEntries.length > 0 && (
              <button onClick={() => setEditedTranslations({})} className="px-3 py-1.5 text-xs text-slate-500 hover:text-slate-700 hover:bg-slate-100 rounded-lg transition-all">
                {t('editor.reset')}
              </button>
            )}
            {syncButton}
            {submitButton}
          </div>
        </div>
        {syncStatus === 'success' && (
          <div className="mt-2 p-2 bg-blue-50 border border-blue-200 rounded-lg flex items-center gap-2">
            <RefreshCw className="w-4 h-4 text-blue-500" />
            <span className="text-xs text-blue-600 font-medium">{t('editor.syncSuccess')}</span>
          </div>
        )}
        {syncStatus === 'error' && (
          <div className="mt-2 p-2 bg-red-50 border border-red-200 rounded-lg flex items-center gap-2">
            <AlertCircle className="w-4 h-4 text-red-500" />
            <span className="text-xs text-red-600 font-medium">{t('editor.syncError')}</span>
          </div>
        )}
        {submitSuccess && (
          <div className="mt-2 p-2 bg-emerald-50 border border-emerald-200 rounded-lg flex items-center gap-2">
            <CheckCircle2 className="w-4 h-4 text-emerald-500" />
            <span className="text-xs text-emerald-600 font-medium">{isAdmin ? t('editor.changesApplied') : t('editor.requestSent')}</span>
          </div>
        )}
      </div>

      {/* Submit Modal */}
      {showSubmitModal && (
        <div className="fixed inset-0 bg-slate-900/40 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-2xl shadow-2xl max-w-lg w-full overflow-hidden">
            <div className="px-6 py-4 border-b border-slate-100">
              <h3 className="text-lg font-semibold text-slate-800">
                {isAdmin ? t('modal.applyTitle') : t('modal.submitTitle')}
              </h3>
              <p className="text-sm text-slate-400 mt-0.5">
                {changedEntries.length} {t('modal.changesReady')}
              </p>
            </div>
            <div className="p-6 space-y-4">
              {!isAdmin && (
                <div>
                  <label className="block text-xs font-medium text-slate-500 mb-1.5">{t('modal.authorName')}</label>
                  <input
                    type="text"
                    value={requestAuthor}
                    onChange={(e) => setRequestAuthor(e.target.value)}
                    placeholder={t('modal.authorPlaceholder')}
                    className="w-full px-3 py-2.5 border border-slate-200 rounded-xl text-sm focus:outline-none focus:ring-2 focus:ring-blue-500/30 focus:border-blue-400 transition-all placeholder-slate-400"
                  />
                </div>
              )}
              {isAdmin && <input type="hidden" value="Administrator" onChange={() => {}} />}
              <div>
                <label className="block text-xs font-medium text-slate-500 mb-1.5">{t('modal.requestName')}</label>
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
                disabled={!requestName.trim() || (!isAdmin && !requestAuthor.trim())}
                className={`flex items-center gap-1.5 px-5 py-2 rounded-xl text-sm font-medium transition-all ${
                  requestName.trim() && (isAdmin || requestAuthor.trim())
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
