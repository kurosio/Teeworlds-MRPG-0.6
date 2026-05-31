import { useEffect, useRef } from 'react';
import { useStore } from './store';
import { loadFromUrlPath } from './utils/fileLoader';
import Header from './components/Header';
import LanguageSelector from './components/LanguageSelector';
import TranslationEditor from './components/TranslationEditor';
import ChangeRequests from './components/ChangeRequests';
import SettingsPage from './components/SettingsPage';
import TopContributors from './components/TopContributors';

function AutoLoader() {
  const { settings, loadFiles } = useStore();
  const attempted = useRef(false);

  useEffect(() => {
    if (attempted.current) return;
    attempted.current = true;

    // Try to load from the configured path
    const path = settings.translationPath;
    if (!path) return;

    loadFromUrlPath(path).then(result => {
      if (result.languages.length > 0) {
        loadFiles(result);
      }
    }).catch(() => {
      // Fall back to demo data silently
    });
  }, []); // Run once on mount

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
          <div className="flex gap-4 items-start" style={{ minHeight: 'calc(100vh - 120px)' }}>
            <div className="w-64 flex-shrink-0 sticky top-6">
              <LanguageSelector />
              <TopContributors />
            </div>
            <div className="flex-1 flex flex-col min-h-0" style={{ minHeight: 'calc(100vh - 120px)' }}>
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
