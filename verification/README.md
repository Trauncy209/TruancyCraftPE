# Download verification

Official v0.5.22.1 APKs are signed with the **Truancy209** release certificate.

| Download | SHA-256 | VirusTotal |
| :--- | :--- | :--- |
| 32-bit APK | `551fa7d30012a9c427f83e38ff760a8687001fe2898dd5b38e3df6f2e6dc78c8` | [Report](https://www.virustotal.com/gui/file/551fa7d30012a9c427f83e38ff760a8687001fe2898dd5b38e3df6f2e6dc78c8) |
| 64-bit APK | `d5857b6f09cc17e151f854667daf7b267e1a438cb70938871ce4b92d9f739adf` | [Report](https://www.virustotal.com/gui/file/d5857b6f09cc17e151f854667daf7b267e1a438cb70938871ce4b92d9f739adf) |

Both reports showed **0/68 detections** when checked on October 6, 2026.
The links point to the exact signed APK downloads.

## Certificate

Certificate identity: `CN=Truancy209, O=Truancy Projects, C=US`.

Certificate SHA-256:

```text
e0e241d7c0730e3d2ef1987f6bda63f24bc56b5cb9c4e94713d09235093628df
```

[Public certificate](release-certificate.pem) · [Payload comparison](payload-verification.json) · [Update notes](../docs/RELEASE-SIGNING.md)

The game files in both builds match v0.5.22 byte for byte. This release changes
the APK signing identity. The private signing key is not distributed.

To check your download:

```sh
sha256sum TruancyCraftPE-v0.5.22.1-64bit.apk
apksigner verify --verbose --print-certs TruancyCraftPE-v0.5.22.1-64bit.apk
```
