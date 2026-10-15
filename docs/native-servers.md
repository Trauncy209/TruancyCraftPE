# Native TCPE servers

Native TCPE worlds and Java servers share the Servers screen. Use **Add → Type: TCPE** for a native host, enter its address, and save. The default port is **25565**. Java remains the default type for existing saved servers and new entries; switching the type does not change the port.

Nearby native worlds appear automatically. Discovery also checks the older 19132–19135 ports, and joining a discovered world uses the port advertised by that host. A saved entry and the same discovered world are combined into one row. Native hosts currently use IPv4 addresses or hostnames that resolve to IPv4.

## Hosting and icons

Open a world with TCPE's existing multiplayer hosting enabled. New hosts listen on UDP port **25565** for native gameplay. This uses the existing TCPE LAN protocol, including its version compatibility checks.

For the server list, a small TCP status service uses the same numeric port. It supplies the host name, current/maximum player count and an optional PNG icon. Internet hosts need both **UDP 25565** for gameplay and **TCP 25565** for listing metadata reachable through their firewall/router. Hosting does not automatically configure port forwarding.

Place a **64 × 64 PNG**, named `server-icon.png`, in the host process's working directory before hosting. The icon must be at most 64 KiB. Restart hosting to reload it. Without a supplied icon, the client shows the TCPE app icon. The shared list refreshes status once per second, with up to four requests in progress.

The TCP service only provides listing metadata. Native connections use the original UDP gameplay path; choosing TCPE does not connect through Java login or require a Microsoft account.

## Verification for this test build

- Real UDP discovery and two simultaneous native RakNet clients: pass.
- Native login, movement relay, chunk request, block update, chat and inventory packet round trips: pass, using test handlers.
- Disconnect/reconnect and metadata listener shutdown: pass.
- Real listing client: player counts and exact 64 × 64 PNG decoding: pass.
- Mixed Java/TCPE menu rendering, native Join routing, separate saved transports, LAN de-duplication, default port and type switch: pass in the native GLES3 UI fixture.
- Exact hostname lookup and bounded handling of incomplete/oversized metadata requests: pass.
- Native connection errors and pre-world disconnects display the error screen; full/refusing hosts reach the failure callback.
- Full rendered world play between a physical phone and Linux host: pending.

This first step restores direct-address native joining and the shared list around the existing in-game host. A separately packaged dedicated server, IPv6 native transport and universal compatibility with other Minecraft editions are not included.

To rerun the transport checks on Linux, complete a CMake build using the Unix Makefiles generator, then run `python3 tools/test-native-network.py --build /path/to/build`. The runner uses isolated temporary files and does not open a game window.
