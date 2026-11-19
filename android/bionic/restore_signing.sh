#!/usr/bin/env bash
# Restore the original keystore if supplied; otherwise make a TEMPORARY, installable
# self-signed APK. Never claim that a fresh key can update an existing installation.
set -Eeuo pipefail
cd -- "$(dirname -- "$0")/../.."

key="out/android-bionic/experimental-development.jks"
temporary_marker="out/android-bionic/temporary-signing.flag"
expected_cert="c0cd3fdceedafc4d3d2d4c24d78e8c1a81a06ef7ebd37e2824f423decbca7ffc"
mode=${TLOU_APK_SIGNING_MODE:-auto}
case "$mode" in
  auto|original) ;;
  *) echo "::error::Invalid TLOU_APK_SIGNING_MODE: $mode (expected auto or original)." >&2; exit 8 ;;
esac
mkdir -p -- "$(dirname -- "$key")"
umask 077

if [[ -f "$temporary_marker" ]]; then
  if [[ "$mode" == original ]]; then
    echo "::error::Strict original signing requested but the existing keystore is temporary." >&2
    exit 8
  fi
  if [[ ! -s "$key" ]]; then
    echo "::error::Temporary signing marker exists without a keystore." >&2
    exit 8
  fi
elif [[ ! -s "$key" && -n "${TLOU_SIGNING_JKS_BASE64:-}" ]]; then
  if ! printf "%s" "$TLOU_SIGNING_JKS_BASE64" | base64 --decode > "$key"; then
    rm -f -- "$key"
    echo "::error::TLOU_SIGNING_JKS_BASE64 could not be decoded." >&2
    exit 8
  fi
  unset TLOU_SIGNING_JKS_BASE64
elif [[ ! -s "$key" && "$mode" == original ]]; then
  echo "::error::Original TLOU signing identity unavailable. Set TLOU_SIGNING_JKS_BASE64 to create a compatible update." >&2
  exit 8
elif [[ ! -s "$key" ]]; then
  echo "::warning::Generating a new temporary signing key for this experimental APK. It CANNOT update an installed TLOU APK. Back up app data and uninstall the older app first." >&2
  keytool -genkeypair -noprompt -storetype PKCS12 -alias tlou-bionic \
    -keyalg RSA -keysize 3072 -sigalg SHA256withRSA -validity 3650 \
    -keystore "$key" \
    -storepass "TLOU-Bionic-development" -keypass "TLOU-Bionic-development" \
    -dname "CN=TLOU Temporary Android Install, O=Experimental" > /dev/null
  touch "$temporary_marker"
fi
[[ -s "$key" ]] || { echo "::error::Missing or empty Android signing keystore" >&2; exit 8; }
chmod 600 -- "$key"
if ! cert=$(keytool -exportcert -alias tlou-bionic -keystore "$key" \
      -storepass "TLOU-Bionic-development" 2>/dev/null | sha256sum | cut -d" " -f1); then
  echo "::error::Invalid signing keystore or missing tlou-bionic alias." >&2
  exit 8
fi
if [[ -f "$temporary_marker" ]]; then
  if [[ "$cert" == "$expected_cert" ]]; then
    echo "::error::A temporary key unexpectedly matches the original signing identity." >&2
    exit 8
  fi
  echo "::warning::TEMPORARY APK SIGNATURE: every fresh build needs a reinstall. Do not use it as a persistent update key." >&2
elif [[ "$cert" != "$expected_cert" ]]; then
  echo "::error::Signing key does not match the original TLOU APK; refusing to present this as a compatible update." >&2
  exit 8
else
  echo "Verified original TLOU APK signing identity (certificate SHA-256 matches pinned release)."
fi
