use azalea_buf::AzBuf;
use azalea_protocol_macros::ClientboundGamePacket;
use azalea_registry::identifier::Identifier;

#[derive(AzBuf, ClientboundGamePacket, Clone, Debug, PartialEq)]
pub struct ClientboundCooldown {
    pub cooldown_group: Identifier,
    #[var]
    pub duration: u32,
}

#[cfg(test)]
mod tests {
    use super::*;
    use azalea_buf::{AzaleaRead, AzaleaWrite};
    use std::io::Cursor;
    #[test]
    fn modern_cooldown_group_wire_round_trip() {
        let packet = ClientboundCooldown { cooldown_group: Identifier::new("minecraft:wind_charge"), duration: 20 };
        let mut bytes = Vec::new();
        packet.azalea_write(&mut bytes).unwrap();
        assert_eq!(bytes[0], 21); // UTF-8 identifier length, not a registry numeric ID.
        assert_eq!(&bytes[1..22], b"minecraft:wind_charge");
        assert_eq!(bytes[22], 20);
        let mut input = Cursor::new(bytes.as_slice());
        assert_eq!(ClientboundCooldown::azalea_read(&mut input).unwrap(), packet);
        assert_eq!(input.position(), 23);
    }
}
