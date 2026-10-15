//! Chat signing, used in Minecraft to allow for messages to be reported.

use std::time::{SystemTime, UNIX_EPOCH};

use azalea_buf::{AzBuf, AzaleaWrite};
use rsa::{
    RsaPrivateKey,
    signature::{RandomizedSigner, SignatureEncoding},
};
use sha2::Sha256;
use uuid::Uuid;

#[derive(AzBuf, Clone, Debug)]
pub struct SaltSignaturePair {
    pub salt: u64,
    pub signature: Vec<u8>,
}

#[derive(AzBuf, Clone, Debug, PartialEq)]
pub struct MessageSignature {
    pub bytes: [u8; 256],
}

#[derive(AzBuf, Clone, Debug, PartialEq)]
pub struct SignedMessageHeader {
    pub previous_signature: Option<MessageSignature>,
    pub sender: Uuid,
}

/// Generates a random u64 to use as a salt
pub fn make_salt() -> u64 {
    rand::random()
}

pub struct SignChatMessageOptions {
    pub account_uuid: Uuid,
    pub chat_session_uuid: Uuid,

    pub message_index: u32,

    /// Can be acquired with [`make_salt`].
    pub salt: u64,

    /// The current time that we're sending the message at.
    pub timestamp: SystemTime,

    /// The message that we're sending in chat.
    pub message: String,

    pub private_key: RsaPrivateKey,
}

pub fn sign_chat_message(opts: &SignChatMessageOptions) -> MessageSignature {
    sign_chat_message_with_last_seen(opts, &[])
}

pub fn chat_signature_payload(account_uuid:Uuid,chat_session_uuid:Uuid,message_index:u32,salt:u64,timestamp:SystemTime,message:&str,last_seen:&[MessageSignature])->Vec<u8> {
    let mut data_to_sign = Vec::new();
    // always 1 for some reason
    1i32.azalea_write(&mut data_to_sign).unwrap();
    // player uuid
    account_uuid.azalea_write(&mut data_to_sign).unwrap();
    // chat session uuid
    chat_session_uuid
        .azalea_write(&mut data_to_sign)
        .unwrap();
    // message index
    message_index.azalea_write(&mut data_to_sign).unwrap();
    // salt
    salt.azalea_write(&mut data_to_sign).unwrap();

    // timestamp as seconds
    let seconds_since_epoch = timestamp
        .duration_since(UNIX_EPOCH)
        .expect("timestamp must be after epoch")
        .as_secs();
    seconds_since_epoch.azalea_write(&mut data_to_sign).unwrap();

    // message length as u32
    let message_len: u32 = message.len().try_into().unwrap();
    message_len.azalea_write(&mut data_to_sign).unwrap();
    // message bytes
    data_to_sign.extend_from_slice(message.as_bytes());

    // last seen messages length
    (last_seen.len() as i32).azalea_write(&mut data_to_sign).unwrap();
    for signature in last_seen { data_to_sign.extend_from_slice(&signature.bytes); }

    data_to_sign
}

pub fn sign_chat_message_with_last_seen(opts: &SignChatMessageOptions, last_seen: &[MessageSignature]) -> MessageSignature {
    let data_to_sign=chat_signature_payload(opts.account_uuid,opts.chat_session_uuid,opts.message_index,opts.salt,opts.timestamp,&opts.message,last_seen);
    let signing_key = rsa::pkcs1v15::SigningKey::<Sha256>::new(opts.private_key.clone());
    let mut rng = rand::rng();
    let signature = signing_key
        .sign_with_rng(&mut rng, &data_to_sign)
        .to_bytes();

    MessageSignature {
        bytes: signature
            .as_ref()
            .try_into()
            .expect("signature must be 256 bytes"),
    }
}

#[cfg(test)]
mod tcpe_secure_chat_tests {
    use super::*;
    use rsa::{RsaPublicKey,pkcs1v15::{VerifyingKey,Signature},signature::Verifier};
    #[test]
    fn signed_payload_verifies_exact_official_last_seen_bytes() {
        let private_key=RsaPrivateKey::new(&mut rand::rng(),2048).unwrap();
        let opts=SignChatMessageOptions{account_uuid:Uuid::from_u128(1),chat_session_uuid:Uuid::from_u128(2),message_index:3,salt:4,timestamp:UNIX_EPOCH+std::time::Duration::from_secs(5),message:"six".into(),private_key};
        let last_seen=[MessageSignature{bytes:[0x80;256]}];
        let signature=sign_chat_message_with_last_seen(&opts,&last_seen);
        // Byte contract independently verified against official 1.21.11
        // SignedMessageChain and SignedMessageBody signature updates.
        let mut expected=vec![0,0,0,1];
        expected.extend_from_slice(&1u128.to_be_bytes());expected.extend_from_slice(&2u128.to_be_bytes());
        expected.extend_from_slice(&3u32.to_be_bytes());expected.extend_from_slice(&4u64.to_be_bytes());expected.extend_from_slice(&5u64.to_be_bytes());
        expected.extend_from_slice(&[0,0,0,3,b's',b'i',b'x',0,0,0,1]);expected.extend_from_slice(&[0x80;256]);
        let key=VerifyingKey::<Sha256>::new(RsaPublicKey::from(&opts.private_key));
        let signature=Signature::try_from(signature.bytes.as_slice()).unwrap();
        key.verify(&expected,&signature).unwrap();
        *expected.last_mut().unwrap()^=1;
        assert!(key.verify(&expected,&signature).is_err());
    }
}

/// Verify exactly the official signed-message byte contract with an announced
/// X.509 RSA chat key. Certificate issuer validation is a separate mandatory step.
pub fn verify_chat_signature(public_der:&[u8],signature:&MessageSignature,account_uuid:Uuid,session_uuid:Uuid,index:u32,salt:u64,timestamp_millis:u64,message:&str,last_seen:&[MessageSignature])->bool {
 use rsa::{RsaPublicKey,pkcs8::DecodePublicKey,pkcs1v15::{VerifyingKey,Signature},signature::Verifier,traits::PublicKeyParts};
 if public_der.len()>4096||last_seen.len()>20{return false;}
 let Ok(key)=RsaPublicKey::from_public_key_der(public_der) else{return false};
 if key.n().bits()!=2048{return false;}
 let Some(timestamp)=UNIX_EPOCH.checked_add(std::time::Duration::from_millis(timestamp_millis)) else{return false};
 let Ok(signature)=Signature::try_from(signature.bytes.as_slice()) else{return false};
 let payload=chat_signature_payload(account_uuid,session_uuid,index,salt,timestamp,message,last_seen);
 VerifyingKey::<Sha256>::new(key).verify(&payload,&signature).is_ok()
}
pub fn verify_player_certificate(service_key_der:&[u8],sender:Uuid,expires_at:u64,player_key_der:&[u8],signature:&[u8])->bool {
 use rsa::{RsaPublicKey,pkcs8::DecodePublicKey,pkcs1v15::{VerifyingKey,Signature},signature::Verifier};
 if service_key_der.len()>8192||player_key_der.len()>4096||signature.len()>1024{return false;}
 let Ok(key)=RsaPublicKey::from_public_key_der(service_key_der) else{return false};
 let Ok(signature)=Signature::try_from(signature) else{return false};
 let mut payload=sender.as_bytes().to_vec();payload.extend_from_slice(&expires_at.to_be_bytes());payload.extend_from_slice(player_key_der);
 VerifyingKey::<sha1::Sha1>::new(key).verify(&payload,&signature).is_ok()
}
