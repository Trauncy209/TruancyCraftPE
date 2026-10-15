//! 1.21.11 LivingEntity glide eligibility and updateFallFlyingMovement.
use azalea_core::{math,position::Vec3};
use azalea_entity::{ActiveEffects,LookDirection,Physics,PlayerAbilities,OnClimbable,inventory::Inventory,metadata::{FallFlying,Health}};
use azalea_inventory::{ItemStack,ItemStackData,components::{EquipmentSlot,Glider,Equippable,Damage,MaxDamage,Unbreakable}};
use azalea_registry::builtin::MobEffect;
use bevy_ecs::prelude::*;
/// Actual SetPassengers relationship for this local connection.
#[derive(Component,Clone,Copy,Debug,PartialEq,Eq)]
pub struct Riding(pub i32);
fn component<T:azalea_inventory::components::DataComponentTrait>(item:&ItemStackData)->Option<std::borrow::Cow<'_,T>>{
 if item.component_patch.iter().any(|(kind,value)|kind==T::KIND&&value.is_none()){None}else{item.get_component::<T>()}
}
/// LivingEntity.canGlideUsing: component-defined equipment, not an Elytra item-ID whitelist.
pub fn can_glide_using(stack:&ItemStack,slot:EquipmentSlot)->bool{
 let ItemStack::Present(item)=stack else{return false;};
 if component::<Glider>(item).is_none()||component::<Equippable>(item).is_none_or(|e|e.slot!=slot){return false;}
 // ItemStack.nextDamageWillBreak is true only for damageable, breakable items.
 let damageable=component::<MaxDamage>(item).filter(|m|m.amount>0).filter(|_|component::<Unbreakable>(item).is_none()&&component::<Damage>(item).is_some());
 !damageable.is_some_and(|max|component::<Damage>(item).map_or(0,|d|d.amount).clamp(0,max.amount)>=max.amount-1)
}
pub fn has_gliding_equipment(inv:&Inventory)->bool{
 EquipmentSlot::values().into_iter().any(|slot|inv.get_equipment(slot).is_some_and(|item|can_glide_using(item,slot)))
}
pub fn may_continue(physics:&Physics,effects:Option<&ActiveEffects>,inventory:Option<&Inventory>,riding:Option<&Riding>)->bool{
 !physics.on_ground()&&riding.is_none()&&!effects.is_some_and(|e|e.get(MobEffect::Levitation).is_some())&&inventory.is_some_and(has_gliding_equipment)
}
/// Native startJump caller uses the same equipment/effect relationship as travel.
pub fn can_start_fall_flying(world:&World,entity:Entity)->bool{
 let Some(physics)=world.get::<Physics>(entity)else{return false;};
 !world.get::<FallFlying>(entity).is_some_and(|v|v.0)&&!physics.is_in_water()&&!world.get::<PlayerAbilities>(entity).is_some_and(|a|a.flying)&&!world.get::<OnClimbable>(entity).is_some_and(|c|**c)&&world.get::<Health>(entity).is_none_or(|h|h.0.is_finite()&&h.0>0.)&&may_continue(physics,world.get::<ActiveEffects>(entity),world.get::<Inventory>(entity),world.get::<Riding>(entity))
}
pub fn effective_gravity(gravity:f64,velocity_y:f64,effects:Option<&ActiveEffects>)->f64{
 if velocity_y<=0.&&effects.is_some_and(|e|e.get(MobEffect::SlowFalling).is_some()){gravity.min(0.01)}else{gravity}
}
// Modern Mth uses a double multiply before its long-indexed float sine table.
fn sin(angle:f64)->f32{math::SIN[((angle*10430.378350470453)as i64&65535)as usize]}
fn cos(angle:f64)->f32{math::SIN[((angle*10430.378350470453+16384.)as i64&65535)as usize]}
/// Official Entity.calculateViewVector f32 radians/products, modern Mth double-index LUT.
pub fn glide_view_vector(direction:LookDirection)->Vec3{
 let pitch=direction.x_rot()*0.017453292_f32;let yaw=-direction.y_rot()*0.017453292_f32;
 Vec3::new((sin(yaw as f64)*cos(pitch as f64))as f64,(-sin(pitch as f64))as f64,(cos(yaw as f64)*cos(pitch as f64))as f64)
}
/// Exact arithmetic/order/constants from official LivingEntity.updateFallFlyingMovement.
pub fn flight_velocity(mut velocity:Vec3,direction:LookDirection,gravity:f64)->Vec3{
 let pitch=direction.x_rot()*0.017453292_f32;let look=glide_view_vector(direction);
 let horizontal_look=(look.x*look.x+look.z*look.z).sqrt();let horizontal_speed=(velocity.x*velocity.x+velocity.z*velocity.z).sqrt();let lift=(pitch as f64).cos().powi(2);
 velocity.y+=gravity*(-1.+lift*0.75);
 if velocity.y<0.&&horizontal_look>0.{let fall=velocity.y*(-0.1)*lift;velocity.x+=look.x*fall/horizontal_look;velocity.y+=fall;velocity.z+=look.z*fall/horizontal_look;}
 if pitch<0.&&horizontal_look>0.{let climb=horizontal_speed*(-sin(pitch as f64)as f64)*0.04;velocity.x+=(-look.x)*climb/horizontal_look;velocity.y+=climb*3.2;velocity.z+=(-look.z)*climb/horizontal_look;}
 if horizontal_look>0.{velocity.x+=(look.x/horizontal_look*horizontal_speed-velocity.x)*0.1;velocity.z+=(look.z/horizontal_look*horizontal_speed-velocity.z)*0.1;}
 Vec3::new(velocity.x*0.9900000095367432,velocity.y*0.9800000190734863,velocity.z*0.9900000095367432)
}
