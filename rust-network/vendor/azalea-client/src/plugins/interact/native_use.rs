//! Native opt-in vanilla item outcome prediction. Inventory/effects remain server-owned.
use azalea_inventory::{ItemStack,components::{Consumable,Food,BlocksAttacks,KineticWeapon,Equippable,ChargedProjectiles,Damage,MaxDamage}};
use azalea_protocol::packets::game::s_interact::InteractionHand;
use azalea_entity::inventory::Inventory;
use bevy_ecs::prelude::*;
pub mod official { include!("official_use_classes.rs"); }
#[derive(Debug,Clone,Copy,PartialEq,Eq)]
pub enum Outcome { Pass, Fail, Consume{duration:u32}, Unknown }
#[derive(Debug,Clone,Copy,Default)]
pub struct Context {pub invulnerable:bool,pub creative:bool,pub food:u32,pub wet:bool,pub riptide:bool}
#[derive(Component,Clone)]
pub struct NativeUseOptions {pub automatic:bool,pub main:Outcome,pub off:Outcome,pub main_stack:ItemStack,pub off_stack:ItemStack}
impl NativeUseOptions {pub fn outcome(&self,hand:InteractionHand)->Outcome{if hand==InteractionHand::OffHand{self.off}else{self.main}}pub fn stack(&self,hand:InteractionHand)->&ItemStack{if hand==InteractionHand::OffHand{&self.off_stack}else{&self.main_stack}}}
#[derive(Component,Clone)]
pub struct PredictedNativeUse {pub hand:InteractionHand,pub stack:ItemStack,pub duration:u32,pub ticks:u32}
fn arrow(stack:&ItemStack)->bool{matches!(stack,ItemStack::Present(i) if matches!(i.kind.to_string().as_str(),"minecraft:arrow"|"minecraft:spectral_arrow"|"minecraft:tipped_arrow"))}
fn ammo(inv:&Inventory,crossbow:bool,creative:bool)->bool{
 // Vanilla allows held fireworks for crossbows, but inventory search uses arrow-only.
 for slot in [45,36+inv.selected_hotbar_slot as usize]{if let Some(s)=inv.inventory_menu.slot(slot){if arrow(s)||(crossbow&&matches!(s,ItemStack::Present(i) if i.kind.to_string()=="minecraft:firework_rocket")){return true;}}}
 creative||(9..45).any(|slot|inv.inventory_menu.slot(slot).is_some_and(arrow))
}
pub fn item_outcome(stack:&ItemStack,inv:&Inventory,context:Context)->Outcome{
 let ItemStack::Present(item)=stack else{return Outcome::Pass;};let id=item.kind.to_string();
 match id.as_str(){
  "minecraft:bow"=>return if ammo(inv,false,context.creative){Outcome::Consume{duration:72000}}else{Outcome::Fail},
  "minecraft:crossbow"=>return if item.get_component::<ChargedProjectiles>().is_some_and(|c|!c.items.is_empty()){Outcome::Consume{duration:0}}else if ammo(inv,true,context.creative){Outcome::Consume{duration:72000}}else{Outcome::Fail},
  "minecraft:trident"=>{let broken=item.get_component::<MaxDamage>().zip(item.get_component::<Damage>()).is_some_and(|(m,d)|d.amount>=m.amount-1);return if broken||(context.riptide&&!context.wet){Outcome::Fail}else{Outcome::Consume{duration:72000}};},
  "minecraft:spyglass"=>return Outcome::Consume{duration:1200},
  _=>{}
 }
 if let Some(c)=item.get_component::<Consumable>(){
  if item.get_component::<Food>().is_some_and(|food|!food.can_always_eat&&!context.invulnerable&&context.food>=20){return Outcome::Fail;}
  return if c.consume_seconds.is_finite()&&c.consume_seconds>=0.{Outcome::Consume{duration:((c.consume_seconds*20.) as i32).max(0) as u32}}else{Outcome::Unknown};
 }
 // Base Item.use prioritizes Consumable, swappable Equippable, BlocksAttacks,
 // then KineticWeapon. Equipping consumes instantly rather than starting a pose.
 if item.get_component::<Equippable>().is_some_and(|e|e.swappable){return Outcome::Consume{duration:0};}
 if item.get_component::<BlocksAttacks>().is_some()||item.get_component::<KineticWeapon>().is_some(){return Outcome::Consume{duration:72000};}
 if official::DEFAULT_ITEM_USE.binary_search(&id.as_str()).is_ok(){Outcome::Pass}else{Outcome::Unknown}
}
pub fn default_block_pass(block:&str,item:&ItemStack)->bool{
 official::DEFAULT_BLOCK_USE.binary_search(&block).is_ok()&&match item{ItemStack::Empty=>true,ItemStack::Present(i)=>official::DEFAULT_ITEM_USE_ON.binary_search(&i.kind.to_string().as_str()).is_ok()}
}
pub fn predict(commands:&mut Commands,entity:Entity,options:&NativeUseOptions,hand:InteractionHand){
 if let Outcome::Consume{duration}=options.outcome(hand){if duration>0{commands.entity(entity).insert((PredictedNativeUse{hand,stack:options.stack(hand).clone(),duration,ticks:0},azalea_entity::metadata::AbstractLivingUsingItem(true),azalea_entity::metadata::AbstractLivingUsingOffHand(hand==InteractionHand::OffHand)));}}
}

#[derive(Component,Clone,Copy)]
pub struct NativeTargetMaxHealth {pub owner:Entity,pub value:f32}
pub fn entity_pass(kind:Option<azalea_registry::builtin::EntityKind>,stack:&ItemStack,health:Option<(f32,f32)>)->bool{
 if default_entity_pass(kind,stack){return true;}
 if kind!=Some(azalea_registry::builtin::EntityKind::IronGolem)||!matches!(stack,ItemStack::Present(i) if i.kind==azalea_registry::builtin::ItemKind::IronIngot){return false;}
 // LivingEntity.heal changes only positive health; clamps to max-health.
 health.is_some_and(|(current,max)|current.is_finite()&&max.is_finite()&&max>0.&&(current<=0.||(current+25.).min(max)==current))
}
pub fn default_entity_pass(kind:Option<azalea_registry::builtin::EntityKind>,stack:&ItemStack)->bool{
 let Some(kind)=kind else{return false;};if kind==azalea_registry::builtin::EntityKind::Player{return true;}
 // Official IronGolem.mobInteract immediately passes for a non-ingot.
 // Ingot healing is conditional on actual health, so keep it server-owned
 // until that target health context is available; never predict healing.
 let golem_pass=kind==azalea_registry::builtin::EntityKind::IronGolem&&!matches!(stack,ItemStack::Present(i) if i.kind==azalea_registry::builtin::ItemKind::IronIngot);
 (golem_pass||official::DEFAULT_MOB_INTERACT.binary_search(&kind.to_string().as_str()).is_ok())&&match stack{ItemStack::Empty=>true,ItemStack::Present(i)=>official::DEFAULT_MOB_ITEM_INTERACT.binary_search(&i.kind.to_string().as_str()).is_ok()}
}
