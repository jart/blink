#!/usr/bin/env bash
set -euo pipefail

DEST="freebsd"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dest)
            DEST="$2"
            shift 2
            ;;
        --dest=*)
            DEST="${1#--dest=}"
            shift
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

MIRROR="https://download.freebsd.org/ftp/snapshots/amd64/amd64"

echo "Fetching snapshot list from ${MIRROR}/ ..."
LATEST_RELEASE=$(curl -fsSL "${MIRROR}/" \
    | grep -oE '[0-9]+\.[0-9]+-CURRENT/' \
    | sort -V -u \
    | tail -n1 \
    | tr -d '/')

if [[ -z "${LATEST_RELEASE}" ]]; then
    echo "Could not determine latest FreeBSD -CURRENT snapshot" >&2
    exit 1
fi

echo "Latest snapshot: ${LATEST_RELEASE}"

RELEASE_URL="${MIRROR}/${LATEST_RELEASE}"

echo "Looking for base tarball at ${RELEASE_URL}/ ..."
TARBALL=$(curl -fsSL "${RELEASE_URL}/" \
    | grep -oE 'base\.txz' \
    | head -n1)

if [[ -z "${TARBALL}" ]]; then
    echo "base.txz not found in ${RELEASE_URL}/" >&2
    exit 1
fi

TARBALL_URL="${RELEASE_URL}/${TARBALL}"

mkdir -p "${DEST}"
TMP_TARBALL=$(mktemp --suffix=.txz)
trap 'rm -f "${TMP_TARBALL}"' EXIT

echo "Downloading ${TARBALL_URL} ..."
curl -fSL -o "${TMP_TARBALL}" "${TARBALL_URL}"

echo "Unpacking into ${DEST}/ ..."
tar -xpf "${TMP_TARBALL}" -C "${DEST}"

mkdir -p "${DEST}/etc"
echo "nameserver 8.8.8.8" > "${DEST}/etc/resolv.conf"

echo "Done. FreeBSD ${LATEST_RELEASE} base unpacked to ${DEST}/"

#echo "We need host /dev/null symlinked into rootfs"
#chmod u+w "${DEST}/dev"
#ln -sf /dev/null "${DEST}/dev/null"
