//! Exact menu identity and protocol slot ranges, independent of titles or contents.
use azalea_inventory::Menu;
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct SlotRange{pub start:usize,pub end:usize}
#[derive(Debug,Clone,PartialEq,Default,serde::Serialize)]pub struct SlotRanges{
 pub container:Option<SlotRange>,pub player_inventory:Option<SlotRange>,pub hotbar:Option<SlotRange>,
 pub crafting:Option<SlotRange>,pub ingredient:Option<SlotRange>,pub fuel:Option<SlotRange>,
 pub result:Option<SlotRange>,pub armor:Option<SlotRange>,pub offhand:Option<SlotRange>,
}
pub fn kind(menu:&Menu)->&'static str{match menu{
 Menu::Player(_)=>"player",Menu::Crafting{..}=>"crafting",Menu::Furnace{..}=>"furnace",Menu::BlastFurnace{..}=>"blast_furnace",Menu::Smoker{..}=>"smoker",
 Menu::Generic9x1{..}=>"generic_9x1",Menu::Generic9x2{..}=>"generic_9x2",Menu::Generic9x3{..}=>"generic_9x3",Menu::Generic9x4{..}=>"generic_9x4",Menu::Generic9x5{..}=>"generic_9x5",Menu::Generic9x6{..}=>"generic_9x6",Menu::Generic3x3{..}=>"generic_3x3",Menu::Crafter3x3{..}=>"crafter_3x3",Menu::Anvil{..}=>"anvil",Menu::Beacon{..}=>"beacon",Menu::BrewingStand{..}=>"brewing_stand",Menu::Enchantment{..}=>"enchantment",Menu::Grindstone{..}=>"grindstone",Menu::Hopper{..}=>"hopper",Menu::Lectern{..}=>"lectern",Menu::Loom{..}=>"loom",Menu::Merchant{..}=>"merchant",Menu::ShulkerBox{..}=>"shulker_box",Menu::Smithing{..}=>"smithing",Menu::CartographyTable{..}=>"cartography_table",Menu::Stonecutter{..}=>"stonecutter",
}}
pub fn ranges(menu:&Menu)->SlotRanges{
 let count=menu.len();let range=|start,end|if start<end&&end<=count{Some(SlotRange{start,end})}else{None};
 let player=menu.player_slots_range();let hotbar=menu.hotbar_slots_range();
 let mut ranges=SlotRanges{container:range(0,*player.start()),player_inventory:range(*player.start(),*hotbar.start()),hotbar:range(*hotbar.start(),*hotbar.end()+1),..Default::default()};
 match menu{
 Menu::Player(_)=>{ranges.crafting=range(1,5);ranges.result=range(0,1);ranges.armor=range(5,9);ranges.offhand=range(45,46);},
 Menu::Crafting{..}=>{ranges.crafting=range(1,10);ranges.result=range(0,1);},
 Menu::Furnace{..}|Menu::BlastFurnace{..}|Menu::Smoker{..}=>{ranges.ingredient=range(0,1);ranges.fuel=range(1,2);ranges.result=range(2,3);},
 _=>{}
 }ranges
}
#[cfg(test)]mod tests{
 use super::*;use azalea_registry::builtin::{MenuKind,ItemKind};
 fn pair(range:&Option<SlotRange>)->Option<(usize,usize)>{range.as_ref().map(|v|(v.start,v.end))}
 #[test]fn player_slots_preserve_crafting_armor_offhand_and_hotbar(){let menu=Menu::Player(Default::default());let r=ranges(&menu);assert_eq!(kind(&menu),"player");assert_eq!(menu.len(),46);assert_eq!(pair(&r.crafting),Some((1,5)));assert_eq!(pair(&r.result),Some((0,1)));assert_eq!(pair(&r.armor),Some((5,9)));assert_eq!(pair(&r.offhand),Some((45,46)));assert_eq!(pair(&r.player_inventory),Some((9,36)));assert_eq!(pair(&r.hotbar),Some((36,45)));assert_eq!(serde_json::to_value(r).unwrap()["offhand"],serde_json::json!({"start":45,"end":46}));}
 #[test]fn crafting_and_furnaces_expose_empty_tappable_semantic_slots(){let menu=Menu::from_kind(MenuKind::Crafting);let r=ranges(&menu);assert_eq!(kind(&menu),"crafting");assert_eq!(pair(&r.container),Some((0,10)));assert_eq!(pair(&r.crafting),Some((1,10)));assert_eq!(pair(&r.result),Some((0,1)));assert_eq!(pair(&r.player_inventory),Some((10,37)));assert_eq!(pair(&r.hotbar),Some((37,46)));for menu_kind in[MenuKind::Furnace,MenuKind::BlastFurnace,MenuKind::Smoker]{let menu=Menu::from_kind(menu_kind);let r=ranges(&menu);assert_eq!(menu.len(),39);assert_eq!(pair(&r.ingredient),Some((0,1)));assert_eq!(pair(&r.fuel),Some((1,2)));assert_eq!(pair(&r.result),Some((2,3)));assert_eq!(pair(&r.player_inventory),Some((3,30)));assert_eq!(pair(&r.hotbar),Some((30,39)));}}
 #[test]fn generic_plugin_menus_are_not_inferred_from_occupied_slots(){let mut menu=Menu::from_kind(MenuKind::Generic9x3);let before=ranges(&menu);*menu.slot_mut(0).unwrap()=azalea_inventory::ItemStack::new(ItemKind::Furnace,1);assert_eq!(kind(&menu),"generic_9x3");assert_eq!(ranges(&menu),before);assert_eq!(pair(&before.container),Some((0,27)));assert!(before.result.is_none()&&before.crafting.is_none()&&before.fuel.is_none());let generic3=Menu::from_kind(MenuKind::Generic3x3);assert_eq!(kind(&generic3),"generic_3x3");assert!(ranges(&generic3).crafting.is_none());}
}
