//! Native freeform touch aim changes action rotation, never camera/physics state.
use super::pick::{NativePickRay,NativePickStamp};
use azalea_entity::{LookDirection,Physics};
use azalea_protocol::{common::movements::MoveFlags,packets::game::{ServerboundUseItem,ServerboundMovePlayerRot,s_interact::InteractionHand}};
use bevy_ecs::prelude::*;
use crate::packet::game::SendGamePacketEvent;

pub fn current(ray:Option<&NativePickRay>,stamp:Option<&NativePickStamp>)->Option<LookDirection>{
 let ray=ray?;let stamp=stamp?;
 if !ray.enabled||ray.revision==0||stamp.input_revision!=ray.revision||stamp.revision==0||!ray.look.y_rot().is_finite()||!ray.look.x_rot().is_finite(){return None;}
 Some(ray.look)
}
pub fn flags(physics:&Physics)->MoveFlags{MoveFlags{on_ground:physics.on_ground(),horizontal_collision:physics.horizontal_collision}}
pub fn differs(a:LookDirection,b:LookDirection)->bool{
 let yaw=(a.y_rot()-b.y_rot()+180.).rem_euclid(360.)-180.;yaw.abs()>0.0001||(a.x_rot()-b.x_rot()).abs()>0.0001
}
pub fn send_use(commands:&mut Commands,entity:Entity,hand:InteractionHand,seq:u32,camera:LookDirection,aim:Option<LookDirection>,flags:MoveFlags){
 let action=aim.unwrap_or(camera);
 commands.trigger(SendGamePacketEvent::new(entity,ServerboundUseItem{hand,seq,x_rot:action.x_rot(),y_rot:action.y_rot()}));
 // The official server applies UseItem rotation before Item.use. Restore its
 // orientation afterward without changing local LookDirection/last-sent state.
 if differs(action,camera){commands.trigger(SendGamePacketEvent::new(entity,ServerboundMovePlayerRot{look_direction:camera,flags}));}
}
