use azalea_protocol::read::{DecompressionError, compression_decoder, read_raw_packet_from_buffer};
use std::io::Cursor;
#[test]
fn compressed_length_must_match() {
    let zlib = [120, 156, 83, 96, 100, 2, 0, 0, 103, 0, 36];
    for declared in [2u8, 4u8] {
        let mut payload = vec![declared];
        payload.extend_from_slice(&zlib);
        assert!(matches!(
            compression_decoder(&mut Cursor::new(payload.as_slice()), 1),
            Err(DecompressionError::MismatchedLength { .. })
        ));
    }
    let mut payload = vec![3u8];
    payload.extend_from_slice(&zlib);
    assert_eq!(
        &*compression_decoder(&mut Cursor::new(payload.as_slice()), 1).unwrap(),
        &[0x20, 1, 2]
    );
}
#[test]
fn fragmented_consecutive_and_empty_frames() {
    let mut buffer = Cursor::new(vec![3, 0x10]);
    assert!(
        read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, None)
            .unwrap()
            .is_none()
    );
    assert_eq!(buffer.position(), 0);
    buffer.get_mut().extend_from_slice(&[0x11, 0x12, 1, 0x22]);
    assert_eq!(
        &*read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, None)
            .unwrap()
            .unwrap(),
        &[0x10, 0x11, 0x12]
    );
    assert_eq!(
        &*read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, None)
            .unwrap()
            .unwrap(),
        &[0x22]
    );
    assert!(buffer.get_ref().is_empty());
    let mut buffer = Cursor::new(vec![1, 0, 2, 0, 0x22]);
    assert_eq!(
        &*read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, Some(256))
            .unwrap()
            .unwrap(),
        &[0x22]
    );
}

#[test]
fn empty_decoded_frames_follow_vanilla_but_invalid_wire_and_ids_fail() {
    use azalea_protocol::{
        packets::login::ServerboundLoginPacket,
        read::{FrameSplitterError, ReadPacketError, deserialize_packet},
    };
    let mut buffer = Cursor::new(vec![1, 0, 1, 0]);
    assert!(
        read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, Some(256))
            .unwrap()
            .is_none()
    );
    assert!(buffer.get_ref().is_empty());
    // Empty frame followed by a partial next frame: retain the exact boundary.
    let mut buffer = Cursor::new(vec![1, 0, 3, 0, 0x22]);
    assert!(
        read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, Some(256))
            .unwrap()
            .is_none()
    );
    buffer.get_mut().push(0x33);
    assert_eq!(
        &*read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, Some(256))
            .unwrap()
            .unwrap(),
        &[0x22, 0x33]
    );
    let mut zero = Cursor::new(vec![0]);
    assert!(matches!(
        *read_raw_packet_from_buffer::<tokio::io::Empty>(&mut zero, None).unwrap_err(),
        ReadPacketError::FrameSplitter {
            source: FrameSplitterError::EmptyLength
        }
    ));
    assert!(matches!(
        *deserialize_packet::<ServerboundLoginPacket>(&mut Cursor::new(&[0x80][..])).unwrap_err(),
        ReadPacketError::ReadPacketId { .. }
    ));
    assert!(matches!(
        *deserialize_packet::<ServerboundLoginPacket>(&mut Cursor::new(&[0x80; 6][..]))
            .unwrap_err(),
        ReadPacketError::ReadPacketId { .. }
    ));
}
