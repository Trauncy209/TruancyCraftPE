# Native inventory transactions v1

Native Survival clients negotiate v1 through the optional inventory snapshot
trailer. TCSMP uses Survival. Native Creative LAN worlds retain their existing
unlimited palette behavior and do not advertise this capability.

A session nonce identifies the login. Commands have monotonically increasing
sequence numbers; duplicate commands only re-send the current acknowledgment.
A rejected command consumes its sequence number, while a gap or wrong session
does not. Server revisions describe snapshots, rather than rejecting legitimate
commands merely because a pickup happened after their original client base.

The server validates recipes, real owned stacks, armor slots, nearby workbenches,
and current open storage before committing changes. Player and storage contents
are committed together. Owner snapshots contain inventory, hotbar, armor, and the
open storage contents at the same acknowledgment. Clients retain and replay
unacknowledged semantic intentions on the new base, preserving newly picked-up
loot and rapid crafting. Counts in legacy client snapshot bodies are never used
as authority once v1 is active.

Hotbar links and queued item actions resolve their expected item identity when
new loot moves a predicted crafting result to another backing slot. Server-owned
tool wear is preserved while subsequent queued mining actions remain valid.
Native placement, mining wear, drops, food completion, bow release, and death
inventory changes run on the server. World actions retain the existing native
block targeting protocol; this does not introduce a complete movement/mining
anti-cheat system.

Legacy Survival peers remain compatible through the shape-checked legacy path.
That compatibility path does not gain the transaction ordering guarantees of v1.
