# Download verification

v0.5.23 downloads are signed and checked. The VirusTotal links below refer to
the exact final download hashes. Live gameplay confirmation of the latest
mining, crafting and storage fixes is still pending.

## What has been checked

- 278 automated Rust checks passed. The default run leaves one existing test
  and the two opt-in HTTPS probes ignored; both HTTPS probes passed separately.
- The HTTPS checks accepted a valid public Minecraft service certificate and
  rejected an untrusted local certificate. No account or login requests were made.
- The Android API 34 networking probe resolved Zeus and received its real
  Java 1.21.11 status response, including the MOTD and server icon.
- The Ubuntu 24.04 Linux build passed library loading and actual offscreen
  GLES3 rendering checks. The verified native build needs glibc 2.38 or newer;
  the final combined build passed those library checks too. Other
  distributions have not been verified.
- Fresh memory worlds passed natural Forest, Rainforest and Taiga population
  checks, including large branched oaks, tall birch and tall spruce. This uses
  normal world generation, not manually placed trees.
- All 11 native fallback item icons passed the offscreen GLES3 check at default
  metadata: batched and direct pixels matched, with no solid red fill or GL
  errors. The next normal item icon and render state stayed correct too.

These checks do not mean every server, resource pack, gameplay situation or
phone has been verified. This is still an alpha.

The mining regression checks also cover releasing a survival log before it
breaks, continued held mining, changing targets and moving out of reach. Native
mouse, keyboard, controller and touch input checks cover held target changes.
Live gameplay acceptance of this fix is still pending.

## Download hashes

SHA-256 values below match the exact signed APKs and Linux archive.

| Download | SHA-256 |
| :--- | :--- |
| `TruancyCraftPE-0.5.23-Android-arm64-v8a.apk` | `b5bb4831490ed261d2a9407482e47d94ddfae72b5318fbd688f37b94bd54524e` |
| `TruancyCraftPE-0.5.23-Android-armeabi-v7a.apk` | `c27bd1928897b2f31bddf061b60cab79e617fcefdea98054cde1e13931232abe` |
| `TruancyCraftPE-0.5.23-Linux-x86_64.tar.gz` | `d8cb82b9d827af06690552357d6b89fbd39bbf75b04227843d2308c76cead850` |

## Android signature

Official Android downloads use the **Truancy209** release certificate:

```text
e0e241d7c0730e3d2ef1987f6bda63f24bc56b5cb9c4e94713d09235093628df
```

Both APK signatures and all packaged assets/libraries have been checked.
The private signing key is not distributed.

[Public certificate](release-certificate.pem) · [Signing notes](../docs/RELEASE-SIGNING.md)

## Linux signature

The Linux archive has a detached signature made with the same release key.
The archive signature was checked against the public certificate above. It will be
included with the release as `TruancyCraftPE-0.5.23-Linux-x86_64.tar.gz.p7s`.

To check it with the archive and public certificate in the same folder:

```sh
openssl cms -verify -binary -inform DER \
  -in TruancyCraftPE-0.5.23-Linux-x86_64.tar.gz.p7s \
  -content TruancyCraftPE-0.5.23-Linux-x86_64.tar.gz \
  -CAfile release-certificate.pem -purpose any -out /dev/null
```

## Scans

Checked 2026-10-15. These are new reports for the exact files above.

| Download | VirusTotal result |
| :--- | :--- |
| Android 64-bit | [0/64 detections](https://www.virustotal.com/gui/file/b5bb4831490ed261d2a9407482e47d94ddfae72b5318fbd688f37b94bd54524e) |
| Android 32-bit | [0/64 detections](https://www.virustotal.com/gui/file/c27bd1928897b2f31bddf061b60cab79e617fcefdea98054cde1e13931232abe) |
| Linux x86_64 | [0/56 detections](https://www.virustotal.com/gui/file/d8cb82b9d827af06690552357d6b89fbd39bbf75b04227843d2308c76cead850) |

Scan results describe the engines that returned a result, not a guarantee of
safety. Some engines do not support these file types or time out. VirusTotal
also displayed an archive-inspection size warning for the APK uploads; these
reports do not certify every embedded asset or library separately.

[Previous release verification: v0.5.22.1](v0.5.22.1/README.md)

The crafting GUI result-delivery and response/retry fixes pass native GLES3 checks, including bone blocks, a full bag and delayed inventory updates. Live confirmation is pending the next game restart.

GUI mode is the default on a new install; your Storage/Menu choice persists across windows and game restarts. The Storage toggle uses PE chest drawers without reopening the server window. Native checks cover nine container layouts and whole/half/quick transfers. One-second server refresh checks cover offscreen rows, slow/offline replies and bounded requests. Live confirmation is pending.
