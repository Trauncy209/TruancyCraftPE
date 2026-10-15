//! Validate player messages before displaying/acknowledging them. Public service
//! roots are fetched asynchronously; game processing never waits on HTTP.
use std::{collections::{HashMap,VecDeque,HashSet},sync::Arc,time::{SystemTime,UNIX_EPOCH,Duration}};
use azalea_crypto::signing::{MessageSignature,verify_chat_signature,verify_player_certificate};
use azalea_protocol::packets::game::{c_player_chat::{ClientboundPlayerChat,FilterMask},c_player_info_update::ClientboundPlayerInfoUpdate,s_chat_session_update::RemoteChatSessionData};
use uuid::Uuid;
use base64::Engine;
const GRACE:u64=8*60*60*1000;
pub fn signature_id(s:&MessageSignature)->String{s.bytes.iter().map(|b|format!("{b:02x}")).collect()}
fn now()->u64{SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default().as_millis() as u64}
#[derive(Clone)]struct Pending{packet:Arc<ClientboundPlayerChat>,last_seen:Vec<MessageSignature>,remote:Option<RemoteChatSessionData>,deleted:bool}
struct Ledger{last_index:u32,last_signature:[u8;256]}
pub struct Verified{pub packet:Arc<ClientboundPlayerChat>,pub display:bool,pub valid:bool}
pub struct Incoming {
 sessions:HashMap<Uuid,RemoteChatSessionData>,ledger:HashMap<(Uuid,Uuid),Ledger>,queue:VecDeque<Pending>,
 roots:Option<Vec<Vec<u8>>>,fetch:Option<tokio::task::JoinHandle<Result<Vec<Vec<u8>>,&'static str>>>,failed_until:u64,roots_at:u64,
 deleted:HashSet<[u8;256]>,deleted_order:VecDeque<[u8;256]>,warned:bool,
}
impl Default for Incoming{fn default()->Self{Self{sessions:HashMap::new(),ledger:HashMap::new(),queue:VecDeque::new(),roots:None,fetch:None,failed_until:0,roots_at:0,deleted:HashSet::new(),deleted_order:VecDeque::new(),warned:false}}}
impl Incoming{
 pub fn reset(&mut self){self.sessions.clear();self.ledger.clear();self.queue.clear();self.deleted.clear();self.deleted_order.clear();self.warned=false;}
 pub fn observe(&mut self,p:&ClientboundPlayerInfoUpdate){if !p.actions.initialize_chat{return;}for entry in p.entries.iter().take(4096){let id=entry.profile.uuid;if let Some(remote)=&entry.chat_session{if self.sessions.len()<4096||self.sessions.contains_key(&id){self.sessions.insert(id,remote.clone());}}else{self.sessions.remove(&id);}}}
 pub fn requires_signed(&self,id:Uuid)->bool{self.sessions.contains_key(&id)}
 pub fn remove(&mut self,ids:&[Uuid]){for id in ids{self.sessions.remove(id);}}
 pub fn delete(&mut self,s:&MessageSignature){for pending in &mut self.queue{if pending.packet.signature.as_ref().is_some_and(|p|p.bytes==s.bytes){pending.deleted=true;}}if self.deleted.insert(s.bytes){self.deleted_order.push_back(s.bytes);}while self.deleted_order.len()>256{if let Some(old)=self.deleted_order.pop_front(){self.deleted.remove(&old);}}}
 pub fn push(&mut self,p:Arc<ClientboundPlayerChat>,last_seen:Vec<MessageSignature>,remote:Option<RemoteChatSessionData>)->bool{
  if self.queue.len()>=256||last_seen.len()>20{return false;}
  let deleted=p.signature.as_ref().is_some_and(|s|self.deleted.contains(&s.bytes));self.queue.push_back(Pending{remote,packet:p,last_seen,deleted});
  if (self.roots.is_none()||now().saturating_sub(self.roots_at)>=6*60*60*1000)&&self.fetch.is_none()&&now()>=self.failed_until{self.fetch=Some(tokio::spawn(fetch_roots()));}true
 }
 pub async fn poll(&mut self)->(Vec<Verified>,bool){
  if self.fetch.as_ref().is_some_and(|f|f.is_finished()){let result=self.fetch.take().unwrap().await;match result{Ok(Ok(roots))=>{self.roots=Some(roots);self.roots_at=now();},_=>self.failed_until=now()+120000}}
  if self.fetch.is_some(){return (Vec::new(),false);}
  let mut result=Vec::new();let mut rejected=false;
  for _ in 0..8{let Some(pending)=self.queue.pop_front() else{break};let p=&pending.packet;let replay=pending.remote.as_ref().and_then(|remote|self.ledger.get(&(p.sender,remote.session_id))).is_some_and(|last|p.signature.as_ref().is_some_and(|signature|p.index==last.last_index&&signature.bytes==last.last_signature));let valid=self.roots.as_ref().is_some_and(|roots|verify_candidate(p,&pending.last_seen,pending.remote.as_ref(),roots,&mut self.ledger,now()));
   let duplicate=p.signature.as_ref().is_some_and(|s|self.deleted.contains(&s.bytes));let display=valid&&!replay&&!pending.deleted&&!duplicate&&!matches!(p.filter_mask,FilterMask::FullyFiltered);
   rejected|=!valid;result.push(Verified{packet:pending.packet,display,valid});
  }
  let warning=rejected&&!self.warned;if warning{self.warned=true;}(result,warning)
 }
}
fn verify_candidate(p:&ClientboundPlayerChat,last_seen:&[MessageSignature],remote:Option<&RemoteChatSessionData>,roots:&[Vec<u8>],ledger:&mut HashMap<(Uuid,Uuid),Ledger>,now:u64)->bool{
 let (Some(signature),Some(remote))=(&p.signature,remote)else{return false};let key=&remote.profile_public_key;
 if key.expires_at>i64::MAX as u64||p.body.timestamp>i64::MAX as u64||now>key.expires_at.saturating_add(GRACE)||!roots.iter().any(|root|verify_player_certificate(root,p.sender,key.expires_at,&key.key,&key.key_signature)){return false;}
 let id=(p.sender,remote.session_id);
 if let Some(last)=ledger.get(&id){if p.index<last.last_index||(p.index==last.last_index&&signature.bytes!=last.last_signature){return false;}}
 if !verify_chat_signature(&key.key,signature,p.sender,remote.session_id,p.index,p.body.salt,p.body.timestamp,&p.body.content,last_seen){return false;}
 if ledger.len()>=4096&&!ledger.contains_key(&id){return false;}
 ledger.insert(id,Ledger{last_index:p.index,last_signature:signature.bytes});true
}
async fn fetch_roots()->Result<Vec<Vec<u8>>,&'static str>{
 let client=azalea_auth::http_client::builder().timeout(Duration::from_secs(15)).redirect(reqwest::redirect::Policy::none()).build().map_err(|_|"Chat verification unavailable")?;
 let response=client.get("https://api.minecraftservices.com/publickeys").send().await.map_err(|_|"Chat verification unavailable")?.error_for_status().map_err(|_|"Chat verification unavailable")?;
 if response.content_length().is_some_and(|len|len>256*1024){return Err("Invalid chat service keys");}
 let mut response=response;let mut bytes=Vec::new();while let Some(chunk)=response.chunk().await.map_err(|_|"Chat verification unavailable")?{if bytes.len()+chunk.len()>256*1024{return Err("Invalid chat service keys");}bytes.extend_from_slice(&chunk);}
 parse_roots(&bytes)
}
fn parse_roots(bytes:&[u8])->Result<Vec<Vec<u8>>,&'static str>{
 let value:serde_json::Value=serde_json::from_slice(bytes).map_err(|_|"Invalid chat service keys")?;
 let list=value["playerCertificateKeys"].as_array().ok_or("Missing chat service keys")?;if list.is_empty()||list.len()>16{return Err("Invalid chat service keys");}
 let mut roots=Vec::new();for entry in list{let text=entry["publicKey"].as_str().ok_or("Invalid chat service key")?;if text.len()>16384{return Err("Invalid chat service key");}let key=base64::engine::general_purpose::STANDARD.decode(text).map_err(|_|"Invalid chat service key")?;if key.len()>8192{return Err("Invalid chat service key");}roots.push(key);}Ok(roots)
}
#[cfg(test)]mod tests{use super::*;
 #[test]fn service_keys_require_certificate_roots_not_skin_property_roots(){assert!(parse_roots(br#"{"profilePropertyKeys":[{"publicKey":"AAAA"}]}"#).is_err());assert!(parse_roots(br#"{"playerCertificateKeys":[]}"#).is_err());assert_eq!(parse_roots(br#"{"playerCertificateKeys":[{"publicKey":"AAAA"}]}"#).unwrap(),vec![vec![0,0,0]]);}
 #[test]fn reset_clears_cross_server_sessions_deletions_and_pending_messages(){let mut i=Incoming::default();i.delete(&MessageSignature{bytes:[7;256]});i.reset();assert!(i.sessions.is_empty()&&i.deleted.is_empty()&&i.queue.is_empty()&&i.ledger.is_empty());}
}

impl Drop for Incoming{fn drop(&mut self){if let Some(fetch)=&self.fetch{fetch.abort();}}}
#[cfg(test)]mod signature_contract_tests {
 use super::*;use azalea_protocol::packets::game::{c_player_chat::{PackedSignedMessageBody,PackedLastSeenMessages,ChatTypeBound},s_chat_session_update::ProfilePublicKeyData};use azalea_registry::{Holder,Registry,data::ChatKind};
 fn fixture()->(ClientboundPlayerChat,RemoteChatSessionData,Vec<Vec<u8>>,Vec<MessageSignature>){
  let path=std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/signed_chat_verified.json");let v:serde_json::Value=serde_json::from_slice(&std::fs::read(path).unwrap()).unwrap();
  let decode=|key:&str|base64::engine::general_purpose::STANDARD.decode(v[key].as_str().unwrap()).unwrap();
  let packet=ClientboundPlayerChat{global_index:0,sender:Uuid::parse_str(v["sender"].as_str().unwrap()).unwrap(),index:3,signature:Some(MessageSignature{bytes:decode("signature").try_into().unwrap()}),body:PackedSignedMessageBody{content:"six".into(),timestamp:5000,salt:4,last_seen:PackedLastSeenMessages{entries:Vec::new()}},unsigned_content:None,filter_mask:FilterMask::PassThrough,chat_type:ChatTypeBound{chat_type:Holder::Reference(ChatKind::from_u32(0).unwrap()),name:azalea_chat::FormattedText::from("Sender"),target_name:None}};
  let remote=RemoteChatSessionData{session_id:Uuid::parse_str(v["session"].as_str().unwrap()).unwrap(),profile_public_key:ProfilePublicKeyData{expires_at:v["expires"].as_u64().unwrap(),key:decode("key"),key_signature:decode("certificate")}};
  (packet,remote,vec![decode("root")],vec![MessageSignature{bytes:decode("last_seen").try_into().unwrap()}])
 }
 #[test]fn actual_signed_body_and_mojang_certificate_contract_reject_tampering_and_wrong_identity(){
  let(p,remote,roots,last)=fixture();let mut ledger=HashMap::new();assert!(verify_candidate(&p,&last,Some(&remote),&roots,&mut ledger,10000));
  let mut tampered=p.clone();tampered.body.content="tampered".into();assert!(!verify_candidate(&tampered,&last,Some(&remote),&roots,&mut ledger,10000));
  let mut wrong=p.clone();wrong.sender=Uuid::from_u128(99);assert!(!verify_candidate(&wrong,&last,Some(&remote),&roots,&mut HashMap::new(),10000));
  let mut wrong_session=remote.clone();wrong_session.session_id=Uuid::from_u128(99);assert!(!verify_candidate(&p,&last,Some(&wrong_session),&roots,&mut HashMap::new(),10000));
  assert!(!verify_candidate(&p,&[],Some(&remote),&roots,&mut HashMap::new(),10000));assert!(!verify_candidate(&p,&last,None,&roots,&mut HashMap::new(),10000));assert!(!verify_candidate(&p,&last,Some(&remote),&[],&mut HashMap::new(),10000));
 }
 #[test]fn incoming_chain_replay_and_official_eight_hour_expiry_grace(){let(p,remote,roots,last)=fixture();let expiry=remote.profile_public_key.expires_at;assert!(verify_candidate(&p,&last,Some(&remote),&roots,&mut HashMap::new(),expiry+GRACE));assert!(!verify_candidate(&p,&last,Some(&remote),&roots,&mut HashMap::new(),expiry+GRACE+1));let mut ledger=HashMap::new();assert!(verify_candidate(&p,&last,Some(&remote),&roots,&mut ledger,10000));let mut old=p.clone();old.index=2;assert!(!verify_candidate(&old,&last,Some(&remote),&roots,&mut ledger,10000));assert!(verify_candidate(&p,&last,Some(&remote),&roots,&mut ledger,10000));}
 #[test]fn deletion_marks_queued_message_even_when_bounded_tombstone_history_rolls_over(){let(p,remote,_,last)=fixture();let s=p.signature.as_ref().unwrap().clone();let mut incoming=Incoming::default();incoming.queue.push_back(Pending{packet:Arc::new(p),remote:Some(remote),last_seen:last,deleted:false});incoming.delete(&s);for n in 0..300u32{let mut bytes=[0u8;256];bytes[..4].copy_from_slice(&n.to_be_bytes());incoming.delete(&MessageSignature{bytes});}assert_eq!(incoming.deleted.len(),256);assert!(incoming.queue[0].deleted);incoming.reset();assert!(incoming.queue.is_empty());}
}
