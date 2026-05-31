import { startTransition, useEffect, useRef } from 'react';
import { useStore } from './store';
import { loadFromServerApi } from './utils/fileLoader';
import Header from './components/Header';
import LanguageSelector from './components/LanguageSelector';
import TranslationEditor from './components/TranslationEditor';
import ChangeRequests from './components/ChangeRequests';
import SettingsPage from './components/SettingsPage';
import TopContributors from './components/TopContributors';

function AutoLoader() {
  const { settings, loadFiles, loadTopContributors } = useStore();
  const attempted = useRef(false);

  useEffect(() => {
    if (attempted.current) return;
    attempted.current = true;

    loadFromServerApi().then(result => {
      if (result.languages.length > 0) {
        startTransition(() => loadFiles(result));
      }
    }).catch(() => {
      // Fall back to local/demo data silently.
    });

    loadTopContributors();

    // Files are loaded only on page start/refresh. Do not poll while the user is
    // editing, changing settings, or just keeping the web editor open: external
    // file changes are picked up on the next browser refresh/open.
  }, [settings.translationPath, loadFiles, loadTopContributors]);

  return null;
}

export default function App() {
  const { activeTab } = useStore();

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-100 via-slate-50 to-blue-50/20">
      <AutoLoader />
      <Header />

      <main className="max-w-[1400px] mx-auto px-4 sm:px-6 py-6">
        {activeTab === 'editor' && (
          <div className="flex gap-4 items-start h-[calc(100vh-120px)] min-h-0">
            <aside className="w-64 flex-shrink-0 sticky top-6 h-[calc(100vh-3rem)] max-h-[calc(100vh-3rem)] overflow-hidden grid grid-rows-[auto_minmax(0,1fr)] gap-3">
              <div className="min-h-0 overflow-hidden">
                <TopContributors />
              </div>
              <div className="min-h-0 overflow-hidden">
                <LanguageSelector />
              </div>
            </aside>
            <div className="flex-1 flex flex-col min-h-0 h-full">
              <TranslationEditor />
            </div>
          </div>
        )}

        {activeTab === 'requests' && <ChangeRequests />}
        {activeTab === 'settings' && <SettingsPage />}
      </main>
    </div>
  );
}
