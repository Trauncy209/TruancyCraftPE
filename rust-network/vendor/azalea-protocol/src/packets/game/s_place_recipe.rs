use azalea_buf::AzBuf;
use azalea_protocol_macros::ServerboundGamePacket;

#[derive(AzBuf, Clone, Debug, PartialEq, ServerboundGamePacket)]
pub struct ServerboundPlaceRecipe {
    #[var]
    pub container_id: i32,
    // 1.21.11 RecipeDisplayId uses a VarInt, not a resource identifier.
    #[var]
    pub recipe: u32,
    pub shift_down: bool,
}
