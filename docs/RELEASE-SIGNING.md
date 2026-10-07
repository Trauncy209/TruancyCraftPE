# Release signing

Starting with **v0.5.22.1**, official TruancyCraftPE APK downloads use the
Truancy209 release certificate. The private key stays outside the repository.
The public certificate, APK SHA-256 hashes and scan links are published with
the release.

This release changes signing only. The 32-bit and 64-bit game files match
v0.5.22 byte for byte. The Android package name remains compatible with the
game's existing native code; a package name is not a signing certificate.

Android will not install an APK over another APK signed by a different key.
If moving from an older debug-signed build, back up your worlds and settings
before replacing that installation. Later builds using this release key can
use the normal update path. Do not delete an installation to work around an
update error until your data is backed up.

## Building signed releases

Build your APK normally, then use:

```sh
TCPE_RELEASE_KEYSTORE=/private/truancy-release.p12 \
TCPE_RELEASE_STOREPASS_FILE=/private/password \
bash ./tools/sign-release-apk.sh input.apk TruancyCraftPE.apk
```

Alternatively, those variables can be supplied to `build.sh`. The existing
debug signing remains available for local development. Set `TCPE_SKIP_INSTALL=1`
when producing artifacts so packaging does not uninstall or replace any app.

Keep the same private release key for future official updates. Never commit
keystores, password files or credentials.
