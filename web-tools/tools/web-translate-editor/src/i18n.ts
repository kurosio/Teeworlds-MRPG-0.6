export type UILanguage = 'en' | 'ru';

const translations = {
  // Header
  'header.title': { en: 'Translation Editor', ru: 'Редактор переводов' },
  'header.subtitle': { en: 'Translation file editor', ru: 'Редактор файлов переводов' },
  'header.tab.editor': { en: 'Editor', ru: 'Редактор' },
  'header.tab.requests': { en: 'Requests', ru: 'Запросы' },
  'header.tab.settings': { en: 'Settings', ru: 'Настройки' },
  'header.admin': { en: 'Admin', ru: 'Админ' },
  'header.login': { en: 'Log in', ru: 'Войти' },
  'header.logout': { en: 'Log out', ru: 'Выйти' },
  'header.loginTitle': { en: 'Administrator login', ru: 'Вход администратора' },
  'header.loginError': { en: 'Invalid username or password', ru: 'Неверный логин или пароль' },
  'header.username': { en: 'Username', ru: 'Логин' },
  'header.password': { en: 'Password', ru: 'Пароль' },
  'header.cancel': { en: 'Cancel', ru: 'Отмена' },

  // Language selector
  'sidebar.files': { en: 'Localization files', ru: 'Файлы локализации' },
  'sidebar.lines': { en: 'lines', ru: 'строк' },

  // Top contributors
  'top.title': { en: 'Top contributors', ru: 'Топ контрибьюторов' },
  'top.subtitle': { en: 'By accepted changes', ru: 'По количеству принятых изменений' },

  // Editor
  'editor.selectFile': { en: 'Select a file', ru: 'Выберите файл' },
  'editor.selectFileDesc': { en: 'Select a localization file from the list on the left to start editing translations', ru: 'Выберите файл локализации из списка слева, чтобы начать редактирование переводов' },
  'editor.total': { en: 'total', ru: 'всего' },
  'editor.translated': { en: 'translated', ru: 'переведено' },
  'editor.untranslated': { en: 'untranslated', ru: 'не переведено' },
  'editor.searchPlaceholder': { en: 'Search by original or translation...', ru: 'Поиск по оригиналу или переводу...' },
  'editor.filterAll': { en: 'All', ru: 'Все' },
  'editor.filterTranslated': { en: 'Translated', ru: 'Переведено' },
  'editor.filterUntranslated': { en: 'Untranslated', ru: 'Не переведено' },
  'editor.filterChanged': { en: 'Changed by you', ru: 'Вами измененные' },
  'editor.enterTranslation': { en: 'Enter translation...', ru: 'Введите перевод...' },
  'editor.matchesOriginal': { en: '⚠ Translation matches original', ru: '⚠ Перевод совпадает с оригиналом' },
  'editor.entriesOf': { en: 'of', ru: 'из' },
  'editor.entries': { en: 'entries', ru: 'записей' },
  'editor.changed': { en: 'changed', ru: 'изменено' },
  'editor.reset': { en: 'Reset', ru: 'Сбросить' },
  'editor.applyChanges': { en: 'Apply changes', ru: 'Применить изменения' },
  'editor.submitRequest': { en: 'Submit request', ru: 'Отправить запрос' },
  'editor.sync': { en: 'Synchronize', ru: 'Синхронизировать' },
  'editor.syncing': { en: 'Synchronizing...', ru: 'Синхронизация...' },
  'editor.syncSuccess': { en: 'Strings synchronized from file', ru: 'Строки синхронизированы из файла' },
  'editor.syncError': { en: 'Synchronization failed', ru: 'Ошибка синхронизации' },
  'editor.changesApplied': { en: 'Changes applied successfully!', ru: 'Изменения успешно применены!' },
  'editor.requestSent': { en: 'Request submitted successfully!', ru: 'Запрос успешно отправлен!' },
  'editor.nothingFound': { en: 'Nothing found', ru: 'Ничего не найдено' },
  'editor.original': { en: 'Original', ru: 'Оригинал' },
  'editor.translation': { en: 'Translation', ru: 'Перевод' },
  'editor.copyOriginal': { en: 'Copy original', ru: 'Вставить оригинал' },
  'editor.clear': { en: 'Clear', ru: 'Очистить' },
  'editor.revert': { en: 'Revert', ru: 'Вернуть' },
  'editor.nextField': { en: 'Next', ru: 'Далее' },
  'editor.nextHint': { en: 'Ctrl+Enter moves to the next field', ru: 'Ctrl+Enter переходит к следующему полю' },
  'editor.characters': { en: 'characters', ru: 'симв.' },

  // Submit modal
  'modal.titleAdmin': { en: 'Apply changes', ru: 'Применить изменения' },
  'modal.titleGuest': { en: 'Submit a change request', ru: 'Отправить запрос на изменение' },
  'modal.applyTitle': { en: 'Apply changes', ru: 'Применить изменения' },
  'modal.submitTitle': { en: 'Submit a change request', ru: 'Отправить запрос на изменение' },
  'modal.changesFor': { en: 'changes for', ru: 'изменений для' },
  'modal.changesReady': { en: 'changes ready', ru: 'изменений готово' },
  'modal.authorLabel': { en: 'Your name / nickname', ru: 'Ваше имя / никнейм' },
  'modal.authorName': { en: 'Your name / nickname', ru: 'Ваше имя / никнейм' },
  'modal.authorPlaceholder': { en: 'e.g. John or Translator42', ru: 'Например: Иван или Translator42' },
  'modal.nameLabel': { en: 'Request title', ru: 'Название запроса' },
  'modal.requestName': { en: 'Request title', ru: 'Название запроса' },
  'modal.namePlaceholder': { en: 'e.g. Fix typos in menu', ru: 'Например: Исправление опечаток в меню' },
  'modal.preview': { en: 'Preview changes:', ru: 'Предпросмотр изменений:' },
  'modal.empty': { en: '(empty)', ru: '(пусто)' },
  'modal.cancel': { en: 'Cancel', ru: 'Отмена' },
  'modal.apply': { en: 'Apply', ru: 'Применить' },
  'modal.submit': { en: 'Submit', ru: 'Отправить' },

  // Change requests
  'requests.title': { en: 'Change requests', ru: 'Запросы на изменение' },
  'requests.pending': { en: 'pending', ru: 'ожидающих' },
  'requests.all': { en: 'All', ru: 'Все' },
  'requests.statusPending': { en: 'Pending', ru: 'Ожидает' },
  'requests.statusApproved': { en: 'Approved', ru: 'Одобрен' },
  'requests.statusRejected': { en: 'Rejected', ru: 'Отклонён' },
  'requests.from': { en: 'from', ru: 'от' },
  'requests.changes': { en: 'changes', ru: 'изменений' },
  'requests.approve': { en: 'Approve', ru: 'Одобрить' },
  'requests.reject': { en: 'Reject', ru: 'Отклонить' },
  'requests.noRequests': { en: 'No requests', ru: 'Запросов нет' },
  'requests.noRequestsHint': { en: 'Translation change requests will appear here', ru: 'Запросы на изменение переводов появятся здесь' },
  'requests.tryChangeFilters': { en: 'Try changing the filters', ru: 'Попробуйте изменить фильтры' },
  'requests.was': { en: 'WAS:', ru: 'БЫЛО:' },
  'requests.became': { en: 'NOW:', ru: 'СТАЛО:' },

  // Settings
  'settings.title': { en: 'Settings', ru: 'Настройки' },
  'settings.subtitle': { en: 'Translation editor configuration', ru: 'Конфигурация редактора переводов' },
  'settings.limitedAccess': { en: 'Limited access', ru: 'Ограниченный доступ' },
  'settings.limitedAccessDesc': { en: 'Settings are read-only. Log in as administrator to modify settings.', ru: 'Настройки доступны только для просмотра. Войдите как администратор, чтобы изменять настройки.' },
  'settings.pathTitle': { en: 'Translation files path', ru: 'Путь к файлам перевода' },
  'settings.pathLabel': { en: 'Root path (from root)', ru: 'Корневой путь (от root)' },
  'settings.pathHint': { en: 'Expected structure:', ru: 'Ожидаемая структура:' },
  'settings.pathIndex': { en: 'language index', ru: 'языковой индекс' },
  'settings.pathRu': { en: 'Russian translation', ru: 'русский перевод' },
  'settings.pathCn': { en: 'Chinese translation', ru: 'китайский перевод' },
  'settings.adminTitle': { en: 'Administrator credentials', ru: 'Учётные данные администратора' },
  'settings.adminLogin': { en: 'Login', ru: 'Логин' },
  'settings.adminPassword': { en: 'Password', ru: 'Пароль' },
  'settings.adminHint': { en: 'The only system administrator for managing change requests', ru: 'Единственный администратор системы для управления запросами на изменение' },
  'settings.resetData': { en: 'Reset data', ru: 'Сбросить данные' },
  'settings.save': { en: 'Save settings', ru: 'Сохранить настройки' },
  'settings.saved': { en: 'Settings saved', ru: 'Настройки сохранены' },
  'settings.resetTitle': { en: 'Reset data?', ru: 'Сбросить данные?' },
  'settings.resetCannotUndo': { en: "This action cannot be undone", ru: 'Это действие нельзя отменить' },
  'settings.resetDesc': { en: 'All translations, change requests, and settings will be reset to default values.', ru: 'Все переводы, запросы на изменение и настройки будут сброшены до значений по умолчанию.' },
  'settings.resetConfirm': { en: 'Reset', ru: 'Сбросить' },
  'settings.cancel': { en: 'Cancel', ru: 'Отмена' },
} as const;

type TranslationKey = keyof typeof translations;

let currentLanguage: UILanguage = 'en';

export function setUILanguage(lang: UILanguage) {
  currentLanguage = lang;
}

export function getUILanguage(): UILanguage {
  return currentLanguage;
}

export function t(key: TranslationKey): string {
  const entry = translations[key];
  if (!entry) return key;
  return entry[currentLanguage] || entry['en'] || key;
}
