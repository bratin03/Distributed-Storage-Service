#!/usr/bin/env bash
set -euo pipefail

# Configuration
OPENSSL_VERSION="3.0.8"
PREFIX="/usr/local/ssl"
SRC_DIR="/usr/local/src"
LD_CONF="/etc/ld.so.conf.d/openssl-${OPENSSL_VERSION}.conf"
BASHRC="${HOME}/.bashrc"

echo "=== 1. Remove any existing custom OpenSSL in ${PREFIX} ==="
sudo rm -rf "${PREFIX}"
sudo rm -f "${LD_CONF}"

echo "=== 2. Update package lists and install build dependencies ==="
sudo apt update
sudo apt install -y build-essential zlib1g-dev wget

echo "=== 3. Download and extract OpenSSL ${OPENSSL_VERSION} ==="
sudo mkdir -p "${SRC_DIR}"
cd "${SRC_DIR}"
sudo wget -O "openssl-${OPENSSL_VERSION}.tar.gz" \
    "https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
sudo tar xzf "openssl-${OPENSSL_VERSION}.tar.gz"
cd "openssl-${OPENSSL_VERSION}"
sudo chown -R "${USER}:${USER}" .

echo "=== 4. Configure, build, and install ==="
./config --prefix="${PREFIX}" --openssldir="${PREFIX}" shared zlib
make -j"$(nproc)"
sudo make install

echo "=== 5. Register ${PREFIX}/lib64 with the dynamic linker ==="
echo "${PREFIX}/lib64" | sudo tee "${LD_CONF}" > /dev/null
sudo ldconfig

echo "=== 6. Verify installation ==="
"${PREFIX}/bin/openssl" version

echo "=== 7. Ensure ~/.bashrc ends with a newline ==="
if [ -n "$(tail -c1 "${BASHRC}")" ]; then
  echo "" >> "${BASHRC}"
fi

echo "=== 8. Update ~/.bashrc for future sessions ==="
grep -qxF "export PATH=\"${PREFIX}/bin:\$PATH\"" "${BASHRC}" || \
    echo "export PATH=\"${PREFIX}/bin:\$PATH\"" >> "${BASHRC}"
grep -qxF "export LD_LIBRARY_PATH=\"${PREFIX}/lib64:\$LD_LIBRARY_PATH\"" "${BASHRC}" || \
    echo "export LD_LIBRARY_PATH=\"${PREFIX}/lib64:\$LD_LIBRARY_PATH\"" >> "${BASHRC}"

echo "=== 9. Reload ~/.bashrc ==="
# shellcheck disable=SC1090
source "${BASHRC}"

echo "All done! OpenSSL version now:"
openssl version
