use std::io::{self, Cursor, Write};
use azalea_buf::{AzaleaRead, AzaleaWrite, BufReadError, AzaleaReadVar};
use azalea_registry::identifier::Identifier;
use azalea_protocol_macros::ServerboundGamePacket;
use simdnbt::owned::Nbt;

#[derive(Clone, Debug, PartialEq, ServerboundGamePacket)]
pub struct ServerboundCustomClickAction {
    pub id: Identifier,
    pub payload: Nbt,
}

// 1.21.11 UNTRUSTED_TAG_CODEC wraps optional network NBT in a VarInt-sized
// byte buffer (ByteBufCodecs.lengthPrefixed(65536)), including TAG_End for None.
impl AzaleaRead for ServerboundCustomClickAction {
    fn azalea_read(buf: &mut Cursor<&[u8]>) -> Result<Self, BufReadError> {
        let id = Identifier::azalea_read(buf)?;
        let length = u32::azalea_read_var(buf)? as usize;
        let start = buf.position() as usize;
        let end = start.checked_add(length).ok_or_else(|| BufReadError::Custom("custom click length overflow".into()))?;
        if length > 65536 || end > buf.get_ref().len() { return Err(BufReadError::Custom("invalid custom click NBT length".into())); }
        let mut payload_buf = Cursor::new(&buf.get_ref()[start..end]);
        let payload = Nbt::azalea_read(&mut payload_buf)?;
        if payload_buf.position() as usize != length { return Err(BufReadError::Custom("trailing custom click NBT bytes".into())); }
        buf.set_position(end as u64);
        Ok(Self { id, payload })
    }
}
impl AzaleaWrite for ServerboundCustomClickAction {
    fn azalea_write(&self, buf: &mut impl Write) -> io::Result<()> {
        self.id.azalea_write(buf)?;
        let mut bytes = Vec::new();
        self.payload.azalea_write(&mut bytes)?;
        if bytes.len() > 65536 { return Err(io::Error::new(io::ErrorKind::InvalidInput, "custom click NBT exceeds limit")); }
        bytes.azalea_write(buf)
    }
}
