#[path="../vendor/azalea-client/src/plugins/chat/secure_state.rs"] mod secure_state;
#[path="../vendor/azalea-client/src/plugins/chat/signed_commands.rs"] mod signed_commands;
use azalea_client::test_utils::prelude::*;
use azalea_protocol::packets::{ConnectionProtocol,game::{ClientboundCommands,c_commands::{BrigadierNodeStub,NodeType,BrigadierParser}}};
#[test] fn actual_packet_handler_retains_server_message_argument_tree(){
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
 let n=|node_type,children,is_executable|BrigadierNodeStub{node_type,children,is_executable,redirect_node:None,is_restricted:false};
 sim.receive_packet(ClientboundCommands{root_index:0,entries:vec![n(NodeType::Root,vec![1],false),n(NodeType::Literal{name:"say".into()},vec![2],false),n(NodeType::Argument{name:"message".into(),parser:BrigadierParser::Message,suggestions_type:None},vec![],true)]});sim.update();
 sim.with_component::<azalea_client::chat::secure_state::SecureChatState>(|state|assert_eq!(state.signed_arguments("say exact raw words"),Ok(vec![("message".into(),"exact raw words".into())])));
}
#[test] fn actual_signed_receipts_send_65_message_ack_and_keep_twenty_signatures(){
 use azalea_protocol::packets::game::{ClientboundPlayerChat,ServerboundGamePacket,c_player_chat::{PackedSignedMessageBody,PackedLastSeenMessages,ChatTypeBound,FilterMask}};
 use azalea_registry::{Holder,Registry,data::ChatKind};
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();let sent=SentPackets::new(&mut sim);sent.clear();
 for n in 0..65 {
  sim.receive_packet(ClientboundPlayerChat{global_index:n,sender:uuid::Uuid::nil(),index:n,signature:Some(azalea_crypto::signing::MessageSignature{bytes:[n as u8;256]}),body:PackedSignedMessageBody{content:format!("message {n}"),timestamp:0,salt:0,last_seen:PackedLastSeenMessages{entries:vec![]}},unsigned_content:None,filter_mask:FilterMask::PassThrough,chat_type:ChatTypeBound{chat_type:Holder::Reference(ChatKind::from_u32(0).unwrap()),name:azalea_chat::FormattedText::from("Sender"),target_name:None}});sim.update();
  let signature=azalea_crypto::signing::MessageSignature{bytes:[n as u8;256]};let mut ack=None;
  sim.with_component_mut::<azalea_client::chat::secure_state::SecureChatState>(|state|{let pending=state.take_received(&signature).unwrap();assert!(pending.last_seen.is_empty());ack=state.complete_received(signature,true);});
  if let Some(messages)=ack{sim.trigger(azalea_client::packet::game::SendGamePacketEvent::new(sim.entity,azalea_protocol::packets::game::ServerboundChatAck{messages}));sim.update();}
 }
 let packets:Vec<_>=std::iter::from_fn(||sent.next()).collect();
 assert_eq!(packets.iter().filter(|p|matches!(p,ServerboundGamePacket::ChatAck(p) if p.messages==65)).count(),1);
 sim.with_component_mut::<azalea_client::chat::secure_state::SecureChatState>(|state|{let(update,list)=state.update();assert_eq!(update.offset,0);assert_eq!(list.len(),20);assert_eq!(list.first().unwrap().bytes[0],45);assert_eq!(list.last().unwrap().bytes[0],64);});
}
#[test] fn actual_chat_composer_uses_server_signed_command_schema_and_ack(){
 use azalea_client::chat::SendChatEvent;
 use azalea_protocol::packets::game::ServerboundGamePacket;
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
 let n=|node_type,children,is_executable|BrigadierNodeStub{node_type,children,is_executable,redirect_node:None,is_restricted:false};
 sim.receive_packet(ClientboundCommands{root_index:0,entries:vec![n(NodeType::Root,vec![1],false),n(NodeType::Literal{name:"say".into()},vec![2],false),n(NodeType::Argument{name:"message".into(),parser:BrigadierParser::Message,suggestions_type:None},vec![],true)]});sim.update();
 sim.with_component_mut::<azalea_client::chat::secure_state::SecureChatState>(|s|s.add(azalea_crypto::signing::MessageSignature{bytes:[10;256]},true));
 let sent=SentPackets::new(&mut sim);sent.clear();sim.write_message(SendChatEvent{entity:sim.entity,content:"/say exact words".into()});sim.update();
 let packets:Vec<_>=std::iter::from_fn(||sent.next()).collect();
 assert!(packets.iter().any(|p|matches!(p,ServerboundGamePacket::ChatCommandSigned(p) if p.command=="say exact words" && p.last_seen_messages.offset==1 && p.last_seen_messages.acknowledged.index(19))));
 assert!(!packets.iter().any(|p|matches!(p,ServerboundGamePacket::ChatCommand(_))));
}
#[test]fn cached_account_token_on_offline_server_does_not_block_unsigned_chat(){
 use azalea_client::chat::SendChatEvent;use azalea_protocol::packets::game::ServerboundGamePacket;
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
 let mut account=azalea_client::Account::offline("Test");account.access_token=Some(std::sync::Arc::new(parking_lot::Mutex::new("test-only-token".into())));sim.app.world_mut().entity_mut(sim.entity).insert(account);
 let sent=SentPackets::new(&mut sim);sent.clear();sim.write_message(SendChatEvent{entity:sim.entity,content:"offline server chat".into()});sim.update();
 assert!(std::iter::from_fn(||sent.next()).any(|p|matches!(p,ServerboundGamePacket::Chat(p) if p.message=="offline server chat"&&p.signature.is_none())));
}
#[test]fn authenticated_server_waits_for_certificate_without_network_request(){
 use azalea_client::chat::SendChatEvent;use azalea_protocol::packets::game::ServerboundGamePacket;
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
 sim.app.world_mut().entity_mut(sim.entity).insert((azalea_client::Account::offline("Test"),azalea_client::login::IsAuthenticated,azalea_client::chat_signing::OnlyRefreshCertsAfter{refresh_at:std::time::Instant::now()+std::time::Duration::from_secs(3600)}));
 let sent=SentPackets::new(&mut sim);sent.clear();sim.write_message(SendChatEvent{entity:sim.entity,content:"wait for certificate".into()});sim.update();
 assert!(!std::iter::from_fn(||sent.next()).any(|p|matches!(p,ServerboundGamePacket::Chat(_))));
}
