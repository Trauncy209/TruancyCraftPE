use azalea_buf::AzBuf;
use azalea_protocol_macros::ServerboundConfigPacket;
use uuid::Uuid;

#[derive(AzBuf, Clone, Debug, PartialEq, ServerboundConfigPacket)]
pub struct ServerboundResourcePack {
    pub id: Uuid,
    pub action: Action,
}

#[derive(AzBuf, Clone, Copy, Debug, Eq, PartialEq)]
pub enum Action {
    SuccessfullyLoaded = 0,
    Declined = 1,
    FailedDownload = 2,
    Accepted = 3,
    Downloaded = 4,
    InvalidUrl = 5,
    FailedReload = 6,
    Discarded = 7,
}

#[cfg(test)]
mod tests {
    use super::*;
    use azalea_buf::{AzaleaRead, AzaleaWrite};
    use std::io::Cursor;

    #[test]
    fn java_1_21_11_resource_pack_status_wire_values() {
        let actions = [Action::SuccessfullyLoaded, Action::Declined, Action::FailedDownload,
            Action::Accepted, Action::Downloaded, Action::InvalidUrl,
            Action::FailedReload, Action::Discarded];
        for (wire_value, action) in actions.into_iter().enumerate() {
            let mut encoded = Vec::new();
            action.azalea_write(&mut encoded).unwrap();
            assert_eq!(encoded, vec![wire_value as u8]);
            let mut input = Cursor::new(encoded.as_slice());
            assert_eq!(Action::azalea_read(&mut input).unwrap(), action);
            let packet = ServerboundResourcePack { id: Uuid::nil(), action };
            let mut encoded_packet = Vec::new();
            packet.azalea_write(&mut encoded_packet).unwrap();
            assert_eq!(encoded_packet.len(), 17);
            assert_eq!(encoded_packet[16], wire_value as u8);
        }
    }
}
