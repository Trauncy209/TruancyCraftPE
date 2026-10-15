use super::effects::{VisualEffect,FireworkPayload};
use bevy_ecs::{world::World,entity::Entity};
pub fn tick(world:&mut World,local:Entity)->Vec<VisualEffect>{
 let mut output=Vec::new();
 for event in azalea_client::native_firework_visuals::drain(world,local){output.push(VisualEffect{kind:"firework_explosion",position:[event.position.x,event.position.y,event.position.z],count:1,offset:[0.;3],firework:Some(FireworkPayload{velocity:[event.velocity.x,event.velocity.y,event.velocity.z],explosions:event.explosions})});}
 for (position,velocity) in azalea_client::native_firework_visuals::trails(world,local){output.push(VisualEffect{kind:"firework_spark",position:[position.x,position.y,position.z],count:1,offset:[0.;3],firework:Some(FireworkPayload{velocity:[velocity.x,velocity.y,velocity.z],explosions:Vec::new()})});}
 output
}
