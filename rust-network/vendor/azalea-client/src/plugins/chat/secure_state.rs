//! Minecraft 1.21.11 last-seen tracking. Mirrors the official 20-entry ring and
//! 128-entry signature cache; state belongs to the connection, not the account.
use std::collections::{VecDeque,HashMap};
use azalea_crypto::signing::MessageSignature;
use azalea_protocol::packets::game::{c_commands::ClientboundCommands,c_player_chat::{PackedMessageSignature,ClientboundPlayerChat,FilterMask},s_chat::LastSeenMessagesUpdate};
use azalea_core::bitset::FixedBitSet;
use bevy_ecs::prelude::Component;
use uuid::Uuid;
use azalea_protocol::packets::game::s_chat_session_update::RemoteChatSessionData;
#[derive(Clone)]pub struct ReceivedChat {pub last_seen:Vec<MessageSignature>,pub remote:Option<RemoteChatSessionData>}
#[derive(Clone)]
struct Entry { signature: MessageSignature, pending: bool }
#[derive(Component)]
pub struct SecureChatState {
    ring: [Option<Entry>;20], tail:usize, offset:u32,
    last:Option<MessageSignature>, cache:Vec<Option<MessageSignature>>,
    pub commands:Option<ClientboundCommands>,
    pub remote_sessions:HashMap<Uuid,RemoteChatSessionData>,
    received:HashMap<[u8;256],ReceivedChat>,received_order:VecDeque<[u8;256]>,
}
impl Default for SecureChatState {
 fn default()->Self {Self{ring:std::array::from_fn(|_|None),tail:0,offset:0,last:None,cache:vec![None;128],commands:None,remote_sessions:HashMap::new(),received:HashMap::new(),received_order:VecDeque::new()}}
}
impl SecureChatState {
 pub fn resolve(&self,s:&PackedMessageSignature)->Option<MessageSignature>{ match s {PackedMessageSignature::Signature(s)=>Some((**s).clone()),PackedMessageSignature::Id(id)=>self.cache.get(*id as usize)?.clone()}}
 pub fn take_received(&mut self,s:&MessageSignature)->Option<ReceivedChat> {self.received.remove(&s.bytes)}
 pub fn receive_deferred(&mut self,p:&ClientboundPlayerChat)->Result<(),()> {
  self.receive_inner(p,false).map(|_|())
 }
 pub fn complete_received(&mut self,s:MessageSignature,displayed:bool)->Option<u32>{self.add(s,displayed);if self.offset>64{Some(self.clear_offset())}else{None}}
 pub fn receive(&mut self,p:&ClientboundPlayerChat)->Result<bool,()> {self.receive_inner(p,true)}
 fn receive_inner(&mut self,p:&ClientboundPlayerChat,track:bool)->Result<bool,()> {
  if p.body.last_seen.entries.len()>20{return Err(()); }
  let mut signatures=VecDeque::new();
  for s in &p.body.last_seen.entries {signatures.push_back(self.resolve(s).ok_or(())?);}
  if let Some(s)=&p.signature {
   if self.received.len()>=256{return Err(());}
   self.received.insert(s.bytes,ReceivedChat{last_seen:signatures.iter().cloned().collect(),remote:self.remote_sessions.get(&p.sender).cloned()});self.received_order.push_back(s.bytes);
   while self.received_order.len()>256 {if let Some(old)=self.received_order.pop_front(){self.received.remove(&old);}}
   signatures.push_back(s.clone());
  }
  let inserted:Vec<_>=signatures.iter().cloned().collect();
  for i in 0..128 {
   let Some(next)=signatures.pop_back() else {break};
   let old=self.cache[i].replace(next);
   if let Some(old)=old {if !inserted.contains(&old){signatures.push_front(old);}}
  }
  if track {if let Some(s)=&p.signature {self.add(s.clone(),!matches!(p.filter_mask,FilterMask::FullyFiltered));}}
  Ok(self.offset>64)
 }
 pub fn add(&mut self,s:MessageSignature,displayed:bool){
  if self.last.as_ref()==Some(&s){return;}
  self.last=Some(s.clone());
  self.ring[self.tail]=displayed.then_some(Entry{signature:s,pending:true});
  self.tail=(self.tail+1)%20;self.offset=self.offset.saturating_add(1);
 }
 pub fn delete(&mut self,s:&MessageSignature){for e in &mut self.ring {if e.as_ref().is_some_and(|e|e.pending&&e.signature==*s){*e=None;break;}}}
 pub fn clear_offset(&mut self)->u32 {std::mem::take(&mut self.offset)}
 pub fn update(&mut self)->(LastSeenMessagesUpdate,Vec<MessageSignature>){
  let offset=self.clear_offset();let mut acknowledged=FixedBitSet::<20>::new();let mut signatures=Vec::new();
  for i in 0..20 {if let Some(e)=&mut self.ring[(self.tail+i)%20] {acknowledged.set(i);signatures.push(e.signature.clone());e.pending=false;}}
  let mut checksum=1i32;
  for s in &signatures {let hash=s.bytes.iter().fold(1i32,|hash,b|hash.wrapping_mul(31).wrapping_add(*b as i8 as i32));checksum=checksum.wrapping_mul(31).wrapping_add(hash);}
  let checksum=match checksum as u8 {0=>1,c=>c};
  (LastSeenMessagesUpdate{offset,acknowledged,checksum},signatures)
 }
}
#[cfg(test)] mod tests {
 use super::*;
 fn s(n:u8)->MessageSignature{MessageSignature{bytes:[n;256]}}
 #[test] fn ring_offsets_duplicates_acknowledged_deletion(){let mut t=SecureChatState::default();for i in 0..25{t.add(s(i),true);}t.add(s(24),true);let (u,list)=t.update();assert_eq!(u.offset,25);assert_eq!(list.iter().map(|s|s.bytes[0]).collect::<Vec<_>>(),(5..25).collect::<Vec<_>>());assert!((0..20).all(|i|u.acknowledged.index(i)));t.delete(&s(24));assert_eq!(t.update().1.len(),20);t.add(s(25),true);t.delete(&s(25));assert_eq!(t.update().1.len(),19);}
 #[test] fn hidden_receipts_advance_without_ack(){let mut t=SecureChatState::default();t.add(s(255),false);let(u,list)=t.update();assert_eq!(u.offset,1);assert_eq!(u.checksum,1);assert!(list.is_empty());}
 #[test] fn checksum_matches_java_signed_byte_hash(){let mut t=SecureChatState::default();t.add(s(255),true);let(u,_)=t.update();let hash=(0..256).fold(1i32,|h,_|h.wrapping_mul(31).wrapping_sub(1));let expected=31i32.wrapping_add(hash) as u8;assert_eq!(u.checksum,if expected==0{1}else{expected});}
}
