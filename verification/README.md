# Download verification

v0.5.23 local test candidates are signed and checked. They have not been
published or uploaded for new scans. Previous scan results do not describe
these builds. Live gameplay confirmation of the mining fixes is still pending.

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
| `TruancyCraftPE-0.5.23-Android-arm64-v8a.apk` | `2667d64ab61a72f8796f489b5826351df1d26a6eecbfa73190d16f94e6ab08fd` |
| `TruancyCraftPE-0.5.23-Android-armeabi-v7a.apk` | `5d555bf087f9eca74642fc41c8b22968e170d374bb3445ba737400bb8cea2aeb` |
| `TruancyCraftPE-0.5.23-Linux-x86_64.tar.gz` | `54d33eec733764b192a5b666d8f881c5bdacc0101e56bead03a5f5277764390b` |

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

New scans have not been uploaded. Report links and the date checked will go
here once they refer to the exact final download hashes. No detection counts
from an older release are carried forward.

[Previous release verification: v0.5.22.1](v0.5.22.1/README.md)

The crafting GUI result-delivery and response/retry fixes pass native GLES3 checks, including bone blocks, a full bag and delayed inventory updates. Live confirmation is pending the next game restart.

GUI mode stays the default for server menus. The Storage toggle uses PE chest drawers without reopening the server window. Native checks cover nine container layouts and whole/half/quick transfers. One-second server refresh checks cover offscreen rows, slow/offline replies and bounded requests. Live confirmation is pending.

GUI mode stays the default for server menus. The Storage toggle uses PE chest drawers without reopening the server window. Native checks cover nine container layouts and whole/half/quick transfers. One-second server refresh checks cover offscreen rows, slow/offline replies and bounded requests. Live confirmation is pending.
