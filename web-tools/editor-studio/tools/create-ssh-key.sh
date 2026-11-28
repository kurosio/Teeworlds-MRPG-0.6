#!/usr/bin/env bash
# Быстрая генерация SSH-ключа для авторизации из Editor Studio (System -> SSH servers).
# Ключ создаётся без парольной фразы, так как веб-сервер запускает OpenSSH с BatchMode=yes.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STUDIO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEFAULT_DIR="${STUDIO_DIR}/ssh-keys"

KEY_NAME="editor_studio_ed25519"
KEY_DIR="${DEFAULT_DIR}"
OUT_PATH=""
KEY_TYPE="ed25519"
KEY_COMMENT="editor-studio@$(hostname -s 2>/dev/null || echo localhost)"
OWNER=""
COPY_TARGET=""
SSH_PORT="22"
FORCE=0
DOCKER_MODE=0

usage() {
  cat <<'EOF'
Быстрая генерация SSH-ключа для Editor Studio (System -> SSH servers).

Использование:
  ./tools/create-ssh-key.sh [параметры]

Параметры:
  -n, --name NAME        имя файла ключа (по умолчанию: editor_studio_ed25519)
  -d, --dir DIR          каталог для ключей (по умолчанию: web-tools/editor-studio/ssh-keys)
  -o, --out PATH         полный путь приватного ключа (перекрывает --dir и --name)
  -t, --type TYPE        тип ключа: ed25519 (по умолчанию), rsa, ecdsa
  -c, --comment TEXT     комментарий к ключу (по умолчанию: editor-studio@<host>)
      --owner USER[:GRP] назначить владельца приватного ключа (например: www-data или 33:33)
      --docker           режим Docker-монтирования (/run/editor-ssh-keys/<имя>)
      --copy-id USER@HOST сразу отправить публичный ключ на игровой сервер через ssh-copy-id
  -p, --port PORT        SSH-порт для --copy-id (по умолчанию: 22)
  -f, --force            перезаписать существующий ключ
  -h, --help             показать эту справку

Примеры:
  ./tools/create-ssh-key.sh
  ./tools/create-ssh-key.sh --name eu1 --copy-id deploy@game.example.com
  sudo ./tools/create-ssh-key.sh --docker --owner 33:33
EOF
}

die() {
  printf 'Ошибка: %s\n' "$1" >&2
  exit 1
}

require_arg() {
  if [[ $# -lt 2 || -z "${2}" ]]; then
    die "параметру ${1} требуется значение."
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -n|--name)
      require_arg "$@"
      KEY_NAME="$2"
      shift 2
      ;;
    -d|--dir)
      require_arg "$@"
      KEY_DIR="$2"
      shift 2
      ;;
    -o|--out)
      require_arg "$@"
      OUT_PATH="$2"
      shift 2
      ;;
    -t|--type)
      require_arg "$@"
      KEY_TYPE="$2"
      shift 2
      ;;
    -c|--comment)
      require_arg "$@"
      KEY_COMMENT="$2"
      shift 2
      ;;
    --owner)
      require_arg "$@"
      OWNER="$2"
      shift 2
      ;;
    --docker)
      DOCKER_MODE=1
      shift
      ;;
    --copy-id)
      require_arg "$@"
      COPY_TARGET="$2"
      shift 2
      ;;
    -p|--port)
      require_arg "$@"
      SSH_PORT="$2"
      shift 2
      ;;
    -f|--force)
      FORCE=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "неизвестный параметр: $1 (см. --help)"
      ;;
  esac
done

command -v ssh-keygen >/dev/null 2>&1 || die "не найден ssh-keygen (установите пакет openssh-client)."

case "${KEY_TYPE}" in
  ed25519|rsa|ecdsa) ;;
  *) die "неподдерживаемый тип ключа '${KEY_TYPE}'. Используйте ed25519, rsa или ecdsa." ;;
esac

if ! [[ "${SSH_PORT}" =~ ^[0-9]+$ ]] || (( SSH_PORT < 1 || SSH_PORT > 65535 )); then
  die "некорректный SSH-порт: ${SSH_PORT}."
fi

if [[ -n "${OUT_PATH}" ]]; then
  PRIV_PATH="${OUT_PATH}"
  TARGET_DIR="$(dirname "${PRIV_PATH}")"
  KEY_BASENAME="$(basename "${PRIV_PATH}")"
else
  [[ "${KEY_NAME}" != */* && -n "${KEY_NAME}" && "${KEY_NAME}" != "." && "${KEY_NAME}" != ".." ]] \
    || die "некорректное имя файла ключа: '${KEY_NAME}'."
  TARGET_DIR="${KEY_DIR}"
  PRIV_PATH="${TARGET_DIR%/}/${KEY_NAME}"
  KEY_BASENAME="${KEY_NAME}"
fi
PUB_PATH="${PRIV_PATH}.pub"

if [[ -e "${PRIV_PATH}" || -e "${PUB_PATH}" ]]; then
  if (( FORCE != 1 )); then
    die "ключ уже существует (${PRIV_PATH}). Используйте --force для перезаписи или задайте другое --name / --out."
  fi
  rm -f -- "${PRIV_PATH}" "${PUB_PATH}"
fi

CREATED_DIR=0
if [[ ! -d "${TARGET_DIR}" ]]; then
  mkdir -p -- "${TARGET_DIR}"
  CREATED_DIR=1
fi
TARGET_DIR_ABS="$(cd "${TARGET_DIR}" && pwd)"
PRIV_PATH="${TARGET_DIR_ABS}/${KEY_BASENAME}"
PUB_PATH="${PRIV_PATH}.pub"

if (( CREATED_DIR == 1 )) || [[ "${TARGET_DIR_ABS}" == "${DEFAULT_DIR}" ]]; then
  chmod 700 -- "${TARGET_DIR_ABS}"
fi

KEYGEN_ARGS=(-t "${KEY_TYPE}" -N "" -C "${KEY_COMMENT}" -f "${PRIV_PATH}" -q)
if [[ "${KEY_TYPE}" == "rsa" ]]; then
  KEYGEN_ARGS+=(-b 4096)
fi

(
  umask 077
  ssh-keygen "${KEYGEN_ARGS[@]}"
)
chmod 600 -- "${PRIV_PATH}"
chmod 644 -- "${PUB_PATH}"

if [[ -z "${OWNER}" && "${DOCKER_MODE}" -eq 1 && "${EUID:-$(id -u)}" -eq 0 ]]; then
  OWNER="33:33"
fi

if [[ -n "${OWNER}" ]]; then
  if chown -- "${OWNER}" "${PRIV_PATH}" "${PUB_PATH}" 2>/dev/null; then
    :
  else
    die "не удалось сменить владельца на '${OWNER}'. Запустите скрипт с sudo."
  fi
fi

FINGERPRINT="$(ssh-keygen -lf "${PUB_PATH}" | awk '{print $2}')"
PUB_CONTENT="$(cat "${PUB_PATH}")"

printf 'SSH-ключ успешно создан.\n\n'
printf 'Приватный ключ:  %s\n' "${PRIV_PATH}"
printf 'Публичный ключ:  %s\n' "${PUB_PATH}"
printf 'Отпечаток:       %s\n' "${FINGERPRINT}"

if (( DOCKER_MODE == 1 )) || [[ "${TARGET_DIR_ABS}" == "${DEFAULT_DIR}" ]]; then
  printf '\nДля Docker (docker-compose.example.yml -> ./ssh-keys:/run/editor-ssh-keys:ro):\n'
  printf '  Путь в поле "Private key path on the web server": /run/editor-ssh-keys/%s\n' "${KEY_BASENAME}"
  if [[ -z "${OWNER}" && "${EUID:-$(id -u)}" -ne 0 ]]; then
    printf '  Примечание: в контейнере PHP работает от www-data (uid 33). При ошибке доступа выполните:\n'
    printf '    sudo chown 33:33 %q\n' "${PRIV_PATH}"
  fi
else
  printf '\nДля Editor Studio:\n'
  printf '  Путь в поле "Private key path on the web server": %s\n' "${PRIV_PATH}"
  printf '  Убедитесь, что приватный ключ доступен на чтение пользователю веб-сервера (обычно www-data) с правами 0600.\n'
fi

if [[ -n "${COPY_TARGET}" ]]; then
  command -v ssh-copy-id >/dev/null 2>&1 || die "ключ создан, но команда ssh-copy-id не найдена."
  printf '\nОтправка публичного ключа на %s (порт %s)...\n' "${COPY_TARGET}" "${SSH_PORT}"
  ssh-copy-id -i "${PUB_PATH}" -p "${SSH_PORT}" -- "${COPY_TARGET}"
  printf 'Публичный ключ добавлен в authorized_keys на %s.\n' "${COPY_TARGET}"
else
  printf '\nПубличный ключ (добавьте в ~/.ssh/authorized_keys на игровом сервере):\n'
  printf '  %s\n' "${PUB_CONTENT}"
  printf '\nБыстрая установка с этой машины:\n'
  printf '  ssh-copy-id -i %q -p %s user@game.example.com\n' "${PUB_PATH}" "${SSH_PORT}"
  PUB_QUOTED="'${PUB_CONTENT//\'/\'\\\'\'}'"
  printf '  # либо вручную:\n'
  printf '  printf '\''%%s\\n'\'' %s | ssh -p %s user@game.example.com '\''mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys'\''\n' "${PUB_QUOTED}" "${SSH_PORT}"
fi
