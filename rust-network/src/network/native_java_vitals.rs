//! Java HUD state derived from authoritative effect/metadata/health packets.
use super::PlayerVitals;
use azalea_protocol::packets::game::ClientboundGamePacket;
use azalea_registry::builtin::MobEffect;
#[derive(Default)]
pub(super) struct State { hardcore:bool, invulnerable:u32,health_known:bool }
impl State {
 pub fn packet(&mut self,p:&ClientboundGamePacket,v:&mut PlayerVitals){match p {
  ClientboundGamePacket::Login(p)=>{self.hardcore=p.hardcore;self.invulnerable=0;self.health_known=false;},
  ClientboundGamePacket::Respawn(_)=>{self.invulnerable=0;self.health_known=false;},
  ClientboundGamePacket::SetHealth(p)=>{if p.health.is_finite(){if self.health_known{if p.health<v.health{self.invulnerable=20;}else if p.health>v.health{self.invulnerable=10;}}self.health_known=true;}},
  _=>{}
 }v.hardcore=self.hardcore;v.invulnerable_ticks=self.invulnerable;}
 pub fn tick(&mut self,w:&bevy_ecs::world::World,e:bevy_ecs::entity::Entity,v:&mut PlayerVitals){
  self.invulnerable=self.invulnerable.saturating_sub(1);v.invulnerable_ticks=self.invulnerable;v.hardcore=self.hardcore;
  let effects=w.get::<azalea_entity::ActiveEffects>(e);
  v.poisoned=effects.is_some_and(|fx|fx.get(MobEffect::Poison).is_some());
  v.withered=effects.is_some_and(|fx|fx.get(MobEffect::Wither).is_some());
  v.regenerating=effects.is_some_and(|fx|fx.get(MobEffect::Regeneration).is_some());
  v.frozen_ticks=w.get::<azalea_entity::metadata::TicksFrozen>(e).map_or(0,|t|t.0.max(0) as u32);
 }
}
#[cfg(test)]mod tests {
 use super::*;
 #[test]fn real_packet_ecs_effects_and_freezing_feed_hud_and_login_respawn_preserve_hardcore(){
  use azalea_client::test_utils::prelude::*;use azalea_protocol::packets::{ConnectionProtocol,game::*};
  let mut sim=Simulation::new(ConnectionProtocol::Game);let mut login=default_login_packet();login.hardcore=true;sim.receive_packet(login.clone());sim.update();
  let mut state=State::default();let mut v=PlayerVitals::default();state.packet(&ClientboundGamePacket::Login(login.clone()),&mut v);
  sim.receive_packet(ClientboundUpdateMobEffect{entity_id:login.player_id,mob_effect:MobEffect::Poison,data:Default::default()});
  sim.receive_packet(ClientboundSetEntityData{id:login.player_id,packed_items:azalea_entity::EntityMetadataItems(vec![azalea_entity::EntityDataItem{index:7,value:azalea_entity::EntityDataValue::Int(140)}])});sim.update();
  state.tick(sim.app.world(),sim.entity,&mut v);assert!(v.poisoned&&v.hardcore);assert_eq!(v.frozen_ticks,140);
  sim.receive_packet(ClientboundRemoveMobEffect{entity_id:login.player_id,effect:MobEffect::Poison});sim.update();state.tick(sim.app.world(),sim.entity,&mut v);assert!(!v.poisoned);
  state.invulnerable=17;state.packet(&ClientboundGamePacket::Respawn(ClientboundRespawn{common:login.common.clone(),data_to_keep:0}),&mut v);assert!(v.hardcore);assert_eq!(v.invulnerable_ticks,0);
  login.hardcore=false;state.packet(&ClientboundGamePacket::Login(login),&mut v);assert!(!v.hardcore);
 }
 #[test]fn actual_ecs_effect_flags_ignore_icon_visibility_and_clear_on_removal(){let mut world=bevy_ecs::world::World::new();let mut effects=azalea_entity::ActiveEffects::default();effects.insert(MobEffect::Poison,Default::default());effects.insert(MobEffect::Wither,Default::default());effects.insert(MobEffect::Regeneration,Default::default());let entity=world.spawn((effects,azalea_entity::metadata::TicksFrozen(140))).id();let mut state=State::default();let mut v=PlayerVitals::default();state.tick(&world,entity,&mut v);assert!(v.poisoned&&v.withered&&v.regenerating);assert_eq!(v.frozen_ticks,140);world.entity_mut(entity).remove::<(azalea_entity::ActiveEffects,azalea_entity::metadata::TicksFrozen)>();state.tick(&world,entity,&mut v);assert!(!v.poisoned&&!v.withered&&!v.regenerating);assert_eq!(v.frozen_ticks,0);}
 #[test]fn authoritative_health_first_packet_damage_and_healing_match_localplayer_hurt_to(){let mut state=State::default();let mut v=PlayerVitals::default();let packet=|health|ClientboundGamePacket::SetHealth(azalea_protocol::packets::game::ClientboundSetHealth{health,food:20,saturation:5.});state.packet(&packet(18.),&mut v);assert_eq!(v.invulnerable_ticks,0,"first server health initializes without flash");v.health=18.;state.packet(&packet(16.),&mut v);assert_eq!(v.invulnerable_ticks,20);v.health=16.;let mut world=bevy_ecs::world::World::new();let e=world.spawn_empty().id();for _ in 0..20{state.tick(&world,e,&mut v);}assert_eq!(v.invulnerable_ticks,0);state.packet(&packet(19.),&mut v);assert_eq!(v.invulnerable_ticks,10,"healing starts its own10tick flash even after damage expired");v.health=19.;for _ in 0..10{state.tick(&world,e,&mut v);}assert_eq!(v.invulnerable_ticks,0);state.packet(&packet(19.),&mut v);assert_eq!(v.invulnerable_ticks,0,"unchanged health does not flash");}
}
