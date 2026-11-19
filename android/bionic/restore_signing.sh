#!/usr/bin/env bash
# Restore only the ORIGINAL TLOU release keystore from a GitHub Actions secret.
# The pinned SHA-256 below is the DER signing certificate from the verified
# successful APK published by run 42482239121 (2026-11-08).
set -Eeuo pipefail
cd -- "$(dirname -- "$0")/../.."
key="out/android-bionic/experimental-development.jks"
expected_cert="c0cd3fdceedafc4d3d2d4c24d78e8c1a81a06ef7ebd37e2824f423decbca7ffc"
if [[ ! -s "$key" ]]; then
  if [[ -z "${TLOU_SIGNING_JKS_BASE64:-}" ]]; then
    echo "::error::Original TLOU signing key is unavailable. GitHub Actions cache tlou-development-signing-v2 is no longer available. Restore your original experimental-development.jks to GitHub Actions secret TLOU_SIGNING_JKS_BASE64 (base64 of the actual .jks file). See android/ANDROID_SIGNING.md. A different key cannot update the existing app." >&2
    exit 8
  fi
  mkdir -p -- "$(dirname -- "$key")"
  umask 077
  if ! printf "%s" "$TLOU_SIGNING_JKS_BASE64" | base64 --decode > "$key"; then
    rm -f -- "$key"
    echo "::error::Could not decode TLOU_SIGNING_JKS_BASE64. Check the GitHub Actions secret contents." >&2
    exit 8
  fi
  unset TLOU_SIGNING_JKS_BASE64
fi
if [[ ! -s "$key" ]]; then
  echo "::error::Original TLOU keystore is empty." >&2
  rm -f -- "$key"
  exit 8
fi
chmod 600 -- "$key"
if ! cert=$(keytool -exportcert -alias tlou-bionic -keystore "$key" \
      -storepass "TLOU-Bionic-development" 2>/dev/null | sha256sum | cut -d" " -f1); then
  echo "::error::Invalid TLOU keystore or missing tlou-bionic alias." >&2
  exit 8
fi
if [[ "$cert" != "$expected_cert" ]]; then
  echo "::error::Keystore certificate does not match the existing TLOU APK. Refusing incompatible update signing." >&2
  exit 8
fi
echo "TLOU original Android signing identity verified (certificate SHA-256 pin matched)."
