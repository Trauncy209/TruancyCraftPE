# 0.5.24.1 movement checks

Movement-only changes compared with `481154513c8aaca753c95674f9d7823fe486998a`: finite-world perimeter collision and recovery, finite-edge tick checks, Java-to-native Survival controller replacement, and suppression of generic movement echoes to their owning player. Weather is excluded.

## Verified

- `tests/gameplay_regressions.cpp` passes against the movement hotfix's Linux client objects: controller replacement; ticking at all finite corners, the reported x=31.92/z=146.38 position and center; collision across each of the four borders; recovery from positions beyond all four borders; remote-player generic-echo suppression. Recovery test probes detach from chunk lists before destruction.
- The isolated two-client dedicated-server lifecycle fixture passed movement, visibility, chat, block/chunk exchange, listing/counts, capacity rejection, save/restart persistence, and absence of generic movement echoes to either owning client.
- Read-only code review confirmed finite bounds explicitly exclude Java-server and infinite worlds, use ordinary entity collision clipping, and update chunk membership through `LevelChunk::addEntity`.

Local evidence retained in the build workspace: `work/release-0.5.24.1-probe-cleanup-build.log`, `work/release-0.5.24.1-probe-cleanup-tests.log`, `work/gameplay-network-regressions.log`, and `work/dedicated-clients-modify.log`.

## Limits

These tests do not establish physical-phone gameplay or performance, every possible saved-world state, or full Java multiplayer regression coverage. The finite-border fixture uses a probe entity rather than a complete touch-input session. Existing server movement-packet ownership/coordinate validation and explicit position corrections to older clients remain separate work; this patch does not claim to provide them. Live-window checks and final artifact/signature verification must be recorded separately before publication.

## Final release checks

Both final Android ABIs report app identifier `com.truancycraft.tcpe`, version `0.5.24.1`, versionCode `6007`, matching packaged native libraries and the existing release certificate. APKs pass 16 KiB alignment verification. The final portable Linux client was opened in a separate window: its title screen showed 0.5.24.1; a new blank-name finite Survival world loaded and accepted walking/jumping. Screenshots remain in `work/movement-0.5.24.1-*.png`. No physical-phone gameplay is claimed.

The final versioned server/client network fixture passes in `work/release-0.5.24.1-lifecycle.log`; operator default-deny/grant/revoke/restart checks pass in `work/release-0.5.24.1-operators.log`. Linux game/server archives have verified detached release signatures and all four downloads have SHA-256 records. One initial discovery test used a pre-version-bump fixture and failed its version-scoped LAN identifier filter; rebuilding the fixture against the final client objects resolved it. No protocol change was introduced.
