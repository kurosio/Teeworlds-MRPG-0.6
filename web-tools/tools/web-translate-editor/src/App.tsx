import { startTransition, useEffect, useRef } from 'react';
import { useStore } from './store';
import { loadFromServerApi, loadServerManifest } from './utils/fileLoader';
import Header from './components/Header';
import TranslationEditor from './components/TranslationEditor';
import ChangeRequests from './components/ChangeRequests';
import SettingsPage from './components/SettingsPage';
import SidebarDock from './components/SidebarDock';

function AutoLoader() {
  const { settings, beginFileLoading, loadFiles, loadChangeRequests } = useStore();
  const attempted = useRef(false);

  useEffect(() => {
    if (attempted.current) return;
    attempted.current = true;

    loadServerManifest().then(manifest => {
      if (manifest.languages.length > 0) {
        beginFileLoading(manifest);
      }
    }).catch(() => {
      // Manifest is only used to show real loading state before the full payload arrives.
    });

    loadFromServerApi().then(result => {
      if (result.languages.length > 0) {
        startTransition(() => { void loadFiles(result); });
      }
      if (!Array.isArray(result.changeRequests)) {
        loadChangeRequests();
      }
    }).catch(() => {
      // Fall back to current in-memory data silently and load global lists separately.
      loadChangeRequests();
    });

    // Files are loaded only on page start/refresh. Do not poll while the user is
    // editing, changing settings, or just keeping the web editor open: external
    // file changes are picked up on the next browser refresh/open.
  }, [settings.translationPath, beginFileLoading, loadFiles, loadChangeRequests]);

  return null;
}

export default function App() {
  const { activeTab, isAdmin, setActiveTab } = useStore();

  useEffect(() => {
    if (activeTab === 'settings' && !isAdmin) setActiveTab('editor');
  }, [activeTab, isAdmin, setActiveTab]);

  return (
    <div className="app-shell min-h-screen">
      <AutoLoader />
      <Header />

      <main className="max-w-[1400px] mx-auto px-4 sm:px-6 py-6">
        {activeTab === 'editor' && (
          <div className="flex gap-4 items-start h-[calc(100vh-120px)] min-h-0 overflow-visible">
            <SidebarDock />
            <div className="flex-1 flex flex-col min-h-0 h-full">
              <TranslationEditor />
            </div>
          </div>
        )}

        {activeTab === 'requests' && <ChangeRequests />}
        {activeTab === 'settings' && isAdmin && <SettingsPage />}
      </main>
    </div>
  );
}
