//! Authoritative global compass targets. Missing targets remain missing.
use azalea_core::position::GlobalPos;
use azalea_protocol::packets::game::ClientboundGamePacket;
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct GlobalTarget{pub dimension:String,pub position:[i32;3]}
impl From<&GlobalPos> for GlobalTarget{fn from(value:&GlobalPos)->Self{Self{dimension:value.dimension.to_string(),position:[value.pos.x,value.pos.y,value.pos.z]}}}
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct LodestoneSnapshot{pub target:Option<GlobalTarget>,pub tracked:bool}
#[derive(Debug,Clone,PartialEq,Default,serde::Serialize)]pub struct CompassSnapshot{pub dimension:String,pub default_spawn:Option<GlobalTarget>,pub last_death:Option<GlobalTarget>}
impl CompassSnapshot{
 pub fn clear(&mut self){*self=Self::default();}
 pub fn ingest(&mut self,packet:&ClientboundGamePacket)->bool{
  match packet{
   ClientboundGamePacket::Login(p)=>{self.dimension=p.common.dimension.to_string();self.default_spawn=None;self.last_death=p.common.last_death_location.as_ref().map(GlobalTarget::from);true},
   ClientboundGamePacket::Respawn(p)=>{self.dimension=p.common.dimension.to_string();self.default_spawn=None;self.last_death=p.common.last_death_location.as_ref().map(GlobalTarget::from);true},
   ClientboundGamePacket::SetDefaultSpawnPosition(p)=>{let target=GlobalTarget::from(&p.global_pos);if self.default_spawn.as_ref()==Some(&target){false}else{self.default_spawn=Some(target);true}},
   ClientboundGamePacket::StartConfiguration(_)|ClientboundGamePacket::Transfer(_)=>{self.clear();true},
   _=>false,
  }
 }
}
#[cfg(test)]mod tests{
use super::*;use azalea_core::{position::BlockPos,game_type::{GameMode,OptionalGameType}};use azalea_protocol::packets::{common::CommonPlayerSpawnInfo,game::*};use azalea_registry::DataRegistry;
fn target(dimension:&str)->GlobalPos{GlobalPos{dimension:dimension.into(),pos:BlockPos{x:12,y:70,z:-34}}}
fn common(dimension:&str)->CommonPlayerSpawnInfo{CommonPlayerSpawnInfo{dimension_type:azalea_registry::data::DimensionKind::new_raw(0),dimension:dimension.into(),seed:0,game_type:GameMode::Survival,previous_game_type:OptionalGameType(None),is_debug:false,is_flat:false,last_death_location:Some(target("minecraft:overworld")),portal_cooldown:0,sea_level:63}}
#[test]fn actual_login_spawn_and_respawn_packets_keep_dimension_and_clear_old_target(){let mut s=CompassSnapshot::default();let login=ClientboundGamePacket::Login(ClientboundLogin{player_id:azalea_world::MinecraftEntityId(1),hardcore:false,levels:vec![],max_players:10,chunk_radius:8,simulation_distance:8,reduced_debug_info:false,show_death_screen:true,do_limited_crafting:false,common:common("minecraft:overworld"),enforces_secure_chat:false});assert!(s.ingest(&login));assert_eq!(s.dimension,"minecraft:overworld");assert!(s.default_spawn.is_none());assert_eq!(s.last_death.as_ref().unwrap().position,[12,70,-34]);let spawn=ClientboundGamePacket::SetDefaultSpawnPosition(ClientboundSetDefaultSpawnPosition{global_pos:target("minecraft:overworld"),yaw:90.,pitch:0.});assert!(s.ingest(&spawn));assert!(!s.ingest(&spawn));assert_eq!(s.default_spawn.as_ref().unwrap().dimension,"minecraft:overworld");let respawn=ClientboundGamePacket::Respawn(ClientboundRespawn{common:common("minecraft:the_nether"),data_to_keep:0});assert!(s.ingest(&respawn));assert_eq!(s.dimension,"minecraft:the_nether");assert!(s.default_spawn.is_none());assert_eq!(s.last_death.as_ref().unwrap().dimension,"minecraft:overworld");}
#[test]fn actual_configuration_transfer_and_disconnect_clear_all_targets(){let mut s=CompassSnapshot{dimension:"minecraft:overworld".into(),default_spawn:Some(GlobalTarget::from(&target("minecraft:overworld"))),last_death:Some(GlobalTarget::from(&target("minecraft:overworld")))};let original=s.clone();assert!(s.ingest(&ClientboundGamePacket::StartConfiguration(ClientboundStartConfiguration)));assert_eq!(s,CompassSnapshot::default());s=original.clone();assert!(s.ingest(&ClientboundGamePacket::Transfer(ClientboundTransfer{host:"example.invalid".into(),port:25565})));assert_eq!(s,CompassSnapshot::default());s=original;s.clear();assert_eq!(s,CompassSnapshot::default());}
}
