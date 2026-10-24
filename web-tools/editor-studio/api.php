<?php
// File API for scenarios (list / load / save JSON in scenarios/).
// Admin only: guests and anonymous users have no access to scenario files.

declare(strict_types=1);

require_once __DIR__ . '/api/db-core.php';

bootstrap_editor_api(true);

// Директория для хранения файлов сценариев
$scenariosDir = __DIR__ . '/scenarios/';
$method = strtoupper((string)($_SERVER['REQUEST_METHOD'] ?? 'GET'));

if (!is_dir($scenariosDir)) {
    mkdir($scenariosDir, 0755, true);
}

/**
 * Безопасное имя файла: только basename из букв/цифр/[_ .-], всегда с расширением .json.
 * Возвращает '' если имя некорректно.
 */
function sanitize_filename(string $filename): string
{
    $name = basename(str_replace('\\', '/', trim($filename)));
    if (str_ends_with(strtolower($name), '.json')) {
        $name = substr($name, 0, -5);
    }
    $name = preg_replace('/[^\p{L}\p{N}_\-. ]/u', '_', $name) ?? '';
    $name = trim($name, '.');
    if ($name === '' || strlen($name) > 200) {
        return '';
    }
    return $name . '.json';
}

// ОБРАБОТКА GET-ЗАПРОСОВ (получение данных)
if ($method === 'GET') {
    if (isset($_GET['action']) && $_GET['action'] === 'list') {
        $files = glob($scenariosDir . '*.json') ?: [];
        $filenames = array_map('basename', $files);
        echo json_encode(['success' => true, 'files' => $filenames], JSON_UNESCAPED_UNICODE);
        exit;
    }

    if (isset($_GET['file'])) {
        $safe = sanitize_filename((string)$_GET['file']);
        $file = $scenariosDir . $safe;
        if ($safe !== '' && is_file($file)) {
            echo file_get_contents($file);
        } else {
            http_response_code(404);
            echo json_encode(['success' => false, 'error' => 'Файл не найден'], JSON_UNESCAPED_UNICODE);
        }
    } else {
        http_response_code(400);
        echo json_encode(['success' => false, 'error' => 'Файл не указан'], JSON_UNESCAPED_UNICODE);
    }
    exit;
}

// ОБРАБОТКА POST-ЗАПРОСОВ (сохранение данных)
if ($method === 'POST') {
    $data = json_decode((string)file_get_contents('php://input'), true);

    if (json_last_error() !== JSON_ERROR_NONE || !isset($data['filename']) || !array_key_exists('content', $data)) {
        http_response_code(400);
        echo json_encode(['success' => false, 'error' => 'Некорректные данные'], JSON_UNESCAPED_UNICODE);
        exit;
    }

    $filename = sanitize_filename((string)$data['filename']);
    if ($filename === '') {
        http_response_code(400);
        echo json_encode(['success' => false, 'error' => 'Некорректное имя файла'], JSON_UNESCAPED_UNICODE);
        exit;
    }

    $filePath = $scenariosDir . $filename;
    $content = json_encode($data['content'], JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);

    if ($content !== false && file_put_contents($filePath, $content, LOCK_EX) !== false) {
        echo json_encode(['success' => true, 'message' => 'Файл ' . $filename . ' успешно сохранен.'], JSON_UNESCAPED_UNICODE);
    } else {
        http_response_code(500);
        echo json_encode(['success' => false, 'error' => 'Не удалось сохранить файл на сервере.'], JSON_UNESCAPED_UNICODE);
    }
    exit;
}

http_response_code(405);
echo json_encode(['success' => false, 'error' => 'Method not allowed']);
