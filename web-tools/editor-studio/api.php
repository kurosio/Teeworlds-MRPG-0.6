<?php
// Устанавливаем заголовки для корректной работы с JSON и CORS
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *'); // В продакшене лучше указать конкретный домен
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');

// Директория для хранения файлов сценариев
$scenariosDir = 'scenarios/';
$method = $_SERVER['REQUEST_METHOD'];

// Обработка preflight-запроса от браузера
if ($method === 'OPTIONS') {
    exit(0);
}

// Если директория не существует, создаем ее
if (!is_dir($scenariosDir)) {
    mkdir($scenariosDir, 0755, true);
}

/**
 * Очищает имя файла от опасных символов, чтобы предотвратить выход из директории.
 * @param string $filename Исходное имя файла.
 * @return string Безопасное имя файла.
 */
function sanitize_filename($filename) {
    // Удаляет ".." , "/" и "\"
    return preg_replace('/(\.\.)|(\/)|(\\\)/', '', $filename);
}

// ОБРАБОТКА GET-ЗАПРОСОВ (получение данных)
if ($method === 'GET') {
    // Если запрошен список файлов
    if (isset($_GET['action']) && $_GET['action'] === 'list') {
        $files = glob($scenariosDir . '*.json');
        // Возвращаем только имена файлов, а не полные пути
        $filenames = array_map('basename', $files);
        echo json_encode(['success' => true, 'files' => $filenames]);
        exit;
    }

    // Если запрошен конкретный файл
    if (isset($_GET['file'])) {
        $file = $scenariosDir . sanitize_filename($_GET['file']);
        if (file_exists($file)) {
            // Отдаем содержимое файла
            echo file_get_contents($file);
        } else {
            // Если файл не найден, возвращаем ошибку 404
            http_response_code(404);
            echo json_encode(['success' => false, 'error' => 'Файл не найден']);
        }
    } else {
        // Если имя файла не передано, возвращаем ошибку 400
        http_response_code(400);
        echo json_encode(['success' => false, 'error' => 'Файл не указан']);
    }
}

// ОБРАБОТКА POST-ЗАПРОСОВ (сохранение данных)
if ($method === 'POST') {
    // Получаем JSON из тела запроса
    $data = json_decode(file_get_contents('php://input'), true);

    // Проверяем корректность полученных данных
    if (json_last_error() !== JSON_ERROR_NONE || !isset($data['filename']) || !isset($data['content'])) {
        http_response_code(400);
        echo json_encode(['success' => false, 'error' => 'Некорректные данные']);
        exit;
    }

    $filename = sanitize_filename($data['filename']);
    // Если расширение не указано, добавляем .json
    if (empty(pathinfo($filename, PATHINFO_EXTENSION))) {
        $filename .= '.json';
    }

    $filePath = $scenariosDir . $filename;
    // Кодируем контент в красивый JSON с поддержкой кириллицы
    $content = json_encode($data['content'], JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);

    // Пытаемся записать данные в файл
    if (file_put_contents($filePath, $content) !== false) {
        echo json_encode(['success' => true, 'message' => 'Файл ' . $filename . ' успешно сохранен.']);
    } else {
        // Если не удалось, возвращаем ошибку 500
        http_response_code(500);
        echo json_encode(['success' => false, 'error' => 'Не удалось сохранить файл на сервере.']);
    }
}
?>
