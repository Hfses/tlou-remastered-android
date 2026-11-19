# Android APK signing (TLOU experimental builds)

Android requires a digital signature for installation. You **do not** need to provide a key
to build an APK: the GitHub workflow can create a new *temporary signing key* automatically.

## Default: build and install without an existing key

On pushes or manual builds, the workflow uses `signing=auto`:

- If GitHub Actions secret `TLOU_SIGNING_JKS_BASE64` contains the **original** keystore,
  its certificate is checked against the signed v0.1.1 APK published in run `42482239121`.
- Otherwise, a fresh temporary PKCS12 keystore is generated inside the GitHub runner
  with `keytool`, and the APK is signed and verified. The private key is **not** committed,
  uploaded as an artifact, or cached.

**WARNING: a temporarily signed APK cannot update the existing app** whose certificate
is different. Back up the current app's files/saves before uninstalling it. You must
uninstall the old app with the same application ID before installing the new APK.
Uninstalling generally deletes the app's local data unless separately backed up.

**Each independently generated temporary key is different.** Consequently, later temporary
APKs also cannot update this temporary build, unless you adopt a persistent signing key.
The APK artifact contains a `.apk.signing.txt` file describing which signature was used.

## Optional: preserve compatible updates with the original key

If you find the original `experimental-development.jks`, preserve it outside the public
repository and set a GitHub Actions repository secret named `TLOU_SIGNING_JKS_BASE64`.

On Windows PowerShell, with the actual file path:

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("C:\caminho\experimental-development.jks")) | Set-Clipboard
```

In GitHub go to **Settings → Secrets and variables → Actions → New repository secret**.
Create `TLOU_SIGNING_JKS_BASE64`, paste the value, and run the workflow again.

Select manual `signing=original` to require an original-key update: this mode fails instead of
generating a temporary key. The original certificate's SHA-256 is pinned to
`c0cd3fdceedafc4d3d2d4c24d78e8c1a81a06ef7ebd37e2824f423decbca7ffc` (verified
in the v2/v3 signing block of the 2026-11-08 APK).
The original signing key was previously stored only in GitHub Actions cache
`tlou-development-signing-v2`, which is no longer accessible.

A signed APK contains the **public** certificate, not its private key. Without a backup,
the original keystore cannot be recreated. A stable replacement key must be stored
securely outside the repository and configured in Actions for future updates.
