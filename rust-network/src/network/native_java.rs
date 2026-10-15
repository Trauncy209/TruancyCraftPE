//! Direct in-process Java 1.21.11 connection. No proxy or Java runtime at execution.
#[path="native_java_golem_status.rs"]
mod golem_status;
#[path="native_java_menu_metadata.rs"]
mod inventory_menu;
#[path="native_java_menu_properties.rs"]
mod menu_properties;
#[path="native_java_recipes.rs"]
pub(super) mod recipes;
#[path="native_java_effects.rs"]
mod effects;
#[path="native_java_firework_effects.rs"]
mod firework_effects;
#[path="native_java_sounds.rs"]
mod sounds;
#[path="native_java_chat.rs"]
mod incoming_chat;
#[path="native_java_targeting.rs"]
mod targeting;
#[path="native_java_elytra.rs"]
mod native_elytra;
#[path="native_java_vitals.rs"]
mod hud_vitals;
use super::native_java_items::{snapshot as item_snapshot, ItemSnapshot};
use super::rich_text::{styled_text,styled_text_with_parent};
use azalea_client::{Account, Client, Event, WalkDirection, SprintDirection};
use azalea_core::position::ChunkSectionBlockPos;
use std::sync::mpsc::{channel, Receiver};
// Status routing uses the current ECS phase, including configuration before spawn.
fn pack_progress(bot:&Client,id:&str,stage:u8){
 let Ok(id)=id.parse() else{return};
 let mut world=bot.ecs.lock();
 if world.get::<azalea_client::InConfigState>(bot.entity).is_some(){
  use azalea_protocol::packets::config::s_resource_pack::{Action,ServerboundResourcePack};
  world.trigger(azalea_client::packet::config::SendConfigPacketEvent::new(bot.entity,ServerboundResourcePack{id,action:match stage{0=>Action::Accepted,1=>Action::Downloaded,_=>Action::SuccessfullyLoaded}}));
 }else if world.get::<azalea_client::InGameState>(bot.entity).is_some(){
  use azalea_protocol::packets::game::s_resource_pack::{Action,ServerboundResourcePack};
  world.trigger(azalea_client::packet::game::SendGamePacketEvent::new(bot.entity,ServerboundResourcePack{id,action:match stage{0=>Action::Accepted,1=>Action::Downloaded,_=>Action::SuccessfullyLoaded}}));
 }
}
#[derive(Debug)]
pub enum Command {
    LoadPackFonts{connection:u64,generation:u64,id:String},
    PackRendererLoaded{connection:u64,generation:u64,id:String},
    PickRay {epoch:u64,revision:u64,enabled:bool,offset:[f64;3],direction:[f64;3]},
    Chat(String),
    Look {
        yaw: f32,
        pitch: f32,
    },
    Walk {
        forward: f32,
        strafe: f32,
        jump: bool,
        sneak: bool,
        sprint: bool,
    },
    SelectHotbar(u8),
    ClickSlot {
        window_id: i32,
        slot: u16,
        right: bool,
        generation:Option<u64>,
    },
    QuickMove {window_id:i32,slot:u16,right:bool,generation:Option<u64>},
    DropSelected(bool),
    SwapOffhand,
    DropCarried{window_id:i32,right:bool,generation:Option<u64>},
    CloseContainer{window_id:i32,generation:Option<u64>},
    PlaceRecipe{window_id:i32,recipe_id:u32,all:bool,generation:u64},
    UseItem(Option<bool>),
    ReleaseUse,
    Mine(bool),
    Attack(i32),
    Interact{ id:i32,hand:Option<bool> },
    DialogCustomClick(String),
    Flight{flying:bool,vertical:f32,toggle:bool},
    Respawn,
    Disconnect,
}
#[derive(Debug, serde::Serialize)]
pub struct ChunkSnapshot {
    pub x: i32,
    pub z: i32,
    pub min_y: i32,
    pub height: usize,
    pub states: Vec<u16>,
}
#[derive(Debug, Clone)]
pub struct LightSnapshot {pub x:i32,pub z:i32,pub min_y:i32,pub height:usize,pub sky_light:Vec<u8>,pub block_light:Vec<u8>,pub light_sections:Vec<u8>}
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct InventorySnapshot {
    pub window_id: i32,
    pub title: String,
    pub menu_kind:String,
    pub slot_ranges:inventory_menu::SlotRanges,
    pub menu_generation:u64,
    pub properties:std::collections::BTreeMap<u16,i32>,
    pub property_entries:Vec<menu_properties::PropertyEntry>,
    pub furnace_status:Option<menu_properties::FurnaceStatus>,
    pub slots: Vec<String>,
    pub carried: String,
    pub carried_item: Option<ItemSnapshot>,
    pub selected_hotbar: u8,
    pub items: Vec<Option<ItemSnapshot>>,
    pub hotbar: Vec<Option<ItemSnapshot>>,
    pub held: Option<ItemSnapshot>,
    pub offhand: Option<ItemSnapshot>,
    pub armor: [Option<ItemSnapshot>; 4],
}
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct EntitySnapshot {
    pub id: i32,
    #[serde(flatten)]pub item_use:RemoteUseSnapshot,
    pub kind: String,
    pub position: [f64; 3],
    pub yaw: f32,
    pub pitch: f32,
    pub head_yaw: f32,
    pub profile: serde_json::Value,
    pub skin_layers: u8,
    pub hitbox: [f32;2],
    pub sneaking: bool,
    pub invisible:bool,pub custom_name:String,pub custom_name_json:String,pub custom_name_visible:bool,pub armor_stand_marker:bool,pub armor_stand:serde_json::Value,pub display:serde_json::Value,
    pub dropped_item: Option<ItemSnapshot>,
    pub firework:serde_json::Value,
    pub equipment: [Option<ItemSnapshot>; 6],
    pub hurt_ticks: u8,
    pub death_ticks: u8,
    pub mob_attack_ticks: u16,
    pub mob_flower_ticks: u16,
    pub living_health: Option<f32>,
    pub living_max_health: Option<f32>,
}
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct PlayerVitals {
    pub poisoned:bool,pub withered:bool,pub regenerating:bool,pub hardcore:bool,
    pub frozen_ticks:u32,pub invulnerable_ticks:u32,
    pub health: f32,
    pub max_health: f32,
    pub absorption: f32,
    pub food: u32,
    pub saturation: f32,
    pub armor: f32,
    pub experience_progress: f32,
    pub experience_level: u32,
    pub total_experience: u32,
    pub show_death_screen: bool,
    pub can_fly: bool,
    pub flying: bool,
    pub attack_min_range:f32,pub attack_max_range:f32,pub attack_hitbox_margin:f32,
    pub use_active:bool,pub use_off_hand:bool,pub use_ticks:u32,pub use_duration:u32,pub use_charge_duration:u32,pub use_animation:String,
    pub attack_strength:f32,pub attack_min_charge:f32,pub attack_piercing:bool,
    pub use_kinetic_delay:i32,pub use_kinetic_dismount:i32,pub use_kinetic_knockback:i32,pub use_kinetic_damage:i32,
}
impl Default for PlayerVitals {
    fn default() -> Self {
        Self {
            poisoned:false,withered:false,regenerating:false,hardcore:false,frozen_ticks:0,invulnerable_ticks:0,
            health: 20.,
            max_health: 20.,
            absorption: 0.,
            food: 20,
            saturation: 5.,
            armor: 0.,
            experience_progress: 0.,
            experience_level: 0,
            total_experience: 0,
            show_death_screen:true,
            can_fly:false,
            flying:false,
            attack_min_range:0.,attack_max_range:3.,attack_hitbox_margin:0.3,
            use_active:false,use_off_hand:false,use_ticks:0,use_duration:0,use_charge_duration:0,use_animation:String::new(),
            attack_strength:1.,attack_min_charge:0.,attack_piercing:false,
            use_kinetic_delay:0,use_kinetic_dismount:0,use_kinetic_knockback:0,use_kinetic_damage:0,
        }
    }
}
#[derive(Debug,serde::Serialize)]
#[serde(tag="operation",rename_all="snake_case")]
pub enum TitleUpdate { Title{text:String},Subtitle{text:String},Times{fade_in:i32,stay:i32,fade_out:i32},Clear{reset_times:bool} }
#[derive(Debug,serde::Serialize)]
#[serde(tag="operation",rename_all="snake_case")]
pub enum BossUpdate {
 Add{id:String,text:String,progress:f32,color:u8,overlay:u8,darken_screen:bool,play_music:bool,create_world_fog:bool},
 Remove{id:String},Progress{id:String,progress:f32},Name{id:String,text:String},
 Style{id:String,color:u8,overlay:u8},Properties{id:String,darken_screen:bool,play_music:bool,create_world_fog:bool},
}
fn boss_update(packet:&azalea_protocol::packets::game::ClientboundBossEvent)->BossUpdate {
 use azalea_protocol::packets::game::c_boss_event::Operation;
 let id=packet.id.to_string();
 let progress=|v:f32|if v.is_finite(){v.clamp(0.,1.)}else{0.};
 match &packet.operation {
 Operation::Add(v)=>BossUpdate::Add{id,text:styled_text(&v.name),progress:progress(v.progress),color:v.style.color as u8,overlay:v.style.overlay as u8,darken_screen:v.properties.darken_screen,play_music:v.properties.play_music,create_world_fog:v.properties.create_world_fog},
 Operation::Remove=>BossUpdate::Remove{id},
 Operation::UpdateProgress(v)=>BossUpdate::Progress{id,progress:progress(*v)},
 Operation::UpdateName(v)=>BossUpdate::Name{id,text:styled_text(v)},
 Operation::UpdateStyle(v)=>BossUpdate::Style{id,color:v.color as u8,overlay:v.overlay as u8},
 Operation::UpdateProperties(v)=>BossUpdate::Properties{id,darken_screen:v.darken_screen,play_music:v.play_music,create_world_fog:v.create_world_fog},
 }
}
#[derive(Debug)]
pub enum Update {
    Targeting(serde_json::Value),
    VisualEffect(effects::VisualEffect),
    Sound(sounds::ServerSound),
    PackFonts(super::resource_pack_download::DownloadedPackFonts),
    PackFontError{connection:u64,generation:u64,id:String,message:String},
    ResourcePacks(super::resource_packs::PackSnapshot),
    Compass(super::native_java_compass::CompassSnapshot),
    Login,
    Spawn,
    Position([f64; 3]),
    LookCorrection {yaw:f32,pitch:f32,relative_yaw:bool,relative_pitch:bool},
    Entities(Vec<EntitySnapshot>),
    RemovedEntities(Vec<i32>),
    Skin(super::player_textures::SkinImage),
    PlayerId(i32),
    Time { day: u64, ticking: bool },
    Vitals(PlayerVitals),
    Hurt { id: i32, yaw: f32 },
    Death(String),
    Chat(String),
    SignedChat{text:String,signature:String},
    DeleteChat(String),
    ActionBar(String),
    Title(TitleUpdate),
    Boss(BossUpdate),
    Scoreboard(serde_json::Value),
    ChatActions(Vec<(String, String)>),
    ConfigurationPhase(String),
    ServerDialog(serde_json::Value),
    Chunk(ChunkSnapshot),
    Light(LightSnapshot),
    UnloadChunk(i32, i32),
    Inventory(InventorySnapshot),
    Recipes(recipes::RecipeSnapshot),
    Disconnected(String),
    Error(String),
}
pub struct Connection {
    pub commands: tokio::sync::mpsc::UnboundedSender<Command>,
    pub updates: Receiver<Update>,
}
impl Drop for Connection {
    fn drop(&mut self) {
        let _ = self.commands.send(Command::Disconnect);
    }
}
impl Connection {
    pub fn offline(address: String, username: String) -> Self {
        Self::start(address, username, false, true)
    }
    pub fn online(address: String, profile_name: String) -> Self {
        Self::start(address, profile_name, true, true)
    }
    /// Diagnostics retain the authoritative world but avoid cloning renderer snapshots.
    pub fn diagnostic(address: String, username: String) -> Self {
        Self::start(address, username, false, false)
    }
    fn start(address: String, identity: String, online: bool, capture_chunks: bool) -> Self {
        let (tx, updates) = channel();
        let (commands, mut rx) = tokio::sync::mpsc::unbounded_channel();
        std::thread::spawn(move || {
            let runtime = match tokio::runtime::Builder::new_multi_thread()
                .worker_threads(2)
                .enable_all()
                .build()
            {
                Ok(r) => r,
                Err(e) => {
                    let _ = tx.send(Update::Error(e.to_string()));
                    return;
                }
            };
            tokio::task::LocalSet::new().block_on(&runtime,async move{
    let account=if online {match super::native_java_auth::account(&identity).await{Ok(account)=>account,Err(message)=>{let _=tx.send(Update::Error(message.into()));return;}}}else{Account::offline(&identity)};
    let timeout_ms=std::env::var("TCPE_JAVA_CONNECT_TIMEOUT_MS").ok().and_then(|v|v.parse::<u64>().ok()).unwrap_or(30000).clamp(100,120000);
    let joining=tokio::time::timeout(std::time::Duration::from_millis(timeout_ms),Client::join(account.clone(),address.as_str()));tokio::pin!(joining);
    let joined=loop {tokio::select! {
      result=&mut joining=>break result,
      command=rx.recv()=>{if matches!(command,Some(Command::Disconnect)|None){return;}}
    }};
    let(mut bot,mut events)=match joined {Ok(Ok(v))=>v,Ok(Err(e))=>{let _=tx.send(Update::Error(e.to_string()));return;},Err(_)=>{let _=tx.send(Update::Error("Connection timed out before the Java session was created".into()));return;}};
    let mut last_progress=tokio::time::Instant::now();
    let mut targeting_epoch=1u64;
    let mut targeting_cache=targeting::OutlineCache::default();
    let mut spawned=false;
    let mut world_dimension:Option<String>=None;
    let mut transport_responded=false;
    let first_packet_deadline=tokio::time::Instant::now()+std::time::Duration::from_millis(timeout_ms);
    let mut observed = std::collections::BTreeMap::new();
    let mut lights=std::collections::BTreeMap::<(i32,i32),LightSnapshot>::new();
    let mut dirty_chunks = std::collections::BTreeSet::new();
    let mut pack_scope=super::resource_packs::PackScope::default();pack_scope.begin_configuration();
    let trace_world=std::env::var_os("TCPE_JAVA_TRACE_WORLD").is_some();
    if trace_world{let local=bot.entity;bot.ecs.lock().add_observer(move |event:bevy_ecs::prelude::On<azalea_client::packet::game::SendGamePacketEvent>|{if event.sent_by!=local{return;}match &event.packet{azalea_protocol::packets::game::ServerboundGamePacket::ClientInformation(p)=>eprintln!("TCPE world sent settings: view_distance={}",p.client_information.view_distance),azalea_protocol::packets::game::ServerboundGamePacket::ChunkBatchReceived(p)=>eprintln!("TCPE world sent batch ack: {}",p.desired_chunks_per_tick),azalea_protocol::packets::game::ServerboundGamePacket::PlayerLoaded(_)=>eprintln!("TCPE world sent player loaded"),azalea_protocol::packets::game::ServerboundGamePacket::AcceptTeleportation(p)=>eprintln!("TCPE world sent teleport ack: {:?}",p),_=>{}}});}

    let mut trace_ticks=0u64;
    let mut compass=super::native_java_compass::CompassSnapshot::default();
    let mut last_inventory = None;
    let mut menu_properties=menu_properties::MenuProperties::default();
    let mut recipes=recipes::Recipes::default();
    let mut banner_registry=super::native_java_items::shields::Registry::default();
    let mut incoming_chat=incoming_chat::Incoming::default();
    let mut last_entities = Vec::new();
    let skin_client=super::player_textures::download_client().ok();
    let mut skin_downloads=tokio::task::JoinSet::<Result<super::player_textures::SkinImage,String>>::new();
    let mut requested_pack_scope=(0u64,0u64);
    let mut pack_renderer_ack=super::resource_packs::PackRendererAcknowledgments::default();
    let mut requested_pack_ids=std::collections::BTreeSet::<String>::new();
    let mut pack_downloads=tokio::task::JoinSet::<(u64,u64,String,Result<super::resource_pack_download::DownloadedPackFonts,String>)>::new();
    let mut requested_skins=std::collections::BTreeSet::new();
    let mut profile_lookups=tokio::task::JoinSet::new();
    let mut requested_profiles=std::collections::BTreeSet::new();
    let mut username_profiles=std::collections::BTreeMap::<String,Option<super::player_textures::PlayerTextures>>::new();
    let trace_entities=std::env::var_os("TCPE_JAVA_TRACE_ENTITIES").is_some();
    let mut traced_kinds=std::collections::BTreeSet::new();
    let mut entity_angles=std::collections::BTreeMap::<i32,[f32;3]>::new();
    let mut equipment=std::collections::BTreeMap::<i32,[Option<ItemSnapshot>;6]>::new();
    let mut equipment_raw=std::collections::BTreeMap::<i32,[azalea_inventory::ItemStack;6]>::new();
    let mut remote_use=std::collections::BTreeMap::<i32,RemoteUseSnapshot>::new();
    let mut animations=std::collections::BTreeMap::<i32,(u8,u8)>::new();
    let mut golem_status=golem_status::GolemStatuses::default();
    let mut living_max_health=std::collections::BTreeMap::<i32,f32>::new();
    let mut recent_attacks=std::collections::VecDeque::<(i32,std::time::Instant)>::new();
    let mut show_death_screen=true;let mut auto_respawn_requested=false;
    let mut hud_state=hud_vitals::State::default();
    let mut controls_alive=true;
    let mut item_cooldowns=NativeItemCooldowns::default();
    let mut vitals=PlayerVitals::default();let mut last_vitals=None;let mut local_id_seen=None;let mut server_armor=None;
    loop{tokio::select!{
     pack=pack_downloads.join_next(),if !pack_downloads.is_empty()=>{
      if let Some(Ok((connection,generation,id,result)))=pack{
       if pack_scope.completion_is_current(connection,generation,&id){match result{
        Ok(fonts)=>{pack_renderer_ack.downloaded(&pack_scope,connection,generation,&id);pack_progress(&bot,&id,1);let _=tx.send(Update::PackFonts(fonts));},
        Err(message)=>{requested_pack_ids.remove(&id);let _=tx.send(Update::PackFontError{connection,generation,id,message});}
       }}
      }
     },
     profile=profile_lookups.join_next(),if !profile_lookups.is_empty()=>{if let Some(Ok(Ok((name,textures))))=profile{username_profiles.insert(name,textures);}},
     skin=skin_downloads.join_next(),if !skin_downloads.is_empty()=>{match skin{Some(Ok(Ok(image)))=>{if trace_entities{eprintln!("Java skin decoded: {}x{}",image.width,image.height);}let _=tx.send(Update::Skin(image));},Some(Ok(Err(_)))=>{if trace_entities{eprintln!("Java skin download failed; fallback retained");}},_=>{}}},
     _=tokio::time::sleep_until(last_progress+std::time::Duration::from_secs(60)),if transport_responded=>{let _=tx.send(Update::Error("Java connection stopped updating".into()));bot.disconnect();break;},
     _=tokio::time::sleep_until(first_packet_deadline),if !transport_responded=>{let _=tx.send(Update::Error("Java server did not respond before the connection deadline".into()));bot.disconnect();break;},
     command=rx.recv()=>{if spawned&&matches!(&command,Some(Command::Mine(true)|Command::UseItem(_)|Command::Attack(_)|Command::Interact{..})){targeting::refresh(&mut bot.ecs.lock());}if trace_entities{if let Some(Command::ClickSlot{window_id,slot,right,..})=&command{eprintln!("Java click dispatch guard: window={} slot={} right={} spawned={} health={}",window_id,slot,right,spawned,vitals.health);}}match command{
      Some(Command::DialogCustomClick(id))=>{
       let in_config=bot.try_query_entity::<Option<&azalea_client::InConfigState>,_>(bot.entity,|state|state.is_some()).unwrap_or(false);
       if in_config {bot.ecs.lock().trigger(azalea_client::packet::config::SendConfigPacketEvent::new(bot.entity,azalea_protocol::packets::config::ServerboundCustomClickAction{id:id.as_str().into(),payload:Default::default()}));}
       else if spawned {bot.ecs.lock().trigger(azalea_client::packet::game::SendGamePacketEvent::new(bot.entity,azalea_protocol::packets::game::ServerboundCustomClickAction{id:id.as_str().into(),payload:Default::default()}));}
      },
      Some(Command::Disconnect)|None=>{bot.disconnect();break;},
      Some(Command::Chat(text)) if spawned=>bot.chat(text),
      Some(Command::PackRendererLoaded{connection,generation,id})=>{if pack_renderer_ack.activated(&pack_scope,connection,generation,&id){pack_progress(&bot,&id,2);}},
      Some(Command::LoadPackFonts{connection,generation,id})=>{
       if pack_scope.completion_is_current(connection,generation,&id)&&pack_downloads.len()<128{if requested_pack_scope!=(connection,generation){requested_pack_scope=(connection,generation);requested_pack_ids.clear();}if let Some(request)=pack_scope.request(&id){if !requested_pack_ids.insert(id.clone()){continue;}pack_progress(&bot,&id,0);pack_downloads.spawn(async move{let result=super::resource_pack_download::download_fonts(request,connection,generation).await;(connection,generation,id,result)});}}
      },
      Some(Command::DropSelected(all)) if spawned&&vitals.health>0.=>{send_held_action(&mut bot.ecs.lock(),bot.entity,if all{azalea_protocol::packets::game::s_player_action::Action::DropAllItems}else{azalea_protocol::packets::game::s_player_action::Action::DropItem});},
      Some(Command::SwapOffhand) if spawned&&vitals.health>0.=>{send_held_action(&mut bot.ecs.lock(),bot.entity,azalea_protocol::packets::game::s_player_action::Action::SwapItemWithOffhand);},
      Some(Command::SelectHotbar(slot)) if spawned && slot < 9 => {
       {let mut world=bot.ecs.lock();let changed=world.get::<azalea_entity::inventory::Inventory>(bot.entity).is_some_and(|inv|inv.selected_hotbar_slot!=slot);if changed{clear_native_item_use(&mut world,bot.entity);}}
       bot.set_selected_hotbar_slot(slot);
      },
      Some(Command::UseItem(requested)) if spawned && vitals.health>0. => {let (hand,ready)=bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv|{let hand=use_hand(inv,requested);(hand,item_cooldowns.ready(inv,hand))}).unwrap_or_default();if !ready&&requested.is_some(){continue;}let mut world=bot.ecs.lock();if super::native_java_use::busy(&world,bot.entity){continue;}if let Some(mut options)=super::native_java_use::options(&world,bot.entity,requested.is_none()){if let Some(inv)=world.get::<azalea_entity::inventory::Inventory>(bot.entity){use azalea_protocol::packets::game::s_interact::InteractionHand;use azalea_client::interact::native_use::Outcome;if !item_cooldowns.ready(inv,InteractionHand::MainHand){options.main=Outcome::Pass;}if !item_cooldowns.ready(inv,InteractionHand::OffHand){options.off=Outcome::Pass;}}world.entity_mut(bot.entity).insert(options);}else{world.entity_mut(bot.entity).remove::<azalea_client::interact::native_use::NativeUseOptions>();}let hand=if requested.is_none(){azalea_protocol::packets::game::s_interact::InteractionHand::MainHand}else{hand};targeting::capture_use_aim(&mut world,bot.entity);world.entity_mut(bot.entity).insert(azalea_client::interact::StartUseItemQueued{hand,force_block:None});},
      Some(Command::ReleaseUse) if spawned => {
       let mut world=bot.ecs.lock();
       targeting::release_use(&mut world,bot.entity);
       clear_native_item_use(&mut world,bot.entity);
      },
      Some(Command::Flight{flying,vertical,toggle}) if spawned&&(vitals.health>0.||vertical==0.)=>{
       let mut world=bot.ecs.lock();
       let current=world.get::<azalea_entity::PlayerAbilities>(bot.entity).is_some_and(|a|a.flying);
       let flying=native_flight_state(current,flying,toggle,vitals.health>0.);
       if apply_native_flight(&mut world,bot.entity,flying,vertical)==Some(true){
        world.trigger(azalea_client::packet::game::SendGamePacketEvent::new(bot.entity,azalea_protocol::packets::game::ServerboundPlayerAbilities{is_flying:flying}));
       }
       refresh_vital_abilities(&world,bot.entity,&mut vitals);
      },
      Some(Command::Respawn) if spawned && vitals.health<=0.=>{bot.ecs.lock().write_message(azalea_client::respawn::PerformRespawnEvent{entity:bot.entity});},
      Some(Command::ClickSlot {window_id,slot,right,generation}) if spawned && vitals.health>0. => {
       let valid = bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv| inv.id == window_id && usize::from(slot)<inv.menu().slots().len()).unwrap_or(false);
       if trace_entities {let state=bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv|(inv.id,inv.menu().slots().len())).ok();eprintln!("Java inventory click: window={} slot={} right={} valid={} current={:?}",window_id,slot,right,valid,state);}
       if valid&&menu_properties.accepts(window_id,generation) {
        use azalea_inventory::operations::{ClickOperation,PickupClick};
        bot.ecs.lock().trigger(azalea_client::inventory::ContainerClickEvent {entity:bot.entity,window_id,operation:ClickOperation::Pickup(if right {PickupClick::Right {slot:Some(slot)}} else {PickupClick::Left {slot:Some(slot)}})});
       }
      },
      Some(Command::QuickMove{window_id,slot,right,generation}) if spawned&&vitals.health>0.=>{
       let valid=bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv|inv.id==window_id&&usize::from(slot)<inv.menu().slots().len()).unwrap_or(false);
       if valid&&menu_properties.accepts(window_id,generation){use azalea_inventory::operations::{ClickOperation,QuickMoveClick};bot.ecs.lock().trigger(azalea_client::inventory::ContainerClickEvent{entity:bot.entity,window_id,operation:ClickOperation::QuickMove(if right{QuickMoveClick::Right{slot}}else{QuickMoveClick::Left{slot}})});}
      },
      Some(Command::DropCarried{window_id,right,generation}) if spawned&&vitals.health>0.=>{
       let valid=bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv|inv.id==window_id&&!inv.carried.is_empty()).unwrap_or(false);
       if valid&&menu_properties.accepts(window_id,generation){use azalea_inventory::operations::{ClickOperation,PickupClick};bot.ecs.lock().trigger(azalea_client::inventory::ContainerClickEvent{entity:bot.entity,window_id,operation:ClickOperation::Pickup(if right{PickupClick::RightOutside}else{PickupClick::LeftOutside})});}
      },
      Some(Command::CloseContainer{window_id:id,generation}) if spawned => {let valid=bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv|inv.id==id).unwrap_or(false);if valid&&menu_properties.accepts(id,generation){menu_properties.close(id);if recipes.close_container(id){let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));}bot.ecs.lock().trigger(azalea_client::inventory::CloseContainerEvent{entity:bot.entity,id});}},
      Some(Command::Interact{id,hand:requested}) if spawned && vitals.health>0.=>{let target=resolve_attack_target(&mut bot.ecs.lock(),bot.entity,id);if let Some(target)=target{let mut world=bot.ecs.lock();if super::native_java_use::busy(&world,bot.entity){continue;}let hand=if requested.is_none(){azalea_protocol::packets::game::s_interact::InteractionHand::MainHand}else{world.get::<azalea_entity::inventory::Inventory>(bot.entity).map(|inv|use_hand(inv,requested)).unwrap_or_default()};if let Some(mut options)=super::native_java_use::options(&world,bot.entity,requested.is_none()){if let Some(inv)=world.get::<azalea_entity::inventory::Inventory>(bot.entity){use azalea_protocol::packets::game::s_interact::InteractionHand;use azalea_client::interact::native_use::Outcome;if !item_cooldowns.ready(inv,InteractionHand::MainHand){options.main=Outcome::Pass;}if !item_cooldowns.ready(inv,InteractionHand::OffHand){options.off=Outcome::Pass;}}world.entity_mut(bot.entity).insert(options);}else{world.entity_mut(bot.entity).remove::<azalea_client::interact::native_use::NativeUseOptions>();}targeting::capture_use_aim(&mut world,bot.entity);world.trigger(azalea_client::interact::EntityInteractEvent{client:bot.entity,target,hand,location:None});}},
      Some(Command::PlaceRecipe{window_id,recipe_id,all,generation}) if spawned&&vitals.health>0.=>{
       if menu_properties.accepts(window_id,Some(generation)){let packet=bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv|if inv.id==window_id{recipes.placement(recipe_id,window_id,inv.menu(),all)}else{None}).ok().flatten();if let Some(packet)=packet{bot.ecs.lock().trigger(azalea_client::packet::game::SendGamePacketEvent::new(bot.entity,packet));}}
      },
      Some(Command::Attack(id)) if spawned && vitals.health>0.=>{
       if !native_attack_charge_ready(&bot.ecs.lock(),bot.entity){continue;}
       if send_native_piercing_attack(&mut bot.ecs.lock(),bot.entity){continue;}
       let target=resolve_weapon_target(&mut bot.ecs.lock(),bot.entity,id);
       if let Some(target)=target {bot.left_click_mine(false);bot.attack(target);recent_attacks.push_back((id,std::time::Instant::now()));while recent_attacks.len()>16{recent_attacks.pop_front();}if trace_entities{eprintln!("Java selected entity attack queued: target={id}");}}
      },
      Some(Command::Mine(pressed)) if spawned && (!pressed||vitals.health>0.) => {
       let target=if pressed {bot.try_query_entity::<&azalea_client::interact::pick::HitResultComponent,_>(bot.entity,|hit|hit.as_entity_hit_result().map(|hit|hit.entity)).ok().flatten()} else {None};
       if let Some(target)=target {bot.left_click_mine(false);if native_attack_charge_ready(&bot.ecs.lock(),bot.entity){bot.attack(target);}} else {bot.left_click_mine(pressed);}
      },
      Some(Command::PickRay{epoch,revision,enabled,offset,direction}) if spawned&&epoch==targeting_epoch=>{
       targeting::apply_ray(&mut bot.ecs.lock(),bot.entity,targeting_epoch,epoch,revision,enabled,offset,direction);
      },
      Some(Command::Look{yaw,pitch}) if spawned&&vitals.health>0.=>{apply_native_look(&mut bot.ecs.lock(),bot.entity,yaw,pitch);},
      Some(Command::Walk{forward,strafe,jump,sneak,sprint}) if spawned&&(vitals.health>0.||(forward==0.&&strafe==0.&&!jump))=>{
       let direction=match((forward>0.) as i8-(forward<0.) as i8,(strafe>0.) as i8-(strafe<0.) as i8){(1,1)=>WalkDirection::ForwardLeft,(1,-1)=>WalkDirection::ForwardRight,(-1,1)=>WalkDirection::BackwardLeft,(-1,-1)=>WalkDirection::BackwardRight,(1,_)=>WalkDirection::Forward,(-1,_)=>WalkDirection::Backward,(_,1)=>WalkDirection::Left,(_,-1)=>WalkDirection::Right,_=>WalkDirection::None};
       if let Some(direction)=native_sprint_direction(forward,strafe,sneak,sprint){bot.sprint(direction);}else{bot.walk(direction);}apply_native_movement(&mut bot.ecs.lock(),bot.entity,forward,strafe,jump,sneak);
      },_=>{}
     }},
     event=events.recv()=>{last_progress=tokio::time::Instant::now();let Some(event)=event else{let _=tx.send(Update::Disconnected("Connection closed".into()));break;};if matches!(&event,Event::Login|Event::Spawn|Event::Packet(_)){transport_responded=true;}match event{
      Event::Login=>{},
      Event::Spawn=>{spawned=true;local_id_seen=bot.try_query_entity::<&azalea_world::MinecraftEntityId,_>(bot.entity,|id|id.0).ok();if let Some(id)=local_id_seen{let _=tx.send(Update::PlayerId(id));}let _=tx.send(Update::Spawn);},
      Event::Tick if spawned=>{
       for effect in firework_effects::tick(&mut bot.ecs.lock(),bot.entity){let _=tx.send(Update::VisualEffect(effect));}
       if let Some(snapshot)=targeting::fresh_snapshot(&mut bot.ecs.lock(),bot.entity,targeting_epoch,&mut targeting_cache){let _=tx.send(Update::Targeting(snapshot));}

       let (verified,warning)=incoming_chat.poll().await;
       for chat in verified {complete_signed_chat(&bot,&tx,chat.packet,chat.display);}
       if warning{let _=tx.send(Update::Chat("A player message could not be verified and was hidden.".into()));}
       item_cooldowns.tick();
       {let mut world=bot.ecs.lock();if end_flight_on_landing(&mut world,bot.entity){world.trigger(azalea_client::packet::game::SendGamePacketEvent::new(bot.entity,azalea_protocol::packets::game::ServerboundPlayerAbilities{is_flying:false}));}}
       let Ok(p)=bot.try_query_entity::<(&azalea_entity::Position,&azalea_entity::dimensions::EntityDimensions),_>(bot.entity,|(pos,dim)|pos.up(dim.eye_height as f64)) else {continue;};
       if trace_world {trace_ticks+=1;if trace_ticks%100==1 {if let Ok(position)=bot.try_query_entity::<&azalea_entity::Position,_>(bot.entity,|v|**v){let _=bot.try_query_entity::<&azalea_client::local_player::InstanceHolder,_>(bot.entity,|holder|{let instance=holder.instance.read();let partial=holder.partial_instance.read();let chunk=azalea_core::position::ChunkPos{x:(position.x.floor() as i32).div_euclid(16),z:(position.z.floor() as i32).div_euclid(16)};eprintln!("TCPE world cache: minY={} height={} center={:?} chunk={:?} owned={} shared={} block={:?}",instance.chunks.min_y,instance.chunks.height,partial.chunks.view_center(),chunk,partial.chunks.limited_get(&chunk).is_some(),instance.chunks.get(&chunk).is_some(),instance.chunks.get_block_state(azalea_core::position::BlockPos{x:position.x.floor() as i32,y:position.y.floor() as i32,z:position.z.floor() as i32}));});}eprintln!("TCPE world tick: eye=({:.3},{:.3},{:.3}) observed={} pending={}",p.x,p.y,p.z,observed.len(),dirty_chunks.len());if let Ok(bounds)=bot.try_query_entity::<&azalea_entity::Physics,_>(bot.entity,|v|v.bounding_box){eprintln!("TCPE world collision bounds: {:?}",bounds);}}}
       let _=tx.send(Update::Position([p.x,p.y,p.z]));
       if let Ok((health,food,saturation))=bot.try_query_entity::<(&azalea_entity::metadata::Health,&azalea_client::local_player::Hunger),_>(bot.entity,|(health,hunger)|(health.0,hunger.food,hunger.saturation)){vitals.health=health;vitals.food=food;vitals.saturation=saturation;}
       guard_health_controls(&bot,vitals.health,&mut controls_alive);if vitals.health>0.{auto_respawn_requested=false;}
       let local_id=bot.try_query_entity::<&azalea_world::MinecraftEntityId,_>(bot.entity,|id|id.0).ok();
       if local_id!=local_id_seen {local_id_seen=local_id;if let Some(id)=local_id {let _=tx.send(Update::PlayerId(id));}}
       golem_status.tick();
       for (hurt,death) in animations.values_mut(){*hurt=hurt.saturating_sub(1);if *death>0 {*death=death.saturating_add(1).min(20);}}
       let mut entities={
        let mut ecs=bot.ecs.lock();
        let mut query=ecs.query::<(bevy_ecs::entity::Entity,&azalea_world::MinecraftEntityId,&azalea_entity::Position,&azalea_entity::EntityKindComponent,&azalea_entity::LoadedBy,Option<&azalea_entity::LookDirection>,Option<&azalea_entity::metadata::Invisible>,Option<&azalea_entity::Crouching>)>();
        let mut entities=query.iter(&ecs).filter(|(_,id,pos,_,loaded,_,_,_)|Some(id.0)!=local_id&&loaded.0.contains(&bot.entity)&&(pos.x-p.x).powi(2)+(pos.z-p.z).powi(2)<128f64.powi(2)).map(|(entity,id,pos,kind,_,look,invisible,crouch)|{let angles=entity_angles.get(&id.0).copied().unwrap_or_else(||[look.map(|v|v.y_rot()).unwrap_or(0.),look.map(|v|v.x_rot()).unwrap_or(0.),look.map(|v|v.y_rot()).unwrap_or(0.)]);EntitySnapshot{id:id.0,item_use:{let off=ecs.get::<azalea_entity::metadata::AbstractLivingUsingOffHand>(entity).is_some_and(|v|v.0);let stack=equipment_raw.get(&id.0).map(|v|&v[if off{1}else{0}]);remote_use_snapshot(&ecs,entity,bot.entity,stack,remote_use.entry(id.0).or_default())},kind:format!("{:?}",kind.0),position:[pos.x,pos.y,pos.z],yaw:angles[0],pitch:angles[1],head_yaw:angles[2],profile:player_profile(&ecs,entity),hitbox:entity_hitbox(&ecs,entity),skin_layers:player_skin_layers(&ecs,entity),sneaking:crouch.is_some_and(|v|**v),invisible:invisible.is_some_and(|v|v.0),
custom_name:ecs.get::<azalea_entity::metadata::CustomName>(entity).and_then(|v|v.0.as_ref()).map(|v|styled_text(v)).unwrap_or_default(),
custom_name_json:ecs.get::<azalea_entity::metadata::CustomName>(entity).and_then(|v|v.0.as_ref()).and_then(|v|serde_json::to_string(v).ok()).unwrap_or_default(),
custom_name_visible:ecs.get::<azalea_entity::metadata::CustomNameVisible>(entity).is_some_and(|v|v.0),armor_stand_marker:ecs.get::<azalea_entity::metadata::ArmorStandMarker>(entity).is_some_and(|v|v.0),armor_stand:armor_stand_metadata(&ecs,entity),display:display_metadata(&ecs,entity),dropped_item:dropped_item(&ecs,entity),firework:firework_metadata(&ecs,entity),equipment:equipment.get(&id.0).cloned().unwrap_or_else(||std::array::from_fn(|_|None)),hurt_ticks:animations.get(&id.0).map(|v|v.0).unwrap_or(0),death_ticks:animations.get(&id.0).map(|v|v.1).unwrap_or(0),mob_attack_ticks:golem_status.snapshot(id.0).attack,mob_flower_ticks:golem_status.snapshot(id.0).flower,living_health:ecs.get::<azalea_entity::metadata::Health>(entity).map(|v|v.0).filter(|v|v.is_finite()&&*v>=0.),living_max_health:living_max_health.get(&id.0).copied()}}).take(512).collect::<Vec<_>>();
        entities.sort_by_key(|entity|entity.id);entities
       };
       banner_registry.entities(&mut entities);
       for entity in &mut entities{

        if let Some(name)=super::player_textures::username_fallback_candidate(&entity.kind,&entity.profile){let key=name.to_ascii_lowercase();
         if let Some(Some(textures))=username_profiles.get(&key){entity.profile["decoded_textures"]=serde_json::to_value(textures).unwrap_or_default();entity.profile["skin_source"]=serde_json::json!("official_username");}
         else if profile_lookups.len()<4&&requested_profiles.len()<128&&!requested_profiles.contains(&key){if let Some(client)=&skin_client{requested_profiles.insert(key);profile_lookups.spawn(super::player_textures::lookup_username(client.clone(),name));}}
        }
       }
       if trace_entities{for entity in &entities{if traced_kinds.insert(entity.kind.clone()){eprintln!("Java entity kind observed: {} display_metadata={} custom_name={}",entity.kind,!entity.display.is_null(),!entity.custom_name.is_empty());}}}
       if let Some(client)=&skin_client {for entity in &entities {
        if skin_downloads.len()>=4 || requested_skins.len()>=128 {break;}
        if let Some(url)=entity.profile.pointer("/decoded_textures/skin_url").and_then(serde_json::Value::as_str){
         if requested_skins.insert(url.to_owned()){skin_downloads.spawn(super::player_textures::download_skin(client.clone(),url.to_owned()));}
        }
       }}
       if let Some(client)=&skin_client {for url in entity_item_skin_urls(&entities) {
        if skin_downloads.len()>=4 || requested_skins.len()>=128 {break;}
        if requested_skins.insert(url.clone()){skin_downloads.spawn(super::player_textures::download_skin(client.clone(),url));}
       }}
       if entities!=last_entities {last_entities=entities.clone();let _=tx.send(Update::Entities(entities));}
       let inventory = bot.try_query_entity::<&azalea_entity::inventory::Inventory,_>(bot.entity,|inv| {menu_properties.reconcile(inv.id,inventory_menu::kind(inv.menu()));InventorySnapshot {
        window_id: inv.id,
        menu_kind:inventory_menu::kind(inv.menu()).into(),
        slot_ranges:inventory_menu::ranges(inv.menu()),
        menu_generation:menu_properties.generation(),
        properties:menu_properties.snapshot(inv.id),
        property_entries:menu_properties.entries(inv.id),
        furnace_status:menu_properties.furnace_status(inv.id),
        title: inv.container_menu_title.as_ref().map(styled_text).unwrap_or_else(||"Inventory".into()),
        slots: inv.menu().slots().iter().map(|item|match item {azalea_inventory::ItemStack::Empty=>String::new(),azalea_inventory::ItemStack::Present(item)=>format!("{} x{}",item.get_component::<azalea_inventory::components::CustomName>().map(|n|n.name.to_string()).or_else(||item.get_component::<azalea_inventory::components::ItemName>().map(|n|n.name.to_string())).unwrap_or_else(||format!("{:?}",item.kind)),item.count)}).collect(),
        carried: match &inv.carried {azalea_inventory::ItemStack::Empty=>String::new(),azalea_inventory::ItemStack::Present(item)=>format!("{} x{}",item.get_component::<azalea_inventory::components::CustomName>().map(|n|n.name.to_string()).or_else(||item.get_component::<azalea_inventory::components::ItemName>().map(|n|n.name.to_string())).unwrap_or_else(||format!("{:?}",item.kind)),item.count)},
        carried_item: item_snapshot(&inv.carried),
        selected_hotbar: inv.selected_hotbar_slot,
        items: inv.menu().slots().iter().map(item_snapshot).collect(),
        hotbar: inv.menu().hotbar_slots_range().clone().map(|slot|inv.menu().slot(slot).and_then(item_snapshot)).collect(),
        held: item_snapshot(inv.held_item()),
        offhand: offhand_snapshot(inv),
        armor: [azalea_inventory::components::EquipmentSlot::Head,azalea_inventory::components::EquipmentSlot::Chest,azalea_inventory::components::EquipmentSlot::Legs,azalea_inventory::components::EquipmentSlot::Feet].map(|slot|inv.get_equipment(slot).and_then(item_snapshot)),
       }});
       let Ok(mut inventory)=inventory else {continue;};
       banner_registry.inventory(&mut inventory);
       if let Some(client)=&skin_client {for url in inventory_skin_urls(&inventory) {
        if skin_downloads.len()>=4 || requested_skins.len()>=128 {break;}
        if requested_skins.insert(url.clone()){skin_downloads.spawn(super::player_textures::download_skin(client.clone(),url));}
       }}
       if let Ok((can_fly,flying))=bot.try_query_entity::<&azalea_entity::PlayerAbilities,_>(bot.entity,|a|(a.can_fly,a.flying)){vitals.can_fly=can_fly;vitals.flying=flying;}
       refresh_vital_absorption(&bot.ecs.lock(),bot.entity,&mut vitals);
       hud_state.tick(&bot.ecs.lock(),bot.entity,&mut vitals);
       refresh_weapon_ranges(&bot.ecs.lock(),bot.entity,&mut vitals);
       {let mut world=bot.ecs.lock();super::native_java_use::tick(&mut world,bot.entity);refresh_native_item_use(&world,bot.entity,&mut vitals);}
       vitals.armor=server_armor.unwrap_or_else(||inventory.armor.iter().flatten().map(|item|item.armor).sum());
       if last_vitals.as_ref()!=Some(&vitals){last_vitals=Some(vitals.clone());let _=tx.send(Update::Vitals(vitals.clone()));}
       if last_inventory.as_ref() != Some(&inventory) {last_inventory=Some(inventory.clone());let _=tx.send(Update::Inventory(inventory));}
       for (x,z) in std::mem::take(&mut dirty_chunks) {
        if !capture_chunks {continue;}
        if let Some(snapshot) = snapshot_chunk(&bot,x,z) {
         let hash=fingerprint(&snapshot.states);
         if observed.get(&(x,z)) != Some(&hash) {
          observed.insert((x,z),hash);let _=tx.send(Update::Chunk(snapshot));
          if let Some(light)=lights.get(&(x,z)){let _=tx.send(Update::Light(light.clone()));}
         }
        } else {
         // Packet events can precede the ECS chunk insertion. Keep the update until ready.
         dirty_chunks.insert((x,z));
        }
       }
      },
      Event::Death(packet)=>{vitals.health=0.;guard_health_controls(&bot,vitals.health,&mut controls_alive);queue_auto_respawn(&bot,show_death_screen,spawned,&mut auto_respawn_requested);let _=tx.send(Update::Vitals(vitals.clone()));let _=tx.send(Update::Death(packet.as_ref().map(|p|p.message.to_string()).unwrap_or_else(||"You died!".into())));},
      Event::Chat(chat)=>{
       if chat_is_action_bar(&chat){let _=tx.send(Update::ActionBar(styled_text(&chat.message())));continue;}
       if let azalea_client::chat::ChatPacket::Player(packet)=&chat {
        if let Some(signature)=&packet.signature {
         let last_seen={let mut world=bot.ecs.lock();world.get_mut::<azalea_client::chat::secure_state::SecureChatState>(bot.entity).and_then(|mut state|state.take_received(signature))};
         if let Some(last_seen)=last_seen{if !incoming_chat.push(packet.clone(),last_seen.last_seen,last_seen.remote){let _=tx.send(Update::Error("Signed chat verification queue exceeded its limit".into()));bot.disconnect();break;}}
         continue;
        }
        if incoming_chat.requires_signed(packet.sender)||matches!(packet.filter_mask,azalea_protocol::packets::game::c_player_chat::FilterMask::FullyFiltered){continue;}
       }
       let message=chat.message();
       let actions=chat_actions(&message);
       let _=tx.send(Update::Chat(styled_text(&message)));
       if !actions.is_empty() {let _=tx.send(Update::ChatActions(actions));}
      },
      Event::ConfigPacket(packet)=>{
       if let azalea_protocol::packets::config::ClientboundConfigPacket::RegistryData(p)=packet.as_ref(){banner_registry.ingest(p);}
       use azalea_protocol::packets::ProtocolPacket;
       if let azalea_protocol::packets::config::ClientboundConfigPacket::ShowDialog(p)=packet.as_ref() {
        if let Ok(tag)=serde_json::to_value(&p.dialog) {let _=tx.send(Update::ServerDialog(tag));}
       }
       match packet.as_ref(){
        azalea_protocol::packets::config::ClientboundConfigPacket::UpdateTags(p)=>{recipes.update_tags(&p.tags);let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));},
        azalea_protocol::packets::config::ClientboundConfigPacket::ResourcePackPush(p)=>{pack_scope.push(super::resource_packs::PackRequest{id:p.id.to_string(),url:p.url.clone(),hash:p.hash.clone(),required:p.required,prompt:p.prompt.as_ref().map(styled_text).unwrap_or_default()});let _=tx.send(Update::ResourcePacks(pack_scope.snapshot()));},
        azalea_protocol::packets::config::ClientboundConfigPacket::ResourcePackPop(p)=>{pack_scope.pop(p.id.as_ref().map(|id|id.to_string()).as_deref());let _=tx.send(Update::ResourcePacks(pack_scope.snapshot()));},_=>{}
       }
       let name=packet.name();
       if !matches!(name,"keep_alive"|"ping"|"registry_data"|"custom_payload") {let _=tx.send(Update::ConfigurationPhase(name.to_owned()));}
      },
      Event::Packet(packet)=>{
       if matches!(packet.as_ref(),azalea_protocol::packets::game::ClientboundGamePacket::StartConfiguration(_)|azalea_protocol::packets::game::ClientboundGamePacket::Transfer(_)){banner_registry.clear();}
       if recipes.ingest(packet.as_ref()){let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));}
       if compass.ingest(packet.as_ref()){let _=tx.send(Update::Compass(compass.clone()));}
       use azalea_protocol::packets::game::ClientboundGamePacket;
       hud_state.packet(packet.as_ref(),&mut vitals);
       match packet.as_ref(){
        ClientboundGamePacket::PlayerInfoUpdate(p)=>incoming_chat.observe(p),
        ClientboundGamePacket::PlayerInfoRemove(p)=>incoming_chat.remove(&p.profile_ids),
        ClientboundGamePacket::Login(_)=>incoming_chat.reset(),
        ClientboundGamePacket::DeleteChat(p)=>{
         let signature={let world=bot.ecs.lock();world.get::<azalea_client::chat::secure_state::SecureChatState>(bot.entity).and_then(|state|state.resolve(&p.signature))};
         if let Some(signature)=signature{incoming_chat.delete(&signature);let _=tx.send(Update::DeleteChat(incoming_chat::signature_id(&signature)));}
        },_=>{}
       }

       if trace_world{match packet.as_ref(){ClientboundGamePacket::LevelChunkWithLight(p)=>eprintln!("TCPE world chunk packet: {},{} bytes={}",p.x,p.z,p.chunk_data.data.len()),ClientboundGamePacket::ChunkBatchStart(_)=>eprintln!("TCPE world batch start"),ClientboundGamePacket::ChunkBatchFinished(p)=>eprintln!("TCPE world batch finish: {:?}",p),ClientboundGamePacket::SetChunkCacheCenter(p)=>eprintln!("TCPE world cache center: {},{}",p.x,p.z),_=>{}}}
       match packet.as_ref() {
        ClientboundGamePacket::Cooldown(p)=>{item_cooldowns.set(p.cooldown_group.to_string(),p.duration);},
        ClientboundGamePacket::UpdateTags(p)=>{recipes.update_tags(&p.tags);let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));},
        ClientboundGamePacket::StopSound(p)=>{let _=tx.send(Update::Sound(sounds::stop(p)));},
        ClientboundGamePacket::Sound(p)=>{if let Some(sound)=sounds::positional(p){let _=tx.send(Update::Sound(sound));}},
        ClientboundGamePacket::SoundEntity(p)=>{if let Some(sound)=sounds::entity(p,&bot.ecs.lock(),bot.entity){let _=tx.send(Update::Sound(sound));}},
        ClientboundGamePacket::Explode(p)=>{if let Some(effect)=effects::gust(&p.explosion_particle,p.center){let _=tx.send(Update::VisualEffect(effect));}if let Some(sound)=sounds::explosion(p){let _=tx.send(Update::Sound(sound));}},
        ClientboundGamePacket::LevelParticles(p)=>{if let Some(effect)=effects::gust_batch(p){let _=tx.send(Update::VisualEffect(effect));}},
        ClientboundGamePacket::ResourcePackPush(p)=>{pack_scope.push(super::resource_packs::PackRequest{id:p.id.to_string(),url:p.url.clone(),hash:p.hash.clone(),required:p.required,prompt:p.prompt.as_ref().map(styled_text).unwrap_or_default()});let _=tx.send(Update::ResourcePacks(pack_scope.snapshot()));},
        ClientboundGamePacket::ResourcePackPop(p)=>{pack_scope.pop(p.id.as_ref().map(|id|id.to_string()).as_deref());let _=tx.send(Update::ResourcePacks(pack_scope.snapshot()));},
        ClientboundGamePacket::GameEvent(p) if matches!(p.event,azalea_protocol::packets::game::c_game_event::EventType::ImmediateRespawn)=>{show_death_screen=p.param==0.;vitals.show_death_screen=show_death_screen;let _=tx.send(Update::Vitals(vitals.clone()));},
        ClientboundGamePacket::PlayerAbilities(_)=>{{let mut world=bot.ecs.lock();if let Ok(mut entity)=world.get_entity_mut(bot.entity){entity.insert(azalea_physics::FlightInput::default());}refresh_vital_abilities(&world,bot.entity,&mut vitals);}let _=tx.send(Update::Vitals(vitals.clone()));},
        ClientboundGamePacket::PlayerPosition(p)=>{{let mut world=bot.ecs.lock();world.entity_mut(bot.entity).remove::<(targeting::HeldAim,azalea_client::interact::pick::NativePickRay,azalea_client::interact::pick::NativePickStamp)>();}if trace_world {eprintln!("TCPE world server correction: {:?} relative={:?}",p.change,p.relative);}let yaw=p.change.look_direction.y_rot();let pitch=p.change.look_direction.x_rot();if yaw.is_finite()&&pitch.is_finite(){let _=tx.send(Update::LookCorrection{yaw,pitch,relative_yaw:p.relative.y_rot,relative_pitch:p.relative.x_rot});}},
        ClientboundGamePacket::Login(_) | ClientboundGamePacket::Respawn(_) =>{clear_native_item_use(&mut bot.ecs.lock(),bot.entity);let (dimension,is_respawn)=match packet.as_ref(){ClientboundGamePacket::Login(p)=>(p.common.dimension.to_string(),false),ClientboundGamePacket::Respawn(p)=>(p.common.dimension.to_string(),true),_=>unreachable!()};let preserve_world=preserve_respawn_world(&mut world_dimension,dimension,is_respawn);if trace_world {eprintln!("TCPE world reset: respawn={} dimension={} preserve={}",is_respawn,world_dimension.as_deref().unwrap_or(""),preserve_world);}if let ClientboundGamePacket::Login(p)=packet.as_ref(){recipes.clear();let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));show_death_screen=p.show_death_screen;pack_scope.game_login();let _=tx.send(Update::ResourcePacks(pack_scope.snapshot()));}auto_respawn_requested=false;spawned=false;if preserve_world{dirty_chunks.extend(observed.keys().copied());}else{lights.clear();dirty_chunks.clear();}observed.clear();last_inventory=None;menu_properties.reset();last_entities.clear();if !preserve_world{entity_angles.clear();equipment.clear();equipment_raw.clear();remote_use.clear();animations.clear();golem_status.clear();living_max_health.clear();super::native_java_use::clear_target_max_health(&mut bot.ecs.lock(),bot.entity);}recent_attacks.clear();item_cooldowns.clear();controls_alive=true;vitals=PlayerVitals::default();vitals.show_death_screen=show_death_screen;last_vitals=None;server_armor=None;local_id_seen=None;targeting_epoch=targeting_epoch.saturating_add(1);{let mut world=bot.ecs.lock();world.entity_mut(bot.entity).remove::<(azalea_client::interact::pick::NativePickRay,azalea_client::interact::pick::NativePickStamp,azalea_client::interact::pick::HitResultComponent)>();}let _=tx.send(Update::Login);let _=tx.send(Update::Targeting(serde_json::json!({"epoch":targeting_epoch,"revision":0,"input_revision":0,"kind":"clear"})));let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));let _=tx.send(Update::Vitals(vitals.clone()));},
        ClientboundGamePacket::StartConfiguration(_)=>{clear_native_item_use(&mut bot.ecs.lock(),bot.entity);world_dimension=None;recipes.clear_tags();recipes.clear();let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));pack_scope.begin_configuration();let _=tx.send(Update::ResourcePacks(pack_scope.snapshot()));spawned=false;observed.clear();lights.clear();dirty_chunks.clear();last_inventory=None;menu_properties.reset();last_entities.clear();entity_angles.clear();equipment.clear();equipment_raw.clear();remote_use.clear();animations.clear();golem_status.clear();living_max_health.clear();super::native_java_use::clear_target_max_health(&mut bot.ecs.lock(),bot.entity);recent_attacks.clear();item_cooldowns.clear();controls_alive=true;vitals=PlayerVitals::default();vitals.show_death_screen=show_death_screen;last_vitals=None;server_armor=None;local_id_seen=None;targeting_epoch=targeting_epoch.saturating_add(1);{let mut world=bot.ecs.lock();world.entity_mut(bot.entity).remove::<(azalea_client::interact::pick::NativePickRay,azalea_client::interact::pick::NativePickStamp,azalea_client::interact::pick::HitResultComponent)>();}let _=tx.send(Update::Login);let _=tx.send(Update::Targeting(serde_json::json!({"epoch":targeting_epoch,"revision":0,"input_revision":0,"kind":"clear"})));let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));let _=tx.send(Update::Vitals(vitals.clone()));},
        ClientboundGamePacket::Transfer(p)=>{world_dimension=None;recipes.clear_tags();recipes.clear();let _=tx.send(Update::Recipes(banner_registry.recipes(recipes.snapshot())));pack_scope.begin_configuration();let _=tx.send(Update::ResourcePacks(pack_scope.snapshot()));
         if p.host.is_empty() || p.host.len()>255 || p.port>65535 {let _=tx.send(Update::Error("Invalid server transfer address".into()));continue;}
         let address=if p.host.contains(':') {format!("[{}]:{}",p.host,p.port)} else {format!("{}:{}",p.host,p.port)};
         let cookies=bot.try_query_entity::<Option<&azalea_client::cookies::ServerCookies>,_>(bot.entity,|c|c.map(|c|c.map.clone())).ok().flatten();
         let _=tx.send(Update::Chat(format!("Transferring to {address}")));
         bot.disconnect();spawned=false;observed.clear();lights.clear();dirty_chunks.clear();last_inventory=None;menu_properties.reset();last_entities.clear();entity_angles.clear();equipment.clear();equipment_raw.clear();remote_use.clear();animations.clear();golem_status.clear();living_max_health.clear();super::native_java_use::clear_target_max_health(&mut bot.ecs.lock(),bot.entity);recent_attacks.clear();item_cooldowns.clear();controls_alive=true;vitals=PlayerVitals::default();vitals.show_death_screen=show_death_screen;last_vitals=None;server_armor=None;local_id_seen=None;
         targeting_epoch=targeting_epoch.saturating_add(1);{let mut world=bot.ecs.lock();world.entity_mut(bot.entity).remove::<(azalea_client::interact::pick::NativePickRay,azalea_client::interact::pick::NativePickStamp,azalea_client::interact::pick::HitResultComponent)>();}let _=tx.send(Update::Login);let _=tx.send(Update::Targeting(serde_json::json!({"epoch":targeting_epoch,"revision":0,"input_revision":0,"kind":"clear"})));
         match Client::join_transfer(account.clone(),address.as_str()).await {
          Ok((next,next_events))=>{if let Some(map)=cookies {next.ecs.lock().entity_mut(next.entity).insert(azalea_client::cookies::ServerCookies{map});} bot=next;events=next_events;},
          Err(e)=>{let _=tx.send(Update::Error(e.to_string()));break;}
         }
        },
        ClientboundGamePacket::ShowDialog(p)=>{
         let value=match &p.dialog {
          azalea_registry::Holder::Direct(tag)=>serde_json::to_value(tag).ok(),
          azalea_registry::Holder::Reference(id)=>bot.with_resolved_registry(*id,|_,tag|serde_json::to_value(tag).ok()).flatten(),
         };
         if let Some(value)=value{let _=tx.send(Update::ServerDialog(value));}
         else {let _=tx.send(Update::Error("Server dialog reference could not be resolved".into()));}
        },
        ClientboundGamePacket::ClearDialog(_)=>{let _=tx.send(Update::ServerDialog(serde_json::json!({"cleared":true})));},
        ClientboundGamePacket::LevelChunkWithLight(p) if capture_chunks=>{
         if let Some(light)=snapshot_light(&bot,p.x,p.z,&p.light_data,lights.get(&(p.x,p.z))){lights.insert((p.x,p.z),light.clone());if observed.contains_key(&(p.x,p.z)){let _=tx.send(Update::Light(light));}}
        },
        ClientboundGamePacket::LightUpdate(p) if capture_chunks=>{
         if let Some(light)=snapshot_light(&bot,p.x,p.z,&p.light_data,lights.get(&(p.x,p.z))){lights.insert((p.x,p.z),light.clone());if observed.contains_key(&(p.x,p.z)){let _=tx.send(Update::Light(light));}}
        },
        ClientboundGamePacket::SetTime(p)=>{let _=tx.send(Update::Time{day:p.day_time,ticking:p.tick_day_time});},
        ClientboundGamePacket::SetObjective(_)|ClientboundGamePacket::SetScore(_)|ClientboundGamePacket::ResetScore(_)|ClientboundGamePacket::SetDisplayObjective(_)|ClientboundGamePacket::SetPlayerTeam(_)=>{if let Some(update)=scoreboard_update(packet.as_ref()){let _=tx.send(Update::Scoreboard(update));}},
        ClientboundGamePacket::BossEvent(p)=>{let update=boss_update(p);if trace_entities{eprintln!("Java boss packet: {:?}",update);}let _=tx.send(Update::Boss(update));},
        ClientboundGamePacket::SetTitleText(p)=>{let _=tx.send(Update::Title(TitleUpdate::Title{text:styled_text(&p.text)}));},
        ClientboundGamePacket::SetSubtitleText(p)=>{let _=tx.send(Update::Title(TitleUpdate::Subtitle{text:styled_text(&p.text)}));},
        ClientboundGamePacket::SetTitlesAnimation(p)=>{let _=tx.send(Update::Title(TitleUpdate::Times{fade_in:p.fade_in as i32,stay:p.stay as i32,fade_out:p.fade_out as i32}));},
        ClientboundGamePacket::ClearTitles(p)=>{let _=tx.send(Update::Title(TitleUpdate::Clear{reset_times:p.reset_times}));},
        ClientboundGamePacket::SetActionBarText(p)=>{let _=tx.send(Update::ActionBar(styled_text(&p.text)));},
        ClientboundGamePacket::SetCursorItem(p)=>{if let Some(mut inventory)=bot.ecs.lock().get_mut::<azalea_entity::inventory::Inventory>(bot.entity){inventory.carried=p.contents.clone();}},
        ClientboundGamePacket::SetEntityData(p)=>{if p.packed_items.0.iter().any(|item|item.index==8)&&bot.ecs.lock().get::<azalea_world::MinecraftEntityId>(bot.entity).is_some_and(|id|*id==p.id){super::native_java_use::authoritative(&mut bot.ecs.lock(),bot.entity);}},
        ClientboundGamePacket::ContainerSetContent(p)=>{if let Some(mut inventory)=bot.ecs.lock().get_mut::<azalea_entity::inventory::Inventory>(bot.entity){apply_authoritative_cursor(&mut inventory,p);}},
        ClientboundGamePacket::SetExperience(p)=>{apply_experience(&mut vitals,p);let _=tx.send(Update::Vitals(vitals.clone()));},
        ClientboundGamePacket::SetHealth(p)=>{vitals.health=p.health;vitals.food=p.food;vitals.saturation=p.saturation;refresh_vital_abilities(&bot.ecs.lock(),bot.entity,&mut vitals);guard_health_controls(&bot,vitals.health,&mut controls_alive);let _=tx.send(Update::Vitals(vitals.clone()));},
        ClientboundGamePacket::OpenScreen(p)=>{let menu=azalea_inventory::Menu::from_kind(p.menu_type);menu_properties.open(p.container_id,inventory_menu::kind(&menu));last_inventory=None;},
        ClientboundGamePacket::ContainerSetData(p)=>{menu_properties.set(p.container_id,p.id,p.value);},
        // Vanilla/Azalea close the active menu regardless of this packet's ID.
        ClientboundGamePacket::ContainerClose(_)=>{menu_properties.reset();last_inventory=None;},
        ClientboundGamePacket::SetEquipment(p)=>{let raw=equipment_raw.entry(p.entity_id.0).or_insert_with(||std::array::from_fn(|_|azalea_inventory::ItemStack::Empty));let slots=equipment.entry(p.entity_id.0).or_insert_with(||std::array::from_fn(|_|None));for (slot,item) in &p.slots.slots {if (*slot as usize)<6 {slots[*slot as usize]=item_snapshot(item);raw[*slot as usize]=item.clone();}}},
        ClientboundGamePacket::HurtAnimation(p)=>{animations.entry(p.id.0).or_default().0=10;let _=tx.send(Update::Hurt{id:p.id.0,yaw:p.yaw});},
        ClientboundGamePacket::DamageEvent(p)=>{animations.entry(p.entity_id.0).or_default().0=10;let _=tx.send(Update::Hurt{id:p.entity_id.0,yaw:0.});if trace_entities{let local=bot.try_query_entity::<&azalea_world::MinecraftEntityId,_>(bot.entity,|id|id.0).ok().or(local_id_seen);if let Some(source)=damage_attribution(p,local){eprintln!("Java server damage attributed to local player: target={} source={:?}",p.entity_id.0,source);if recent_attacks.iter().rev().any(|(target,time)|recent_direct_damage(p,local,*target,time.elapsed())){eprintln!("Java server direct damage matched recent selected attack: target={}",p.entity_id.0);}}}},
        ClientboundGamePacket::EntityEvent(p)=>{golem_status.event(p.entity_id.0,p.event_id);if p.event_id==3 {animations.entry(p.entity_id.0).or_default().1=1;}},
        ClientboundGamePacket::UpdateAttributes(p)=>{for value in &p.values {if format!("{:?}",value.attribute)=="MaxHealth" {
         let mut amount=value.base;for modifier in &value.modifiers{if format!("{:?}",modifier.operation)=="AddValue"{amount+=modifier.amount;}}let base=amount;for modifier in &value.modifiers{if format!("{:?}",modifier.operation)=="AddMultipliedBase"{amount+=base*modifier.amount;}}for modifier in &value.modifiers{if format!("{:?}",modifier.operation)=="AddMultipliedTotal"{amount*=1.+modifier.amount;}}
         let amount=amount as f32;if amount.is_finite()&&amount>0. {living_max_health.insert(p.entity_id.0,amount);}else{living_max_health.remove(&p.entity_id.0);}super::native_java_use::target_max_health(&mut bot.ecs.lock(),bot.entity,p.entity_id.0,living_max_health.get(&p.entity_id.0).copied());
        }}if bot.try_query_entity::<&azalea_world::MinecraftEntityId,_>(bot.entity,|id|id.0==p.entity_id.0).unwrap_or(false){for value in &p.values {let kind=format!("{:?}",value.attribute);if kind=="Armor" || kind=="MaxHealth" {
          let mut amount=value.base;for modifier in &value.modifiers{if format!("{:?}",modifier.operation)=="AddValue"{amount+=modifier.amount;}}let base=amount;for modifier in &value.modifiers{if format!("{:?}",modifier.operation)=="AddMultipliedBase"{amount+=base*modifier.amount;}}for modifier in &value.modifiers{if format!("{:?}",modifier.operation)=="AddMultipliedTotal"{amount*=1.+modifier.amount;}}if kind=="Armor"{server_armor=Some(amount as f32);vitals.armor=amount as f32;}else{vitals.max_health=amount as f32;}
        }}}},
        ClientboundGamePacket::AddEntity(p)=>{living_max_health.remove(&p.id.0);golem_status.spawn(p.id.0,format!("{:?}",p.entity_type)=="IronGolem");entity_angles.insert(p.id.0,[p.y_rot as f32*360./256.,p.x_rot as f32*360./256.,p.y_head_rot as f32*360./256.]);},
        ClientboundGamePacket::RotateHead(p)=>{entity_angles.entry(p.entity_id.0).or_default()[2]=p.y_head_rot as f32*360./256.;},
        ClientboundGamePacket::MoveEntityRot(p)=>{let angles=entity_angles.entry(p.entity_id.0).or_default();angles[0]=p.y_rot as f32*360./256.;angles[1]=p.x_rot as f32*360./256.;},
        ClientboundGamePacket::MoveEntityPosRot(p)=>{let angles=entity_angles.entry(p.entity_id.0).or_default();angles[0]=p.y_rot as f32*360./256.;angles[1]=p.x_rot as f32*360./256.;},
        ClientboundGamePacket::EntityPositionSync(p)=>{let angles=entity_angles.entry(p.id.0).or_default();angles[0]=p.values.look_direction.y_rot();angles[1]=p.values.look_direction.x_rot();},
        ClientboundGamePacket::RemoveEntities(p)=>{for id in &p.entity_ids{entity_angles.remove(&id.0);equipment.remove(&id.0);equipment_raw.remove(&id.0);remote_use.remove(&id.0);animations.remove(&id.0);golem_status.remove(id.0);living_max_health.remove(&id.0);super::native_java_use::target_max_health(&mut bot.ecs.lock(),bot.entity,id.0,None);recent_attacks.retain(|(target,_)|*target!=id.0);}let _=tx.send(Update::RemovedEntities(p.entity_ids.iter().map(|id|id.0).collect()));},
        ClientboundGamePacket::BlockUpdate(p)=>{dirty_chunks.insert((p.pos.x.div_euclid(16),p.pos.z.div_euclid(16)));},
        ClientboundGamePacket::SectionBlocksUpdate(p)=>{dirty_chunks.insert((p.section_pos.x,p.section_pos.z));},
        ClientboundGamePacket::ForgetLevelChunk(p)=>{lights.remove(&(p.pos.x,p.pos.z));observed.remove(&(p.pos.x,p.pos.z));dirty_chunks.remove(&(p.pos.x,p.pos.z));let _=tx.send(Update::UnloadChunk(p.pos.x,p.pos.z));},
        _=>{}
       }
      },
      Event::ReceiveChunk(pos) if capture_chunks=>{
       if let Some(snapshot) = snapshot_chunk(&bot,pos.x,pos.z) {
        observed.insert((pos.x,pos.z),fingerprint(&snapshot.states));
        let _ = tx.send(Update::Chunk(snapshot));
        if trace_world {eprintln!("TCPE world chunk captured: {},{}",pos.x,pos.z);}
        if let Some(light)=lights.get(&(pos.x,pos.z)){let _=tx.send(Update::Light(light.clone()));}
       } else {
        dirty_chunks.insert((pos.x,pos.z));
        if trace_world {eprintln!("TCPE world chunk deferred: {},{}",pos.x,pos.z);}
       }
      },
      Event::Disconnect(reason)=>{let _=tx.send(Update::Disconnected(reason.map(|r|r.to_string()).unwrap_or_else(||"Connection closed".into())));break;},_=>{}
     }}
    }}
    compass.clear();let _=tx.send(Update::Compass(compass));
   });
        });
        Self { commands, updates }
    }
}

fn fingerprint(states: &[u16]) -> u64 {
    use std::hash::{Hash, Hasher};
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    states.hash(&mut hasher);
    hasher.finish()
}
fn snapshot_chunk(bot: &Client, x: i32, z: i32) -> Option<ChunkSnapshot> {
    let instance = bot
        .try_query_entity::<&azalea_client::local_player::InstanceHolder, _>(bot.entity, |holder| {
            holder.instance.clone()
        })
        .ok()?;
    let instance = instance.read();
    let min_y = instance.chunks.min_y;
    let chunk = instance
        .chunks
        .get(&azalea_core::position::ChunkPos { x, z })?;
    let chunk = chunk.read();
    let height = chunk.sections.len() * 16;
    let mut states = vec![0; 16 * height * 16];
    for x in 0u8..16 {
        for z in 0u8..16 {
            for (section_index, section) in chunk.sections.iter().enumerate() {
                for y in 0u8..16 {
                    states[(x as usize * 16 + z as usize) * height
                        + section_index * 16
                        + y as usize] = section
                        .get_block_state(ChunkSectionBlockPos { x, y, z })
                        .id();
                }
            }
        }
    }
    Some(ChunkSnapshot {
        x,
        z,
        min_y,
        height,
        states,
    })
}

fn chat_actions(message: &azalea_chat::FormattedText) -> Vec<(String, String)> {
    let mut result = Vec::new();
    fn visit(message: &azalea_chat::FormattedText, result: &mut Vec<(String, String)>) {
        if let Some(azalea_chat::click_event::ClickEvent::RunCommand { command }) =
            &message.get_base().style.click_event
        {
            result.push((styled_text(message), command.clone()));
        }
        for child in &message.get_base().siblings {
            visit(child, result);
        }
    }
    visit(message, &mut result);
    result
}

fn snapshot_light(bot:&Client,x:i32,z:i32,data:&azalea_protocol::packets::game::c_light_update::ClientboundLightUpdatePacketData,previous:Option<&LightSnapshot>)->Option<LightSnapshot>{
 let (min_y,height)=bot.try_query_entity::<&azalea_client::local_player::InstanceHolder,_>(bot.entity,|holder|{let world=holder.instance.read();(world.chunks.min_y,world.chunks.height as usize)}).ok()?;
 if height==0||height>4096||height%16!=0{return None;}
 let mut light=previous.filter(|p|p.min_y==min_y&&p.height==height).cloned().unwrap_or_else(||LightSnapshot{x,z,min_y,height,sky_light:vec![0;height*128],block_light:vec![0;height*128],light_sections:vec![0;height/16]});
 merge_light_planes(&mut light,data)?;Some(light)
}
fn merge_light_planes(light:&mut LightSnapshot,data:&azalea_protocol::packets::game::c_light_update::ClientboundLightUpdatePacketData)->Option<()>{
 for (mask,empty,updates,plane,flag) in [(&data.sky_y_mask,&data.empty_sky_y_mask,&data.sky_updates,&mut light.sky_light,1u8),(&data.block_y_mask,&data.empty_block_y_mask,&data.block_updates,&mut light.block_light,2u8)]{
  let mut update=0;
  for section in 0..light.height/16+2{
   let supplied=mask.get(section).unwrap_or(false);let zero=empty.get(section).unwrap_or(false);
   if supplied&&zero{return None;}
   let bytes=if supplied{let bytes=updates.get(update)?;update+=1;if bytes.len()!=2048{return None;}Some(bytes)}else{None};
   if section==0||section==light.height/16+1{continue;}
   let begin=(section-1)*2048;
   if let Some(bytes)=bytes{plane[begin..begin+2048].copy_from_slice(bytes);light.light_sections[section-1]|=flag;}
   else if zero{plane[begin..begin+2048].fill(0);light.light_sections[section-1]|=flag;}
  }
  if update!=updates.len(){return None;}
 }
 Some(())
}

#[cfg(test)]mod light_tests{
 use super::*;use azalea_protocol::packets::game::c_light_update::ClientboundLightUpdatePacketData as Data;
 #[test]fn padded_masks_and_partial_updates(){
  let mut light=LightSnapshot{x:-1,z:-2,min_y:-64,height:32,sky_light:vec![0;4096],block_light:vec![0;4096],light_sections:vec![0;2]};
  let mut data=Data::default();data.sky_y_mask=azalea_core::bitset::BitSet::new(64);data.empty_sky_y_mask=azalea_core::bitset::BitSet::new(64);data.empty_block_y_mask=azalea_core::bitset::BitSet::new(64);data.sky_y_mask.set(0);data.sky_y_mask.set(1);data.sky_y_mask.set(3);data.sky_updates=vec![vec![0x33;2048],vec![0xf2;2048],vec![0x44;2048]];data.empty_block_y_mask.set(2);
  assert!(merge_light_planes(&mut light,&data).is_some());assert_eq!(light.light_sections,vec![1,2]);assert_eq!(light.sky_light[0],0xf2);assert_eq!(light.sky_light[2048],0);
  let mut data=Data::default();data.sky_y_mask=azalea_core::bitset::BitSet::new(64);data.empty_sky_y_mask=azalea_core::bitset::BitSet::new(64);data.empty_block_y_mask=azalea_core::bitset::BitSet::new(64);data.empty_sky_y_mask.set(2);assert!(merge_light_planes(&mut light,&data).is_some());assert_eq!(light.light_sections,vec![1,3]);assert_eq!(light.sky_light[0],0xf2);
 }
 #[test]fn malformed_arrays_and_conflicting_masks_rejected(){
  let mut light=LightSnapshot{x:0,z:0,min_y:-64,height:16,sky_light:vec![0;2048],block_light:vec![0;2048],light_sections:vec![0]};let mut data=Data::default();data.sky_y_mask=azalea_core::bitset::BitSet::new(64);data.empty_sky_y_mask=azalea_core::bitset::BitSet::new(64);data.empty_block_y_mask=azalea_core::bitset::BitSet::new(64);data.sky_y_mask.set(1);data.sky_updates=vec![vec![0;2047]];assert!(merge_light_planes(&mut light,&data).is_none());data.sky_updates[0].push(0);data.empty_sky_y_mask.set(1);assert!(merge_light_planes(&mut light,&data).is_none());
 }
}

fn display_metadata(ecs:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->serde_json::Value{
 use azalea_entity::metadata::*;use serde_json::json;
 if ecs.get::<AbstractDisplay>(entity).is_none(){return serde_json::Value::Null;}
 let mut value=json!({});
 macro_rules! scalar {($field:literal,$kind:ty)=>{if let Some(v)=ecs.get::<$kind>(entity){value[$field]=json!(v.0);}};}
 macro_rules! vector {($field:literal,$kind:ty)=>{if let Some(v)=ecs.get::<$kind>(entity){value[$field]=json!([v.0.x,v.0.y,v.0.z]);}};}
 macro_rules! rotation {($field:literal,$kind:ty)=>{if let Some(v)=ecs.get::<$kind>(entity){value[$field]=json!([v.0.x,v.0.y,v.0.z,v.0.w]);}};}
 vector!("translation",Translation);vector!("scale",Scale);rotation!("left_rotation",LeftRotation);rotation!("right_rotation",RightRotation);
 scalar!("billboard",BillboardRenderConstraints);scalar!("brightness",BrightnessOverride);scalar!("view_range",ViewRange);scalar!("width",AbstractDisplayWidth);scalar!("height",AbstractDisplayHeight);
 scalar!("line_width",LineWidth);scalar!("background_color",BackgroundColor);scalar!("text_opacity",TextOpacity);scalar!("style_flags",StyleFlags);scalar!("item_transform",ItemDisplayItemDisplay);
 if let Some(v)=ecs.get::<Text>(entity){value["text"]=json!(styled_text(&v.0));value["text_json"]=json!(serde_json::to_string(&v.0).unwrap_or_default());}
 if let Some(v)=ecs.get::<BlockDisplayBlockState>(entity){value["block_state"]=json!(v.0.id());}
 if let Some(v)=ecs.get::<ItemDisplayItemStack>(entity){value["item"]=json!(item_snapshot(&v.0));}
 value
}

#[cfg(test)]mod display_tests{
 use super::*;
 #[test]fn default_text_display_opacity_matches_java_and_keeps_explicit_override(){
  use azalea_entity::metadata::{TextDisplayMetadataBundle,TextOpacity};
  let mut ecs=bevy_ecs::world::World::new();let entity=ecs.spawn(TextDisplayMetadataBundle::default()).id();
  assert_eq!(ecs.get::<TextOpacity>(entity).unwrap().0,255);
  assert_eq!(display_metadata(&ecs,entity)["text_opacity"],255);
  ecs.entity_mut(entity).insert(TextOpacity(77));assert_eq!(display_metadata(&ecs,entity)["text_opacity"],77);
 }

 #[test]fn display_snapshot_retains_unicode_rgb_transform_and_transparency(){
  use azalea_entity::metadata::*;use azalea_core::position::Vec3f32;
  let mut ecs=bevy_ecs::world::World::new();
  let text:azalea_chat::FormattedText=serde_json::from_value(serde_json::json!({"text":"雪","color":"#0123ab","italic":true})).unwrap();
  let entity=ecs.spawn((AbstractDisplay,Text(Box::new(text)),Translation(Vec3f32{x:-1.,y:2.,z:3.}),Scale(Vec3f32{x:2.,y:3.,z:4.}),BillboardRenderConstraints(3),BackgroundColor(i32::MIN),TextOpacity(0),LineWidth(120))).id();
  let value=display_metadata(&ecs,entity);
  assert!(value["text"].as_str().unwrap().contains("§x§0§1§2§3§a§b§o雪"));
  assert_eq!(value["translation"],serde_json::json!([-1.,2.,3.]));assert_eq!(value["scale"],serde_json::json!([2.,3.,4.]));assert_eq!(value["billboard"],3);assert_eq!(value["background_color"],i32::MIN);assert_eq!(value["text_opacity"],0);
  let component:serde_json::Value=serde_json::from_str(value["text_json"].as_str().unwrap()).unwrap();assert_eq!(component["color"],"#0123AB");
  let ordinary=ecs.spawn_empty().id();assert!(display_metadata(&ecs,ordinary).is_null());
 }
}

fn armor_stand_metadata(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->serde_json::Value{
 use azalea_entity::metadata::*;
 if world.get::<azalea_entity::EntityKindComponent>(entity).is_none_or(|v|v.0!=azalea_registry::builtin::EntityKind::ArmorStand){return serde_json::Value::Null}
 let pose=|v:Option<&azalea_entity::Rotations>,fallback:[f32;3]|v.map(|v|[v.x,v.y,v.z]).filter(|p|p.iter().all(|n|n.is_finite()&&n.abs()<=36000.)).unwrap_or(fallback);
 serde_json::json!({"small":world.get::<Small>(entity).is_some_and(|v|v.0),"show_arms":world.get::<ShowArms>(entity).is_some_and(|v|v.0),
 // Vendored ShowBasePlate stores the wire's NO_BASE_PLATE bit.
 "show_base_plate":!world.get::<ShowBasePlate>(entity).is_some_and(|v|v.0),
 "poses":[pose(world.get::<HeadPose>(entity).map(|v|&v.0),[0.;3]),pose(world.get::<BodyPose>(entity).map(|v|&v.0),[0.;3]),pose(world.get::<LeftArmPose>(entity).map(|v|&v.0),[-10.,0.,-10.]),pose(world.get::<RightArmPose>(entity).map(|v|&v.0),[-15.,0.,10.]),pose(world.get::<LeftLegPose>(entity).map(|v|&v.0),[-1.,0.,-1.]),pose(world.get::<RightLegPose>(entity).map(|v|&v.0),[1.,0.,1.])]})
}
fn player_profile(world:&bevy_ecs::world::World, entity:bevy_ecs::entity::Entity)->serde_json::Value {
    let (uuid,name,properties)=if let Some(profile)=world.get::<azalea_client::player::GameProfileComponent>(entity){
        (Some(profile.uuid),Some(profile.name.as_str()),profile.properties.as_ref())
    }else if let Some(profile)=world.get::<azalea_entity::metadata::Profile>(entity){
        match &*profile.0.unpack{
            azalea_inventory::components::PartialOrFullProfile::Full(profile)=>(Some(profile.uuid),Some(profile.name.as_str()),profile.properties.as_ref()),
            azalea_inventory::components::PartialOrFullProfile::Partial(profile)=>(profile.id,profile.name.as_deref(),&profile.properties),
        }
    }else{return serde_json::Value::Null;};
    let textures=properties.map.get("textures");
    serde_json::json!({"uuid":uuid.map(|id|id.to_string()),"name":name,
        "textures":textures.map(|v|v.value.as_str()),"textures_signature":textures.and_then(|v|v.signature.as_deref()),"decoded_textures":textures.and_then(|v|super::player_textures::decode(&v.value))})
}
#[cfg(test)] mod profile_tests {
    use super::*;
    #[test]fn mannequin_full_profile_preserves_server_texture_and_signature(){
        use base64::Engine;
        let mut full=azalea_auth::game_profile::GameProfile::default();full.name="ServerNpc".into();
        let encoded=base64::engine::general_purpose::STANDARD.encode(serde_json::to_vec(&serde_json::json!({"textures":{"SKIN":{"url":format!("https://textures.minecraft.net/texture/{}","a".repeat(64)),"metadata":{"model":"slim"}}}})).unwrap());
        std::sync::Arc::make_mut(&mut full.properties).map.insert("textures".into(),azalea_auth::game_profile::ProfilePropertyValue{value:encoded.clone(),signature:Some("supplied-signature".into())});
        let mut profile=azalea_inventory::components::Profile::default();profile.unpack=Box::new(azalea_inventory::components::PartialOrFullProfile::Full(full));
        let mut world=bevy_ecs::world::World::new();let entity=world.spawn(azalea_entity::metadata::Profile(profile)).id();let value=player_profile(&world,entity);
        assert_eq!(value["name"],"ServerNpc");assert_eq!(value["textures"],encoded);assert_eq!(value["textures_signature"],"supplied-signature");assert_eq!(value["decoded_textures"]["slim"],true);
    }
    #[test]fn mannequin_partial_profile_preserves_missing_identity_and_supplied_name(){
        let mut world=bevy_ecs::world::World::new();let entity=world.spawn(azalea_entity::metadata::Profile(Default::default())).id();
        let empty=player_profile(&world,entity);assert!(empty["uuid"].is_null()&&empty["name"].is_null()&&empty["textures"].is_null());
        let mut profile=azalea_inventory::components::Profile::default();
        profile.unpack=Box::new(azalea_inventory::components::PartialOrFullProfile::Partial(azalea_inventory::components::PartialProfile{name:Some("ActualNpc".into()),..Default::default()}));
        world.entity_mut(entity).insert(azalea_entity::metadata::Profile(profile));let named=player_profile(&world,entity);assert_eq!(named["name"],"ActualNpc");assert!(named["uuid"].is_null());
    }
    #[test] fn missing_profile_does_not_invent_identity() {
        let mut world=bevy_ecs::world::World::new();let entity=world.spawn_empty().id();
        assert!(player_profile(&world,entity).is_null());
        let mut profile=azalea_client::player::GameProfileComponent(Default::default());
        profile.name="RealPlayer".to_owned();world.entity_mut(entity).insert(profile);
        let result=player_profile(&world,entity);assert_eq!(result["name"],"RealPlayer");
        assert!(result["textures"].is_null());assert!(result["textures_signature"].is_null());
        assert_eq!(result["uuid"],"00000000-0000-0000-0000-000000000000");
    }
}

fn player_skin_layers(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->u8{
 world.get::<azalea_entity::metadata::PlayerModeCustomisation>(entity).map(|v|v.0&127).unwrap_or(0)
}
#[cfg(test)] mod skin_layer_tests{
 use super::*;
 #[test] fn server_layer_bits_are_preserved_and_missing_is_default(){
  let mut world=bevy_ecs::world::World::new();let entity=world.spawn_empty().id();assert_eq!(player_skin_layers(&world,entity),0);
  world.entity_mut(entity).insert(azalea_entity::metadata::PlayerModeCustomisation(0x42));assert_eq!(player_skin_layers(&world,entity),0x42);
  world.entity_mut(entity).insert(azalea_entity::metadata::PlayerModeCustomisation(0xff));assert_eq!(player_skin_layers(&world,entity),0x7f);
 }
}

fn refresh_vital_abilities(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,vitals:&mut PlayerVitals){
 if let Some(a)=world.get::<azalea_entity::PlayerAbilities>(entity){vitals.can_fly=a.can_fly;vitals.flying=a.flying;}
}
#[cfg(test)] mod vital_ability_order_tests{
 use super::*;
 #[test] fn newer_ability_state_survives_health_publication(){
  let mut world=bevy_ecs::world::World::new();let mut abilities=azalea_entity::PlayerAbilities::default();abilities.can_fly=true;
  let entity=world.spawn(abilities).id();let mut vitals=PlayerVitals::default();vitals.health=7.;refresh_vital_abilities(&world,entity,&mut vitals);assert!(vitals.can_fly&&!vitals.flying);
  world.get_mut::<azalea_entity::PlayerAbilities>(entity).unwrap().flying=true;refresh_vital_abilities(&world,entity,&mut vitals);assert!(vitals.flying&&vitals.health==7.);
  {let mut a=world.get_mut::<azalea_entity::PlayerAbilities>(entity).unwrap();a.flying=false;a.can_fly=false;}refresh_vital_abilities(&world,entity,&mut vitals);assert!(!vitals.flying&&!vitals.can_fly&&vitals.health==7.);
 }
}
fn request_auto_respawn(show_screen:bool,spawned:bool,requested:&mut bool)->bool{
 if show_screen||!spawned||*requested{return false;}*requested=true;true
}
fn queue_auto_respawn(bot:&Client,show_screen:bool,spawned:bool,requested:&mut bool){
 if request_auto_respawn(show_screen,spawned,requested){bot.ecs.lock().write_message(azalea_client::respawn::PerformRespawnEvent{entity:bot.entity});}
}
#[cfg(test)] mod auto_respawn_tests{
 use super::*;
 #[test] fn server_setting_and_spawn_gate_allow_only_one_automatic_request(){
  let mut requested=false;assert!(!request_auto_respawn(true,true,&mut requested));assert!(!request_auto_respawn(false,false,&mut requested));assert!(request_auto_respawn(false,true,&mut requested));assert!(!request_auto_respawn(false,true,&mut requested));
  assert!(PlayerVitals::default().show_death_screen);
  let mut bot=Client{entity:bevy_ecs::entity::Entity::PLACEHOLDER,ecs:std::sync::Arc::default()};{let mut world=bot.ecs.lock();world.init_resource::<bevy_ecs::message::Messages<azalea_client::respawn::PerformRespawnEvent>>();bot.entity=world.spawn_empty().id();}
  requested=false;queue_auto_respawn(&bot,false,true,&mut requested);queue_auto_respawn(&bot,false,true,&mut requested);assert_eq!(bot.ecs.lock().resource::<bevy_ecs::message::Messages<azalea_client::respawn::PerformRespawnEvent>>().len(),1);
 }
}
fn guard_health_controls(bot:&Client,health:f32,was_alive:&mut bool){
 let alive=health.is_finite()&&health>0.;if !alive&&*was_alive{clear_dead_controls(bot);}*was_alive=alive;
}
fn clear_dead_controls(bot:&Client){
 use azalea_client::movement::StartWalkEvent;
 let mut world=bot.ecs.lock();if world.get_entity(bot.entity).is_err(){return;}
 if let Some(mut jumping)=world.get_mut::<azalea_entity::Jumping>(bot.entity){jumping.0=false;}
 if let Some(mut state)=world.get_mut::<azalea_physics::local_player::PhysicsState>(bot.entity){state.move_direction=WalkDirection::None;state.move_vector=Default::default();state.trying_to_sprint=false;state.trying_to_crouch=false;}
 if let Some(mut sprinting)=world.get_mut::<azalea_entity::metadata::Sprinting>(bot.entity){sprinting.0=false;}
 if let Some(mut attrs)=world.get_mut::<azalea_entity::Attributes>(bot.entity){attrs.movement_speed.remove(&azalea_entity::attributes::sprinting_modifier().id);}
 world.entity_mut(bot.entity).insert((azalea_client::movement::AnalogMovement{forward:0.,left:0.},azalea_physics::FlightInput::default()));
 // Walk events run after sprint events, so an earlier pending sprint cannot
 // overwrite this stop during the next normal client update.
 if world.contains_resource::<bevy_ecs::message::Messages<StartWalkEvent>>(){world.write_message(StartWalkEvent{entity:bot.entity,direction:WalkDirection::None});}
}
#[cfg(test)] mod death_controls_tests{
 use super::*;
 #[test] fn death_clears_held_inputs_once_and_preserves_server_velocity(){
  use azalea_physics::local_player::PhysicsState;
  use azalea_entity::{Jumping,Attributes,Physics,metadata::Sprinting};
  use azalea_client::movement::{StartWalkEvent,StartSprintEvent};
  use bevy_ecs::system::RunSystemOnce;
  let mut bot=Client{entity:bevy_ecs::entity::Entity::PLACEHOLDER,ecs:std::sync::Arc::default()};
  {let mut world=bot.ecs.lock();world.init_resource::<bevy_ecs::message::Messages<StartWalkEvent>>();world.init_resource::<bevy_ecs::message::Messages<StartSprintEvent>>();let mut state=PhysicsState::default();state.move_direction=WalkDirection::Forward;state.trying_to_sprint=true;state.trying_to_crouch=true;let mut physics=Physics::default();physics.velocity=azalea_core::position::Vec3::new(0.2,0.4,0.6);bot.entity=world.spawn((state,physics,Jumping(true),Sprinting(true),Attributes::new(azalea_registry::builtin::EntityKind::Player),azalea_client::movement::AnalogMovement{forward:1.,left:1.},azalea_physics::FlightInput{vertical:1.,takeoff_pending:true})).id();}
  bot.ecs.lock().write_message(StartSprintEvent{entity:bot.entity,direction:azalea_client::SprintDirection::Forward});
  let mut alive=true;guard_health_controls(&bot,0.,&mut alive);assert!(!alive);
  {let world=bot.ecs.lock();let state=world.get::<PhysicsState>(bot.entity).unwrap();assert_eq!(state.move_direction,WalkDirection::None);assert!(!state.trying_to_sprint&&!state.trying_to_crouch);assert!(!world.get::<Jumping>(bot.entity).unwrap().0);assert_eq!(world.get::<azalea_client::movement::AnalogMovement>(bot.entity).unwrap().forward,0.);assert_eq!(world.get::<azalea_physics::FlightInput>(bot.entity).unwrap().vertical,0.);assert_eq!(world.get::<Physics>(bot.entity).unwrap().velocity,azalea_core::position::Vec3::new(0.2,0.4,0.6));assert_eq!(world.resource::<bevy_ecs::message::Messages<StartWalkEvent>>().len(),1);}
  guard_health_controls(&bot,0.,&mut alive);assert_eq!(bot.ecs.lock().resource::<bevy_ecs::message::Messages<StartWalkEvent>>().len(),1);
  {let mut world=bot.ecs.lock();world.run_system_once(azalea_client::movement::handle_sprint).unwrap();world.run_system_once(azalea_client::movement::handle_walk).unwrap();let state=world.get::<PhysicsState>(bot.entity).unwrap();assert_eq!(state.move_direction,WalkDirection::None);assert!(!state.trying_to_sprint);}
  guard_health_controls(&bot,20.,&mut alive);assert!(alive);
 }
}
#[derive(Debug,PartialEq)]
enum DamageAttribution{Direct,Caused}
fn damage_attribution(packet:&azalea_protocol::packets::game::ClientboundDamageEvent,local:Option<i32>)->Option<DamageAttribution>{
 let local=local?;if packet.entity_id.0==local{return None;}
 let local=u32::try_from(local).ok()?;
 if packet.source_direct_id.0==Some(local){Some(DamageAttribution::Direct)}
 else if packet.source_cause_id.0==Some(local){Some(DamageAttribution::Caused)}else{None}
}
fn recent_direct_damage(packet:&azalea_protocol::packets::game::ClientboundDamageEvent,local:Option<i32>,target:i32,age:std::time::Duration)->bool{
 packet.entity_id.0==target&&age<=std::time::Duration::from_secs(3)&&damage_attribution(packet,local)==Some(DamageAttribution::Direct)
}
#[cfg(test)] mod damage_attribution_tests{
 use super::*;
 #[test] fn server_damage_proof_excludes_self_unknown_and_other_attackers(){
  use azalea_protocol::packets::game::c_damage_event::OptionalEntityId;
  let mut p=azalea_protocol::packets::game::ClientboundDamageEvent{entity_id:azalea_world::MinecraftEntityId(22),source_type_id:0,source_cause_id:OptionalEntityId(Some(11)),source_direct_id:OptionalEntityId(Some(11)),source_position:None};
  assert_eq!(damage_attribution(&p,Some(11)),Some(DamageAttribution::Direct));
  assert!(recent_direct_damage(&p,Some(11),22,std::time::Duration::from_millis(100)));
  assert!(!recent_direct_damage(&p,Some(11),23,std::time::Duration::ZERO));
  assert!(!recent_direct_damage(&p,Some(11),22,std::time::Duration::from_secs(4)));
  p.source_direct_id=OptionalEntityId(Some(33));assert_eq!(damage_attribution(&p,Some(11)),Some(DamageAttribution::Caused));
  p.source_cause_id=OptionalEntityId(None);assert_eq!(damage_attribution(&p,Some(11)),None);
  p.source_direct_id=OptionalEntityId(None);assert_eq!(damage_attribution(&p,Some(0)),None);
  p.source_direct_id=OptionalEntityId(Some(0));assert_eq!(damage_attribution(&p,Some(0)),Some(DamageAttribution::Direct));
  assert_eq!(damage_attribution(&p,None),None);assert_eq!(damage_attribution(&p,Some(-1)),None);
  p.entity_id=azalea_world::MinecraftEntityId(0);assert_eq!(damage_attribution(&p,Some(0)),None);
 }
}
fn end_flight_on_landing(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->bool{
 if !world.get::<azalea_entity::Physics>(entity).is_some_and(|p|p.on_ground()){return false;}
 if !world.get::<azalea_client::local_player::LocalGameMode>(entity).is_some_and(|m|m.current!=azalea_core::game_type::GameMode::Spectator){return false;}
 let flying=world.get::<azalea_entity::PlayerAbilities>(entity).is_some_and(|a|a.flying);
 if !flying{return false;}
 apply_native_flight(world,entity,false,0.)==Some(true)
}
fn native_flight_state(current:bool,requested:bool,toggle:bool,alive:bool)->bool{if toggle&&alive{requested}else{current}}
fn apply_native_flight(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,flying:bool,vertical:f32)->Option<bool>{
 if !vertical.is_finite()||!(-1.0..=1.0).contains(&vertical){return None;}
 let pending=world.get::<azalea_physics::FlightInput>(entity).is_some_and(|i|i.takeoff_pending);
 let grounded=world.get::<azalea_entity::Physics>(entity).is_some_and(|p|p.on_ground());
 let changed={let mut abilities=world.get_mut::<azalea_entity::PlayerAbilities>(entity)?;
  if flying&&!abilities.can_fly&&!abilities.flying{return None;}
  let changed=abilities.flying!=flying;abilities.flying=flying;changed
 };
 world.entity_mut(entity).insert(azalea_physics::FlightInput{vertical:if flying{vertical}else{0.},takeoff_pending:flying&&(pending||(changed&&grounded))});
 Some(changed)
}
#[cfg(test)] mod flight_permission_tests{
 use super::*;
 #[test] fn grounded_creative_flight_ends_once_but_spectator_flight_remains(){
  use azalea_client::local_player::LocalGameMode;use azalea_core::game_type::GameMode;
  let mut world=bevy_ecs::world::World::new();let mut abilities=azalea_entity::PlayerAbilities::default();abilities.can_fly=true;abilities.flying=true;
  let entity=world.spawn((abilities,azalea_entity::Physics::default(),LocalGameMode{current:GameMode::Creative,previous:None},azalea_physics::FlightInput{vertical:1.,..Default::default()})).id();
  assert!(!end_flight_on_landing(&mut world,entity));world.get_mut::<azalea_entity::Physics>(entity).unwrap().set_on_ground(true);
  assert!(end_flight_on_landing(&mut world,entity));assert!(!world.get::<azalea_entity::PlayerAbilities>(entity).unwrap().flying);assert_eq!(world.get::<azalea_physics::FlightInput>(entity).unwrap().vertical,0.);assert!(!end_flight_on_landing(&mut world,entity));
  world.get_mut::<LocalGameMode>(entity).unwrap().current=GameMode::Spectator;world.get_mut::<azalea_entity::PlayerAbilities>(entity).unwrap().flying=true;
  assert!(!end_flight_on_landing(&mut world,entity));assert!(world.get::<azalea_entity::PlayerAbilities>(entity).unwrap().flying);
 }
 #[test] fn stale_input_preserves_server_flight_and_death_cannot_toggle(){
  assert!(native_flight_state(true,false,false,true));assert!(!native_flight_state(false,true,false,true));
  assert!(native_flight_state(false,true,true,true));assert!(!native_flight_state(true,false,true,true));
  assert!(native_flight_state(true,false,true,false));assert!(!native_flight_state(false,true,true,false));
 }
 #[test] fn flight_command_drives_actual_physics_vertical_system(){
  use bevy_ecs::system::RunSystemOnce;
  let mut world=bevy_ecs::world::World::new();let mut abilities=azalea_entity::PlayerAbilities::default();abilities.can_fly=true;abilities.flying_speed=0.05;
  let entity=world.spawn((abilities,azalea_entity::Physics::default(),azalea_entity::LocalEntity,azalea_entity::HasClientLoaded)).id();
  assert_eq!(apply_native_flight(&mut world,entity,true,1.),Some(true));
  world.run_system_once(azalea_physics::apply_flight_vertical_input).unwrap();
  assert_eq!(world.get::<azalea_entity::Physics>(entity).unwrap().velocity.y,(0.05f32*3.) as f64);
  apply_native_flight(&mut world,entity,false,1.);let previous=world.get::<azalea_entity::Physics>(entity).unwrap().velocity.y;
  world.run_system_once(azalea_physics::apply_flight_vertical_input).unwrap();assert_eq!(world.get::<azalea_entity::Physics>(entity).unwrap().velocity.y,previous);
 }
 #[test] fn permission_gate_and_repeated_input_do_not_repeat_toggle(){
  let mut world=bevy_ecs::world::World::new();let entity=world.spawn(azalea_entity::PlayerAbilities::default()).id();
  assert_eq!(apply_native_flight(&mut world,entity,true,1.),None);
  assert!(!world.get::<azalea_entity::PlayerAbilities>(entity).unwrap().flying);
  world.get_mut::<azalea_entity::PlayerAbilities>(entity).unwrap().can_fly=true;
  let mut physics=azalea_entity::Physics::default();physics.set_on_ground(true);world.entity_mut(entity).insert(physics);
  assert_eq!(apply_native_flight(&mut world,entity,true,1.),Some(true));
  assert!(world.get::<azalea_physics::FlightInput>(entity).unwrap().takeoff_pending);
  assert_eq!(world.get::<azalea_physics::FlightInput>(entity).unwrap().vertical,1.);
  assert_eq!(apply_native_flight(&mut world,entity,true,-1.),Some(false));
  assert!(world.get::<azalea_physics::FlightInput>(entity).unwrap().takeoff_pending);
  assert_eq!(apply_native_flight(&mut world,entity,false,-1.),Some(true));
  assert_eq!(world.get::<azalea_physics::FlightInput>(entity).unwrap().vertical,0.);
  assert_eq!(apply_native_flight(&mut world,entity,true,f32::NAN),None);
 }
}
fn native_sprint_direction(forward:f32,strafe:f32,sneak:bool,sprint:bool)->Option<SprintDirection>{
 if !sprint||sneak||forward<=0.||!forward.is_finite()||!strafe.is_finite(){return None;}
 Some(if strafe>0.{SprintDirection::ForwardLeft}else if strafe<0.{SprintDirection::ForwardRight}else{SprintDirection::Forward})
}
#[cfg(test)] mod native_sprint_tests{
 use super::*;
 #[test] fn sprint_direction_preserves_strafe_and_stops_on_crouch_or_zero_input(){
  assert!(matches!(native_sprint_direction(1.,0.,false,true),Some(SprintDirection::Forward)));
  assert!(matches!(native_sprint_direction(1.,0.5,false,true),Some(SprintDirection::ForwardLeft)));
  assert!(matches!(native_sprint_direction(1.,-0.5,false,true),Some(SprintDirection::ForwardRight)));
  for (forward,sneak,sprint) in [(0.,false,true),(-1.,false,true),(1.,true,true),(1.,false,false),(f32::NAN,false,true)]{assert!(native_sprint_direction(forward,0.,sneak,sprint).is_none());}
 }
}
fn entity_hitbox(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->[f32;2]{
 let dimensions=world.get::<azalea_entity::dimensions::EntityDimensions>(entity);
 let mut bounds=dimensions.map(|d|[d.width,d.height]).unwrap_or([0.,0.]);
 if let Some(width)=world.get::<azalea_entity::metadata::InteractionWidth>(entity){bounds[0]=width.0;}
 if let Some(height)=world.get::<azalea_entity::metadata::InteractionHeight>(entity){bounds[1]=height.0;}
 bounds.map(|v|if v.is_finite()&&v>=0.{v}else{0.})
}
fn within_attack_range(eye:[f64;3],target:[f64;3],width:f32,height:f32,reach:f64)->bool{
 if !reach.is_finite()||reach<0.||!width.is_finite()||!height.is_finite()||width<0.||height<0.||eye.iter().chain(target.iter()).any(|v|!v.is_finite()){return false;}
 let half=f64::from(width)/2.;let low=[target[0]-half,target[1],target[2]-half];let high=[target[0]+half,target[1]+f64::from(height),target[2]+half];
 (0..3).map(|i|{let d=eye[i]-eye[i].clamp(low[i],high[i]);d*d}).sum::<f64>()<=reach*reach
}

fn resolve_weapon_target(world:&mut bevy_ecs::world::World,local:bevy_ecs::entity::Entity,id:i32)->Option<bevy_ecs::entity::Entity>{
 let item_range=world.get::<azalea_entity::inventory::Inventory>(local).and_then(|inv|match inv.held_item(){azalea_inventory::ItemStack::Present(item)=>item.get_component::<azalea_inventory::components::AttackRange>(),_=>None});
 let Some(range)=item_range else{return resolve_attack_target(world,local,id);};
 let p=world.get::<azalea_entity::Position>(local)?;let dim=world.get::<azalea_entity::dimensions::EntityDimensions>(local)?;let eye=[p.x,p.y+f64::from(dim.eye_height),p.z];
 let creative=world.get::<azalea_client::local_player::LocalGameMode>(local).is_some_and(|g|g.current==azalea_core::game_type::GameMode::Creative);
 let spectator=world.get::<azalea_client::local_player::LocalGameMode>(local).is_some_and(|g|g.current==azalea_core::game_type::GameMode::Spectator);
 let min=if spectator{0.}else if creative{range.min_creative_reach}else{range.min_reach};let max=if creative{range.max_creative_reach}else{range.max_reach};
 let min=f64::from((min-range.hitbox_margin).max(0.));let max=f64::from(max+range.hitbox_margin);if !min.is_finite()||!max.is_finite()||max<min{return None;}
 let mut query=world.query::<(bevy_ecs::entity::Entity,&azalea_world::MinecraftEntityId,&azalea_entity::LoadedBy,&azalea_entity::Position,&azalea_entity::dimensions::EntityDimensions)>();
 query.iter(world).find_map(|(entity,server_id,loaded,p,_)|{let bounds=entity_hitbox(world,entity);let hit=within_attack_range(eye,[p.x,p.y,p.z],bounds[0],bounds[1],max)&& (min==0.||!within_attack_range(eye,[p.x,p.y,p.z],bounds[0],bounds[1],(min-f64::EPSILON).max(0.)));if entity!=local&&server_id.0==id&&loaded.0.contains(&local)&&hit{Some(entity)}else{None}})
}

fn resolve_attack_target(world:&mut bevy_ecs::world::World,local:bevy_ecs::entity::Entity,id:i32)->Option<bevy_ecs::entity::Entity>{
 let p=world.get::<azalea_entity::Position>(local)?;
 let dimensions=world.get::<azalea_entity::dimensions::EntityDimensions>(local)?;
 let eye=[p.x,p.y+f64::from(dimensions.eye_height),p.z];
 let reach=world.get::<azalea_entity::Attributes>(local)?.entity_interaction_range.calculate();
 let mut query=world.query::<(bevy_ecs::entity::Entity,&azalea_world::MinecraftEntityId,&azalea_entity::LoadedBy,&azalea_entity::Position,&azalea_entity::dimensions::EntityDimensions)>();
 query.iter(world).find_map(|(entity,server_id,loaded,p,_d)|if entity!=local&&server_id.0==id&&loaded.0.contains(&local)&&within_attack_range(eye,[p.x,p.y,p.z],entity_hitbox(world,entity)[0],entity_hitbox(world,entity)[1],reach){Some(entity)}else{None})
}
#[cfg(test)] mod metadata_dimensions_tests{
 use super::*;
 #[test] fn armorstand_and_interaction_metadata_update_actual_dimensions_and_bounds(){
  use bevy_ecs::system::RunSystemOnce;
  use azalea_entity::{EntityKindComponent,Pose,Physics,Position,dimensions::EntityDimensions,metadata::{Small,ArmorStandMarker,InteractionWidth,InteractionHeight}};
  use azalea_registry::builtin::EntityKind;use azalea_core::position::Vec3;
  let mut world=bevy_ecs::world::World::new();
  let stand=world.spawn((EntityDimensions::from(EntityKind::ArmorStand),EntityKindComponent(EntityKind::ArmorStand),Pose::Standing,Physics::default(),Position::new(Vec3::default()),Small(true),ArmorStandMarker(false))).id();
  world.run_system_once(azalea_entity::update_dimensions).unwrap();world.run_system_once(azalea_entity::update_bounding_box).unwrap();
  let d=world.get::<EntityDimensions>(stand).unwrap();assert_eq!([d.width,d.height,d.eye_height],[0.25,0.9875,0.9875]);
  world.run_system_once(azalea_entity::update_dimensions).unwrap();assert_eq!(entity_hitbox(&world,stand),[0.25,0.9875]);
  world.get_mut::<ArmorStandMarker>(stand).unwrap().0=true;world.run_system_once(azalea_entity::update_dimensions).unwrap();world.run_system_once(azalea_entity::update_bounding_box).unwrap();assert_eq!(entity_hitbox(&world,stand),[0.,0.]);
  world.get_mut::<ArmorStandMarker>(stand).unwrap().0=false;world.get_mut::<Small>(stand).unwrap().0=false;world.run_system_once(azalea_entity::update_dimensions).unwrap();assert_eq!(entity_hitbox(&world,stand),[0.5,1.975]);
  let interaction=world.spawn((EntityDimensions::from(EntityKind::Interaction),EntityKindComponent(EntityKind::Interaction),Pose::Standing,Physics::default(),Position::new(Vec3::default()),InteractionWidth(3.),InteractionHeight(2.))).id();
  world.run_system_once(azalea_entity::update_dimensions).unwrap();world.run_system_once(azalea_entity::update_bounding_box).unwrap();
  assert_eq!(entity_hitbox(&world,interaction),[3.,2.]);let bbox=world.get::<Physics>(interaction).unwrap().bounding_box;assert_eq!(bbox.max.x-bbox.min.x,3.);assert_eq!(bbox.max.y-bbox.min.y,2.);
  world.get_mut::<InteractionWidth>(interaction).unwrap().0=5.;world.run_system_once(azalea_entity::update_dimensions).unwrap();world.run_system_once(azalea_entity::update_bounding_box).unwrap();let bbox=world.get::<Physics>(interaction).unwrap().bounding_box;assert_eq!(bbox.max.x-bbox.min.x,5.);
 }
}
#[cfg(test)] mod interaction_hitbox_tests{
 use super::*;
 #[test] fn interaction_metadata_drives_selected_target_reach(){
  use azalea_entity::{Position,LoadedBy,Attributes,dimensions::EntityDimensions,metadata::{InteractionWidth,InteractionHeight}};
  use azalea_core::position::Vec3;
  let mut world=bevy_ecs::world::World::new();
  let local=world.spawn((Position::new(Vec3::default()),EntityDimensions::new(0.6,1.8),Attributes::new(azalea_registry::builtin::EntityKind::Player))).id();
  let mut loaded=LoadedBy(Default::default());loaded.0.insert(local);
  let target=world.spawn((azalea_world::MinecraftEntityId(456),Position::new(Vec3::new(4.,0.,0.)),EntityDimensions::new(0.,0.),loaded,InteractionWidth(4.),InteractionHeight(3.))).id();
  assert_eq!(entity_hitbox(&world,target),[4.,3.]);
  assert_eq!(resolve_attack_target(&mut world,local,456),Some(target));
  world.entity_mut(target).insert(InteractionWidth(0.));assert_eq!(resolve_attack_target(&mut world,local,456),None);
  world.entity_mut(target).insert((InteractionWidth(f32::NAN),InteractionHeight(-1.)));assert_eq!(entity_hitbox(&world,target),[0.,0.]);
 }
}
#[cfg(test)] mod attack_range_tests{
 use super::*;
 #[test] fn reach_uses_nearest_hitbox_and_server_range(){assert!(within_attack_range([0.,1.62,0.],[3.25,0.,0.],0.6,1.8,3.));assert!(!within_attack_range([0.,1.62,0.],[3.5,0.,0.],0.6,1.8,3.));assert!(within_attack_range([0.,1.62,0.],[4.5,0.,0.],0.6,1.8,5.));assert!(!within_attack_range([0.,1.62,0.],[1.,0.,0.],0.6,1.8,f64::NAN));}
}
#[cfg(test)] mod selected_target_tests {
 use super::*;
 #[test] fn selected_id_is_scoped_to_loaded_session_and_retired_ids_are_rejected(){
  use azalea_entity::{Position,LoadedBy,Attributes,dimensions::EntityDimensions};
  use azalea_core::position::Vec3;
  let mut world=bevy_ecs::world::World::new();
  let local=world.spawn((Position::default(),EntityDimensions::new(0.6,1.8),Attributes::new(azalea_registry::builtin::EntityKind::Player))).id();
  let outsider=world.spawn((azalea_world::MinecraftEntityId(123),Position::new(Vec3::new(2.,0.,0.)),EntityDimensions::new(0.6,1.8),LoadedBy(Default::default()))).id();
  assert_eq!(resolve_attack_target(&mut world,local,123),None);
  let mut loaded=LoadedBy(Default::default());loaded.0.insert(local);
  let target=world.spawn((azalea_world::MinecraftEntityId(123),Position::new(Vec3::new(2.,0.,0.)),EntityDimensions::new(0.6,1.8),loaded)).id();
  assert_eq!(resolve_attack_target(&mut world,local,123),Some(target));
  assert_eq!(resolve_attack_target(&mut world,local,456),None);
  world.despawn(target);assert_eq!(resolve_attack_target(&mut world,local,123),None);
  world.entity_mut(outsider).insert(Position::new(Vec3::new(100.,0.,0.)));assert_eq!(resolve_attack_target(&mut world,local,123),None);
 }
}

fn apply_experience(vitals:&mut PlayerVitals, packet:&azalea_protocol::packets::game::ClientboundSetExperience) {
 vitals.experience_progress=if packet.experience_progress.is_finite(){packet.experience_progress.clamp(0.,1.)}else{0.};
 vitals.experience_level=packet.experience_level;
 vitals.total_experience=packet.total_experience;
}
#[cfg(test)] mod experience_vitals_tests {
 use super::*;
 #[test] fn authoritative_experience_preserves_other_vitals() {
  let mut v=PlayerVitals::default();v.health=3.5;v.flying=true;
  let mut p=azalea_protocol::packets::game::ClientboundSetExperience{experience_progress:0.75,experience_level:42,total_experience:1234};
  apply_experience(&mut v,&p);assert_eq!((v.experience_progress,v.experience_level,v.total_experience),(0.75,42,1234));assert_eq!(v.health,3.5);assert!(v.flying);
  for (input,expected) in [(f32::NAN,0.),(f32::INFINITY,0.),(-0.2,0.),(1.2,1.),(0.,0.)] {p.experience_progress=input;apply_experience(&mut v,&p);assert_eq!(v.experience_progress,expected);}
  assert_eq!(PlayerVitals::default().experience_level,0);
 }
}

fn refresh_vital_absorption(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,vitals:&mut PlayerVitals){
 let amount=world.get::<azalea_entity::metadata::PlayerAbsorption>(entity).map(|v|v.0).unwrap_or(0.);
 vitals.absorption=if amount.is_finite(){amount.max(0.)}else{0.};
}
#[cfg(test)] mod absorption_vitals_tests{use super::*;
 #[test]fn metadata_absorption_is_authoritative_and_removal_clears_it(){
  let mut w=bevy_ecs::world::World::new();let e=w.spawn(azalea_entity::metadata::PlayerAbsorption(5.5)).id();let mut v=PlayerVitals::default();refresh_vital_absorption(&w,e,&mut v);assert_eq!(v.absorption,5.5);
  for amount in [-1.,f32::NAN,f32::INFINITY,0.,4.] {w.entity_mut(e).insert(azalea_entity::metadata::PlayerAbsorption(amount));refresh_vital_absorption(&w,e,&mut v);assert_eq!(v.absorption,if amount.is_finite(){amount.max(0.)}else{0.});}
  w.entity_mut(e).remove::<azalea_entity::metadata::PlayerAbsorption>();refresh_vital_absorption(&w,e,&mut v);assert_eq!(v.absorption,0.);
 }
}

fn apply_authoritative_cursor(inventory:&mut azalea_entity::inventory::Inventory,packet:&azalea_protocol::packets::game::ClientboundContainerSetContent)->bool{
 if inventory.id!=packet.container_id{return false;}
 inventory.state_id=packet.state_id;inventory.carried=packet.carried_item.clone();true
}
#[cfg(test)]mod authoritative_cursor_tests{use super::*;
 #[test]fn cancelled_click_full_sync_clears_prediction_and_updates_revision(){
  let mut inventory=azalea_entity::inventory::Inventory::default();inventory.id=7;inventory.state_id=1;inventory.carried=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::DiamondSword,1);
  let mut p=azalea_protocol::packets::game::ClientboundContainerSetContent{container_id:7,state_id:42,items:vec![],carried_item:azalea_inventory::ItemStack::Empty};assert!(apply_authoritative_cursor(&mut inventory,&p));assert!(matches!(inventory.carried,azalea_inventory::ItemStack::Empty));assert_eq!(inventory.state_id,42);
  p.carried_item=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Stone,3);assert!(apply_authoritative_cursor(&mut inventory,&p));assert_eq!(inventory.carried,p.carried_item);
  p.container_id=0;p.state_id=99;p.carried_item=azalea_inventory::ItemStack::Empty;assert!(!apply_authoritative_cursor(&mut inventory,&p));assert_eq!(inventory.state_id,42);assert!(!matches!(inventory.carried,azalea_inventory::ItemStack::Empty));
  inventory.id=0;assert!(apply_authoritative_cursor(&mut inventory,&p));assert_eq!(inventory.state_id,99);assert!(matches!(inventory.carried,azalea_inventory::ItemStack::Empty));
 }
}

fn chat_is_action_bar(chat:&azalea_client::chat::ChatPacket)->bool{matches!(chat,azalea_client::chat::ChatPacket::System(packet) if packet.overlay)}
#[cfg(test)]mod action_bar_route_tests{use super::*;
 #[test]fn system_overlay_is_separate_from_normal_chat(){
  let mut p=azalea_protocol::packets::game::ClientboundSystemChat{content:azalea_chat::FormattedText::from("Ready!"),overlay:true};
  assert!(chat_is_action_bar(&azalea_client::chat::ChatPacket::System(std::sync::Arc::new(p.clone()))));p.overlay=false;assert!(!chat_is_action_bar(&azalea_client::chat::ChatPacket::System(std::sync::Arc::new(p))));
 }
}

#[cfg(test)]mod boss_route_tests{use super::*;
 #[test]fn every_boss_operation_preserves_identity_style_and_properties(){
 use azalea_protocol::packets::game::{ClientboundBossEvent,c_boss_event::*};
 let id="00000000-0000-0000-0000-000000000011".parse().unwrap();
 let style=Style{color:BossBarColor::Purple,overlay:BossBarOverlay::Notched12};
 let properties=Properties{darken_screen:true,play_music:false,create_world_fog:true};
 let operations=vec![Operation::Add(AddOperation{name:azalea_chat::FormattedText::from("雪 ✦"),progress:0.75,style:style.clone(),properties:properties.clone()}),Operation::Remove,Operation::UpdateProgress(f32::NAN),Operation::UpdateName(azalea_chat::FormattedText::from("Updated")),Operation::UpdateStyle(style),Operation::UpdateProperties(properties)];
 for (operation,expected) in operations.into_iter().zip(["add","remove","progress","name","style","properties"]){let value=serde_json::to_value(boss_update(&ClientboundBossEvent{id,operation})).unwrap();assert_eq!(value["id"],id.to_string());assert_eq!(value["operation"],expected);if expected=="add"{assert_eq!(value["text"],"§r雪 ✦");assert_eq!(value["progress"],0.75);}if expected=="add"||expected=="style"{assert_eq!(value["color"],5);assert_eq!(value["overlay"],3);}if expected=="progress"{assert_eq!(value["progress"],0.);}if expected=="add"||expected=="properties"{assert_eq!(value["darken_screen"],true);assert_eq!(value["play_music"],false);assert_eq!(value["create_world_fog"],true);}}
 }
}

fn dropped_item(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->Option<ItemSnapshot>{
 if let Some(v)=world.get::<azalea_entity::metadata::ItemItem>(entity){return item_snapshot(&v.0);}
 if let Some(v)=world.get::<azalea_entity::metadata::FireworksItem>(entity){return item_snapshot(&v.0);}
 world.get::<azalea_entity::metadata::AbstractThrownItemProjectileItemStack>(entity).and_then(|v|item_snapshot(&v.0))
}
fn firework_metadata(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->serde_json::Value{
 use azalea_entity::metadata::{FireworkRocket,ShotAtAngle,AttachedToTarget};
 if world.get::<FireworkRocket>(entity).is_none(){return serde_json::Value::Null;}
 serde_json::json!({"shot_at_angle":world.get::<ShotAtAngle>(entity).is_some_and(|v|v.0),"attached_to_target":world.get::<AttachedToTarget>(entity).and_then(|v|v.0.0)})
}
#[cfg(test)]mod dropped_item_tests{use super::*;
 #[test]fn firework_actual_metadata_defaults_and_updates_preserve_custom_item_angle_attachment_and_empty(){
 use azalea_entity::{metadata,EntityDataItem,EntityDataValue,OptionalUnsignedInt};use azalea_inventory::{ItemStack,components};use azalea_registry::builtin::{EntityKind,ItemKind};
 let mut world=bevy_ecs::world::World::new();let entity=world.spawn(metadata::FireworkRocketMetadataBundle::default()).id();
 let stack=serde_json::to_value(dropped_item(&world,entity)).unwrap();assert_eq!(stack["resource_id"],"minecraft:firework_rocket");assert_eq!(firework_metadata(&world,entity)["shot_at_angle"],false);assert!(firework_metadata(&world,entity)["attached_to_target"].is_null());
 let custom=ItemStack::new(ItemKind::FireworkRocket,1).with_component(components::ItemModel{resource_location:"test:server_rocket".into()});
 let mut queue=bevy_ecs::world::CommandQueue::default();{
  let mut commands=bevy_ecs::system::Commands::new(&mut queue,&world);metadata::apply_metadata(&mut commands.entity(entity),EntityKind::FireworkRocket,vec![EntityDataItem{index:8,value:EntityDataValue::ItemStack(custom)},EntityDataItem{index:9,value:EntityDataValue::OptionalUnsignedInt(OptionalUnsignedInt(Some(123)))},EntityDataItem{index:10,value:EntityDataValue::Boolean(true)}]).unwrap();
 }queue.apply(&mut world);
 assert_eq!(serde_json::to_value(dropped_item(&world,entity)).unwrap()["item_model"],"test:server_rocket");assert_eq!(firework_metadata(&world,entity),serde_json::json!({"shot_at_angle":true,"attached_to_target":123}));
 let mut queue=bevy_ecs::world::CommandQueue::default();{let mut commands=bevy_ecs::system::Commands::new(&mut queue,&world);metadata::apply_metadata(&mut commands.entity(entity),EntityKind::FireworkRocket,vec![EntityDataItem{index:8,value:EntityDataValue::ItemStack(ItemStack::Empty)},EntityDataItem{index:9,value:EntityDataValue::OptionalUnsignedInt(OptionalUnsignedInt(None))},EntityDataItem{index:10,value:EntityDataValue::Boolean(false)}]).unwrap();}queue.apply(&mut world);
 assert!(dropped_item(&world,entity).is_none());assert_eq!(firework_metadata(&world,entity)["shot_at_angle"],false);assert!(firework_metadata(&world,entity)["attached_to_target"].is_null());
 let unrelated=world.spawn_empty().id();assert!(firework_metadata(&world,unrelated).is_null());
 }
 #[test]fn thrown_projectile_authoritative_item_stack_preserves_pack_model_components_and_empty_transition(){
 use azalea_inventory::{ItemStack,components};use azalea_entity::metadata::AbstractThrownItemProjectileItemStack;use azalea_registry::builtin::ItemKind;
 let mut world=bevy_ecs::world::World::new();
 for (kind,id) in [(ItemKind::Snowball,"minecraft:snowball"),(ItemKind::Egg,"minecraft:egg"),(ItemKind::EnderPearl,"minecraft:ender_pearl"),(ItemKind::ExperienceBottle,"minecraft:experience_bottle")]{
 let stack=ItemStack::new(kind,1).with_component(components::ItemModel{resource_location:"test:custom_projectile".into()}).with_component(components::CustomModelData{floats:vec![12.],flags:vec![true],strings:vec!["server_variant".into()],colors:vec![0x123456]});
 let entity=world.spawn(AbstractThrownItemProjectileItemStack(stack)).id();let value=serde_json::to_value(dropped_item(&world,entity)).unwrap();assert_eq!(value["resource_id"],id);assert_eq!(value["count"],1);assert_eq!(value["item_model"],"test:custom_projectile");assert_eq!(value["custom_model_data"]["strings"][0],"server_variant");assert_eq!(value["custom_model_data"]["colors"][0],0x123456);
 world.entity_mut(entity).insert(AbstractThrownItemProjectileItemStack(ItemStack::Empty));assert!(dropped_item(&world,entity).is_none());world.entity_mut(entity).remove::<AbstractThrownItemProjectileItemStack>();assert!(dropped_item(&world,entity).is_none());
 }
 }

 #[test]fn actual_item_metadata_preserves_stack_and_empty_transition(){
 let mut world=bevy_ecs::world::World::new();let entity=world.spawn(azalea_entity::metadata::ItemItem(azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Stone,16))).id();let value=serde_json::to_value(dropped_item(&world,entity)).unwrap();assert_eq!(value["resource_id"],"minecraft:stone");assert_eq!(value["count"],16);
 world.entity_mut(entity).insert(azalea_entity::metadata::ItemItem(azalea_inventory::ItemStack::Empty));assert!(dropped_item(&world,entity).is_none());world.entity_mut(entity).remove::<azalea_entity::metadata::ItemItem>();assert!(dropped_item(&world,entity).is_none());
 }
}

fn scoreboard_number_format(value:&Option<azalea_chat::numbers::NumberFormat>)->serde_json::Value{
 use azalea_chat::numbers::NumberFormat;
 match value{None=>serde_json::Value::Null,Some(NumberFormat::Blank)=>serde_json::json!({"kind":"blank"}),Some(NumberFormat::Fixed{value})=>serde_json::json!({"kind":"fixed","text":styled_text(value)}),Some(NumberFormat::Styled{style})=>{let mut data=Vec::new();style.write(&mut data);serde_json::json!({"kind":"styled","text":value.as_ref().and_then(|format|format.styled_component("0")).map(|component|{let text=styled_text(&component);text.strip_suffix('0').unwrap_or(&text).to_owned()}).unwrap_or_else(||"§r".to_owned()),"nbt_base64":base64::Engine::encode(&base64::engine::general_purpose::STANDARD,data)})}}
}
fn scoreboard_update(packet:&azalea_protocol::packets::game::ClientboundGamePacket)->Option<serde_json::Value>{
 use azalea_protocol::packets::game::{ClientboundGamePacket as P,c_set_objective::Method};use serde_json::json;
 Some(match packet{
 P::SetObjective(p)=>match &p.method{
 Method::Remove=>json!({"operation":"objective_remove","objective":p.objective_name}),
 Method::Add{display_name,render_type,number_format}|Method::Change{display_name,render_type,number_format}=>json!({"operation":if matches!(p.method,Method::Add{..}){"objective_add"}else{"objective_change"},"objective":p.objective_name,"text":styled_text(display_name),"render_type":*render_type as u8,"number_format":scoreboard_number_format(number_format)})},
 P::SetScore(p)=>json!({"operation":"score","objective":p.objective_name,"owner":p.owner,"score":p.score as i32,"display":p.display.as_ref().map(styled_text),"number_format":scoreboard_number_format(&p.number_format)}),
 P::ResetScore(p)=>json!({"operation":"reset","objective":p.objective_name,"owner":p.owner}),
 P::SetDisplayObjective(p)=>json!({"operation":"display","slot":p.slot as u8,"objective":p.objective_name}),
 P::SetPlayerTeam(p)=>{
 use azalea_protocol::packets::game::c_set_player_team::Method as T;
 let (operation,parameters,members)=match &p.method{T::Add((parameters,members))=>("team_add",Some(parameters),Some(members)),T::Remove=>("team_remove",None,None),T::Change(parameters)=>("team_change",Some(parameters),None),T::Join(members)=>("team_join",None,Some(members)),T::Leave(members)=>("team_leave",None,Some(members))};
 let mut value=json!({"operation":operation,"team":p.name});
 if let Some(members)=members{value["members"]=json!(members);}
 if let Some(p)=parameters{let mut parent=azalea_chat::style::Style::default();parent.color=p.color.color().and_then(|rgb|azalea_chat::style::TextColor::parse(&format!("#{rgb:06x}")));value["parameters"]=json!({"text":styled_text(&p.display_name),"options":p.options,"visibility":p.nametag_visibility as u8,"collision":p.collision_rule as u8,"color":p.color as u8,"prefix":styled_text_with_parent(&p.player_prefix,&parent),"suffix":styled_text_with_parent(&p.player_suffix,&parent),"prefix_component":p.player_prefix,"suffix_component":p.player_suffix});}value
 },
 _=>return None,
 })
}
#[cfg(test)]mod scoreboard_route_tests{use super::*;
 #[test]fn signed_score_and_all_objective_reset_scope_are_retained(){use azalea_protocol::packets::game::{ClientboundGamePacket as P,ClientboundSetScore,ClientboundResetScore};let value=scoreboard_update(&P::SetScore(ClientboundSetScore{owner:"雪".into(),objective_name:"board".into(),score:u32::MAX,display:None,number_format:Some(azalea_chat::numbers::NumberFormat::Blank)})).unwrap();assert_eq!(value["score"],-1);assert_eq!(value["owner"],"雪");assert_eq!(value["number_format"]["kind"],"blank");let reset=scoreboard_update(&P::ResetScore(ClientboundResetScore{owner:"雪".into(),objective_name:None})).unwrap();assert!(reset["objective"].is_null());}
}

#[cfg(test)]mod scoreboard_team_tests{use super::*;
 #[test]fn team_operations_keep_membership_and_rich_parameters_separate(){
 use azalea_protocol::packets::game::{ClientboundGamePacket as P,ClientboundSetPlayerTeam,c_set_player_team::*};
 let parameters=Parameters{display_name:azalea_chat::FormattedText::from("Team ✦"),options:3,nametag_visibility:NameTagVisibility::HideForOtherTeams,collision_rule:CollisionRule::Never,color:azalea_chat::style::ChatFormatting::Aqua,player_prefix:azalea_chat::FormattedText::from("雪"),player_suffix:azalea_chat::FormattedText::from("✦")};
 let members=vec!["Owner1".to_string(),"雪".to_string()];
 for (method,operation) in [Method::Add((parameters.clone(),members.clone())),Method::Remove,Method::Change(parameters),Method::Join(members.clone()),Method::Leave(members)].into_iter().zip(["team_add","team_remove","team_change","team_join","team_leave"]){let v=scoreboard_update(&P::SetPlayerTeam(ClientboundSetPlayerTeam{name:"native".into(),method})).unwrap();assert_eq!(v["operation"],operation);assert_eq!(v["team"],"native");if operation=="team_add"||operation=="team_change"{assert_eq!(v["parameters"]["color"],11);assert_eq!(v["parameters"]["options"],3);assert_eq!(v["parameters"]["visibility"],2);assert_eq!(v["parameters"]["collision"],1);assert_eq!(v["parameters"]["prefix"],"§r§x§5§5§f§f§f§f雪");assert!(!v["parameters"]["prefix_component"].is_null());}if operation=="team_change"||operation=="team_remove"{assert!(v.get("members").is_none());}else{assert_eq!(v["members"][1],"雪");}}
 }
}

fn apply_native_look(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,yaw:f32,pitch:f32)->bool{
 if !yaw.is_finite()||!pitch.is_finite(){return false;}
 let Some(mut direction)=world.get_mut::<azalea_entity::LookDirection>(entity) else{return false;};
 direction.update(azalea_entity::LookDirection::new(yaw,pitch));true
}
#[cfg(test)]mod native_transfer_look_tests{use super::*;
 #[test]fn look_update_tolerates_replaced_player_components(){
 let mut world=bevy_ecs::world::World::new();let entity=world.spawn_empty().id();
 assert!(!apply_native_look(&mut world,entity,90.,20.));
 world.entity_mut(entity).insert(azalea_entity::LookDirection::new(0.,0.));
 assert!(apply_native_look(&mut world,entity,90.,20.));let look=world.get::<azalea_entity::LookDirection>(entity).unwrap();assert_eq!(look.y_rot(),90.);assert!((look.x_rot()-20.).abs()<0.15);
 assert!(!apply_native_look(&mut world,entity,f32::NAN,0.));
 world.entity_mut(entity).remove::<azalea_entity::LookDirection>();assert!(!apply_native_look(&mut world,entity,10.,0.));
 world.despawn(entity);assert!(!apply_native_look(&mut world,entity,10.,0.));
 }
}

fn apply_native_movement(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,forward:f32,left:f32,jump:bool,sneak:bool)->bool{
 if !forward.is_finite()||!left.is_finite()||world.get::<azalea_entity::Jumping>(entity).is_none()||world.get::<azalea_physics::local_player::PhysicsState>(entity).is_none(){return false;}
 native_elytra::jump_edge(world,entity,jump);
 if let Some(mut value)=world.get_mut::<azalea_entity::Jumping>(entity){value.0=jump;}
 if let Some(mut value)=world.get_mut::<azalea_physics::local_player::PhysicsState>(entity){value.trying_to_crouch=sneak;}
 world.entity_mut(entity).insert(azalea_client::movement::AnalogMovement{forward,left});true
}
#[cfg(test)]mod native_transfer_movement_tests{use super::*;
 #[test]fn movement_does_not_recreate_missing_transfer_state(){
 let mut world=bevy_ecs::world::World::new();let entity=world.spawn_empty().id();assert!(!apply_native_movement(&mut world,entity,1.,0.,true,true));assert!(world.get::<azalea_client::movement::AnalogMovement>(entity).is_none());
 world.entity_mut(entity).insert((azalea_entity::Jumping(false),azalea_physics::local_player::PhysicsState::default()));assert!(apply_native_movement(&mut world,entity,0.5,-0.5,true,true));assert!(world.get::<azalea_entity::Jumping>(entity).unwrap().0);assert!(world.get::<azalea_physics::local_player::PhysicsState>(entity).unwrap().trying_to_crouch);assert_eq!(world.get::<azalea_client::movement::AnalogMovement>(entity).unwrap().left,-0.5);
 assert!(apply_native_movement(&mut world,entity,0.,0.,false,false));assert!(!world.get::<azalea_entity::Jumping>(entity).unwrap().0);
 world.entity_mut(entity).remove::<azalea_physics::local_player::PhysicsState>();assert!(!apply_native_movement(&mut world,entity,1.,0.,true,true));assert!(!world.get::<azalea_entity::Jumping>(entity).unwrap().0);world.despawn(entity);assert!(!apply_native_movement(&mut world,entity,0.,0.,false,false));
 }
}

#[cfg(test)] mod transfer_world_load_tests {
 #[test] fn new_login_clears_previous_world_loading_and_spawn_markers() {
  use azalea_entity::{HasClientLoaded,InLoadedChunk,Dead};
  use azalea_client::events::SentSpawnEvent;
  let mut world=bevy_ecs::world::World::new();
  let (tx,mut rx)=tokio::sync::mpsc::unbounded_channel();
  let entity=world.spawn((HasClientLoaded,InLoadedChunk,SentSpawnEvent,Dead,azalea_client::events::LocalPlayerEvents(tx))).id();
  use bevy_ecs::system::RunSystemOnce;
  world.run_system_once(azalea_client::events::spawn_listener).unwrap();
  assert!(rx.try_recv().is_err());
  {let mut commands=world.commands();azalea_client::packet::game::reset_world_load_state(&mut commands,entity);}
  world.flush();
  assert!(world.get::<HasClientLoaded>(entity).is_none());
  assert!(world.get::<InLoadedChunk>(entity).is_none());
  assert!(world.get::<SentSpawnEvent>(entity).is_none());
  assert!(world.get::<Dead>(entity).is_none());
  // Destination chunks may now mark loading again without the old spawn guard.
  world.entity_mut(entity).insert(InLoadedChunk);
  assert!(world.get::<InLoadedChunk>(entity).is_some());
  assert!(world.get::<SentSpawnEvent>(entity).is_none());
  world.run_system_once(azalea_client::events::spawn_listener).unwrap();
  assert!(matches!(rx.try_recv().unwrap(),azalea_client::Event::Spawn));
  assert!(world.get::<SentSpawnEvent>(entity).is_some());
  world.run_system_once(azalea_client::events::spawn_listener).unwrap();
  assert!(rx.try_recv().is_err());
 }
}

#[cfg(test)] mod teleport_collision_tests {
 #[test] fn server_teleport_moves_collision_bounds_before_any_physics_tick() {
  use azalea_core::position::Vec3;
  use azalea_entity::{Position,Physics,LookDirection,dimensions::EntityDimensions};
  use azalea_protocol::{common::movements::{PositionMoveRotation,RelativeMovements},packets::game::ClientboundPlayerPosition};
  let mut world=bevy_ecs::world::World::new();
  let dims=EntityDimensions{width:0.6,height:1.8,eye_height:1.62,fixed:false};
  let entity=world.spawn((Position::new(Vec3::new(0.,0.,0.)),Physics::default(),LookDirection::new(0.,0.),dims.clone())).id();
  for destination in [Vec3::new(-7493.3,88.,10534.3),Vec3::new(16.,67.,-12.),Vec3::new(-2.,-50.,3.)] {
   azalea_client::packet::game::GamePacketHandler{ecs:&mut world,player:entity}.player_position(&ClientboundPlayerPosition{id:1,change:PositionMoveRotation{pos:destination,delta:Vec3::ZERO,look_direction:LookDirection::new(90.,0.)},relative:RelativeMovements::default()});
   let position=world.get::<Position>(entity).unwrap();let physics=world.get::<Physics>(entity).unwrap();
   assert_eq!(**position,destination);assert_eq!(physics.bounding_box,dims.make_bounding_box(destination));assert_eq!(physics.velocity,Vec3::ZERO);
  }
 }
}

fn preserve_respawn_world(previous:&mut Option<String>,next:String,is_respawn:bool)->bool{
 let preserve=is_respawn&&previous.as_deref()==Some(next.as_str());*previous=Some(next);preserve
}
#[cfg(test)]mod respawn_world_tests{
 use super::*;
 #[test]fn every_game_login_publishes_current_client_settings(){
  use azalea_client::{test_utils::prelude::*,packet::game::SendGamePacketEvent};use azalea_protocol::packets::{ConnectionProtocol,game::ServerboundGamePacket};use bevy_ecs::prelude::*;use std::sync::{Arc,Mutex};
  let mut simulation=Simulation::new(ConnectionProtocol::Game);simulation.app.world_mut().get_mut::<azalea_client::ClientInformation>(simulation.entity).unwrap().view_distance=7;
  let values=Arc::new(Mutex::new(Vec::new()));let captured=values.clone();simulation.app.world_mut().add_observer(move |event:On<SendGamePacketEvent>|{if let ServerboundGamePacket::ClientInformation(p)=&event.packet{captured.lock().unwrap().push(p.client_information.view_distance);}});
  for expected in [1,2]{simulation.receive_packet(default_login_packet());simulation.tick();let sent=values.lock().unwrap();assert_eq!(sent.len(),expected);assert!(sent.iter().all(|distance|*distance==7));}
 }
 #[test]fn packet_respawn_retains_same_world_chunk_and_clears_other_world(){
  use azalea_client::test_utils::prelude::*;use azalea_protocol::packets::ConnectionProtocol;use azalea_registry::{DataRegistry,data::DimensionKind,identifier::Identifier};use azalea_core::position::ChunkPos;
  let mut simulation=Simulation::new(ConnectionProtocol::Game);simulation.receive_packet(default_login_packet());simulation.tick();simulation.receive_packet(make_basic_empty_chunk(ChunkPos::new(0,0),24));simulation.tick();let before=simulation.chunk(ChunkPos::new(0,0)).unwrap();
  simulation.receive_packet(azalea_protocol::packets::game::ClientboundAddEntity{id:42.into(),uuid:Default::default(),entity_type:azalea_registry::builtin::EntityKind::Zombie,position:azalea_core::position::Vec3::new(1.,64.,1.),x_rot:0,y_rot:0,y_head_rot:0,data:0,movement:Default::default()});simulation.tick();let entity=simulation.app.world().get::<azalea_entity::indexing::EntityIdIndex>(simulation.entity).unwrap().get_by_minecraft_entity(azalea_world::MinecraftEntityId(42)).unwrap();assert!(simulation.app.world().get::<azalea_entity::LoadedBy>(entity).unwrap().contains(&simulation.entity));

  simulation.receive_packet(make_basic_respawn_packet(DimensionKind::new_raw(0),Identifier::new("minecraft:overworld")));simulation.tick();let after=simulation.chunk(ChunkPos::new(0,0)).expect("same-world respawn must retain loaded chunk");assert!(std::sync::Arc::ptr_eq(&before,&after));assert!(simulation.app.world().get::<azalea_entity::LoadedBy>(entity).unwrap().contains(&simulation.entity));
  simulation.receive_packet(make_basic_respawn_packet(DimensionKind::new_raw(0),Identifier::new("test:other_world")));simulation.tick();assert!(simulation.chunk(ChunkPos::new(0,0)).is_none());assert!(simulation.app.world().get::<azalea_entity::LoadedBy>(entity).is_none_or(|loaded|!loaded.contains(&simulation.entity)));
 }
 #[test]fn only_same_world_respawn_retains_cached_state(){let mut world=None;assert!(!preserve_respawn_world(&mut world,"minecraft:overworld".into(),false));assert!(preserve_respawn_world(&mut world,"minecraft:overworld".into(),true));assert!(!preserve_respawn_world(&mut world,"minecraft:the_nether".into(),true));assert!(preserve_respawn_world(&mut world,"minecraft:the_nether".into(),true));assert!(!preserve_respawn_world(&mut world,"minecraft:the_nether".into(),false));world=None;assert!(!preserve_respawn_world(&mut world,"minecraft:the_nether".into(),true));}
}

fn use_hand(inv:&azalea_entity::inventory::Inventory,requested:Option<bool>)->azalea_protocol::packets::game::s_interact::InteractionHand{
 use azalea_protocol::packets::game::s_interact::InteractionHand;
 let off=requested.unwrap_or_else(||inv.held_item().is_empty()&&inv.get_equipment(azalea_inventory::components::EquipmentSlot::Offhand).is_some_and(|i|!i.is_empty()));
 if off{InteractionHand::OffHand}else{InteractionHand::MainHand}
}
fn offhand_snapshot(inv:&azalea_entity::inventory::Inventory)->Option<ItemSnapshot>{
 inv.get_equipment(azalea_inventory::components::EquipmentSlot::Offhand).and_then(item_snapshot)
}
#[cfg(test)]mod offhand_inventory_tests{
 use super::*;
 #[test]fn block_and_air_use_packets_preserve_selected_hand(){
  use azalea_protocol::packets::game::{ServerboundGamePacket,s_interact::InteractionHand};
  use azalea_client::{interact::{StartUseItemQueued,BlockStatePredictionHandler,pick::HitResultComponent},packet::game::SendGamePacketEvent};
  use bevy_ecs::{prelude::*,system::RunSystemOnce};use std::sync::{Arc,Mutex};
  let mut world=World::new();let packets=Arc::new(Mutex::new(Vec::<ServerboundGamePacket>::new()));let captured=packets.clone();world.add_observer(move |event:On<SendGamePacketEvent>|{captured.lock().unwrap().push(event.packet.clone());});
  for hand in [InteractionHand::OffHand,InteractionHand::MainHand]{for miss in [true,false]{packets.lock().unwrap().clear();let hit=azalea_core::hit_result::HitResult::Block(azalea_core::hit_result::BlockHitResult{location:Default::default(),direction:azalea_core::direction::Direction::Up,block_pos:Default::default(),inside:false,world_border:false,miss});world.spawn((StartUseItemQueued{hand,force_block:None},BlockStatePredictionHandler::default(),HitResultComponent::from(hit),azalea_entity::LookDirection::default()));world.run_system_once(azalea_client::interact::handle_start_use_item_queued).unwrap();world.flush();let sent=packets.lock().unwrap();assert_eq!(sent.len(),1);match &sent[0]{ServerboundGamePacket::UseItem(p)=>{assert!(miss);assert_eq!(p.hand,hand);assert_eq!(p.seq,1);},ServerboundGamePacket::UseItemOn(p)=>{assert!(!miss);assert_eq!(p.hand,hand);assert_eq!(p.seq,1);},_=>panic!("unexpected use packet")}}}
 }
 #[test]fn entity_interaction_packets_preserve_selected_hand(){
  use azalea_protocol::packets::game::{ServerboundGamePacket,s_interact::{InteractionHand,ActionType}};
  use azalea_client::{interact::{EntityInteractEvent,BlockStatePredictionHandler,pick::HitResultComponent},packet::game::SendGamePacketEvent};
  use bevy_ecs::prelude::*;use std::sync::{Arc,Mutex};
  let mut world=World::new();let packets=Arc::new(Mutex::new(Vec::<ServerboundGamePacket>::new()));let captured=packets.clone();world.add_observer(move |event:On<SendGamePacketEvent>|{captured.lock().unwrap().push(event.packet.clone());});world.add_observer(azalea_client::interact::handle_entity_interact);
  let target=world.spawn((azalea_entity::Position::new(azalea_core::position::Vec3::new(1.,0.,0.)),azalea_entity::EntityKindComponent(azalea_registry::builtin::EntityKind::Player))).id();
  let mut index=azalea_entity::indexing::EntityIdIndex::default();index.insert(azalea_world::MinecraftEntityId(42),target);
  let mut inventory=azalea_entity::inventory::Inventory::default();for slot in [36,45]{*inventory.inventory_menu.slot_mut(slot).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Shield,1);}
  let hit=azalea_core::hit_result::HitResult::Block(azalea_core::hit_result::BlockHitResult{location:Default::default(),direction:azalea_core::direction::Direction::Up,block_pos:Default::default(),inside:false,world_border:false,miss:true});
  let client=world.spawn((azalea_client::PhysicsState::default(),index,HitResultComponent::from(hit),azalea_entity::LookDirection::default(),BlockStatePredictionHandler::default(),inventory)).id();
  for hand in [InteractionHand::OffHand,InteractionHand::MainHand]{packets.lock().unwrap().clear();world.trigger(EntityInteractEvent{client,target,hand,location:Some(azalea_core::position::Vec3::new(1.25,0.5,0.25))});world.flush();let sent=packets.lock().unwrap();assert_eq!(sent.len(),3);for (i,packet) in sent.iter().enumerate(){match packet{ServerboundGamePacket::Interact(p)=>{assert_eq!(p.entity_id.0,42);match p.action{ActionType::InteractAt{hand:h,location}=>{assert_eq!(i,0);assert_eq!(h,hand);assert_eq!(location,azalea_core::position::Vec3::new(0.25,0.5,0.25));},ActionType::Interact{hand:h}=>{assert_eq!(i,1);assert_eq!(h,hand);},_=>panic!("unexpected attack")}},ServerboundGamePacket::UseItem(p)=>{assert_eq!(i,2);assert_eq!(p.hand,hand);assert!(p.seq>0);},_=>panic!("unexpected packet")}}}
 }
 #[test]fn selected_use_hand_defaults_and_overrides(){
  use azalea_protocol::packets::game::s_interact::InteractionHand;
  let mut inv=azalea_entity::inventory::Inventory::default();assert_eq!(use_hand(&inv,None),InteractionHand::MainHand);
  *inv.inventory_menu.slot_mut(45).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Shield,1);assert_eq!(use_hand(&inv,None),InteractionHand::OffHand);
  *inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::DiamondSword,1);assert_eq!(use_hand(&inv,None),InteractionHand::MainHand);assert_eq!(use_hand(&inv,Some(true)),InteractionHand::OffHand);assert_eq!(use_hand(&inv,Some(false)),InteractionHand::MainHand);
 }
 #[test]fn offhand_uses_authoritative_player_inventory_and_clears(){
  let mut inventory=azalea_entity::inventory::Inventory::default();
  *inventory.inventory_menu.slot_mut(45).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Shield,1);
  let shield=offhand_snapshot(&inventory).unwrap();assert_eq!(shield.resource_id,"minecraft:shield");assert_eq!(shield.count,1);
  inventory.id=7;assert_eq!(offhand_snapshot(&inventory),Some(shield));
  *inventory.inventory_menu.slot_mut(45).unwrap()=azalea_inventory::ItemStack::Empty;assert!(offhand_snapshot(&inventory).is_none());
 }
}

fn item_skin_url(item:&ItemSnapshot)->Option<String>{item.profile.as_ref().and_then(|p|p.skin_url.clone())}
fn inventory_skin_urls(inv:&InventorySnapshot)->Vec<String>{
 inv.items.iter().chain(inv.hotbar.iter()).chain(inv.armor.iter()).chain(std::iter::once(&inv.carried_item)).chain(std::iter::once(&inv.held)).chain(std::iter::once(&inv.offhand)).flatten().filter_map(item_skin_url).collect()
}
fn entity_item_skin_urls(entities:&[EntitySnapshot])->Vec<String>{
 let mut urls=Vec::new();for entity in entities {
  urls.extend(entity.equipment.iter().chain(std::iter::once(&entity.dropped_item)).flatten().filter_map(item_skin_url));
  if let Some(url)=entity.display.pointer("/item/profile/skin_url").and_then(serde_json::Value::as_str){urls.push(url.to_owned());}
 }urls
}

fn send_held_action(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,action:azalea_protocol::packets::game::s_player_action::Action)->bool {
 use azalea_protocol::packets::game::s_player_action::{Action,ServerboundPlayerAction};
 if !matches!(action,Action::DropItem|Action::DropAllItems|Action::SwapItemWithOffhand){return false;}
 let Some(inv)=world.get::<azalea_entity::inventory::Inventory>(entity) else{return false;};
 if inv.id!=0 || !inv.carried.is_empty(){return false;}
 let main=inv.get_equipment(azalea_inventory::components::EquipmentSlot::Mainhand).is_some_and(|v|!v.is_empty());
 let off=inv.get_equipment(azalea_inventory::components::EquipmentSlot::Offhand).is_some_and(|v|!v.is_empty());
 if !main && (action!=Action::SwapItemWithOffhand || !off){return false;}
 // Keep inventory snapshots authoritative; the server confirms dropped/swapped slots.
 world.trigger(azalea_client::packet::game::SendGamePacketEvent::new(entity,ServerboundPlayerAction{action,pos:Default::default(),direction:azalea_core::direction::Direction::Down,seq:0}));
 true
}
#[cfg(test)]mod native_held_action_tests {
 use super::*;use bevy_ecs::prelude::*;use std::sync::{Arc,Mutex};
 use azalea_protocol::packets::game::{ServerboundGamePacket,s_player_action::Action};
 #[test]fn drop_and_swap_emit_exact_packets_without_optimistic_inventory_changes(){
  let mut world=World::new();let packets=Arc::new(Mutex::new(Vec::<ServerboundGamePacket>::new()));let captured=packets.clone();world.add_observer(move |event:On<azalea_client::packet::game::SendGamePacketEvent>|{captured.lock().unwrap().push(event.packet.clone());});
  let mut inventory=azalea_entity::inventory::Inventory::default();let held=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Mace,1);*inventory.inventory_menu.slot_mut(36).unwrap()=held.clone();let entity=world.spawn(inventory).id();
  for action in[Action::DropItem,Action::DropAllItems,Action::SwapItemWithOffhand]{assert!(send_held_action(&mut world,entity,action));world.flush();let sent=packets.lock().unwrap();let ServerboundGamePacket::PlayerAction(packet)=sent.last().unwrap() else{panic!("wrong packet")};assert_eq!(packet.action,action);assert_eq!(packet.pos,Default::default());assert_eq!(packet.direction,azalea_core::direction::Direction::Down);assert_eq!(packet.seq,0);assert_eq!(world.get::<azalea_entity::inventory::Inventory>(entity).unwrap().inventory_menu.slot(36).unwrap(),&held);}
  assert_eq!(packets.lock().unwrap().len(),3);
  world.get_mut::<azalea_entity::inventory::Inventory>(entity).unwrap().id=7;assert!(!send_held_action(&mut world,entity,Action::DropItem));
  {let mut inv=world.get_mut::<azalea_entity::inventory::Inventory>(entity).unwrap();inv.id=0;inv.carried=held.clone();}assert!(!send_held_action(&mut world,entity,Action::SwapItemWithOffhand));
  {let mut inv=world.get_mut::<azalea_entity::inventory::Inventory>(entity).unwrap();inv.carried=azalea_inventory::ItemStack::Empty;*inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::Empty;*inv.inventory_menu.slot_mut(45).unwrap()=held;}
  assert!(!send_held_action(&mut world,entity,Action::DropAllItems));assert!(send_held_action(&mut world,entity,Action::SwapItemWithOffhand));world.flush();assert_eq!(packets.lock().unwrap().len(),4);assert!(!send_held_action(&mut world,entity,Action::StartDestroyBlock));
 }
}

#[cfg(test)]mod armor_stand_metadata_tests{
 use super::*;use azalea_entity::{EntityKindComponent,metadata::*};
 #[test]fn vanilla_default_poses_and_inverted_no_base_plate_bit_survive_bridge(){let mut world=bevy_ecs::world::World::new();let e=world.spawn((EntityKindComponent(azalea_registry::builtin::EntityKind::ArmorStand),ArmorStandMetadataBundle::default())).id();let value=armor_stand_metadata(&world,e);assert_eq!(value["show_base_plate"],true);assert_eq!(value["show_arms"],false);assert_eq!(value["poses"][2],serde_json::json!([-10.,0.,-10.]));assert_eq!(value["poses"][3],serde_json::json!([-15.,0.,10.]));world.entity_mut(e).insert((Small(true),ShowArms(true),ShowBasePlate(true),HeadPose(azalea_entity::Rotations{x:45.,y:30.,z:5.})));let value=armor_stand_metadata(&world,e);assert_eq!(value["small"],true);assert_eq!(value["show_arms"],true);assert_eq!(value["show_base_plate"],false);assert_eq!(value["poses"][0],serde_json::json!([45.,30.,5.]));}
 #[test]fn other_entities_and_invalid_rotations_never_gain_invented_stand_geometry(){let mut world=bevy_ecs::world::World::new();let e=world.spawn(EntityKindComponent(azalea_registry::builtin::EntityKind::Zombie)).id();assert!(armor_stand_metadata(&world,e).is_null());world.entity_mut(e).insert((EntityKindComponent(azalea_registry::builtin::EntityKind::ArmorStand),HeadPose(azalea_entity::Rotations{x:f32::NAN,y:0.,z:0.})));assert_eq!(armor_stand_metadata(&world,e)["poses"][0],serde_json::json!([0.,0.,0.]));}
}

// Java 1.21.11 cooldowns are keyed by resource identifier, including custom
// use_cooldown component groups. The server controls start/reset durations.
#[derive(Default)]
struct NativeItemCooldowns(std::collections::HashMap<String,u32>);
impl NativeItemCooldowns {
 fn set(&mut self,group:String,ticks:u32){if ticks==0{self.0.remove(&group);}else{self.0.insert(group,ticks);}}
 fn tick(&mut self){self.0.retain(|_,ticks|{*ticks=ticks.saturating_sub(1);*ticks>0});}
 fn clear(&mut self){self.0.clear();}
 fn ready(&self,inv:&azalea_entity::inventory::Inventory,hand:azalea_protocol::packets::game::s_interact::InteractionHand)->bool{
  let slot=match hand{azalea_protocol::packets::game::s_interact::InteractionHand::MainHand=>azalea_inventory::components::EquipmentSlot::Mainhand,azalea_protocol::packets::game::s_interact::InteractionHand::OffHand=>azalea_inventory::components::EquipmentSlot::Offhand};
  let Some(azalea_inventory::ItemStack::Present(item))=inv.get_equipment(slot) else{return true;};
  let group=item.get_component::<azalea_inventory::components::UseCooldown>().and_then(|v|v.cooldown_group.clone()).map(|v|v.to_string()).unwrap_or_else(||item.kind.to_string());
  !self.0.contains_key(&group)
 }
}
#[cfg(test)]mod native_weapon_protocol_tests{
 use super::*;
 #[test]fn spear_uses_component_min_max_range_without_extending_entity_interaction(){
  use azalea_entity::{Position,LoadedBy,Attributes,dimensions::EntityDimensions};use azalea_core::position::Vec3;
  let mut inv=azalea_entity::inventory::Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::IronSpear,1);
  let mut world=bevy_ecs::world::World::new();let local=world.spawn((Position::new(Vec3::default()),EntityDimensions::new(0.6,1.8),Attributes::new(azalea_registry::builtin::EntityKind::Player),inv)).id();let mut loaded=LoadedBy(Default::default());loaded.0.insert(local);let target=world.spawn((azalea_world::MinecraftEntityId(123),loaded,Position::new(Vec3::new(4.4,0.,0.)),EntityDimensions::new(0.6,1.8))).id();
  assert_eq!(resolve_weapon_target(&mut world,local,123),Some(target));assert_eq!(resolve_attack_target(&mut world,local,123),None);world.entity_mut(target).insert(Position::new(Vec3::new(1.,0.,0.)));assert_eq!(resolve_weapon_target(&mut world,local,123),None);world.entity_mut(target).insert(Position::new(Vec3::new(6.,0.,0.)));assert_eq!(resolve_weapon_target(&mut world,local,123),None);
 }

 #[test]fn cooldown_wire_is_modern_identifier_and_ticks(){
  use azalea_buf::{AzaleaRead,AzaleaWrite};use std::io::Cursor;
  let p=azalea_protocol::packets::game::ClientboundCooldown{cooldown_group:azalea_registry::identifier::Identifier::new("minecraft:wind_charge"),duration:20};
  let mut b=Vec::new();p.azalea_write(&mut b).unwrap();assert_eq!(b[0],21);assert_eq!(&b[1..22],b"minecraft:wind_charge");assert_eq!(b[22],20);let mut c=Cursor::new(b.as_slice());assert_eq!(azalea_protocol::packets::game::ClientboundCooldown::azalea_read(&mut c).unwrap(),p);assert_eq!(c.position(),23);
 }
 #[test]fn cooldowns_expire_reset_and_do_not_disable_other_hand(){
  use azalea_protocol::packets::game::s_interact::InteractionHand::{MainHand,OffHand};
  let mut inv=azalea_entity::inventory::Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::WindCharge,4);*inv.inventory_menu.slot_mut(45).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Mace,1);
  let mut cd=NativeItemCooldowns::default();cd.set("minecraft:wind_charge".into(),2);assert!(!cd.ready(&inv,MainHand));assert!(cd.ready(&inv,OffHand));cd.tick();assert!(!cd.ready(&inv,MainHand));cd.tick();assert!(cd.ready(&inv,MainHand));cd.set("minecraft:wind_charge".into(),100);cd.set("minecraft:wind_charge".into(),0);assert!(cd.ready(&inv,MainHand));cd.set("minecraft:wind_charge".into(),100);cd.clear();assert!(cd.ready(&inv,MainHand));
 }
}

fn native_held_piercing(world:&bevy_ecs::world::World,local:bevy_ecs::entity::Entity)->bool{
 world.get::<azalea_entity::inventory::Inventory>(local).is_some_and(|inv|match inv.held_item(){azalea_inventory::ItemStack::Present(item)=>item.get_component::<azalea_inventory::components::PiercingWeapon>().is_some(),_=>false})
}
fn send_native_piercing_attack(world:&mut bevy_ecs::world::World,local:bevy_ecs::entity::Entity)->bool{
 if !native_held_piercing(world,local){return false;}
 if !world.get::<azalea_entity::inventory::Inventory>(local).is_some_and(|inv|inv.id==0&&inv.carried.is_empty()){return true;}
 world.trigger(azalea_client::packet::game::SendGamePacketEvent::new(local,azalea_protocol::packets::game::s_player_action::ServerboundPlayerAction{action:azalea_protocol::packets::game::s_player_action::Action::Stab,pos:Default::default(),direction:azalea_core::direction::Direction::Down,seq:0}));
 if let Some(mut ticker)=world.get_mut::<azalea_client::attack::TicksSinceLastAttack>(local){ticker.0=0;}
 if let Some(mut scale)=world.get_mut::<azalea_client::attack::AttackStrengthScale>(local){scale.0=0.;}
 true
}

fn native_attack_charge_ready(world:&bevy_ecs::world::World,local:bevy_ecs::entity::Entity)->bool{
 let required=world.get::<azalea_entity::inventory::Inventory>(local).and_then(|inv|match inv.held_item(){azalea_inventory::ItemStack::Present(item)=>item.get_component::<azalea_inventory::components::MinimumAttackCharge>().map(|v|v.value),_=>None}).unwrap_or(0.);
 let actual=world.get::<azalea_client::attack::AttackStrengthScale>(local).map(|v|v.0).unwrap_or(1.);
 required.is_finite()&&actual.is_finite()&&actual>=required.clamp(0.,1.)
}

#[derive(Debug,Clone,Default,PartialEq,serde::Serialize)]
pub struct NativeUseParameters {
 pub use_duration:u32,pub use_charge_duration:u32,pub use_animation:String,
 pub use_kinetic_delay:i32,pub use_kinetic_dismount:i32,pub use_kinetic_knockback:i32,pub use_kinetic_damage:i32,
}
fn native_use_parameters(item:Option<&azalea_inventory::ItemStack>)->NativeUseParameters {
 use azalea_inventory::{ItemStack,components::{Consumable,KineticWeapon}};
 let mut params=NativeUseParameters::default();
 let (duration,animation)=match item{
  Some(ItemStack::Present(item))=>{
   let specific=match item.kind.to_string().as_str(){"minecraft:bow"=>Some((72000,"bow".into())),"minecraft:crossbow"=>Some((72000,"crossbow".into())),"minecraft:trident"=>Some((72000,"spear".into())),"minecraft:spyglass"=>Some((1200,"spyglass".into())),"minecraft:brush"=>Some((200,"brush".into())),_=>None};
   if let Some(specific)=specific {specific}
   else if let Some(c)=item.get_component::<Consumable>(){let secs=c.consume_seconds;if secs.is_finite()&&secs>=0.{(((secs*20.) as i32).max(0) as u32,serde_json::to_value(c.animation).ok().and_then(|v|v.as_str().map(str::to_owned)).unwrap_or_default())}else{(0,String::new())}}
   else if item.get_component::<azalea_inventory::components::BlocksAttacks>().is_some(){(72000,"block_kind".into())}
   else if item.get_component::<KineticWeapon>().is_some(){(72000,"spear".into())}
   else{(0,String::new())}
  },_ =>(0,String::new())
 };
 let kinetic=item.and_then(|stack|match stack{ItemStack::Present(item)=>item.get_component::<KineticWeapon>().map(|v|v.into_owned()),_=>None});
 params.use_kinetic_delay=kinetic.as_ref().map(|v|v.delay_ticks).unwrap_or(0).max(0);
 params.use_kinetic_dismount=kinetic.as_ref().and_then(|v|v.dismount_conditions.as_ref()).map(|v|v.max_duration_ticks).unwrap_or(0).max(0);
 params.use_kinetic_knockback=kinetic.as_ref().and_then(|v|v.knockback_conditions.as_ref()).map(|v|v.max_duration_ticks).unwrap_or(0).max(0);
 params.use_kinetic_damage=kinetic.as_ref().and_then(|v|v.damage_conditions.as_ref()).map(|v|v.max_duration_ticks).unwrap_or(0).max(0);
 params.use_duration=duration;params.use_animation=animation;params
}
#[derive(Debug,Clone,Default,PartialEq,serde::Serialize)]
pub struct RemoteUseSnapshot {
 pub using_item:bool,pub using_offhand:bool,pub use_ticks:u32,
 #[serde(flatten)]pub parameters:NativeUseParameters,
}
fn remote_use_snapshot(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,local:bevy_ecs::entity::Entity,item:Option<&azalea_inventory::ItemStack>,previous:&mut RemoteUseSnapshot)->RemoteUseSnapshot {
 let active=world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(entity).is_some_and(|v|v.0);
 let off=world.get::<azalea_entity::metadata::AbstractLivingUsingOffHand>(entity).is_some_and(|v|v.0);
 let mut params=native_use_parameters(item);params.use_charge_duration=super::native_java_use::crossbow_charge_duration(world,local,item);
 let continuing=active&&previous.using_item&&previous.using_offhand==off&&previous.parameters==params;
 previous.use_ticks=if active{if continuing{previous.use_ticks.saturating_add(1)}else{0}}else{0};
 previous.using_item=active;previous.using_offhand=off;previous.parameters=params;previous.clone()
}

fn predict_native_air_shield_use(world:&mut bevy_ecs::world::World,local:bevy_ecs::entity::Entity,hand:azalea_protocol::packets::game::s_interact::InteractionHand)->bool {
 use azalea_inventory::{ItemStack,components::{BlocksAttacks,EquipmentSlot}};
 use azalea_entity::metadata::{AbstractLivingUsingItem,AbstractLivingUsingOffHand};
 use azalea_protocol::packets::game::s_interact::InteractionHand;
 if world.get::<azalea_client::local_player::LocalGameMode>(local).is_some_and(|g|g.current==azalea_core::game_type::GameMode::Spectator){return false;}
 let air=world.get::<azalea_client::interact::pick::HitResultComponent>(local).is_some_and(|hit|matches!(&**hit,azalea_core::hit_result::HitResult::Block(block) if block.miss));
 if !air{return false;}
 let shield=world.get::<azalea_entity::inventory::Inventory>(local).and_then(|inv|inv.get_equipment(if hand==InteractionHand::OffHand{EquipmentSlot::Offhand}else{EquipmentSlot::Mainhand})).is_some_and(|stack|matches!(stack,ItemStack::Present(item) if item.kind==azalea_registry::builtin::ItemKind::Shield&&item.get_component::<BlocksAttacks>().is_some()));
 if !shield{return false;}
 world.entity_mut(local).insert((AbstractLivingUsingItem(true),AbstractLivingUsingOffHand(hand==InteractionHand::OffHand)));true
}

pub(crate) fn clear_native_item_use(world:&mut bevy_ecs::world::World,local:bevy_ecs::entity::Entity){
 use azalea_entity::metadata::{AbstractLivingUsingItem,AbstractLivingUsingOffHand};
 if let Ok(mut entity)=world.get_entity_mut(local){entity.remove::<targeting::HeldAim>();entity.remove::<azalea_client::interact::StartUseItemQueued>();entity.remove::<azalea_client::interact::native_use::PredictedNativeUse>();entity.remove::<azalea_client::interact::native_use::NativeUseOptions>();}
 if let Some(mut flag)=world.get_mut::<AbstractLivingUsingItem>(local){flag.0=false;}
 if let Some(mut flag)=world.get_mut::<AbstractLivingUsingOffHand>(local){flag.0=false;}
}

fn refresh_native_item_use(world:&bevy_ecs::world::World,local:bevy_ecs::entity::Entity,vitals:&mut PlayerVitals){
 use azalea_inventory::{ItemStack,components::{MinimumAttackCharge,EquipmentSlot}};
 let active=world.get::<azalea_entity::metadata::AbstractLivingUsingItem>(local).is_some_and(|v|v.0);
 let off=world.get::<azalea_entity::metadata::AbstractLivingUsingOffHand>(local).is_some_and(|v|v.0);
 let inventory=world.get::<azalea_entity::inventory::Inventory>(local);
 let item=inventory.and_then(|inv|if off{inv.get_equipment(EquipmentSlot::Offhand)}else{Some(inv.held_item())});
 let mut params=native_use_parameters(item);params.use_charge_duration=super::native_java_use::crossbow_charge_duration(world,local,item);
 let duration=params.use_duration;let animation=params.use_animation;
 vitals.use_kinetic_delay=params.use_kinetic_delay;vitals.use_kinetic_dismount=params.use_kinetic_dismount;
 vitals.use_kinetic_knockback=params.use_kinetic_knockback;vitals.use_kinetic_damage=params.use_kinetic_damage;
 let continuing=vitals.use_active&&active&&vitals.use_off_hand==off&&vitals.use_animation==animation&&vitals.use_duration==duration;
 vitals.use_ticks=if active{if continuing{vitals.use_ticks.saturating_add(1)}else{0}}else{0};
 vitals.use_active=active;vitals.use_off_hand=off;vitals.use_duration=duration;vitals.use_charge_duration=params.use_charge_duration;vitals.use_animation=animation;
 vitals.attack_piercing=native_held_piercing(world,local);
 vitals.attack_strength=world.get::<azalea_client::attack::AttackStrengthScale>(local).map(|v|v.0).filter(|v|v.is_finite()).unwrap_or(1.).clamp(0.,1.);
 vitals.attack_min_charge=inventory.and_then(|inv|match inv.held_item(){ItemStack::Present(item)=>item.get_component::<MinimumAttackCharge>().map(|v|v.value),_=>None}).filter(|v|v.is_finite()).unwrap_or(0.).clamp(0.,1.);
}

fn refresh_weapon_ranges(world:&bevy_ecs::world::World,local:bevy_ecs::entity::Entity,vitals:&mut PlayerVitals){
 let creative=world.get::<azalea_client::local_player::LocalGameMode>(local).is_some_and(|g|g.current==azalea_core::game_type::GameMode::Creative);
 let range=world.get::<azalea_entity::inventory::Inventory>(local).and_then(|inv|match inv.held_item(){azalea_inventory::ItemStack::Present(item)=>item.get_component::<azalea_inventory::components::AttackRange>().map(|v|v.into_owned()),_=>None});
 let (min,max,margin)=range.map(|v|(if creative{v.min_creative_reach}else{v.min_reach},if creative{v.max_creative_reach}else{v.max_reach},v.hitbox_margin)).unwrap_or((0.,world.get::<azalea_entity::Attributes>(local).map(|a|a.entity_interaction_range.calculate() as f32).unwrap_or(3.),0.3));
 if min.is_finite()&&max.is_finite()&&margin.is_finite()&&min>=0.&&max>=min&&margin>=0.{vitals.attack_min_range=min;vitals.attack_max_range=max;vitals.attack_hitbox_margin=margin;}
}

fn complete_signed_chat(bot:&Client,tx:&std::sync::mpsc::Sender<Update>,packet:std::sync::Arc<azalea_protocol::packets::game::c_player_chat::ClientboundPlayerChat>,display:bool){
 if let Some(signature)=&packet.signature {
  let mut world=bot.ecs.lock();let ack=world.get_mut::<azalea_client::chat::secure_state::SecureChatState>(bot.entity).and_then(|mut state|state.complete_received(signature.clone(),display));
  if let Some(messages)=ack{world.trigger(azalea_client::packet::game::SendGamePacketEvent::new(bot.entity,azalea_protocol::packets::game::ServerboundChatAck{messages}));}
 }
 if display{let message=packet.message();let text=styled_text(&message);let signature=packet.signature.as_ref().map(incoming_chat::signature_id).unwrap_or_default();let _=tx.send(Update::SignedChat{text,signature});let actions=chat_actions(&message);if !actions.is_empty(){let _=tx.send(Update::ChatActions(actions));}}
}

#[cfg(test)]mod native_active_use_tests{
 use super::*;
 #[test]fn air_shield_prediction_respects_hit_hand_and_release(){
  use azalea_protocol::packets::game::s_interact::InteractionHand;
  let mut inv=azalea_entity::inventory::Inventory::default();*inv.inventory_menu.slot_mut(45).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Shield,1);
  let hit=|miss|azalea_client::interact::pick::HitResultComponent::from(azalea_core::hit_result::HitResult::Block(azalea_core::hit_result::BlockHitResult{location:Default::default(),direction:azalea_core::direction::Direction::Up,block_pos:Default::default(),inside:false,world_border:false,miss}));
  let mut world=bevy_ecs::world::World::new();let e=world.spawn((inv,hit(false))).id();
  assert!(!predict_native_air_shield_use(&mut world,e,InteractionHand::OffHand));world.entity_mut(e).insert(hit(true));assert!(!predict_native_air_shield_use(&mut world,e,InteractionHand::MainHand));assert!(predict_native_air_shield_use(&mut world,e,InteractionHand::OffHand));
  let mut v=PlayerVitals::default();refresh_native_item_use(&world,e,&mut v);assert!(v.use_active&&v.use_off_hand);assert_eq!(v.use_animation,"block_kind");clear_native_item_use(&mut world,e);refresh_native_item_use(&world,e,&mut v);assert!(!v.use_active);
 }
 #[test]fn released_use_clears_both_flags_and_elapsed_without_changing_remote_entity(){
  use azalea_entity::metadata::{AbstractLivingUsingItem,AbstractLivingUsingOffHand};
  let mut inv=azalea_entity::inventory::Inventory::default();
  *inv.inventory_menu.slot_mut(45).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Shield,1);
  let mut world=bevy_ecs::world::World::new();let local=world.spawn((inv,AbstractLivingUsingItem(true),AbstractLivingUsingOffHand(true))).id();
  world.entity_mut(local).insert(azalea_client::interact::StartUseItemQueued{hand:azalea_protocol::packets::game::s_interact::InteractionHand::OffHand,force_block:None});
  let remote=world.spawn((AbstractLivingUsingItem(true),AbstractLivingUsingOffHand(true))).id();
  let mut v=PlayerVitals::default();refresh_native_item_use(&world,local,&mut v);refresh_native_item_use(&world,local,&mut v);assert_eq!(v.use_ticks,1);
  clear_native_item_use(&mut world,local);refresh_native_item_use(&world,local,&mut v);assert!(!v.use_active&&!v.use_off_hand);assert_eq!(v.use_ticks,0);assert!(world.get::<azalea_client::interact::StartUseItemQueued>(local).is_none());
  assert!(world.get::<AbstractLivingUsingItem>(remote).unwrap().0);assert!(world.get::<AbstractLivingUsingOffHand>(remote).unwrap().0);
  clear_native_item_use(&mut world,local);
 }
 #[test]fn server_flags_select_offhand_and_reset_elapsed(){
  use azalea_entity::metadata::{AbstractLivingUsingItem,AbstractLivingUsingOffHand};
  let mut inv=azalea_entity::inventory::Inventory::default();
  *inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Bow,1);
  *inv.inventory_menu.slot_mut(45).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Shield,1);
  let mut world=bevy_ecs::world::World::new();let e=world.spawn((inv,AbstractLivingUsingItem(true),AbstractLivingUsingOffHand(true),azalea_client::attack::AttackStrengthScale(0.5))).id();
  let mut v=PlayerVitals::default();refresh_native_item_use(&world,e,&mut v);assert!(v.use_active&&v.use_off_hand);assert_eq!(v.use_animation,"block_kind");assert_eq!(v.use_duration,72000);assert_eq!(v.use_ticks,0);
  refresh_native_item_use(&world,e,&mut v);assert_eq!(v.use_ticks,1);
  world.entity_mut(e).insert(AbstractLivingUsingOffHand(false));refresh_native_item_use(&world,e,&mut v);assert_eq!(v.use_animation,"bow");assert_eq!(v.use_ticks,0);
  world.entity_mut(e).insert(AbstractLivingUsingItem(false));refresh_native_item_use(&world,e,&mut v);assert!(!v.use_active);assert_eq!(v.use_ticks,0);assert_eq!(v.attack_strength,0.5);
 }
 #[test]fn exact_spear_minimum_charge_rejects_repeated_early_attacks(){
  let mut inv=azalea_entity::inventory::Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::IronSpear,1);
  let mut world=bevy_ecs::world::World::new();let e=world.spawn((inv,azalea_client::attack::AttackStrengthScale(0.99))).id();assert!(!native_attack_charge_ready(&world,e));world.entity_mut(e).insert(azalea_client::attack::AttackStrengthScale(1.));assert!(native_attack_charge_ready(&world,e));
  let mut v=PlayerVitals::default();refresh_native_item_use(&world,e,&mut v);assert_eq!(v.attack_min_charge,1.);assert_eq!(v.use_animation,"spear");
 }
}

#[cfg(test)]mod native_piercing_attack_tests{
 use super::*;use bevy_ecs::prelude::*;use std::sync::{Arc,Mutex};
 #[test]fn exact_stab_packet_and_cooldown_without_selected_target(){
  use azalea_protocol::packets::game::{ServerboundGamePacket,s_player_action::Action};use azalea_buf::AzaleaWrite;
  let mut world=World::new();let sent=Arc::new(Mutex::new(Vec::new()));let captured=sent.clone();world.add_observer(move |event:On<azalea_client::packet::game::SendGamePacketEvent>|captured.lock().unwrap().push(event.packet.clone()));
  let mut inv=azalea_entity::inventory::Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::IronSpear,1);
  let e=world.spawn((inv,azalea_client::attack::TicksSinceLastAttack(30),azalea_client::attack::AttackStrengthScale(1.))).id();assert!(send_native_piercing_attack(&mut world,e));world.flush();
  let p=sent.lock().unwrap();assert_eq!(p.len(),1);let ServerboundGamePacket::PlayerAction(packet)=&p[0]else{panic!("wrong packet")};assert_eq!(packet.action,Action::Stab);assert_eq!(packet.direction,azalea_core::direction::Direction::Down);assert_eq!(packet.pos,Default::default());assert_eq!(packet.seq,0);let mut bytes=Vec::new();packet.azalea_write(&mut bytes).unwrap();assert_eq!(bytes,vec![7,0,0,0,0,0,0,0,0,0,0]);assert_eq!(world.get::<azalea_client::attack::TicksSinceLastAttack>(e).unwrap().0,0);assert!(!native_attack_charge_ready(&world,e));
 }
}
#[cfg(test)]mod remote_use_snapshot_tests{
 use super::*;
 #[test]fn remote_server_flags_and_component_timings_survive_flat_snapshot(){
  use azalea_entity::metadata::{AbstractLivingUsingItem,AbstractLivingUsingOffHand};
  let mut world=bevy_ecs::world::World::new();let e=world.spawn((AbstractLivingUsingItem(true),AbstractLivingUsingOffHand(true))).id();let shield=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Shield,1);let spear=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::IronSpear,1);let mut old=RemoteUseSnapshot::default();
  let first=remote_use_snapshot(&world,e,e,Some(&shield),&mut old);let json=serde_json::to_value(&first).unwrap();assert_eq!(json["using_item"],true);assert_eq!(json["using_offhand"],true);assert_eq!(json["use_animation"],"block_kind");assert_eq!(json["use_duration"],72000);assert_eq!(remote_use_snapshot(&world,e,e,Some(&shield),&mut old).use_ticks,1);
  world.entity_mut(e).insert(AbstractLivingUsingOffHand(false));let changed=remote_use_snapshot(&world,e,e,Some(&spear),&mut old);assert_eq!(changed.use_ticks,0);assert_eq!(changed.parameters.use_animation,"spear");let azalea_inventory::ItemStack::Present(stack)=&spear else{panic!()};let kinetic=stack.get_component::<azalea_inventory::components::KineticWeapon>().unwrap();assert_eq!(changed.parameters.use_kinetic_delay,kinetic.delay_ticks);assert_eq!(changed.parameters.use_kinetic_damage,kinetic.damage_conditions.as_ref().unwrap().max_duration_ticks);
  world.entity_mut(e).insert(AbstractLivingUsingItem(false));let stopped=remote_use_snapshot(&world,e,e,Some(&spear),&mut old);assert!(!stopped.using_item);assert_eq!(stopped.use_ticks,0);
 }
}
#[cfg(test)]mod crossbow_forward_tests{
 use super::*;
 #[test]fn actual_self_and_remote_crossbow_timing_forward_separate_charge_duration(){
  use azalea_entity::metadata::AbstractLivingUsingItem;
  let mut world=bevy_ecs::world::World::new();let item=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::Crossbow,1);let mut inv=azalea_entity::inventory::Inventory::default();*inv.inventory_menu.slot_mut(36).unwrap()=item.clone();let e=world.spawn((inv,AbstractLivingUsingItem(true))).id();let mut v=PlayerVitals::default();refresh_native_item_use(&world,e,&mut v);let json=serde_json::to_value(v).unwrap();assert_eq!(json["use_animation"],"crossbow");assert_eq!(json["use_duration"],72000);assert_eq!(json["use_charge_duration"],25);let mut previous=RemoteUseSnapshot::default();let snapshot=remote_use_snapshot(&world,e,e,Some(&item),&mut previous);let json=serde_json::to_value(snapshot).unwrap();assert_eq!(json["use_charge_duration"],25);assert_eq!(json["use_animation"],"crossbow");
 }
 #[test]fn native_crossbow_subclass_animation_and_server_components_match_item_bytecode(){
  use azalea_inventory::{ItemStack,components::{Consumable,BlocksAttacks}};use azalea_registry::builtin::ItemKind;
  let consumable=ItemStack::new(ItemKind::Apple,1).get_component::<Consumable>().unwrap().into_owned();let bow=ItemStack::new(ItemKind::Crossbow,1).with_component(consumable);let params=native_use_parameters(Some(&bow));assert_eq!(params.use_animation,"crossbow");assert_eq!(params.use_duration,72000);
  let blocks=ItemStack::new(ItemKind::Shield,1).get_component::<BlocksAttacks>().unwrap().into_owned();let custom=ItemStack::new(ItemKind::DiamondSword,1).with_component(blocks);let params=native_use_parameters(Some(&custom));assert_eq!(params.use_animation,"block_kind");assert_eq!(params.use_duration,72000);
 }

}

#[cfg(test)] mod native_death_prediction_tests {
 use bevy_ecs::{prelude::*,system::RunSystemOnce};
 use azalea_entity::{Dead,LocalEntity,HasClientLoaded,Physics,Position,EntityBundle};
 use azalea_physics::{LocalDeathTicks,LocalDeathFrozen,advance_local_death,local_player::PhysicsState};
 #[test] fn local_death_stops_prediction_on_twentieth_tick_and_reset_preserves_entity() {
  let mut world=World::new();world.init_resource::<azalea_world::InstanceContainer>();
  let _instance=world.resource_mut::<azalea_world::InstanceContainer>().get_or_insert("minecraft:overworld".parse().unwrap(),384,-64,&azalea_core::registry_holder::RegistryHolder::default());
  let player=world.spawn((EntityBundle::new(uuid::Uuid::nil(),azalea_core::position::Vec3::new(2.,70.,3.),azalea_registry::builtin::EntityKind::Player,"minecraft:overworld".parse().unwrap()),LocalEntity,HasClientLoaded,Dead,PhysicsState::default(),azalea_client::movement::LastSentLookDirection::default())).id();
  let remote=world.spawn(Dead).id();
  for tick in 1..=19 { world.run_system_once(advance_local_death).unwrap();assert_eq!(world.get::<LocalDeathTicks>(player).unwrap().0,tick);assert!(world.get::<LocalDeathFrozen>(player).is_none()); }
  world.run_system_once(advance_local_death).unwrap();assert!(world.get::<LocalDeathFrozen>(player).is_some());assert!(world.get::<LocalDeathTicks>(remote).is_none());
  world.get_mut::<Physics>(player).unwrap().velocity=azalea_core::position::Vec3::new(0.2,-3.,0.4);
  let before=**world.get::<Position>(player).unwrap();
  for _ in 0..100 {world.run_system_once(advance_local_death).unwrap();world.run_system_once(azalea_physics::travel::travel).unwrap();world.run_system_once(azalea_client::movement::send_position).unwrap();}
  assert_eq!(**world.get::<Position>(player).unwrap(),before);assert_eq!(world.get::<PhysicsState>(player).unwrap().position_remainder,0);
  assert!(world.get::<HasClientLoaded>(player).is_some());assert!(world.get_entity(player).is_ok());
  {let mut commands=world.commands();azalea_client::packet::game::reset_world_load_state(&mut commands,player);}world.flush();
  assert!(world.get::<Dead>(player).is_none());assert!(world.get::<LocalDeathTicks>(player).is_none());assert!(world.get::<LocalDeathFrozen>(player).is_none());assert!(world.get_entity(player).is_ok());
  // Positive control: the very same real physics and packet systems run again
  // after actual world-reset readiness, rather than a vacuous unmatched query.
  world.entity_mut(player).insert(HasClientLoaded);
  world.run_system_once(azalea_physics::travel::travel).unwrap();assert!(world.get::<Position>(player).unwrap().y<before.y);
  world.run_system_once(azalea_client::movement::send_position).unwrap();assert_eq!(world.get::<PhysicsState>(player).unwrap().position_remainder,0);
  assert!(world.get::<azalea_entity::LastSentPosition>(player).unwrap().y<before.y);
 }
 #[test] fn removing_dead_clears_only_local_death_lifecycle() {
  let mut world=World::new();let player=world.spawn((LocalEntity,LocalDeathTicks(20),LocalDeathFrozen,HasClientLoaded)).id();
  world.run_system_once(advance_local_death).unwrap();assert!(world.get::<LocalDeathTicks>(player).is_none());assert!(world.get::<LocalDeathFrozen>(player).is_none());assert!(world.get::<HasClientLoaded>(player).is_some());
 }
}

pub(crate) fn valid_pick_ray(enabled:bool,offset:[f64;3],direction:[f64;3],revision:u64)->bool{targeting::ray(enabled,offset,direction,revision).is_some()}
