#!/usr/bin/env bash
set -euo pipefail

ARCHIVE=${1:?Usage: verify_bee_backup.sh ARCHIVE.tar.gz}
test -f "$ARCHIVE"
test -f "$ARCHIVE.sha256"
sha256sum -c "$ARCHIVE.sha256"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
tar -C "$WORK" -xzf "$ARCHIVE"
ROOT=$(find "$WORK" -mindepth 1 -maxdepth 1 -type d | head -1)
test -n "$ROOT"
(
  cd "$ROOT"
  sha256sum -c SHA256SUMS
)
echo "backup_valid=$ARCHIVE"
