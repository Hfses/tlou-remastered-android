# Restore the original TLOU Android APK signing key

The existing Android app is signed with a private keystore named `experimental-development.jks`.
**A different key cannot update the installed app with the same application ID.**

The original build on 2026-11-08 (GitHub Actions run `42482239121`) successfully restored
`tlou-development-signing-v2` from Actions cache. On 2026-11-19 the cache restore was a miss.
GitHub Actions caches are not durable backups for private signing keys. The APK artifact has
the public certificate but **not** the private signing key; no code can reconstruct it.

## One-time recovery (requires the actual original JKS file)

1. Search your own PC or secure backup for `experimental-development.jks`. Never commit the
   keystore to this public repository, and never paste its base64 into a chat or issue.
2. On Windows PowerShell, substitute the actual local file path:

   ```powershell
   [Convert]::ToBase64String([IO.File]::ReadAllBytes("C:\caminho\experimental-development.jks")) | Set-Clipboard
   ```

3. Open this repository on GitHub → **Settings → Secrets and variables → Actions → New repository secret**.
4. Set the name to `TLOU_SIGNING_JKS_BASE64` and paste the base64 copied to the clipboard.
5. Run **TLOU Bionic Full Native and APK** again from the Actions tab, on the desired branch.

The workflow decodes the key inside the temporary runner and verifies the signing certificate
against SHA-256 `c0cd3fdceedafc4d3d2d4c24d78e8c1a81a06ef7ebd37e2824f423decbca7ffc`,
obtained from the verified 2026-11-08 v0.1.1 APK. Mismatched keys are rejected. The key is
never uploaded as a build artifact or saved to a GitHub Actions cache.

If no original keystore exists anywhere, a new signing identity would require an incompatible
new install (or changing application ID for a side-by-side version). It cannot silently
upgrade the currently installed package, and app data may be lost on uninstall.
