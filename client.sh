#!/usr/bin/env bash
set -euo pipefail

#–– Logging helper ––
log() {
  local GREEN="\033[1;32m"
  local YELLOW="\033[1;33m"
  local RED="\033[1;31m"
  local RESET="\033[0m"
  local TS="[$(date +'%Y-%m-%d %H:%M:%S')]"
  echo -e "${GREEN}${TS}${RESET} ${YELLOW}$*${RESET}"
}

err() {
  local RED="\033[1;31m"
  local RESET="\033[0m"
  local TS="[$(date +'%Y-%m-%d %H:%M:%S')]"
  echo -e "${RED}${TS}${RESET} $*" >&2
}

#–– Base directory ––
BASE_DIR="$(dirname "$(readlink -f "$0")")/CODE/src/client"

log "Changing to client directory: $BASE_DIR"
cd "$BASE_DIR"

log "Building client"
make

log "Running database setup"
./db_setup.sh

log "Running directory structure setup"
./dir_setup.sh

#–– Usage info ––
usage() {
  err "Username and password not provided or empty."
  log "Usage: $0 <username> <password>"
  log "Falling back to interactive/default mode"
  ./client.out
}

#–– Launch client ––
run_client() {
  local USERNAME=$1
  local PASSWORD=$2
  log "Launching client with provided credentials"
  ./client.out "$USERNAME" "$PASSWORD"
}

#–– Main argument handling ––
if [[ $# -ne 2 ]]; then
  usage
else
  if [[ -z "${1:-}" || -z "${2:-}" ]]; then
    usage
  else
    run_client "$1" "$2"
  fi
fi
