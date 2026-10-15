//! Read packets from a stream.

use std::{
    backtrace::Backtrace,
    env,
    fmt::Debug,
    io,
    io::{Cursor, Read},
    sync::LazyLock,
};

use azalea_buf::{AzaleaReadVar, BufReadError};
use azalea_crypto::Aes128CfbDec;
use flate2::read::ZlibDecoder;
use futures::StreamExt;
use futures_lite::future;
use thiserror::Error;
use tokio::io::AsyncRead;
use tokio_util::{
    bytes::Buf,
    codec::{BytesCodec, FramedRead},
};
use tracing::trace;

use crate::packets::ProtocolPacket;

#[derive(Debug, Error)]
pub enum ReadPacketError {
    #[error("Error reading packet {packet_name} (id {packet_id}): {source}")]
    Parse {
        packet_id: u32,
        packet_name: String,
        backtrace: Box<Backtrace>,
        source: BufReadError,
    },
    #[error("Unknown packet id {id} in state {state_name}")]
    UnknownPacketId { state_name: String, id: u32 },
    #[error("Couldn't read packet id")]
    ReadPacketId { source: BufReadError },
    #[error(transparent)]
    Decompress {
        #[from]
        #[backtrace]
        source: DecompressionError,
    },
    #[error(transparent)]
    FrameSplitter {
        #[from]
        #[backtrace]
        source: FrameSplitterError,
    },
    #[error("Leftover data after reading packet {packet_name}: {data:?}")]
    LeftoverData { data: Vec<u8>, packet_name: String },
    #[error(transparent)]
    IoError {
        #[from]
        #[backtrace]
        source: io::Error,
    },
    #[error("Connection closed")]
    ConnectionClosed,
}

#[derive(Debug, Error)]
pub enum FrameSplitterError {
    #[error("Frame length cannot be zero")]
    EmptyLength,
    #[error("Couldn't read VarInt length for packet. The previous packet may have been corrupted")]
    LengthRead {
        #[from]
        source: BufReadError,
    },
    #[error("Io error")]
    Io {
        #[from]
        #[backtrace]
        source: io::Error,
    },
    #[error("Packet is longer than {max} bytes (is {size})")]
    BadLength { max: usize, size: usize },
    #[error("Connection reset by peer")]
    ConnectionReset,
    #[error("Connection closed")]
    ConnectionClosed,
}

/// Read a length, then read that amount of bytes from the `Cursor<Vec<u8>>`. If
/// there's not enough data, `None` is returned.
fn parse_frame(buffer: &mut Cursor<Vec<u8>>) -> Result<Box<[u8]>, FrameSplitterError> {
    // copy the buffer first and read from the copy, then once we make sure
    // the packet is all good we read it fully
    let mut buffer_copy = Cursor::new(&buffer.get_ref()[buffer.position() as usize..]);
    // Packet Length
    let length = match u32::azalea_read_var(&mut buffer_copy) {
        Ok(length) => length as usize,
        Err(err) => match err {
            BufReadError::Io { source } => return Err(FrameSplitterError::Io { source }),
            _ => return Err(err.into()),
        },
    };

    if length == 0 {
        return Err(FrameSplitterError::EmptyLength);
    }

    if length > buffer_copy.remaining() {
        return Err(FrameSplitterError::BadLength {
            max: buffer_copy.remaining(),
            size: length,
        });
    }

    // we read from the copy and we know it's legit, so we can take those bytes
    // from the real buffer now

    // the length of the varint that says the length of the whole packet
    let varint_length = buffer.remaining() - buffer_copy.remaining();

    buffer.advance(varint_length);
    let data =
        buffer.get_ref()[buffer.position() as usize..buffer.position() as usize + length].to_vec();
    buffer.advance(length);

    if buffer.position() == buffer.get_ref().len() as u64 {
        // reset the inner vec once we've reached the end of the buffer so we don't keep
        // leaking memory
        buffer.get_mut().clear();

        // we just cap the capacity to 64KB instead of resetting it to save some
        // allocations.
        // and the reason we bother capping it at all is to avoid wasting memory if we
        // get a big packet once and then never again.
        buffer.get_mut().shrink_to(1024 * 64);

        buffer.set_position(0);
    }

    Ok(data.into_boxed_slice())
}

fn frame_splitter(buffer: &mut Cursor<Vec<u8>>) -> Result<Option<Box<[u8]>>, FrameSplitterError> {
    // https://tokio.rs/tokio/tutorial/framing
    let read_frame = parse_frame(buffer);
    match read_frame {
        Ok(frame) => return Ok(Some(frame)),
        Err(err) => match err {
            FrameSplitterError::BadLength { .. } | FrameSplitterError::Io { .. } => {
                // we probably just haven't read enough yet
            }
            _ => return Err(err),
        },
    }

    Ok(None)
}

pub fn deserialize_packet<P: ProtocolPacket + Debug>(
    stream: &mut Cursor<&[u8]>,
) -> Result<P, Box<ReadPacketError>> {
    // Packet ID. The underlying azalea-buf VarInt reader stops after five
    // bytes even if the continuation bit remains set. Vanilla rejects that
    // malformed ID; do not let empty-frame compatibility conceal it.
    let remaining = &stream.get_ref()[stream.position() as usize..];
    if !remaining.iter().take(5).any(|byte| byte & 0x80 == 0) {
        return Err(Box::new(ReadPacketError::ReadPacketId {
            source: BufReadError::InvalidVarInt,
        }));
    }
    let packet_id =
        u32::azalea_read_var(stream).map_err(|e| ReadPacketError::ReadPacketId { source: e })?;
    P::read(packet_id, stream)
}

// this is always true in multiplayer, false in singleplayer
static VALIDATE_DECOMPRESSED: bool = true;

pub static MAXIMUM_UNCOMPRESSED_LENGTH: u32 = 8_388_608;

#[derive(Debug, Error)]
pub enum DecompressionError {
    #[error("Couldn't read VarInt length for data")]
    LengthReadError {
        #[from]
        source: BufReadError,
    },
    #[error("Io error")]
    Io {
        #[from]
        #[backtrace]
        source: io::Error,
    },
    #[error("Badly compressed packet - size of {size} is below server threshold of {threshold}")]
    BelowCompressionThreshold { size: u32, threshold: u32 },
    #[error(
        "Badly compressed packet - size of {size} is larger than protocol maximum of {maximum}"
    )]
    AboveCompressionThreshold { size: u32, maximum: u32 },
    #[error("Decompressed packet length {actual} differs from declared length {declared}")]
    MismatchedLength { declared: u32, actual: usize },
}

/// Get the decompressed bytes from a packet.
///
/// The stream must have already been decrypted before passing it to this
/// function.
pub fn compression_decoder(
    stream: &mut Cursor<&[u8]>,
    compression_threshold: u32,
) -> Result<Box<[u8]>, DecompressionError> {
    // Data Length
    let n = u32::azalea_read_var(stream)?;
    if n == 0 {
        // no data size, no compression
        let buf = stream.get_ref()[stream.position() as usize..]
            .to_vec()
            .into_boxed_slice();
        stream.set_position(stream.get_ref().len() as u64);
        return Ok(buf);
    }

    if VALIDATE_DECOMPRESSED {
        if n < compression_threshold {
            return Err(DecompressionError::BelowCompressionThreshold {
                size: n,
                threshold: compression_threshold,
            });
        }
        if n > MAXIMUM_UNCOMPRESSED_LENGTH {
            return Err(DecompressionError::AboveCompressionThreshold {
                size: n,
                maximum: MAXIMUM_UNCOMPRESSED_LENGTH,
            });
        }
    }

    // VALIDATE_DECOMPRESSED should always be true, so the max they can make us
    // allocate here is 2mb
    let mut decoded_buf = Vec::with_capacity(n as usize);

    let decoder = ZlibDecoder::new(stream);
    // Read one byte beyond the declared size to detect oversized output without
    // letting a compressed frame allocate an unbounded decompressed buffer.
    decoder
        .take(u64::from(n) + 1)
        .read_to_end(&mut decoded_buf)?;
    if decoded_buf.len() != n as usize {
        return Err(DecompressionError::MismatchedLength {
            declared: n,
            actual: decoded_buf.len(),
        });
    }

    Ok(decoded_buf.into_boxed_slice())
}

/// Read a single packet from a stream.
///
/// The buffer is required because servers may send multiple packets in the
/// same frame, so we need to store the packet data that's left to read.
///
/// The current protocol state must be passed as a generic.
///
/// For the non-waiting version, see [`try_read_packet`].
pub async fn read_packet<P: ProtocolPacket + Debug, R>(
    stream: &mut R,
    buffer: &mut Cursor<Vec<u8>>,
    compression_threshold: Option<u32>,
    cipher: &mut Option<Aes128CfbDec>,
) -> Result<P, Box<ReadPacketError>>
where
    R: AsyncRead + Unpin + Send + Sync,
{
    let raw_packet = read_raw_packet(stream, buffer, compression_threshold, cipher).await?;
    let packet = deserialize_packet(&mut Cursor::new(&raw_packet))?;
    Ok(packet)
}

/// Try to read a single packet from a stream. Returns None if we haven't
/// received a full packet yet.
pub fn try_read_packet<P: ProtocolPacket + Debug, R>(
    stream: &mut R,
    buffer: &mut Cursor<Vec<u8>>,
    compression_threshold: Option<u32>,
    cipher: &mut Option<Aes128CfbDec>,
) -> Result<Option<P>, Box<ReadPacketError>>
where
    R: AsyncRead + Unpin + Send + Sync,
{
    let Some(raw_packet) = try_read_raw_packet(stream, buffer, compression_threshold, cipher)?
    else {
        return Ok(None);
    };
    let packet = deserialize_packet(&mut Cursor::new(&raw_packet))?;
    Ok(Some(packet))
}

pub async fn read_raw_packet<R>(
    stream: &mut R,
    buffer: &mut Cursor<Vec<u8>>,
    compression_threshold: Option<u32>,
    // this has to be a &mut Option<T> instead of an Option<&mut T> because
    // otherwise the borrow checker complains about the cipher being moved
    cipher: &mut Option<Aes128CfbDec>,
) -> Result<Box<[u8]>, Box<ReadPacketError>>
where
    R: AsyncRead + Unpin + Send + Sync,
{
    loop {
        if let Some(buf) = read_raw_packet_from_buffer::<R>(buffer, compression_threshold)? {
            // we got a full packet!!
            return Ok(buf);
        };

        let bytes = read_and_decrypt_frame(stream, cipher).await?;
        buffer.get_mut().extend_from_slice(&bytes);
    }
}
/// Read a packet from the stream, then if necessary decrypt it, decompress
/// it, and split it.
pub fn try_read_raw_packet<R>(
    stream: &mut R,
    buffer: &mut Cursor<Vec<u8>>,
    compression_threshold: Option<u32>,
    cipher: &mut Option<Aes128CfbDec>,
) -> Result<Option<Box<[u8]>>, Box<ReadPacketError>>
where
    R: AsyncRead + Unpin + Send + Sync,
{
    loop {
        if let Some(buf) = read_raw_packet_from_buffer::<R>(buffer, compression_threshold)? {
            // we got a full packet!!
            return Ok(Some(buf));
        };
        let Some(bytes) = try_read_and_decrypt_frame(stream, cipher)? else {
            // no data received
            return Ok(None);
        };
        // we got some data, so add it to the buffer and try again
        buffer.get_mut().extend_from_slice(&bytes);
    }
}

async fn read_and_decrypt_frame<R>(
    stream: &mut R,
    cipher: &mut Option<Aes128CfbDec>,
) -> Result<Box<[u8]>, Box<ReadPacketError>>
where
    R: AsyncRead + Unpin + Send + Sync,
{
    let mut framed = FramedRead::new(stream, BytesCodec::new());

    let Some(message) = framed.next().await else {
        return Err(Box::new(ReadPacketError::ConnectionClosed));
    };
    let bytes = message.map_err(ReadPacketError::from)?;

    let mut bytes = bytes.to_vec().into_boxed_slice();

    // decrypt if necessary
    if let Some(cipher) = cipher {
        azalea_crypto::decrypt_packet(cipher, &mut bytes);
    }

    Ok(bytes)
}
fn try_read_and_decrypt_frame<R>(
    stream: &mut R,
    cipher: &mut Option<Aes128CfbDec>,
) -> Result<Option<Box<[u8]>>, Box<ReadPacketError>>
where
    R: AsyncRead + Unpin + Send + Sync,
{
    let mut framed = FramedRead::new(stream, BytesCodec::new());

    let Some(message) = future::block_on(future::poll_once(framed.next())) else {
        // nothing yet
        return Ok(None);
    };
    let Some(message) = message else {
        return Err(Box::new(ReadPacketError::ConnectionClosed));
    };
    let bytes = message.map_err(ReadPacketError::from)?;
    let mut bytes = bytes.to_vec().into_boxed_slice();

    // decrypt if necessary
    if let Some(cipher) = cipher {
        azalea_crypto::decrypt_packet(cipher, &mut bytes);
    }

    Ok(Some(bytes))
}

pub fn read_raw_packet_from_buffer<R>(
    buffer: &mut Cursor<Vec<u8>>,
    compression_threshold: Option<u32>,
) -> Result<Option<Box<[u8]>>, Box<ReadPacketError>>
where
    R: AsyncRead + Unpin + Send + Sync,
{
    loop {
        let Some(mut buf) = frame_splitter(buffer).map_err(ReadPacketError::from)? else {
            // no full packet yet :(
            return Ok(None);
        };

        let frame_length = buf.len();
        if let Some(compression_threshold) = compression_threshold {
            buf = compression_decoder(&mut Cursor::new(&buf[..]), compression_threshold)
                .map_err(ReadPacketError::from)?;
        }

        if buf.is_empty() {
            // Vanilla 1.21.11 CompressionDecoder emits an empty buffer for
            // DataLength=0 with no payload. Netty ByteToMessageDecoder.callDecode
            // checks isReadable, so PacketDecoder never attempts a packet ID.
            // Consume this complete frame and continue, preserving subsequent
            // buffered packets. An outer FrameLength=0 remains a framing error.
            trace!(
                frame_length,
                ?compression_threshold,
                "Skipped empty decoded frame as vanilla does"
            );
            continue;
        }

        if tracing::enabled!(tracing::Level::TRACE) {
            static DO_NOT_CUT_OFF_PACKET_LOGS: LazyLock<bool> = LazyLock::new(|| {
                env::var("AZALEA_DO_NOT_CUT_OFF_PACKET_LOGS")
                    .map(|s| s == "1" || s == "true")
                    .unwrap_or(false)
            });

            let buf_string: String = {
                if !*DO_NOT_CUT_OFF_PACKET_LOGS && buf.len() > 500 {
                    let cut_off_buf = &buf[..500];
                    format!("{cut_off_buf:?}...")
                } else {
                    format!("{buf:?}")
                }
            };
            trace!("Reading packet with bytes: {buf_string}");
        };

        return Ok(Some(buf));
    }
}

#[cfg(test)]
mod framing_regressions {
    use super::*;
    use flate2::{Compression, write::ZlibEncoder};
    use std::io::Write;

    #[test]
    fn compressed_length_must_match() {
        let mut encoded = ZlibEncoder::new(Vec::new(), Compression::default());
        encoded.write_all(&[0x20, 0x01, 0x02]).unwrap();
        let encoded = encoded.finish().unwrap();
        for declared in [2u8, 4u8] {
            let mut payload = vec![declared];
            payload.extend_from_slice(&encoded);
            assert!(matches!(
                compression_decoder(&mut Cursor::new(payload.as_slice()), 1),
                Err(DecompressionError::MismatchedLength { .. })
            ));
        }
        let mut payload = vec![3u8];
        payload.extend_from_slice(&encoded);
        assert_eq!(
            &*compression_decoder(&mut Cursor::new(payload.as_slice()), 1).unwrap(),
            &[0x20, 0x01, 0x02]
        );
    }

    #[test]
    fn fragmented_and_consecutive_frames_preserve_boundaries() {
        let mut buffer = Cursor::new(vec![3, 0x10]);
        assert!(frame_splitter(&mut buffer).unwrap().is_none());
        assert_eq!(buffer.position(), 0);
        buffer.get_mut().extend_from_slice(&[0x11, 0x12, 1, 0x22]);
        assert_eq!(
            &*frame_splitter(&mut buffer).unwrap().unwrap(),
            &[0x10, 0x11, 0x12]
        );
        assert_eq!(&*frame_splitter(&mut buffer).unwrap().unwrap(), &[0x22]);
        assert!(buffer.get_ref().is_empty());
    }

    #[test]
    fn compressed_empty_uncompressed_frame_is_skipped() {
        // PacketLength=1, DataLength=0, no packet ID: a complete but invalid
        // server frame, not an incomplete TCP read. It must remain diagnosable.
        let mut buffer = Cursor::new(vec![1, 0, 2, 0, 0x22]);
        assert_eq!(
            &*read_raw_packet_from_buffer::<tokio::io::Empty>(&mut buffer, Some(256))
                .unwrap()
                .unwrap(),
            &[0x22]
        );
        assert!(buffer.get_ref().is_empty());
        let mut only_empty = Cursor::new(vec![1, 0]);
        assert!(
            read_raw_packet_from_buffer::<tokio::io::Empty>(&mut only_empty, Some(256))
                .unwrap()
                .is_none()
        );
    }
}
