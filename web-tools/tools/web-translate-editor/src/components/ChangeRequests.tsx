import { useEffect, useState } from 'react';
import { useStore } from '../store';
import { t } from '../i18n';
import {
  ClipboardList, Check, X, Clock, CheckCircle2, XCircle,
  ArrowRight, ChevronDown, ChevronUp, Trash2, Tag, Filter,
} from 'lucide-react';

export default function ChangeRequests() {
  const { changeRequests, isAdmin, approveRequest, rejectRequest, deleteRequest, languages, requestFilterTag, setRequestFilterTag, loadChangeRequests } = useStore();
  const [expandedId, setExpandedId] = useState<string | null>(null);
  const [statusFilter, setStatusFilter] = useState<'all' | 'pending' | 'approved' | 'rejected'>('all');
  const [editedEntries, setEditedEntries] = useState<Record<string, string[]>>({});

  useEffect(() => {
    loadChangeRequests();
  }, [loadChangeRequests]);

  const filteredRequests = changeRequests
    .filter(r => statusFilter === 'all' || r.status === statusFilter)
    .filter(r => !requestFilterTag || r.languageFile === requestFilterTag)
    .sort((a, b) => new Date(b.createdAt).getTime() - new Date(a.createdAt).getTime());

  const uniqueTags = [...new Set(changeRequests.map(r => r.languageFile))];

  const getStatusBadge = (status: string) => {
    switch (status) {
      case 'pending':
        return (
          <span className="inline-flex items-center gap-1 px-2.5 py-1 bg-amber-50 text-amber-600 border border-amber-200 rounded-full text-xs font-medium">
            <Clock className="w-3 h-3" />{t('requests.statusPending')}
          </span>
        );
      case 'approved':
        return (
          <span className="inline-flex items-center gap-1 px-2.5 py-1 bg-emerald-50 text-emerald-600 border border-emerald-200 rounded-full text-xs font-medium">
            <CheckCircle2 className="w-3 h-3" />{t('requests.statusApproved')}
          </span>
        );
      case 'rejected':
        return (
          <span className="inline-flex items-center gap-1 px-2.5 py-1 bg-red-50 text-red-600 border border-red-200 rounded-full text-xs font-medium">
            <XCircle className="w-3 h-3" />{t('requests.statusRejected')}
          </span>
        );
    }
  };

  const formatDate = (dateStr: string) => {
    const d = new Date(dateStr);
    return d.toLocaleString('ru-RU', { day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit' });
  };

  const ensureRequestEdits = (request: typeof changeRequests[number]) => {
    setEditedEntries(prev => prev[request.id]
      ? prev
      : { ...prev, [request.id]: request.entries.map(entry => entry.newTranslation) });
  };

  const toggleRequest = (request: typeof changeRequests[number]) => {
    if (expandedId === request.id) {
      setExpandedId(null);
      return;
    }
    ensureRequestEdits(request);
    setExpandedId(request.id);
  };

  const getRequestEntriesForApprove = (request: typeof changeRequests[number]) => {
    const edited = editedEntries[request.id];
    if (!edited) return request.entries;
    return request.entries.map((entry, index) => ({
      ...entry,
      newTranslation: edited[index] ?? entry.newTranslation,
    }));
  };

  const setRequestEntryTranslation = (requestId: string, index: number, value: string) => {
    setEditedEntries(prev => {
      const current = prev[requestId] ? [...prev[requestId]] : [];
      current[index] = value;
      return { ...prev, [requestId]: current };
    });
  };

  const resetRequestEntryTranslation = (request: typeof changeRequests[number], index: number) => {
    setRequestEntryTranslation(request.id, index, request.entries[index]?.newTranslation || '');
  };

  const handleApproveRequest = async (request: typeof changeRequests[number]) => {
    await approveRequest(request.id, getRequestEntriesForApprove(request));
    setEditedEntries(prev => {
      const next = { ...prev };
      delete next[request.id];
      return next;
    });
  };

  return (
    <div className="max-w-5xl mx-auto space-y-4">
      {/* Header */}
      <div className="theme-panel rounded-2xl shadow-sm border p-5">
        <div className="flex items-center justify-between flex-wrap gap-4">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 theme-accent-box rounded-xl flex items-center justify-center">
              <ClipboardList className="w-5 h-5 text-white" />
            </div>
            <div>
              <h2 className="text-lg font-semibold text-slate-800">{t('requests.title')}</h2>
              <p className="text-sm text-slate-400">{changeRequests.filter(r => r.status === 'pending').length} {t('requests.pending')}</p>
            </div>
          </div>
          <div className="flex items-center gap-2 flex-wrap">
            {uniqueTags.length > 0 && (
              <div className="flex items-center gap-1 mr-2">
                <Tag className="w-3.5 h-3.5 text-slate-400" />
                <button
                  onClick={() => setRequestFilterTag(null)}
                  className={`px-2.5 py-1 rounded-lg text-xs font-medium transition-all ${
                    !requestFilterTag ? 'theme-filter-active border' : 'theme-filter-button border'
                  }`}
                >
                  {t('requests.all')}
                </button>
                {uniqueTags.map(tag => {
                  const lang = languages.find(l => l.file === tag);
                  return (
                    <button
                      key={tag}
                      onClick={() => setRequestFilterTag(requestFilterTag === tag ? null : tag)}
                      className={`px-2.5 py-1 rounded-lg text-xs font-medium transition-all ${
                        requestFilterTag === tag ? 'theme-filter-active border' : 'theme-filter-button border'
                      }`}
                    >
                      {lang?.name || tag} ({tag})
                    </button>
                  );
                })}
              </div>
            )}
            <div className="flex items-center gap-1 theme-filter-shell rounded-xl border p-0.5">
              <Filter className="w-3.5 h-3.5 text-slate-400 ml-2" />
              {(['all', 'pending', 'approved', 'rejected'] as const).map(s => (
                <button
                  key={s}
                  onClick={() => setStatusFilter(s)}
                  className={`px-2.5 py-1 rounded-lg text-xs font-medium transition-all ${
                    statusFilter === s ? 'theme-filter-active shadow-sm' : 'theme-filter-button'
                  }`}
                >
                  {s === 'all' ? t('requests.all') : s === 'pending' ? t('requests.statusPending') : s === 'approved' ? t('requests.statusApproved') : t('requests.statusRejected')}
                </button>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* List */}
      <div className="space-y-2">
        {filteredRequests.map(request => {
          const isExpanded = expandedId === request.id;
          return (
            <div key={request.id} className="theme-panel rounded-xl shadow-sm border overflow-hidden transition-all">
              <div className="px-5 py-3.5 flex items-center gap-4 cursor-pointer hover:bg-slate-50/50 transition-colors" onClick={() => toggleRequest(request)}>
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2 flex-wrap">
                    <h3 className="text-sm font-semibold text-slate-800">{request.name}</h3>
                    {getStatusBadge(request.status)}
                    <span className="inline-flex items-center gap-1 px-2 py-0.5 bg-indigo-50 text-indigo-600 border border-indigo-100 rounded-full text-[10px] font-bold uppercase">{request.languageFile}</span>
                  </div>
                  <div className="flex items-center gap-3 mt-1 flex-wrap">
                    <span className="text-xs text-blue-500 font-medium">{t('requests.from')}: {request.author}</span>
                    <span className="text-xs text-slate-400">•</span>
                    <span className="text-xs text-slate-400">{formatDate(request.createdAt)}</span>
                    <span className="text-xs text-slate-400">•</span>
                    <span className="text-xs text-slate-500">{request.entries.length} {t('requests.changes')}</span>
                    <span className="text-xs text-slate-400">•</span>
                    <span className="text-xs text-slate-500">{request.languageName}</span>
                  </div>
                </div>
                <div className="flex items-center gap-2">
                  {isAdmin && request.status === 'pending' && (
                    <>
                      <button onClick={(e) => { e.stopPropagation(); handleApproveRequest(request); }} className="flex items-center gap-1 px-3 py-1.5 bg-emerald-500 hover:bg-emerald-600 text-white rounded-lg text-xs font-medium transition-colors shadow-sm">
                        <Check className="w-3 h-3" />{t('requests.approve')}
                      </button>
                      <button onClick={(e) => { e.stopPropagation(); rejectRequest(request.id); }} className="flex items-center gap-1 px-3 py-1.5 bg-red-500 hover:bg-red-600 text-white rounded-lg text-xs font-medium transition-colors shadow-sm">
                        <X className="w-3 h-3" />{t('requests.reject')}
                      </button>
                    </>
                  )}
                  {isAdmin && request.status !== 'pending' && (
                    <button onClick={(e) => { e.stopPropagation(); deleteRequest(request.id); }} className="flex items-center gap-1 px-2 py-1.5 text-slate-400 hover:text-red-500 hover:bg-red-50 rounded-lg text-xs transition-all">
                      <Trash2 className="w-3.5 h-3.5" />
                    </button>
                  )}
                  {isExpanded ? <ChevronUp className="w-4 h-4 text-slate-400" /> : <ChevronDown className="w-4 h-4 text-slate-400" />}
                </div>
              </div>
              {isExpanded && (
                <div className="border-t border-slate-100 bg-slate-50/50">
                  <div className="px-5 py-3 space-y-2">
                    {isAdmin && request.status === 'pending' && (
                      <div className="rounded-lg border border-blue-100 bg-blue-50 px-3 py-2 text-xs text-blue-700">
                        {t('requests.adminEditHint')}
                      </div>
                    )}
                    {request.entries.map((entry, i) => {
                      const editedValue = editedEntries[request.id]?.[i] ?? entry.newTranslation;
                      const wasCorrected = editedValue !== entry.newTranslation;
                      return (
                        <div key={`${entry.original}-${i}`} className={`theme-translation-box rounded-lg p-3 border ${wasCorrected ? 'border-blue-200 ring-1 ring-blue-100' : 'border-slate-100'}`}>
                          <p className="text-xs text-slate-500 font-mono mb-2 break-all">{entry.original}</p>
                          <div className="grid grid-cols-[1fr_auto_1.2fr] gap-3 items-start">
                            <div className="min-w-0">
                              <span className="text-[10px] text-slate-400 font-medium">{t('requests.was')}</span>
                              <p className={`text-sm font-mono break-all ${entry.oldTranslation ? 'text-slate-600' : 'text-slate-300 italic'}`}>
                                {entry.oldTranslation || t('modal.empty')}
                              </p>
                            </div>
                            <ArrowRight className="w-4 h-4 text-slate-300 flex-shrink-0 mt-5" />
                            <div className="min-w-0">
                              <div className="mb-1 flex items-center justify-between gap-2">
                                <span className="text-[10px] text-emerald-500 font-medium">{isAdmin && request.status === 'pending' ? t('requests.adminCorrection') : t('requests.became')}</span>
                                {isAdmin && request.status === 'pending' && wasCorrected && (
                                  <button
                                    type="button"
                                    onClick={() => resetRequestEntryTranslation(request, i)}
                                    className="text-[10px] font-medium text-slate-400 hover:text-blue-600"
                                  >
                                    {t('requests.resetCorrection')}
                                  </button>
                                )}
                              </div>
                              {isAdmin && request.status === 'pending' ? (
                                <textarea
                                  value={editedValue}
                                  onChange={(event) => setRequestEntryTranslation(request.id, i, event.target.value)}
                                  rows={2}
                                  className="w-full resize-y rounded-lg border border-slate-200 bg-white px-3 py-2 text-sm font-mono text-slate-800 outline-none transition-colors focus:border-blue-400 focus:ring-2 focus:ring-blue-100"
                                />
                              ) : (
                                <p className={`text-sm font-mono break-all font-medium ${entry.newTranslation ? 'text-emerald-600' : 'text-slate-300 italic'}`}>
                                  {entry.newTranslation || t('modal.empty')}
                                </p>
                              )}
                            </div>
                          </div>
                        </div>
                      );
                    })}
                    {isAdmin && request.status === 'pending' && (
                      <div className="sticky bottom-0 flex justify-end border-t border-slate-100 bg-slate-50/95 px-1 py-2 backdrop-blur">
                        <button onClick={() => handleApproveRequest(request)} className="flex items-center gap-1 px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white rounded-lg text-xs font-semibold transition-colors shadow-sm">
                          <Check className="w-3.5 h-3.5" />{t('requests.approveWithCorrections')}
                        </button>
                      </div>
                    )}
                  </div>
                </div>
              )}
            </div>
          );
        })}
        {filteredRequests.length === 0 && (
          <div className="theme-panel rounded-2xl shadow-sm border py-16 text-center">
            <ClipboardList className="w-12 h-12 text-slate-200 mx-auto mb-3" />
            <h3 className="text-sm font-medium text-slate-400">{t('requests.noRequests')}</h3>
            <p className="text-xs text-slate-300 mt-1">
              {statusFilter !== 'all' || requestFilterTag ? t('requests.tryChangeFilters') : t('requests.noRequestsHint')}
            </p>
          </div>
        )}
      </div>
    </div>
  );
}
