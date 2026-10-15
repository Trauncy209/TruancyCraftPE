# Powered Creeper texture provenance

The texture `data/images/armor/power.png` is the unchanged `textures/entity/creeper/creeper_armor.png` from the user-provided Minecraft Java 1.21.4 asset archive `/home/truancy/Downloads/1.21.4 assets(2).zip` (entry `1.21.4 assets/assets/minecraft/textures/entity/creeper/creeper_armor.png`). It is used under the user's explicit instruction to reuse supplied assets. It was not taken from 4jcraft and is not represented as GPL-licensed or original TCPE artwork. Mojang/Microsoft retain their asset rights; this record grants no additional redistribution rights.

Dimensions: 64×32 RGBA PNG. SHA-256: `6aeb93bfc947d6ccf1e4dd101ff34fdffa276328a8c2514dd5298eb733dbca98`.

The independently implemented charged shell expands the native Creeper model by 2 model units, scrolls both texture axes by 0.01 per entity tick, and draws additive unlit half-bright geometry. GLES3's existing texture-matrix shader uniform supplies the scroll. Texture-coordinate matrices, lighting, blend mode and color are restored after the armor pass. Visual/reference and physical phone verification remain pending.
