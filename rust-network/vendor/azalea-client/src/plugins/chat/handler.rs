use std::time::{SystemTime, UNIX_EPOCH};

use azalea_protocol::packets::{
    Packet,
    game::{ServerboundChat, ServerboundChatCommand, ServerboundChatCommandSigned, s_chat_command_signed::ArgumentSignature},
};
use bevy_ecs::prelude::*;

use super::ChatKind;
use crate::packet::game::SendGamePacketEvent;
#[cfg(feature = "online-mode")]
use crate::{Account, chat_signing::ChatSigningSession};

/// Send a chat packet to the server of a specific kind (chat message or
/// command). Usually you just want [`SendChatEvent`] instead.
///
/// Usually setting the kind to `Message` will make it send a chat message even
/// if it starts with a slash, but some server implementations will always do a
/// command if it starts with a slash.
///
/// If you're wondering why this isn't two separate events, it's so ordering is
/// preserved if multiple chat messages and commands are sent at the same time.
///
/// [`SendChatEvent`]: super::SendChatEvent
#[derive(Message)]
pub struct SendChatKindEvent {
    pub entity: Entity,
    pub content: String,
    pub kind: ChatKind,
}

pub fn handle_send_chat_kind_event(
    mut events: MessageReader<SendChatKindEvent>,
    mut commands: Commands,
    #[cfg(feature = "online-mode")] mut query: Query<(&Account, Option<&mut ChatSigningSession>,Option<&crate::plugins::login::IsAuthenticated>)>,
    mut states: Query<&mut super::secure_state::SecureChatState>,
    mut received: MessageWriter<super::ChatReceivedEvent>,
) {
    for event in events.read() {
        let content = event
            .content
            .chars()
            .filter(|c| !matches!(c, '\x00'..='\x1F' | '\x7F' | '§'))
            .take(256)
            .collect::<String>();

        #[cfg(feature = "online-mode")]
        if query.get_mut(event.entity).is_ok_and(|(_, session, authenticated)| authenticated.is_some() && session.is_none()) {
            received.write(super::ChatReceivedEvent { entity:event.entity, packet:super::ChatPacket::new("Signed chat is not ready. Wait for the account certificate, then send again.") });
            continue;
        }
        let timestamp = SystemTime::now();
        let Ok(mut state) = states.get_mut(event.entity) else { continue };
        let signed_arguments = if matches!(event.kind, ChatKind::Command) {
            state.signed_arguments(&content).unwrap_or_default()
        } else { Vec::new() };
        let needs_update = matches!(event.kind, ChatKind::Message) || !signed_arguments.is_empty();
        let (last_seen_messages, last_seen) = if needs_update { state.update() } else { (Default::default(), Vec::new()) };

        let packet = match event.kind {
            ChatKind::Message => {
                let salt = azalea_crypto::signing::make_salt();

                #[cfg(feature = "online-mode")]
                let signature = if let Ok((account, Some(mut chat_session), _)) = query.get_mut(event.entity)
                {
                    Some(create_signature(
                        account,
                        &mut chat_session,
                        salt,
                        timestamp,
                        &content,
                        &last_seen,
                    ))
                } else {
                    None
                };
                #[cfg(not(feature = "online-mode"))]
                let signature = None;

                ServerboundChat {
                    message: content,
                    timestamp: timestamp
                        .duration_since(UNIX_EPOCH)
                        .expect("Time shouldn't be before epoch")
                        .as_millis()
                        .try_into()
                        .expect("Instant should fit into a u64"),
                    salt,
                    signature,
                    last_seen_messages,
                }
            }
            .into_variant(),
            ChatKind::Command => {
                if signed_arguments.is_empty() {
                    ServerboundChatCommand { command: content }.into_variant()
                } else {
                    let salt = azalea_crypto::signing::make_salt();
                    let mut argument_signatures = Vec::new();
                    #[cfg(feature = "online-mode")]
                    if let Ok((account, Some(mut chat_session), _)) = query.get_mut(event.entity) {
                        for (name, argument) in signed_arguments {
                            argument_signatures.push(ArgumentSignature { name, signature: create_signature(account, &mut chat_session, salt, timestamp, &argument, &last_seen) });
                        }
                    }
                    ServerboundChatCommandSigned {
                        command: content, timestamp: timestamp.duration_since(UNIX_EPOCH).expect("after epoch").as_millis() as u64,
                        salt, argument_signatures, last_seen_messages,
                    }.into_variant()
                }
            }
        };

        commands.trigger(SendGamePacketEvent::new(event.entity, packet));
    }
}

#[cfg(feature = "online-mode")]
pub fn create_signature(
    account: &Account,
    chat_session: &mut ChatSigningSession,
    salt: u64,
    timestamp: SystemTime,
    message: &str,
    last_seen: &[azalea_crypto::signing::MessageSignature],
) -> azalea_crypto::signing::MessageSignature {
    use azalea_crypto::signing::SignChatMessageOptions;

    let certs = account.certs.lock();
    let certs = certs.as_ref().expect("certs shouldn't be set back to None");

    let signature = azalea_crypto::signing::sign_chat_message_with_last_seen(&SignChatMessageOptions {
        account_uuid: account.uuid.expect("account must have a uuid"),
        chat_session_uuid: chat_session.session_id,
        message_index: chat_session.messages_sent,
        salt,
        timestamp,
        message: message.to_owned(),
        private_key: certs.private_key.clone(),
    }, last_seen);

    chat_session.messages_sent += 1;

    signature
}
