#!/usr/bin/env bash

#–– Logging helper ––
log() {
  echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*"
}

#–– Base directory for all services ––
BASE_DIR="$(dirname "$(readlink -f "$0")")/CODE/src"
declare -a TERM_PIDS=()

#–– Helper: ensure a Python venv exists & dependencies are installed ––
ensure_venv() {
  local D="$1"
  log "Ensuring virtualenv in $D"
  pushd "$D" >/dev/null || { log "ERROR: Cannot cd into $D"; exit 1; }

  if [[ ! -d "venv" ]]; then
    log "Creating virtualenv in $D/venv..."
    if ! python3 -m venv venv; then
      log "ERROR: python3 -m venv failed. Is python3-venv installed?"
      popd >/dev/null; exit 1
    fi

    log "Activating venv and installing requirements..."
    source venv/bin/activate
    if [[ -f "requirements.txt" ]]; then
      log "Upgrading pip..."
      if ! pip install --upgrade pip; then
        log "ERROR: pip upgrade failed"; deactivate; popd >/dev/null; exit 1
      fi
      log "Installing dependencies from requirements.txt..."
      if ! pip install -r requirements.txt; then
        log "ERROR: pip install failed"; deactivate; popd >/dev/null; exit 1
      fi
    else
      log "Warning: no requirements.txt found in $D"
    fi
    deactivate
  else
    log "Virtualenv already exists in $D, skipping"
  fi

  popd >/dev/null
}

#–– Function to launch an xterm, optionally activate venv, then run CMD ––
launch_xterm() {
  local DIR="$1"
  local CMD="$2"
  log "Launching xterm for $DIR → $CMD"
  xterm -hold -e bash -ic "
    cd '$DIR' || exit 1
    if [[ -f 'venv/bin/activate' ]]; then
      source venv/bin/activate
    fi
    $CMD
    echo
    echo '=== Process finished with exit code \$? ==='
    exec bash
  " &
  TERM_PIDS+=( $! )
}

#–– Cleanup handler ––
cleanup() {
  log "Caught Ctrl-C → terminating all xterms..."
  for pid in "${TERM_PIDS[@]}"; do
    kill "$pid" 2>/dev/null || true
  done
  exit 0
}
trap cleanup SIGINT

log "Starting core services setup"

#–– 1) Core services setup in main shell ––
log "Setting up password_db"
pushd "$BASE_DIR/password_db" >/dev/null
  ./create.sh && ./start.sh
popd >/dev/null

log "Setting up authentication_server"
pushd "$BASE_DIR/authentication_server" >/dev/null
  make
  ./stop_nginx.sh && sleep 1
  ./start_nginx.sh
popd >/dev/null

log "Setting up signupserver"
pushd "$BASE_DIR/signupserver" >/dev/null
  ./compile.sh
  ./stop_nginx.sh && sleep 1
  ./start_nginx.sh
popd >/dev/null

log "Setting up notification_server"
pushd "$BASE_DIR/notification_server" >/dev/null
  make
  ./stop_nginx.sh && sleep 1
  ./start_nginx.sh
popd >/dev/null

log "Setting up metaserver"
pushd "$BASE_DIR/metaserver" >/dev/null
  make
  ./stop_nginx.sh && sleep 1
  ./start_nginx.sh
popd >/dev/null

#–– Additional direct launches (if needed) ––
log "Launching standalone password_db"
cd CODE/src/password_db
./create.sh
./start.sh
cd - >/dev/null

#–– 2) data_blockserver venv + launch ––
ensure_venv "$BASE_DIR/data_blockserver"
launch_xterm "$BASE_DIR/data_blockserver" "foreman start"

#–– 3) meta_blockserver venv + launch ––
ensure_venv "$BASE_DIR/meta_blockserver"
launch_xterm "$BASE_DIR/meta_blockserver" "foreman start"

#–– 4) Launch remaining services in xterms ––
launch_xterm "$BASE_DIR/authentication_server" "foreman start"
launch_xterm "$BASE_DIR/signupserver"         "foreman start"
launch_xterm "$BASE_DIR/notification_server"   "foreman start"
launch_xterm "$BASE_DIR/metaserver"            "foreman start"

log "All services up. Press Ctrl-C here to shut down spawned xterms."
wait
