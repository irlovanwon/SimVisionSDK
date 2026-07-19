#!/bin/bash
#
# SimVisionSDK - Generate a self-signed TLS certificate for the HTTPS admin server.
# Usage: ./scripts/gen_certs.sh [output_dir]
#
set -u

OUT_DIR="${1:-$(cd "$(dirname "$0")/.." && pwd)/certs}"
mkdir -p "${OUT_DIR}"

CRT="${OUT_DIR}/server.crt"
KEY="${OUT_DIR}/server.key"

if [ -f "${CRT}" ] && [ -f "${KEY}" ]; then
    echo "[SimVisionSDK] Certs already exist at ${OUT_DIR}"
    exit 0
fi

echo "[SimVisionSDK] Generating self-signed cert -> ${CRT}"
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "${KEY}" \
    -out "${CRT}" \
    -days 3650 \
    -subj "/CN=SimVisionSDK" \
    -addext "subjectAltName=IP:127.0.0.1" >/dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "[SimVisionSDK] TLS cert generated."
else
    echo "[SimVisionSDK] ERROR: openssl failed." >&2
    exit 1
fi
