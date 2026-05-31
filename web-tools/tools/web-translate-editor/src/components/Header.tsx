import { useEffect, useRef, useState } from 'react';
import { useStore } from '../store';
import { UI_LANGUAGE_OPTIONS, t } from '../i18n';
import { Globe, FileText, Settings, LogIn, LogOut, ShieldCheck, ClipboardList, Languages } from 'lucide-react';

export default function Header() {
  const { activeTab, setActiveTab, isAdmin, login, logout, changeRequests, uiLanguage, setUiLanguage } = useStore();
  const [showLogin, setShowLogin] = useState(false);
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [showLanguageMenu, setShowLanguageMenu] = useState(false);
  const languageMenuRef = useRef<HTMLDivElement | null>(null);

  const pendingCount = changeRequests.filter(r => r.status === 'pending').length;

  const handleLogin = async () => {
    const success = await login(username, password);
    if (success) {
      setShowLogin(false);
      setUsername('');
      setPassword('');
      setError('');
    } else {
      setError(t('header.loginError'));
    }
  };

  const currentUiLanguage = UI_LANGUAGE_OPTIONS.find(option => option.code === uiLanguage) || UI_LANGUAGE_OPTIONS[0];

  useEffect(() => {
    if (!showLanguageMenu) return;

    const handlePointerDown = (event: PointerEvent) => {
      if (!languageMenuRef.current?.contains(event.target as Node)) {
        setShowLanguageMenu(false);
      }
    };

    document.addEventListener('pointerdown', handlePointerDown);
    return () => document.removeEventListener('pointerdown', handlePointerDown);
  }, [showLanguageMenu]);

  return (
    <header className="bg-gradient-to-r from-slate-900 via-slate-800 to-slate-900 text-white shadow-2xl border-b border-slate-700/50">
      <div className="max-w-7xl mx-auto px-4 sm:px-6">
        <div className="flex items-center justify-between h-16">
          {/* Logo */}
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 bg-gradient-to-br from-blue-500 to-indigo-600 rounded-xl flex items-center justify-center shadow-lg shadow-blue-500/25">
              <Globe className="w-5 h-5" />
            </div>
            <div>
              <h1 className="text-lg font-bold tracking-tight">{t('header.title')}</h1>
              <p className="text-xs text-slate-400 -mt-0.5">{t('header.subtitle')}</p>
            </div>
          </div>

          {/* Navigation */}
          <nav className="flex items-center gap-1">
            <button
              onClick={() => setActiveTab('editor')}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all duration-200 ${
                activeTab === 'editor'
                  ? 'bg-blue-600/90 text-white shadow-lg shadow-blue-500/25'
                  : 'text-slate-300 hover:text-white hover:bg-slate-700/50'
              }`}
            >
              <FileText className="w-4 h-4" />
              <span className="hidden sm:inline">{t('header.tab.editor')}</span>
            </button>

            <button
              onClick={() => setActiveTab('requests')}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all duration-200 relative ${
                activeTab === 'requests'
                  ? 'bg-blue-600/90 text-white shadow-lg shadow-blue-500/25'
                  : 'text-slate-300 hover:text-white hover:bg-slate-700/50'
              }`}
            >
              <ClipboardList className="w-4 h-4" />
              <span className="hidden sm:inline">{t('header.tab.requests')}</span>
              {pendingCount > 0 && (
                <span className="absolute -top-1 -right-1 bg-red-500 text-white text-xs w-5 h-5 rounded-full flex items-center justify-center font-bold shadow-lg">
                  {pendingCount}
                </span>
              )}
            </button>

            <button
              onClick={() => setActiveTab('settings')}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all duration-200 ${
                activeTab === 'settings'
                  ? 'bg-blue-600/90 text-white shadow-lg shadow-blue-500/25'
                  : 'text-slate-300 hover:text-white hover:bg-slate-700/50'
              }`}
            >
              <Settings className="w-4 h-4" />
              <span className="hidden sm:inline">{t('header.tab.settings')}</span>
            </button>
          </nav>

          {/* Right side: lang switch + auth */}
          <div className="flex items-center gap-3">
            {/* UI Language dropdown */}
            <div className="relative" ref={languageMenuRef}>
              <button
                type="button"
                onClick={() => setShowLanguageMenu(value => !value)}
                className="flex items-center gap-2 px-2.5 py-1.5 rounded-lg text-xs font-medium border border-slate-600/50 text-slate-300 hover:text-white hover:bg-slate-700/50 transition-all min-w-[132px] justify-between"
                title={t('header.uiLanguage')}
                aria-haspopup="listbox"
                aria-expanded={showLanguageMenu}
              >
                <span className="flex items-center gap-1.5 min-w-0">
                  <Languages className="w-3.5 h-3.5 flex-shrink-0" />
                  <span className="truncate">{currentUiLanguage.nativeName}</span>
                </span>
                <span className="uppercase font-bold text-[10px] text-slate-400">{currentUiLanguage.code}</span>
              </button>

              {showLanguageMenu && (
                <div
                  className="absolute right-0 top-full mt-2 w-56 max-h-80 overflow-y-auto rounded-xl border border-slate-600/60 bg-slate-800 shadow-2xl z-50 p-1.5"
                  role="listbox"
                  aria-label={t('header.uiLanguage')}
                >
                  {UI_LANGUAGE_OPTIONS.map(option => (
                    <button
                      key={option.code}
                      type="button"
                      onClick={() => {
                        setUiLanguage(option.code);
                        setShowLanguageMenu(false);
                      }}
                      className={`w-full flex items-center justify-between gap-3 px-3 py-2 rounded-lg text-sm text-left transition-colors ${
                        uiLanguage === option.code
                          ? 'bg-blue-600/90 text-white'
                          : 'text-slate-300 hover:text-white hover:bg-slate-700/60'
                      }`}
                      role="option"
                      aria-selected={uiLanguage === option.code}
                    >
                      <span className="font-medium">{option.nativeName}</span>
                      <span className="text-xs opacity-70 uppercase">{option.code}</span>
                    </button>
                  ))}
                </div>
              )}
            </div>

            {/* Auth */}
            {isAdmin ? (
              <div className="flex items-center gap-2">
                <div className="flex items-center gap-1.5 bg-emerald-500/15 border border-emerald-500/30 px-3 py-1.5 rounded-lg">
                  <ShieldCheck className="w-4 h-4 text-emerald-400" />
                  <span className="text-sm text-emerald-300 font-medium">{t('header.admin')}</span>
                </div>
                <button
                  onClick={logout}
                  className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-sm text-slate-300 hover:text-white hover:bg-slate-700/50 transition-all"
                >
                  <LogOut className="w-4 h-4" />
                  <span className="hidden sm:inline">{t('header.logout')}</span>
                </button>
              </div>
            ) : (
              <div className="relative">
                <button
                  onClick={() => setShowLogin(!showLogin)}
                  className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-sm text-slate-300 hover:text-white hover:bg-slate-700/50 transition-all border border-slate-600/50"
                >
                  <LogIn className="w-4 h-4" />
                  <span className="hidden sm:inline">{t('header.login')}</span>
                </button>

                {showLogin && (
                  <div className="absolute right-0 top-full mt-2 w-72 bg-slate-800 rounded-xl shadow-2xl border border-slate-600/50 p-4 z-50">
                    <h3 className="text-sm font-semibold mb-3 text-slate-200">{t('header.loginTitle')}</h3>
                    {error && (
                      <div className="mb-3 p-2 bg-red-500/15 border border-red-500/30 rounded-lg text-xs text-red-300">
                        {error}
                      </div>
                    )}
                    <input
                      type="text"
                      placeholder={t('header.username')}
                      value={username}
                      onChange={(e) => setUsername(e.target.value)}
                      className="w-full mb-2 px-3 py-2 bg-slate-700/50 border border-slate-600/50 rounded-lg text-sm text-white placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500/50"
                      onKeyDown={(e) => e.key === 'Enter' && handleLogin()}
                    />
                    <input
                      type="password"
                      placeholder={t('header.password')}
                      value={password}
                      onChange={(e) => setPassword(e.target.value)}
                      className="w-full mb-3 px-3 py-2 bg-slate-700/50 border border-slate-600/50 rounded-lg text-sm text-white placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-blue-500/50 focus:border-blue-500/50"
                      onKeyDown={(e) => e.key === 'Enter' && handleLogin()}
                    />
                    <div className="flex gap-2">
                      <button
                        onClick={handleLogin}
                        className="flex-1 px-3 py-2 bg-blue-600 hover:bg-blue-500 rounded-lg text-sm font-medium transition-colors"
                      >
                        {t('header.login')}
                      </button>
                      <button
                        onClick={() => { setShowLogin(false); setError(''); }}
                        className="px-3 py-2 bg-slate-700 hover:bg-slate-600 rounded-lg text-sm transition-colors"
                      >
                        {t('header.cancel')}
                      </button>
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>
        </div>
      </div>
    </header>
  );
}
