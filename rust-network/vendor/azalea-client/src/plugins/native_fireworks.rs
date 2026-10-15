//! Authoritative attached firework propulsion (official1.21.11 ddy.tick).
use azalea_entity::{HasClientLoaded,LocalEntity,LookDirection,Physics,metadata::{AttachedToTarget,FallFlying,FireworkRocket,Health}};
use azalea_physics::elytra::glide_view_vector as view_vector;
use azalea_world::{InstanceName,MinecraftEntityId};
use bevy_ecs::prelude::*;
/// Applied after local travel, as the attached rocket's own entity tick does.
/// No fabricated rocket, inventory decrement, lifetime or launch prediction.
pub fn boost(mut players:Query<(&MinecraftEntityId,&InstanceName,&LookDirection,&FallFlying,Option<&Health>,&mut Physics),(With<LocalEntity>,With<HasClientLoaded>,Without<azalea_entity::Dead>,Without<azalea_physics::LocalDeathFrozen>)>,rockets:Query<(&AttachedToTarget,&InstanceName),With<FireworkRocket>>){
 for (id,instance,look,gliding,health,mut physics) in &mut players{
  let Ok(target_id)=u32::try_from(id.0)else{continue;};
  if !gliding.0||health.is_some_and(|h|!h.0.is_finite()||h.0<=0.){continue;}
  for (attached,rocket_instance) in &rockets{
   if attached.0.0!=Some(target_id)||instance!=rocket_instance{continue;}
   let direction=view_vector(*look);let old=physics.velocity;
   physics.velocity=azalea_core::position::Vec3::new(old.x+direction.x*0.1+(direction.x*1.5-old.x)*0.5,old.y+direction.y*0.1+(direction.y*1.5-old.y)*0.5,old.z+direction.z*0.1+(direction.z*1.5-old.z)*0.5);
  }
 }
}
