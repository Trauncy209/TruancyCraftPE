//! Per-connection pack state. Disk caching never makes a pack globally active.
use std::sync::atomic::{AtomicU64,Ordering};
static CONNECTIONS:AtomicU64=AtomicU64::new(1);
#[derive(Clone,Debug,serde::Serialize,serde::Deserialize)]
pub struct PackRequest {pub id:String,pub url:String,pub hash:String,pub required:bool,pub prompt:String}
#[derive(Clone,Debug,serde::Serialize)]
pub struct PackSnapshot {pub connection:u64,pub generation:u64,pub requests:Vec<PackRequest>}
pub struct PackScope {connection:u64,generation:u64,in_configuration:bool,packs:Vec<PackRequest>}
impl Default for PackScope {fn default()->Self{Self{connection:CONNECTIONS.fetch_add(1,Ordering::Relaxed),generation:0,in_configuration:false,packs:Vec::new()}}}
impl PackScope {
 pub fn snapshot(&self)->PackSnapshot{PackSnapshot{connection:self.connection,generation:self.generation,requests:self.packs.clone()}}
 pub fn clear(&mut self){self.generation=self.generation.wrapping_add(1);self.packs.clear();}
 pub fn begin_configuration(&mut self){self.clear();self.in_configuration=true;}
 pub fn game_login(&mut self){if !self.in_configuration{self.clear();}self.in_configuration=false;}
 pub fn push(&mut self,pack:PackRequest){self.generation=self.generation.wrapping_add(1);self.packs.retain(|p|p.id!=pack.id);self.packs.push(pack);}
 pub fn pop(&mut self,id:Option<&str>){self.generation=self.generation.wrapping_add(1);if let Some(id)=id{self.packs.retain(|p|p.id!=id);}else{self.packs.clear();}}
 pub fn request(&self,id:&str)->Option<PackRequest>{self.packs.iter().find(|p|p.id==id).cloned()}
 pub fn completion_is_current(&self,connection:u64,generation:u64,id:&str)->bool{connection==self.connection&&generation==self.generation&&self.packs.iter().any(|p|p.id==id)}
}
#[cfg(test)] mod tests {
 use super::*;
 fn pack(id:&str)->PackRequest{PackRequest{id:id.into(),url:"https://example.invalid/pack.zip".into(),hash:"".into(),required:false,prompt:"".into()}}
 #[test] fn configuration_pack_survives_initial_login_but_not_next_backend_login(){let mut s=PackScope::default();s.begin_configuration();s.push(pack("practice"));s.game_login();assert_eq!(s.snapshot().requests.len(),1);s.game_login();assert!(s.snapshot().requests.is_empty());}
 #[test] fn popped_or_replaced_pack_cannot_be_activated_by_late_download(){let mut s=PackScope::default();s.push(pack("a"));let g=s.snapshot().generation;assert!(s.completion_is_current(s.snapshot().connection,g,"a"));s.pop(Some("a"));assert!(!s.completion_is_current(s.snapshot().connection,g,"a"));s.push(pack("a"));assert!(!s.completion_is_current(s.snapshot().connection,g,"a"));s.clear();assert!(s.snapshot().requests.is_empty());}
 #[test] fn previous_connection_cannot_activate_same_id_in_new_connection(){let mut old=PackScope::default();let mut new=PackScope::default();old.push(pack("a"));new.push(pack("a"));let key=old.snapshot();assert!(!new.completion_is_current(key.connection,key.generation,"a"));}
 #[test] fn push_order_preserves_override_priority(){let mut s=PackScope::default();s.push(pack("z"));s.push(pack("a"));assert_eq!(s.snapshot().requests.iter().map(|p|p.id.as_str()).collect::<Vec<_>>(),vec!["z","a"]);s.push(pack("z"));assert_eq!(s.snapshot().requests.iter().map(|p|p.id.as_str()).collect::<Vec<_>>(),vec!["a","z"]);}
 #[test] fn pop_one_preserves_others_and_pop_all_clears_stack(){let mut s=PackScope::default();s.push(pack("a"));s.push(pack("b"));s.pop(Some("a"));assert_eq!(s.snapshot().requests[0].id,"b");s.pop(None);assert!(s.snapshot().requests.is_empty());}
}

/// Renderer completion is distinct from download, and only a complete current
/// stack can report success. Old connections/generations and repeats are inert.
#[derive(Default)]pub struct PackRendererAcknowledgments{
 scope:Option<(u64,u64)>,ready:std::collections::BTreeSet<String>,loaded:std::collections::BTreeSet<String>,
}
impl PackRendererAcknowledgments{
 pub fn downloaded(&mut self,scope:&PackScope,connection:u64,generation:u64,id:&str)->bool{
  if !scope.completion_is_current(connection,generation,id){return false;}
  if self.scope!=Some((connection,generation)){self.scope=Some((connection,generation));self.ready.clear();self.loaded.clear();}
  self.ready.insert(id.to_owned())
 }
 pub fn activated(&mut self,scope:&PackScope,connection:u64,generation:u64,id:&str)->bool{
  if self.scope!=Some((connection,generation))||!scope.completion_is_current(connection,generation,id){return false;}
  if !scope.packs.iter().all(|p|self.ready.contains(&p.id)||self.loaded.contains(&p.id)){return false;}
  if !self.ready.remove(id){return false;}self.loaded.insert(id.to_owned())
 }
}
#[cfg(test)]mod renderer_ack_tests{
 use super::*;
 fn pack(id:&str)->PackRequest{PackRequest{id:id.into(),url:"http://127.0.0.1/owned.zip".into(),hash:String::new(),required:false,prompt:String::new()}}
 #[test]fn loaded_requires_decoded_complete_current_stack_and_is_once_only(){let mut scope=PackScope::default();scope.push(pack("a"));scope.push(pack("b"));let snap=scope.snapshot();let mut ack=PackRendererAcknowledgments::default();assert!(!ack.activated(&scope,snap.connection,snap.generation,"a"));assert!(ack.downloaded(&scope,snap.connection,snap.generation,"a"));assert!(!ack.activated(&scope,snap.connection,snap.generation,"a"));assert!(ack.downloaded(&scope,snap.connection,snap.generation,"b"));assert!(!ack.activated(&scope,snap.connection+1,snap.generation,"a"));assert!(!ack.activated(&scope,snap.connection,snap.generation+1,"a"));assert!(!ack.activated(&scope,snap.connection,snap.generation,"unknown"));assert!(ack.activated(&scope,snap.connection,snap.generation,"a"));assert!(!ack.activated(&scope,snap.connection,snap.generation,"a"));assert!(ack.activated(&scope,snap.connection,snap.generation,"b"));scope.pop(Some("a"));assert!(!ack.activated(&scope,snap.connection,snap.generation,"b"));}
 #[test]fn same_id_in_next_generation_requires_its_new_decoded_completion(){let mut scope=PackScope::default();scope.push(pack("a"));let old=scope.snapshot();let mut ack=PackRendererAcknowledgments::default();ack.downloaded(&scope,old.connection,old.generation,"a");assert!(ack.activated(&scope,old.connection,old.generation,"a"));scope.push(pack("a"));let new=scope.snapshot();assert!(!ack.activated(&scope,new.connection,new.generation,"a"));assert!(!ack.downloaded(&scope,old.connection,old.generation,"a"));assert!(ack.downloaded(&scope,new.connection,new.generation,"a"));assert!(ack.activated(&scope,new.connection,new.generation,"a"));}
}
