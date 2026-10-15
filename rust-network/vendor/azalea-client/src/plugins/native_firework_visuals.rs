//! NEXT client-only visual capture. No entity creation, damage, motion or inventory prediction.
use azalea_core::position::Vec3;
use azalea_entity::{EntityKindComponent,LoadedBy,LookDirection,Physics,Position,HumanoidArm,indexing::EntityIdIndex,inventory::Inventory,metadata::{FireworkRocket,FireworksItem,AttachedToTarget,PlayerMainHand}};
use azalea_inventory::{ItemStack,components::{Fireworks,FireworkExplosion,EquipmentSlot}};
use azalea_protocol::packets::game::ClientboundSetEquipment;
use azalea_registry::builtin::{EntityKind,ItemKind};
use azalea_world::{InstanceName,MinecraftEntityId};
use bevy_ecs::prelude::*;
use std::collections::{HashSet,VecDeque};
#[derive(Clone,Debug)]pub struct Explosion{pub position:Vec3,pub velocity:Vec3,pub explosions:Vec<FireworkExplosion>}
#[derive(Component,Default)]pub struct Queue{pub pending:VecDeque<Explosion>,seen:HashSet<Entity>,order:VecDeque<Entity>}
#[derive(Component,Default)]struct Hands{main:ItemStack,off:ItemStack}
fn target(world:&World,local:Entity,id:MinecraftEntityId)->Option<Entity>{world.get::<EntityIdIndex>(local)?.get_by_minecraft_entity(id)}
pub fn equipment(world:&mut World,local:Entity,packet:&ClientboundSetEquipment){
 let Some(entity)=target(world,local,packet.entity_id)else{return;};let mut hands=world.entity_mut(entity).take::<Hands>().unwrap_or_default();
 for (slot,item) in &packet.slots.slots{match slot{EquipmentSlot::Mainhand=>hands.main=item.clone(),EquipmentSlot::Offhand=>hands.off=item.clone(),_=>{}}}
 world.entity_mut(entity).insert(hands);
}
fn rocket(item:&ItemStack)->bool{matches!(item,ItemStack::Present(i) if i.kind==ItemKind::FireworkRocket)}
/// Exact base Entity.getHandHoldingItemAngle: players only, offhand only if the
/// rocket is offhand and not mainhand; arm +/-80 degrees, half-block offset.
fn pose(world:&World,local:Entity,entity:Entity)->Option<(Vec3,Vec3)>{
 let mut pos=**world.get::<Position>(entity)?;let mut velocity=world.get::<Physics>(entity)?.velocity;
 if let Some(id)=world.get::<AttachedToTarget>(entity).and_then(|a|a.0.0).and_then(|id|i32::try_from(id).ok()){
  if let Some(owner)=target(world,local,MinecraftEntityId(id)).filter(|owner|world.get::<InstanceName>(*owner)==world.get::<InstanceName>(entity)){
   if let(Some(p),Some(physics))=(world.get::<Position>(owner),world.get::<Physics>(owner)){
    pos=**p;velocity=physics.velocity;
    if world.get::<EntityKindComponent>(owner).is_some_and(|kind|kind.0==EntityKind::Player){
     let off_only=if let Some(inv)=world.get::<Inventory>(owner){inv.get_equipment(EquipmentSlot::Offhand).is_some_and(rocket)&&!rocket(inv.held_item())}else{world.get::<Hands>(owner).is_some_and(|h|rocket(&h.off)&&!rocket(&h.main))};
     let main_right=world.get::<PlayerMainHand>(owner).is_none_or(|arm|arm.0==HumanoidArm::Right);let right=main_right!=off_only;
     let yaw=world.get::<LookDirection>(owner).map(|l|l.y_rot()).unwrap_or(0.)+if right{80.}else{-80.};
     let offset=azalea_physics::elytra::glide_view_vector(LookDirection::new(yaw,0.));pos.x+=offset.x*0.5;pos.z+=offset.z*0.5;
    }
   }
  }
 }
 if [pos.x,pos.y,pos.z,velocity.x,velocity.y,velocity.z].iter().any(|v|!v.is_finite())||[pos.x,pos.y,pos.z].iter().any(|v|v.abs()>30_000_000.){return None;}Some((pos,velocity))
}
pub fn status(world:&mut World,local:Entity,id:MinecraftEntityId,status:u8){
 if status!=17{return;}let Some(entity)=target(world,local,id)else{return;};
 if world.get::<InstanceName>(entity)!=world.get::<InstanceName>(local)||world.get::<FireworkRocket>(entity).is_none()||!world.get::<LoadedBy>(entity).is_some_and(|loaded|loaded.contains(&local)){return;}
 if world.get::<Queue>(local).is_some_and(|q|q.seen.contains(&entity)){return;}
 let Some((position,velocity))=pose(world,local,entity)else{return;};
 let Some(FireworksItem(ItemStack::Present(item)))=world.get::<FireworksItem>(entity)else{return;};let Some(component)=item.get_component::<Fireworks>()else{return;};
 // The wire allows at most256 explosions. Reject excessive per-color vectors,
 // rather than silently changing authored color probabilities.
 if component.explosions.is_empty()||component.explosions.len()>256||component.explosions.iter().any(|e|e.colors.len()>256||e.fade_colors.len()>256){return;}
 let event=Explosion{position,velocity,explosions:component.explosions.clone()};
 if world.get::<Queue>(local).is_none(){world.entity_mut(local).insert(Queue::default());}
 let mut queue=world.get_mut::<Queue>(local).unwrap();if queue.pending.len()>=256{return;}queue.seen.insert(entity);queue.order.push_back(entity);if queue.order.len()>4096{if let Some(old)=queue.order.pop_front(){queue.seen.remove(&old);}}queue.pending.push_back(event);
}
pub fn drain(world:&mut World,local:Entity)->Vec<Explosion>{world.get_mut::<Queue>(local).map(|mut queue|queue.pending.drain(..).collect()).unwrap_or_default()}
pub fn clear(world:&mut World,local:Entity){if let Ok(mut local)=world.get_entity_mut(local){local.remove::<Queue>();}}
pub fn trails(world:&mut World,local:Entity)->Vec<(Vec3,Vec3)>{
 let ids=world.query_filtered::<(Entity,&LoadedBy),With<FireworkRocket>>().iter(world).filter(|(entity,loaded)|world.get::<InstanceName>(*entity)==world.get::<InstanceName>(local)&&loaded.contains(&local)&&!world.get::<Queue>(local).is_some_and(|q|q.seen.contains(entity))).map(|(entity,_)|entity).take(256).collect::<Vec<_>>();
 ids.into_iter().filter_map(|entity|pose(world,local,entity)).collect()
}
