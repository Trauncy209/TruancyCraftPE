//! Authoritative recipe display book; never synthesize crafting output or ingredients.
use super::super::native_java_items::{snapshot,ItemSnapshot};
use azalea_inventory::{ItemStack,Menu};
use azalea_protocol::{common::recipe::{RecipeDisplayData,SlotDisplayData},packets::game::{ClientboundGamePacket,s_place_recipe::ServerboundPlaceRecipe}};
use std::collections::BTreeMap;
use azalea_registry::{Registry,builtin::ItemKind};
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct Recipe {
 pub id:u32,pub category:String,pub width:u8,pub height:u8,pub kind:&'static str,pub result:ItemSnapshot,
 pub ingredients:Vec<Option<ItemSnapshot>>,pub placeable:bool,pub preview_complete:bool,
}
#[cfg(test)]mod ghost_packet_tests{
 use super::*;use azalea_protocol::{common::recipe::*,packets::game::*};use azalea_registry::builtin::{MenuKind,RecipeBookCategory};
 fn display(kind:ItemKind)->SlotDisplayData{SlotDisplayData::ItemKind(ItemStackDisplay{item:kind})}
 fn shaped(size:u32)->RecipeDisplayData{RecipeDisplayData::Shaped(ShapedCraftingRecipeDisplay{width:size,height:size,ingredients:vec![display(if size==2{ItemKind::OakPlanks}else{ItemKind::IronIngot});(size*size)as usize],result:display(if size==2{ItemKind::CraftingTable}else{ItemKind::IronBlock}),crafting_station:display(ItemKind::CraftingTable)})}
 fn ghost(window:i32,size:u32)->ClientboundGamePacket{let p=ClientboundPlaceGhostRecipe{container_id:window,recipe:shaped(size)};let mut bytes=Vec::new();p.write(&mut bytes).unwrap();ClientboundPlaceGhostRecipe::read(&mut std::io::Cursor::new(bytes.as_slice())).unwrap()}
 fn open(book:&mut Recipes,window:i32,kind:MenuKind){book.ingest(&ClientboundGamePacket::OpenScreen(ClientboundOpenScreen{container_id:window,menu_type:kind,title:azalea_chat::FormattedText::from("Crafting fixture")}));}
 #[test]fn actual_ghost_wire_is_preview_only_and_scoped_to_real_crafting_menu(){
  let mut book=Recipes::default();assert!(book.ingest(&ghost(0,2)));let snap=book.snapshot();assert!(snap.entries.is_empty());assert_eq!(snap.ghost.as_ref().unwrap().result.resource_id,"minecraft:crafting_table");let json=serde_json::to_value(snap).unwrap();assert!(json["ghost"].get("id").is_none());assert!(book.placement(0,0,&Menu::Player(Default::default()),false).is_none());
  assert!(!book.ingest(&ghost(0,3)));open(&mut book,7,MenuKind::Crafting);assert!(book.snapshot().ghost.is_none());assert!(!book.ingest(&ghost(0,2)));assert!(book.ingest(&ghost(7,3)));assert_eq!(book.snapshot().ghost.unwrap().width,3);
  open(&mut book,8,MenuKind::Generic9x3);assert!(!book.ingest(&ghost(8,2)));assert!(!book.close_container(7));book.close_container(8);assert!(book.ingest(&ghost(0,2)));
 }
 #[test]fn actual_inventory_close_and_reset_remove_ghost_without_unlocking_any_recipe(){
  let mut book=Recipes::default();open(&mut book,7,MenuKind::Crafting);book.ingest(&ghost(7,3));let revision=book.snapshot().revision;
  let stale=ClientboundGamePacket::ContainerSetContent(ClientboundContainerSetContent{container_id:8,state_id:1,items:vec![ItemStack::Empty;46],carried_item:ItemStack::Empty});assert!(!book.ingest(&stale));assert!(book.snapshot().ghost.is_some());
  let content=ClientboundGamePacket::ContainerSetContent(ClientboundContainerSetContent{container_id:7,state_id:2,items:vec![ItemStack::Empty;46],carried_item:ItemStack::Empty});assert!(book.ingest(&content));assert!(book.snapshot().ghost.is_none());assert!(book.snapshot().revision>revision);
  book.ingest(&ghost(7,3));assert!(book.close_container(7));assert!(book.snapshot().ghost.is_none());assert!(!book.ingest(&ghost(7,3)));assert!(book.ingest(&ghost(0,2)));book.clear();assert!(book.snapshot().ghost.is_none());assert!(book.snapshot().entries.is_empty());
 }
 #[test]fn only_server_advertised_numeric_display_ids_prepare_correct_grid_and_all_wire(){
  let mut book=Recipes::default();let p=ClientboundRecipeBookAdd{replace:true,entries:vec![c_recipe_book_add::Entry{flags:0,contents:c_recipe_book_add::RecipeDisplayEntry{id:600,display:shaped(3),group:0,category:RecipeBookCategory::CraftingBuildingBlocks,crafting_requirements:None}}]};let mut wire=Vec::new();p.write(&mut wire).unwrap();let decoded=ClientboundRecipeBookAdd::read(&mut std::io::Cursor::new(wire.as_slice())).unwrap();book.ingest(&decoded);
  assert!(book.placement(600,0,&Menu::Player(Default::default()),true).is_none());let menu=Menu::from_kind(MenuKind::Crafting);assert!(book.placement(599,7,&menu,true).is_none());for all in [false,true]{let packet=book.placement(600,7,&menu,all).unwrap();let mut bytes=Vec::new();packet.write(&mut bytes).unwrap();assert_eq!(bytes,vec![7,0xd8,4,u8::from(all)]);}
 }
}
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct GhostRecipe{pub container_id:i32,pub width:u8,pub height:u8,pub kind:&'static str,pub result:ItemSnapshot,pub ingredients:Vec<Option<ItemSnapshot>>,pub preview_complete:bool}
#[derive(Debug,Clone,PartialEq,serde::Serialize)]pub struct RecipeSnapshot{pub revision:u64,pub entries:Vec<Recipe>,pub ghost:Option<GhostRecipe>}
#[derive(Debug,Default)]pub struct Recipes{revision:u64,entries:BTreeMap<u32,Recipe>,displays:BTreeMap<u32,RecipeDisplayData>,tags:BTreeMap<String,ItemKind>,categories:BTreeMap<u32,String>,active_window:i32,crafting_window:bool,ghost:Option<GhostRecipe>}
fn item(display:&SlotDisplayData,depth:u8,tags:&BTreeMap<String,ItemKind>)->(Option<ItemSnapshot>,bool){
 if depth>8{return(None,false)}
 match display{
  SlotDisplayData::Empty=>(None,true),
  SlotDisplayData::ItemKind(v)=>(snapshot(&ItemStack::new(v.item,1)),true),
  SlotDisplayData::ItemStack(v)=>(snapshot(&v.stack),!v.stack.is_empty()),
  SlotDisplayData::WithRemainder(v)=>item(&v.input,depth+1,tags),
  SlotDisplayData::Composite(v)=>{for alternative in v.contents.iter().take(64){let (value,resolved)=item(alternative,depth+1,tags);if resolved&&value.is_some(){return(value,true)}}(None,false)},
  SlotDisplayData::Tag(id)=>match tags.get(&id.to_string()){Some(kind)=>(snapshot(&ItemStack::new(*kind,1)),true),None=>(None,false)},
  // Fuel and trim previews are not guessed.
  _=>(None,false),
 }
}
fn recipe(id:u32,display:&RecipeDisplayData,tags:&BTreeMap<String,ItemKind>)->Option<Recipe>{
 let (width,height,kind,ingredients,result)=match display{
  RecipeDisplayData::Shaped(v) if v.width>0&&v.width<=3&&v.height>0&&v.height<=3&&v.ingredients.len()==(v.width*v.height)as usize=>(v.width as u8,v.height as u8,"shaped",&v.ingredients,&v.result),
  RecipeDisplayData::Shapeless(v) if !v.ingredients.is_empty()&&v.ingredients.len()<=9=>{let width=if v.ingredients.len()<=4{2}else{3};(width,((v.ingredients.len()+width as usize-1)/width as usize)as u8,"shapeless",&v.ingredients,&v.result)},
  _=>return None,
 };
 let (result,result_resolved)=item(result,0,tags);let result=result?;
 let mut preview_complete=result_resolved;let ingredients=ingredients.iter().map(|v|{let(value,resolved)=item(v,0,tags);preview_complete&=resolved;value}).collect();
 Some(Recipe{id,category:String::new(),width,height,kind,result,ingredients,placeable:result_resolved,preview_complete})
}
impl Recipes{
 pub fn snapshot(&self)->RecipeSnapshot{RecipeSnapshot{revision:self.revision,entries:self.entries.values().cloned().collect(),ghost:self.ghost.clone()}}
 pub fn clear(&mut self){self.entries.clear();self.displays.clear();self.categories.clear();self.ghost=None;self.active_window=0;self.crafting_window=false;self.revision=self.revision.wrapping_add(1)}
 pub fn close_container(&mut self,id:i32)->bool{if self.active_window!=id{return false}self.active_window=0;self.crafting_window=false;let changed=self.ghost.take().is_some();if changed{self.revision=self.revision.wrapping_add(1)}changed}
 pub fn clear_tags(&mut self){self.tags.clear()}
 pub fn update_tags(&mut self,tags:&azalea_protocol::common::tags::TagMap){
  self.tags.clear();for(registry,values)in &tags.0{if registry.to_string()!="minecraft:item"{continue}for tag in values.iter().take(16384){if let Some(kind)=tag.elements.iter().take(65536).filter_map(|id|u32::try_from(*id).ok().and_then(ItemKind::from_u32)).find(|v|*v!=ItemKind::Air){self.tags.insert(tag.name.to_string(),kind);}}}
  self.entries=self.displays.iter().filter_map(|(id,display)|recipe(*id,display,&self.tags).map(|mut r|{r.category=self.categories.get(id).cloned().unwrap_or_default();(*id,r)})).collect();self.revision=self.revision.wrapping_add(1);
 }
 pub fn ingest(&mut self,p:&ClientboundGamePacket)->bool{match p{
  ClientboundGamePacket::RecipeBookAdd(p)=>{if p.replace{self.entries.clear();self.displays.clear();self.categories.clear();self.ghost=None};for entry in &p.entries{let id=entry.contents.id;if let Some(mut value)=recipe(id,&entry.contents.display,&self.tags){if self.displays.contains_key(&id)||self.displays.len()<4096{value.category=format!("{:?}",entry.contents.category);self.categories.insert(id,value.category.clone());self.displays.insert(id,entry.contents.display.clone());self.entries.insert(id,value);}}else{self.entries.remove(&id);self.displays.remove(&id);self.categories.remove(&id);}}},
  ClientboundGamePacket::RecipeBookRemove(p)=>{self.ghost=None;for id in &p.recipes{self.entries.remove(id);self.displays.remove(id);self.categories.remove(id);}},
  ClientboundGamePacket::OpenScreen(p)=>{self.active_window=p.container_id;self.crafting_window=p.menu_type==azalea_registry::builtin::MenuKind::Crafting;if self.ghost.take().is_none(){return false}},
  ClientboundGamePacket::ContainerClose(p)=>return self.close_container(p.container_id),
  ClientboundGamePacket::PlaceGhostRecipe(p)=>{if p.container_id!=self.active_window||p.container_id<0||(p.container_id!=0&&!self.crafting_window){return false}let Some(r)=recipe(0,&p.recipe,&self.tags)else{return false};let size=if p.container_id==0{2}else{3};if r.width>size||r.height>size{return false}self.ghost=Some(GhostRecipe{container_id:p.container_id,width:r.width,height:r.height,kind:r.kind,result:r.result,ingredients:r.ingredients,preview_complete:r.preview_complete});},
  ClientboundGamePacket::ContainerSetContent(p)=>{if !self.ghost.as_ref().is_some_and(|g|g.container_id==p.container_id||p.container_id==0){return false}self.ghost=None;},
  ClientboundGamePacket::ContainerSetSlot(p)=>{if !self.ghost.as_ref().is_some_and(|g|g.container_id==p.container_id||p.container_id==0){return false}self.ghost=None;},
  ClientboundGamePacket::SetPlayerInventory(_)|ClientboundGamePacket::SetCursorItem(_)=>{if self.ghost.take().is_none(){return false}},
  _=>return false,
 }self.revision=self.revision.wrapping_add(1);true}
 pub fn placement(&self,id:u32,window_id:i32,menu:&Menu,all:bool)->Option<ServerboundPlaceRecipe>{
  let recipe=self.entries.get(&id)?;if !recipe.placeable||id>i32::MAX as u32||window_id<0{return None}
  let size=match menu{Menu::Player(_)if window_id==0=>2,Menu::Crafting{..}if window_id>0=>3,_=>return None};
  if recipe.width>size||recipe.height>size{return None}
  Some(ServerboundPlaceRecipe{container_id:window_id,recipe:id,shift_down:all})
 }
}
#[cfg(test)]mod tests{
 use super::*;use azalea_protocol::common::recipe::*;use azalea_protocol::packets::game::c_recipe_book_add::*;use azalea_registry::builtin::{ItemKind,RecipeBookCategory};
 fn display(kind:ItemKind)->SlotDisplayData{SlotDisplayData::ItemKind(ItemStackDisplay{item:kind})}
 fn add(id:u32,ingredients:Vec<SlotDisplayData>,replace:bool)->ClientboundGamePacket{ClientboundGamePacket::RecipeBookAdd(ClientboundRecipeBookAdd{replace,entries:vec![Entry{flags:0,contents:RecipeDisplayEntry{id,display:RecipeDisplayData::Shapeless(ShapelessCraftingRecipeDisplay{ingredients,result:display(ItemKind::Mace),crafting_station:display(ItemKind::CraftingTable)}),group:0,category:RecipeBookCategory::CraftingEquipment,crafting_requirements:None}}]})}
 #[test]fn add_remove_replace_and_clear_are_server_scoped(){let mut book=Recipes::default();assert!(book.ingest(&add(130,vec![display(ItemKind::HeavyCore),display(ItemKind::BreezeRod)],false)));let r=book.snapshot();assert_eq!(r.entries[0].result.resource_id,"minecraft:mace");assert_eq!(r.entries[0].category,"CraftingEquipment");assert!(r.entries[0].placeable);assert_eq!(r.entries[0].ingredients[1].as_ref().unwrap().resource_id,"minecraft:breeze_rod");book.ingest(&add(131,vec![display(ItemKind::OakPlanks)],true));assert_eq!(book.snapshot().entries.len(),1);assert_eq!(book.snapshot().entries[0].id,131);book.ingest(&ClientboundGamePacket::RecipeBookRemove(azalea_protocol::packets::game::c_recipe_book_remove::ClientboundRecipeBookRemove{recipes:vec![131]}));assert!(book.snapshot().entries.is_empty());book.clear();assert!(book.snapshot().revision>r.revision);}
 #[test]fn unresolved_ingredients_are_explicit_and_grid_menu_and_id_are_validated(){let mut book=Recipes::default();book.ingest(&add(1,vec![SlotDisplayData::Tag("minecraft:planks".parse().unwrap())],false));assert!(!book.snapshot().entries[0].preview_complete);assert!(book.snapshot().entries[0].placeable);assert!(book.placement(1,0,&Menu::Player(Default::default()),false).is_some());book.ingest(&add(2,vec![display(ItemKind::OakPlanks);9],false));assert!(book.placement(2,0,&Menu::Player(Default::default()),false).is_none());let crafting=Menu::from_kind(azalea_registry::builtin::MenuKind::Crafting);assert!(book.placement(2,3,&crafting,true).unwrap().shift_down);assert!(book.placement(999,3,&crafting,false).is_none());assert!(book.placement(2,3,&Menu::from_kind(azalea_registry::builtin::MenuKind::Generic9x3),false).is_none());}
 #[test]fn advertised_tags_resolve_previews_and_config_clear_never_reuses_previous_tags(){
  let mut book=Recipes::default();book.ingest(&add(5,vec![SlotDisplayData::Tag("minecraft:planks".parse().unwrap())],false));assert!(!book.snapshot().entries[0].preview_complete);
  let tags=azalea_protocol::common::tags::TagMap([( "minecraft:item".parse().unwrap(),vec![azalea_protocol::common::tags::Tags{name:"minecraft:planks".parse().unwrap(),elements:vec![ItemKind::BirchPlanks.to_u32()as i32]}])].into_iter().collect());book.update_tags(&tags);
  assert!(book.snapshot().entries[0].preview_complete);assert_eq!(book.snapshot().entries[0].ingredients[0].as_ref().unwrap().resource_id,"minecraft:birch_planks");
  book.clear_tags();book.clear();book.ingest(&add(5,vec![SlotDisplayData::Tag("minecraft:planks".parse().unwrap())],false));assert!(!book.snapshot().entries[0].preview_complete);
 }
 #[test]fn numeric_recipe_wire_matches_official_varint_codec(){let packet=ServerboundPlaceRecipe{container_id:7,recipe:300,shift_down:true};let mut bytes=Vec::new();packet.write(&mut bytes).unwrap();assert_eq!(bytes,vec![7,0xac,2,1]);let decoded=ServerboundPlaceRecipe::read(&mut std::io::Cursor::new(bytes.as_slice())).unwrap();assert!(matches!(decoded,azalea_protocol::packets::game::ServerboundGamePacket::PlaceRecipe(p) if p==packet));}
}
