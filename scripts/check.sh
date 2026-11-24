#!/usr/bin/env bash
# Runs every automated check of the repository with one command.
#
#   scripts/check.sh                # static checks, build, unit tests
#   scripts/check.sh --quick        # static checks only, a second, no build
#   scripts/check.sh -j 8           # more parallel build jobs
#   scripts/check.sh --strict       # warnings of the static checks fail the run
#
# The script stops at the first failing step, says what failed and where the
# full log of that step is. Exit code: 0 - everything passed, 1 - a check
# failed, 2 - the environment cannot run the checks.
#
# Nothing here is specific to one machine: the build directory, the generator
# and the number of jobs are detected or can be given on the command line.
set -Eeuo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)

BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
JOBS="${JOBS:-}"
GENERATOR="${GENERATOR:-}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
LOG_DIR=""
QUICK=0
SKIP_BUILD=0
SKIP_TESTS=0
STRICT=0
TEST_ARGS=()

usage() {
	sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
	cat <<-'EOF'

	Options:
	  -j, --jobs N        parallel build jobs (default: all cores)
	      --build-dir DIR build directory (default: <root>/build, BUILD_DIR)
	      --generator G   CMake generator (default: Ninja when installed, else Unix Makefiles)
	      --build-type T  CMake build type (default: RelWithDebInfo, BUILD_TYPE)
	      --quick         only the static checks, no build and no tests
	      --skip-build    reuse the binaries of the build directory
	      --skip-tests    build but do not run the unit tests
	      --strict        treat the warnings of the static checks as errors
	  -h, --help          this text
	      -- ARGS...      arguments for the test runner
	EOF
}

# ---------------------------------------------------------------- output

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
	RED=$'\033[31m'; GREEN=$'\033[32m'; YELLOW=$'\033[33m'; BOLD=$'\033[1m'; RESET=$'\033[0m'
else
	RED=""; GREEN=""; YELLOW=""; BOLD=""; RESET=""
fi

STEP_NAMES=()
STEP_STATES=()
STEP_TIMES=()
STEP_LOGS=()
CURRENT=""
STARTED=0
FAILED_STEP=""
FAILED_LOG=""

log() { printf '%s\n' "$*"; }
step() {
	CURRENT="$1"
	STARTED=$SECONDS
	printf '\n%s==> %s%s\n' "$BOLD" "$1" "$RESET"
}
die() {
	printf '%s\n' "${RED}error${RESET}: $*" >&2
	exit 2
}
on_error() {
	local status=$1 line=$2 command=$3
	trap - ERR
	printf '\n%s\n' "${RED}failed${RESET}: ${CURRENT:-startup} (exit $status, scripts/check.sh:$line)" >&2
	printf '%s\n' "  command: $command" >&2
	if [[ -n "$FAILED_LOG" && -f "$FAILED_LOG" ]]; then
		printf '%s\n' "  full log: $FAILED_LOG" >&2
		printf '%s\n' "  last lines:" >&2
		tail -n 30 "$FAILED_LOG" | sed 's/^/    /' >&2
	fi
	print_summary
	exit 1
}
trap 'on_error $? $LINENO "$BASH_COMMAND"' ERR

record() { # name state seconds [log]
	STEP_NAMES+=("$1")
	STEP_STATES+=("$2")
	STEP_TIMES+=("$3")
	STEP_LOGS+=("${4:-}")
}

print_summary() {
	[[ ${#STEP_NAMES[@]} -eq 0 ]] && return 0
	printf '\n%s\n' "${BOLD}summary${RESET}"
	local index
	for index in "${!STEP_NAMES[@]}"; do
		local state=${STEP_STATES[$index]}
		local color=$GREEN
		[[ "$state" == "FAIL" ]] && color=$RED
		[[ "$state" == "SKIP" ]] && color=$YELLOW
		printf '  %s%-4s%s %-24s %3ss\n' "$color" "$state" "$RESET" "${STEP_NAMES[$index]}" "${STEP_TIMES[$index]}"
	done
	printf '  %s total %ss\n' "" "$SECONDS"
	if [[ -n "$FAILED_LOG" ]]; then
		log "  log of the failed step: $FAILED_LOG"
	fi
}

run_logged() { # name command...
	local name=$1
	shift
	local log_file="$LOG_DIR/${name// /_}.log"
	FAILED_LOG="$log_file"
	printf '  $ %s\n' "$*"
	# The output stays visible and is kept for the failure report.
	set +e
	"$@" 2>&1 | tee "$log_file"
	local status=${PIPESTATUS[0]}
	set -e
	local elapsed=$((SECONDS - STARTED))
	if [[ $status -ne 0 ]]; then
		record "$CURRENT" FAIL "$elapsed" "$log_file"
		return "$status"
	fi
	record "$CURRENT" PASS "$elapsed" "$log_file"
	FAILED_LOG=""
	return 0
}

skip() {
	record "$CURRENT" SKIP "$((SECONDS - STARTED))"
	printf '  %s\n' "${YELLOW}skipped${RESET}: $*"
}

have() { command -v "$1" >/dev/null 2>&1; }

# ---------------------------------------------------------------- arguments

while [[ $# -gt 0 ]]; do
	case "$1" in
		-j|--jobs) [[ $# -ge 2 ]] || die "$1 needs a value"; JOBS=$2; shift 2 ;;
		--jobs=*) JOBS=${1#*=}; shift ;;
		--build-dir) [[ $# -ge 2 ]] || die "$1 needs a value"; BUILD_DIR=$2; shift 2 ;;
		--build-dir=*) BUILD_DIR=${1#*=}; shift ;;
		--generator) [[ $# -ge 2 ]] || die "$1 needs a value"; GENERATOR=$2; shift 2 ;;
		--generator=*) GENERATOR=${1#*=}; shift ;;
		--build-type) [[ $# -ge 2 ]] || die "$1 needs a value"; BUILD_TYPE=$2; shift 2 ;;
		--build-type=*) BUILD_TYPE=${1#*=}; shift ;;
		--quick) QUICK=1; shift ;;
		--skip-build) SKIP_BUILD=1; shift ;;
		--skip-tests) SKIP_TESTS=1; shift ;;
		--strict) STRICT=1; shift ;;
		-h|--help) usage; exit 0 ;;
		--) shift; TEST_ARGS=("$@"); break ;;
		*) die "unknown option: $1 (see --help)" ;;
	esac
done

# The logs stay next to the build, an ignored directory, so a failed step can
# still be read after the script exits.
LOG_DIR="$BUILD_DIR/check-logs"
rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"
cd "$ROOT_DIR"

# ---------------------------------------------------------------- preflight

step "environment"
have git || die "git is needed to read the repository"
have python3 || die "python3 is needed for the static checks and fake MariaDB tests"
if [[ $QUICK -eq 0 ]]; then
	have cmake || die "cmake is needed to build (see docs/development.md)"
	if [[ -z "$GENERATOR" ]]; then
		if have ninja; then GENERATOR="Ninja"; else GENERATOR="Unix Makefiles"; fi
	fi
	if [[ -z "$JOBS" ]]; then
		if have nproc; then JOBS=$(nproc); else JOBS=4; fi
	fi
fi
log "  root:      $ROOT_DIR"
log "  python3:   $(python3 -V 2>&1)"
[[ $QUICK -eq 0 ]] && log "  cmake:     $(cmake --version | head -n 1)"
[[ $QUICK -eq 0 ]] && log "  generator: $GENERATOR"
[[ $QUICK -eq 0 ]] && log "  build:     $BUILD_DIR ($BUILD_TYPE, $JOBS jobs)"
record "$CURRENT" PASS "$((SECONDS - STARTED))"

# ---------------------------------------------------------------- static checks

step "static checks"
STYLE_ARGS=()
[[ $STRICT -eq 1 ]] && STYLE_ARGS+=(--strict)
run_logged "static checks" python3 "$SCRIPT_DIR/check_style.py" "${STYLE_ARGS[@]}"

if [[ $QUICK -eq 1 ]]; then
	step "build and tests"
	skip "--quick was given: no build, no tests"
	print_summary
	log ""
	log "${GREEN}static checks passed${RESET}"
	exit 0
fi

# ---------------------------------------------------------------- configure

step "configure"
CMAKE_ARGS=(-S "$ROOT_DIR" -B "$BUILD_DIR" -G "$GENERATOR" "-DCMAKE_BUILD_TYPE=$BUILD_TYPE" -DDOWNLOAD_GTEST=ON)
if have ccache; then
	CMAKE_ARGS+=("-DCMAKE_C_COMPILER_LAUNCHER=ccache" "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
fi
if [[ $SKIP_BUILD -eq 1 ]]; then
	skip "--skip-build was given"
else
	run_logged "configure" cmake "${CMAKE_ARGS[@]}"
fi

# ---------------------------------------------------------------- build

step "build"
if [[ $SKIP_BUILD -eq 1 ]]; then
	skip "--skip-build was given"
else
	run_logged "build" cmake --build "$BUILD_DIR" --target mmoteeworlds_srv testrunner -j "$JOBS"
fi

# ---------------------------------------------------------------- unit tests

step "unit tests"
TESTRUNNER="$BUILD_DIR/testrunner"
if [[ $SKIP_TESTS -eq 1 ]]; then
	skip "--skip-tests was given"
elif [[ ! -x "$TESTRUNNER" ]]; then
	die "$TESTRUNNER is missing: build it first (scripts/check.sh) or drop --skip-build"
else
	# A stuck test must not block a machine forever.
	if have timeout; then
		run_logged "unit tests" timeout --foreground 900 "$TESTRUNNER" "${TEST_ARGS[@]}"
	else
		run_logged "unit tests" "$TESTRUNNER" "${TEST_ARGS[@]}"
	fi
fi

print_summary
log ""
log "${GREEN}every check passed${RESET}"
