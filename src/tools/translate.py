import os
import hashlib
import json
import re

# Настройки
PROJECT_DIR = "./"  # Укажите путь к вашему C++ проекту
HASH_FILE = "file_hashes.json"
TRANSLATION_FILE = "translations.txt"
TRANSLATION_PATTERN = re.compile(r'"((?:\\"|[^"])*?)"')  # Поиск строк в "..." с учетом экранированных кавычек
VALID_TEXT_PATTERN = re.compile(r'[a-zA-Zа-яА-Я]')  # Проверка на наличие букв
FORMAT_SPECIFIERS_PATTERN = re.compile(r'%d|%s|%ld|%f')  # Исключение строк с форматными спецификаторами
INCLUDE_PATTERN = re.compile(r'#include\s+["<].*[">]')  # Исключение строк с директивами include
DATABASE_FUNCTION_PATTERN = re.compile(r'\bDatabase\b')  # Исключение строк, содержащих вызовы Database
CHAT_FUNCTION_PATTERN = re.compile(r'Chat\s*\(.*?,\s*"(.*?)"')  # Извлечение аргумента после первой запятой в Chat
BROADCAST_FUNCTION_PATTERN = re.compile(r'Broadcast\s*\(.*?,.*?,.*?,\s*"(.*?)"')  # Извлечение аргумента после третьей запятой в Broadcast
ADD_FUNCTION_PATTERN = re.compile(r'Add\s*\("(.*?)"')  # Извлечение аргумента в Add
ADDMENU_FUNCTION_PATTERN = re.compile(r'AddMenu\s*\(.*?(?:,\s*"(.*?)")?')  # Извлечение аргумента после первой или второй запятой в AddMenu
ADDOPTION_FUNCTION_PATTERN = re.compile(r'AddOption\s*\(.*?,\s*"(.*?)"|AddOption\s*\(.*?,.*?,\s*"(.*?)"|AddOption\s*\(.*?,.*?,.*?,\s*"(.*?)"')  # Извлечение аргумента после первой, двух или трех запятых в AddOption

def calculate_file_hash(filepath):
    """Вычисляет хеш-сумму файла."""
    hasher = hashlib.md5()
    with open(filepath, 'rb') as f:
        hasher.update(f.read())
    return hasher.hexdigest()


def load_hashes():
    """Загружает сохраненные хеши файлов."""
    if os.path.exists(HASH_FILE):
        with open(HASH_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    return {}


def save_hashes(hashes):
    """Сохраняет хеши файлов."""
    with open(HASH_FILE, 'w', encoding='utf-8') as f:
        json.dump(hashes, f, indent=4)


def load_existing_translations():
    """Загружает существующие строки перевода."""
    if os.path.exists(TRANSLATION_FILE):
        with open(TRANSLATION_FILE, 'r', encoding='utf-8') as f:
            return set(line.strip() for line in f)
    return set()


def save_translations(translations):
    """Сохраняет строки перевода."""
    with open(TRANSLATION_FILE, 'w', encoding='utf-8') as f:
        for line in sorted(translations):
            f.write(f"{line}\n")


def extract_translations_from_file(filepath):
    """Извлекает строки перевода из C++ файла, собирая только аргументы в кавычках из функций Chat, Broadcast, Add, AddMenu и AddOption."""
    translations = set()
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            if INCLUDE_PATTERN.search(line) or DATABASE_FUNCTION_PATTERN.search(line):
                continue
            # Извлечение аргументов
            for pattern in [CHAT_FUNCTION_PATTERN, BROADCAST_FUNCTION_PATTERN, ADD_FUNCTION_PATTERN, ADDMENU_FUNCTION_PATTERN, ADDOPTION_FUNCTION_PATTERN]:
                matches = pattern.findall(line)
                for match in matches:
                    if match:
                        translations.add(f"{match}\n=={match}\n")
    return translations


def main():
    existing_hashes = load_hashes()
    existing_translations = load_existing_translations()
    new_hashes = {}
    updated_translations = set(existing_translations)

    for root, _, files in os.walk(PROJECT_DIR):
        for file in files:
            if file.endswith(('.cpp', '.h')):
                filepath = os.path.join(root, file)
                file_hash = calculate_file_hash(filepath)
                new_hashes[filepath] = file_hash

                if filepath not in existing_hashes or existing_hashes[filepath] != file_hash:
                    print(f"Processing {filepath}...")
                    extracted = extract_translations_from_file(filepath)
                    for translation in extracted:
                        if translation not in existing_translations:
                            updated_translations.add(translation)

    save_hashes(new_hashes)
    save_translations(updated_translations)
    print("Translation extraction completed.")


if __name__ == "__main__":
    main()
