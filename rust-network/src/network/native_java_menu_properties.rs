//! Ordered, window-scoped container properties. Property wire shorts are signed.
use std::collections::BTreeMap;
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct FurnaceStatus{
 pub burn_remaining:Option<u32>,pub burn_total:Option<u32>,pub cook_progress:Option<u32>,pub cook_total:Option<u32>,
}
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct PropertyEntry{pub id:i32,pub value:i32}
#[derive(Debug)]pub struct MenuProperties{window_id:i32,kind:String,generation:u64,values:BTreeMap<u16,i32>}
impl Default for MenuProperties{fn default()->Self{Self{window_id:0,kind:"player".into(),generation:0,values:BTreeMap::new()}}}
impl MenuProperties{
 pub fn generation(&self)->u64{self.generation}
 pub fn accepts(&self,window_id:i32,generation:Option<u64>)->bool{window_id==self.window_id&&generation.is_none_or(|g|g==self.generation)}
 pub fn open(&mut self,window_id:i32,kind:&str){if window_id<0{return}self.window_id=window_id;self.kind=kind.into();self.generation=self.generation.wrapping_add(1);self.values.clear();}
 pub fn reconcile(&mut self,window_id:i32,kind:&str){if self.window_id!=window_id||self.kind!=kind{self.open(window_id,kind);}}
 pub fn reset(&mut self){self.open(0,"player");}
 pub fn close(&mut self,window_id:i32)->bool{if self.window_id!=window_id{return false}self.reset();true}
 pub fn set(&mut self,window_id:i32,id:u16,value:u16)->bool{
  if window_id!=self.window_id||id>i16::MAX as u16||(!self.values.contains_key(&id)&&self.values.len()>=256){return false}
  self.values.insert(id,i32::from(value as i16));true
 }
 pub fn snapshot(&self,window_id:i32)->BTreeMap<u16,i32>{if window_id==self.window_id{self.values.clone()}else{BTreeMap::new()}}
 pub fn entries(&self,window_id:i32)->Vec<PropertyEntry>{if window_id!=self.window_id{return Vec::new()}self.values.iter().map(|(id,value)|PropertyEntry{id:i32::from(*id),value:*value}).collect()}
 pub fn furnace_status(&self,window_id:i32)->Option<FurnaceStatus>{
  if window_id!=self.window_id||!matches!(self.kind.as_str(),"furnace"|"blast_furnace"|"smoker"){return None}
  let field=|id|self.values.get(&id).copied().filter(|v|*v>=0).map(|v|v as u32);
  Some(FurnaceStatus{burn_remaining:field(0),burn_total:field(1),cook_progress:field(2),cook_total:field(3)})
 }
}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn same_id_reopen_rejects_stale_commands_but_accepts_legacy_absent_generation(){let mut s=MenuProperties::default();s.open(7,"furnace");let old=s.generation();assert!(s.accepts(7,Some(old)));assert!(s.accepts(7,None));assert!(!s.accepts(8,Some(old)));s.open(7,"furnace");assert!(!s.accepts(7,Some(old)));assert!(s.accepts(7,Some(s.generation()))&&s.accepts(7,None));s.close(7);assert!(!s.accepts(7,None));}
 #[test]fn matching_window_properties_are_signed_and_furnace_fields_authoritative(){let mut s=MenuProperties::default();s.open(4,"furnace");assert!(s.set(4,0,25)&&s.set(4,1,100)&&s.set(4,2,40)&&s.set(4,3,200));assert!(!s.set(5,0,999));assert_eq!(s.furnace_status(4),Some(FurnaceStatus{burn_remaining:Some(25),burn_total:Some(100),cook_progress:Some(40),cook_total:Some(200)}));assert!(s.set(4,2,u16::MAX));assert_eq!(s.snapshot(4)[&2],-1);assert_eq!(s.furnace_status(4).unwrap().cook_progress,None);assert_eq!(serde_json::to_value(s.snapshot(4)).unwrap()["2"],-1);assert!(s.snapshot(99).is_empty());}
 #[test]fn every_open_including_reused_ids_clears_epoch_and_mismatching_close_does_not(){let mut s=MenuProperties::default();s.open(7,"furnace");s.set(7,1,100);let old=s.generation();s.reconcile(7,"furnace");assert_eq!(s.generation(),old);assert_eq!(s.snapshot(7)[&1],100);assert!(!s.close(8));assert_eq!(s.generation(),old);s.open(7,"furnace");assert!(s.generation()>old&&s.snapshot(7).is_empty());assert!(s.close(7));assert!(s.snapshot(7).is_empty());assert!(!s.set(7,0,1));assert_eq!(s.furnace_status(0),None);}
 #[test]fn reset_on_login_respawn_configuration_and_transfer_never_leaks_properties(){for _reason in["login","respawn","configuration","transfer"]{let mut s=MenuProperties::default();s.open(8,"smoker");s.set(8,0,50);let old=s.generation();s.reset();assert!(s.generation()>old);assert!(s.snapshot(8).is_empty());assert_eq!(s.furnace_status(8),None);assert!(!s.set(8,0,50));s.reconcile(9,"blast_furnace");assert_eq!(s.furnace_status(9).unwrap().burn_total,None);}}
 #[test]fn raw_negative_values_and_large_valid_ids_survive_bounded_map(){let mut s=MenuProperties::default();s.open(2,"generic_9x3");assert!(s.set(2,32767,0x8000));assert_eq!(s.snapshot(2)[&32767],-32768);assert!(!s.set(2,32768,1));for id in 0..255{s.set(2,id,1);}assert_eq!(s.snapshot(2).len(),256);assert!(!s.set(2,256,1));assert!(s.set(2,0,u16::MAX));assert_eq!(s.snapshot(2)[&0],-1);assert_eq!(s.furnace_status(2),None);}
}
