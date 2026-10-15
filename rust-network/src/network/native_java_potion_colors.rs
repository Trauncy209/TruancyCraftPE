//! Java 1.21.11 potion tint rules, derived from the official client bytecode.
//! Data: MobEffects/Potions static registrations, PotionContents.getColorOptional.
use azalea_inventory::components;
use std::{collections::BTreeMap,sync::OnceLock};
#[derive(serde::Deserialize)]struct BaseEffect{effect:String,amplifier:i32}
#[derive(serde::Deserialize)]struct Data{mob_effect_colors:BTreeMap<String,u32>,potions:BTreeMap<String,Vec<BaseEffect>>}
fn data()->&'static Data{static DATA:OnceLock<Data>=OnceLock::new();DATA.get_or_init(||serde_json::from_str(include_str!("native_java_potion_data.json")).expect("verified official potion tint data"))}
fn id(value:impl std::fmt::Display)->String{let value=value.to_string();value.strip_prefix("minecraft:").unwrap_or(&value).to_owned()}
/// None means the model's tint source must use its own default color.
pub fn resolve(contents:&components::PotionContents)->Option<i32>{
    if let Some(color)=contents.custom_color{return Some((color as u32&0xffffff)as i32)}
    if contents.custom_effects.len()>1024{return None}
    let table=data();let mut sum=[0u64;3];let mut total=0u64;
    let mut add=|effect:&str,amplifier:i32|->Option<()> {let color=*table.mob_effect_colors.get(effect)?;let weight=amplifier.clamp(0,255)as u64+1;for(i,shift)in[16,8,0].into_iter().enumerate(){sum[i]+=weight*((color>>shift)&255)as u64;}total+=weight;Some(())};
    if let Some(potion)=contents.potion{for effect in table.potions.get(&id(potion))?{add(&effect.effect,effect.amplifier)?;}}
    for effect in &contents.custom_effects{if effect.details.show_particles{add(&id(effect.id),effect.details.amplifier)?;}}
    if total==0{return None}Some((((sum[0]/total)<<16)|((sum[1]/total)<<8)|(sum[2]/total))as i32)
}
#[cfg(test)]mod tests{
use super::*;use azalea_registry::{builtin::{MobEffect,Potion},Registry};
fn effect(kind:MobEffect,amplifier:i32,visible:bool)->components::MobEffectInstance{let mut details=components::MobEffectDetails::new();details.amplifier=amplifier;details.show_particles=visible;components::MobEffectInstance{id:kind,details}}
#[test]fn official_data_covers_every_pinned_registry_member(){let table=data();assert_eq!(table.mob_effect_colors.len(),40);assert_eq!(table.potions.len(),46);for n in 0..40{assert!(table.mob_effect_colors.contains_key(&id(MobEffect::from_u32(n).unwrap())));}for n in 0..46{let potion=Potion::from_u32(n).unwrap();for e in table.potions.get(&id(potion)).unwrap(){assert!(table.mob_effect_colors.contains_key(&e.effect));}}assert!(MobEffect::from_u32(40).is_none()&&Potion::from_u32(46).is_none());}
#[test]fn custom_color_wins_empty_effects_use_model_default(){let mut c=components::PotionContents::new();assert_eq!(resolve(&c),None);c.potion=Some(Potion::Water);assert_eq!(resolve(&c),None);c.potion=Some(Potion::Awkward);assert_eq!(resolve(&c),None);c.potion=Some(Potion::Healing);assert_eq!(resolve(&c),Some(0xf82423));c.custom_color=Some(0x123456);assert_eq!(resolve(&c),Some(0x123456));c.custom_color=Some(-1);assert_eq!(resolve(&c),Some(0xffffff));}
#[test]fn mixed_visible_effects_match_integer_amplifier_weighting(){let mut c=components::PotionContents::new();c.potion=Some(Potion::TurtleMaster);assert_eq!(resolve(&c),Some(0x8d82e6));c.potion=Some(Potion::StrongTurtleMaster);assert_eq!(resolve(&c),Some(0x8d85e6));c.potion=Some(Potion::Healing);c.custom_effects=vec![effect(MobEffect::InstantDamage,2,true),effect(MobEffect::Poison,255,false)];let healing=data().mob_effect_colors["instant_health"];let damage=data().mob_effect_colors["instant_damage"];let expected=[16,8,0].into_iter().map(|shift|((((healing>>shift)&255)+3*((damage>>shift)&255))/4)<<shift).sum::<u32>();assert_eq!(resolve(&c),Some(expected as i32));c.custom_effects[0].details.ambient=true;c.custom_effects[0].details.show_icon=false;c.custom_effects[0].details.duration=-1;c.custom_effects[0].details.hidden_effect=Some(Box::new(components::MobEffectDetails::new()));assert_eq!(resolve(&c),Some(expected as i32));}
#[test]fn amplifier_clamp_and_invisible_only_effects_are_safe(){let mut c=components::PotionContents::new();c.custom_effects=vec![effect(MobEffect::Poison,0,false)];assert_eq!(resolve(&c),None);c.custom_effects=vec![effect(MobEffect::Speed,-100,true),effect(MobEffect::Slowness,1000,true)];let clamped=resolve(&c);c.custom_effects[0].details.amplifier=0;c.custom_effects[1].details.amplifier=255;assert_eq!(resolve(&c),clamped);}
}
