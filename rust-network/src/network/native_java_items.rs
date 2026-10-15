//! Typed Java inventory summaries; visual IDs map only names present in original TCPE assets.
#[path="native_java_potion_colors.rs"]
mod potion_colors;
#[path="native_java_shields.rs"]
pub(super) mod shields;
use azalea_inventory::{components, ItemStack};
use std::{collections::BTreeMap, sync::OnceLock};
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct ItemSnapshot {
    pub kind: String,
    pub resource_id: String,
    pub name: String,
    pub lore: Vec<String>,
    pub count: i32,
    pub damage: i32,
    pub max_damage: i32,
    pub original_id: Option<i16>,
    pub original_data: i16,
    pub armor: f32,
    pub dyed_color: Option<i32>,
    pub potion_color: Option<i32>,
    pub shield:Option<shields::Shield>,
    pub lodestone_tracker:Option<super::native_java_compass::LodestoneSnapshot>,
    pub profile: Option<ItemProfileSnapshot>,
    pub item_model: Option<String>,
    pub custom_model_data: Option<CustomModelDataSnapshot>,
    pub charged_projectile:String,
}
/// Component presence is distinct from an unresolved/empty profile. Resource patches
/// are pack identifiers, never network addresses.
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct ItemProfileSnapshot {
 pub present: bool, pub full: bool, pub name: Option<String>, pub uuid: Option<String>,
 pub skin_url: Option<String>, pub slim: bool, pub skin_patch: ItemSkinPatchSnapshot,
}
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct ItemSkinPatchSnapshot {
 pub texture: Option<String>, pub cape: Option<String>, pub elytra: Option<String>, pub model: Option<String>,
}
fn profile_snapshot(value: &components::Profile) -> ItemProfileSnapshot {
 let (full,name,uuid,properties)=match value.unpack.as_ref() {
  components::PartialOrFullProfile::Partial(v)=>(false,v.name.clone(),v.id.map(|id|id.to_string()),&v.properties),
  components::PartialOrFullProfile::Full(v)=>(true,Some(v.name.clone()),Some(v.uuid.to_string()),v.properties.as_ref()),
 };
 let decoded=properties.map.get("textures").and_then(|p|super::player_textures::decode(&p.value));
 let patch=value.skin_patch.as_ref();
 ItemProfileSnapshot{present:true,full,name,uuid,skin_url:decoded.as_ref().and_then(|d|d.skin_url.clone()),slim:decoded.as_ref().is_some_and(|d|d.slim),skin_patch:ItemSkinPatchSnapshot{
 texture:patch.body.as_ref().map(|v|v.id.to_string()),cape:patch.cape.as_ref().map(|v|v.id.to_string()),elytra:patch.elytra.as_ref().map(|v|v.id.to_string()),model:patch.model.map(|v|match v{components::PlayerModelType::Wide=>"wide",components::PlayerModelType::Slim=>"slim"}.into())}}
}
/// Keep indexed values intact; malformed or oversized components fall back to the base item.
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct CustomModelDataSnapshot {
    pub floats: Vec<f32>,
    pub flags: Vec<bool>,
    pub strings: Vec<String>,
    pub colors: Vec<i32>,
}
fn model_identifier(value: &str) -> Option<String> {
    let (namespace, path) = value.split_once(':')?;
    if value.len() > 256 || namespace.is_empty() || path.is_empty()
        || !namespace.bytes().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || b"_.-".contains(&c))
        || !path.bytes().all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || b"/._-".contains(&c))
        || path.split('/').any(|part| part == "..") { return None; }
    Some(value.to_owned())
}
fn model_data(value: &components::CustomModelData) -> Option<CustomModelDataSnapshot> {
    const MAX_VALUES: usize = 1024;
    if [value.floats.len(), value.flags.len(), value.strings.len(), value.colors.len()]
        .iter().any(|n| *n > MAX_VALUES)
        || value.floats.iter().any(|v| !v.is_finite())
        || value.strings.iter().any(|v| v.len() > 2048)
        || value.strings.iter().map(String::len).sum::<usize>() > 65536 { return None; }
    Some(CustomModelDataSnapshot { floats: value.floats.clone(), flags: value.flags.clone(),
        strings: value.strings.clone(), colors: value.colors.clone() })
}
fn visual(kind: &str) -> Option<(i16, i16)> {
    static MAP: OnceLock<BTreeMap<String, (i16, i16)>> = OnceLock::new();
    let map = MAP.get_or_init(|| {
        let mut map = BTreeMap::new();
        for line in include_str!("../ui-item-registry.txt").lines() {
            let fields: Vec<_> = line.split('\t').collect();
            if fields.len() < 6 {
                continue;
            }
            let key = fields[5]
                .chars()
                .filter(char::is_ascii_alphanumeric)
                .flat_map(char::to_lowercase)
                .collect::<String>();
            if let (Ok(id), Ok(data)) = (fields[0].parse::<i16>(), fields[1].parse::<i16>()) {
                map.entry(key).or_insert((id, data));
            }
        }
        map
    });
    let key = kind.to_ascii_lowercase();
    map.get(&key)
        .copied()
        .or_else(|| {
            map.get(&key.replace("golden", "gold").replace("chainmail", "chain"))
                .copied()
        })
        .or_else(|| {
            for (prefix, base) in [
                ("Leather", 298),
                ("Chainmail", 302),
                ("Iron", 306),
                ("Diamond", 310),
                ("Golden", 314),
            ] {
                if let Some(suffix) = kind.strip_prefix(prefix) {
                    if let Some(slot) = ["Helmet", "Chestplate", "Leggings", "Boots"]
                        .iter()
                        .position(|name| *name == suffix)
                    {
                        return Some((base + slot as i16, 0));
                    }
                }
            }
            None
        })
}
pub fn snapshot(item: &ItemStack) -> Option<ItemSnapshot> {
    let ItemStack::Present(item) = item else {
        return None;
    };
    let kind = format!("{:?}", item.kind);
    let visual = visual(&kind);
    let armor = item
        .get_component::<components::AttributeModifiers>()
        .map(|a| {
            a.modifiers
                .iter()
                .filter(|m| format!("{:?}", m.kind) == "Armor")
                .map(|m| m.modifier.amount as f32)
                .sum()
        })
        .unwrap_or(0.);
    Some(ItemSnapshot {
        resource_id: item.kind.to_string(),
        charged_projectile:item.get_component::<components::ChargedProjectiles>().filter(|v|!v.items.is_empty()).map(|v|if v.items.iter().any(|s|matches!(s,ItemStack::Present(i) if i.kind==azalea_registry::builtin::ItemKind::FireworkRocket)){"rocket"}else{"arrow"}).unwrap_or("none").into(),
        name: item
            .get_component::<components::CustomName>()
            .map(|n| super::rich_text::styled_text(&n.name))
            .or_else(|| {
                item.get_component::<components::ItemName>()
                    .map(|n| super::rich_text::styled_text(&n.name))
            })
            .unwrap_or_else(|| kind.clone()),
        lore: item.get_component::<components::Lore>().map(|value|value.lines.iter().take(32).map(|line|super::rich_text::styled_text(line).chars().take(2048).collect()).collect()).unwrap_or_default(),
        kind,
        count: item.count,
        damage: item
            .get_component::<components::Damage>()
            .map(|d| d.amount)
            .unwrap_or(0),
        max_damage: item
            .get_component::<components::MaxDamage>()
            .map(|d| d.amount)
            .unwrap_or(0),
        original_id: visual.map(|v| v.0),
        original_data: visual.map(|v| v.1).unwrap_or(0),
        armor,
        shield:shields::snapshot(item),
        dyed_color: item.get_component::<components::DyedColor>().map(|c| (c.rgb as u32&0xffffff)as i32),
        potion_color:item.get_component::<components::PotionContents>().and_then(|v|potion_colors::resolve(&v)),
        lodestone_tracker:item.get_component::<components::LodestoneTracker>().map(|v|super::native_java_compass::LodestoneSnapshot{target:v.target.as_ref().map(super::native_java_compass::GlobalTarget::from),tracked:v.tracked}),
        profile: item.get_component::<components::Profile>().map(|v|profile_snapshot(&v)),
        item_model: item.get_component::<components::ItemModel>().and_then(|v| model_identifier(&v.resource_location.to_string())),
        custom_model_data: item.get_component::<components::CustomModelData>().and_then(|value| model_data(&value)),
    })
}
impl ItemSnapshot {
    pub fn label(&self) -> String {
        format!("{} x{}", self.name, self.count)
    }
}

#[cfg(test)]mod popup_lore_tests{use super::*;
 #[test]fn popup_items_keep_actual_server_lore_colors_and_line_limits(){
  let line:azalea_chat::FormattedText=serde_json::from_value(serde_json::json!({"text":"Join Practice","color":"#123450","bold":true})).unwrap();
  let item=ItemStack::new(azalea_registry::builtin::ItemKind::DiamondSword,1).with_component(components::Lore{lines:vec![line;40]});let value=snapshot(&item).unwrap();assert_eq!(value.lore.len(),32);assert_eq!(value.count,1);assert_eq!(value.resource_id,"minecraft:diamond_sword");assert_eq!(value.lore[0],"§r§x§1§2§3§4§5§0§lJoin Practice");
 }
}

#[cfg(test)]
#[path = "native_java_item_models_tests.rs"]
mod item_model_tests;
#[cfg(test)]mod potion_color_snapshot_tests{
use super::*;
#[test]fn authoritative_item_colors_survive_snapshot_and_keep_absent_defaults(){
 let base=ItemStack::new(azalea_registry::builtin::ItemKind::PlayerHead,1);
 let mut potion=components::PotionContents::new();potion.custom_color=Some(0x123456);
 let item=base.clone().with_component(components::DyedColor{rgb:0x654321}).with_component(potion);
 let value=snapshot(&item).unwrap();assert_eq!(value.dyed_color,Some(0x654321));assert_eq!(value.potion_color,Some(0x123456));
 let json=serde_json::to_value(value).unwrap();assert_eq!(json["potion_color"],0x123456);assert_eq!(json["dyed_color"],0x654321);
 let empty=snapshot(&base).unwrap();assert_eq!(empty.potion_color,None);assert_eq!(empty.dyed_color,None);
}
}

#[cfg(test)]mod compass_item_tests{use super::*;#[test]fn present_null_lodestone_target_remains_distinct_from_absent_tracker(){let base=ItemStack::new(azalea_registry::builtin::ItemKind::Compass,1);assert!(snapshot(&base).unwrap().lodestone_tracker.is_none());let item=base.with_component(components::LodestoneTracker{target:None,tracked:false});let value=snapshot(&item).unwrap();let tracker=value.lodestone_tracker.as_ref().unwrap();assert!(tracker.target.is_none()&&!tracker.tracked);let json=serde_json::to_value(value).unwrap();assert!(json["lodestone_tracker"].is_object());assert!(json["lodestone_tracker"]["target"].is_null());assert_eq!(json["lodestone_tracker"]["tracked"],false);}}
#[cfg(test)]mod item_profile_tests{
 use super::*;
 #[test]fn absent_empty_partial_full_and_resource_patch_remain_distinct(){
  let base=ItemStack::new(azalea_registry::builtin::ItemKind::PlayerHead,1);
  assert!(snapshot(&base).unwrap().profile.is_none());
  let mut component=components::Profile::default();
  let empty=snapshot(&base.clone().with_component(component.clone())).unwrap().profile.unwrap();assert!(empty.present&&!empty.full);assert!(empty.skin_url.is_none()&&empty.name.is_none());
  let components::PartialOrFullProfile::Partial(partial)=component.unpack.as_mut() else {panic!()};partial.name=Some("ActualName".into());
  component.skin_patch.body=Some(components::ResourceTexture{id:"custom:heads/boss".parse().unwrap()});
  let value=snapshot(&base.clone().with_component(component)).unwrap().profile.unwrap();assert_eq!(value.name.as_deref(),Some("ActualName"));assert_eq!(value.skin_patch.texture.as_deref(),Some("custom:heads/boss"));assert!(value.skin_url.is_none());
  let full=components::Profile{unpack:Box::new(components::PartialOrFullProfile::Full(Default::default())),skin_patch:Default::default()};assert!(snapshot(&base.with_component(full)).unwrap().profile.unwrap().full);
 }
}

#[cfg(test)]mod mace_snapshot_tests {
 use super::*;
 #[test]fn mace_keeps_modern_identity_and_durability_without_legacy_alias() {
  let item=ItemStack::new(azalea_registry::builtin::ItemKind::Mace,1)
   .with_component(components::Damage{amount:17})
   .with_component(components::MaxDamage{amount:500});
  let value=snapshot(&item).unwrap();
  assert_eq!(value.kind,"Mace");assert_eq!(value.resource_id,"minecraft:mace");
  assert_eq!(value.original_id,None);assert_eq!(value.count,1);
  assert_eq!(value.damage,17);assert_eq!(value.max_damage,500);
  let json=serde_json::to_value(value).unwrap();assert_eq!(json["resource_id"],"minecraft:mace");assert!(json["original_id"].is_null());
 }
}
