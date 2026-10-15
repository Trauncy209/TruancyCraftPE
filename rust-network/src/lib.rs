//! Native protocol774 transport ABI. Runs in the TCPE process; no proxy.
pub mod network {
    pub mod native_java;
    pub mod native_java_use;
    pub mod native_java_auth;
    pub mod native_java_items;
    pub mod native_java_compass;
    pub mod rich_text;
    pub mod player_textures;
    pub mod server_status;
    pub mod resource_packs;
    pub mod resource_pack_fonts;
    pub mod resource_pack_download;
    pub mod resource_pack_sounds;
    pub mod resource_pack_models;
}
use network::native_java::{Command, Connection, Update};
use serde_json::{json, Value};
use std::{
    ffi::c_char,
    panic::{catch_unwind, AssertUnwindSafe},
    slice, str,
};
#[repr(C)]
pub struct TcpeJavaEvent {
    pub kind: u32,
    pub x: i32,
    pub z: i32,
    pub min_y: i32,
    pub height: u32,
    pub payload: *const u8,
    pub payload_len: usize,
    pub states: *const u16,
    pub states_len: usize,
    pub sky_light:*const u8,pub sky_light_len:usize,
    pub block_light:*const u8,pub block_light_len:usize,
    pub light_sections:*const u8,pub light_sections_len:usize,
}
pub struct Session {
    connection: Connection,
    online: bool,
    payload: Vec<u8>,
    states: Vec<u16>,
    sky_light:Vec<u8>,block_light:Vec<u8>,light_sections:Vec<u8>,
    value: Value,
}
unsafe fn text<'a>(data: *const c_char, len: usize, max: usize) -> Option<&'a str> {
    if data.is_null() || len > max {
        return None;
    }
    str::from_utf8(slice::from_raw_parts(data as *const u8, len)).ok()
}
#[repr(C)]
pub struct TcpeJavaAccountStatus {pub revision:u64,pub phase:u32,pub available:bool,pub username:[u8;17],pub message:[u8;256],pub user_code:[u8;33],pub verification_uri:[u8;256],pub expires_at:u64}
fn account_text<const N:usize>(value:&str)->[u8;N]{let mut out=[0;N];let n=value.len().min(N-1);out[..n].copy_from_slice(&value.as_bytes()[..n]);out}
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_account_status(out:*mut TcpeJavaAccountStatus)->bool{if out.is_null(){return false;}catch_unwind(AssertUnwindSafe(||{let v=network::native_java_auth::snapshot();*out=TcpeJavaAccountStatus{revision:v.revision,phase:v.phase,available:v.available,username:account_text(&v.username),message:account_text(&v.message),user_code:account_text(&v.user_code),verification_uri:account_text(&v.verification_uri),expires_at:v.expires_at};true})).unwrap_or(false)}
#[no_mangle]
pub extern "C" fn tcpe_java_account_action(action:u32)->bool{catch_unwind(||match action{0=>{network::native_java_auth::cancel();true},1=>{network::native_java_auth::begin_sign_in();true},2=>{network::native_java_auth::sign_out();true},3=>network::native_java_auth::import_prism().is_ok(),_=>false}).unwrap_or(false)}
/// Inputs are explicit UTF8 lengths, never NUL-terminated implicit reads.
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_connect(
    address: *const c_char,
    address_len: usize,
    identity: *const c_char,
    identity_len: usize,
    online: bool,
) -> *mut Session {
    catch_unwind(|| {
        let Some(address) = text(address, address_len, 1024).filter(|s| !s.is_empty()) else {
            return std::ptr::null_mut();
        };
        let Some(identity) = text(identity, identity_len, 320).filter(|s| !s.is_empty()) else {
            return std::ptr::null_mut();
        };
        if !online
            && (identity.len() > 16
                || !identity
                    .bytes()
                    .all(|c| c.is_ascii_alphanumeric() || c == b'_'))
        {
            return std::ptr::null_mut();
        }
        let connection = if online {
            Connection::online(address.into(), identity.into())
        } else {
            Connection::offline(address.into(), identity.into())
        };
        Box::into_raw(Box::new(Session {
            connection,online,
            payload: Vec::new(),
            states: Vec::new(),sky_light:Vec::new(),block_light:Vec::new(),light_sections:Vec::new(),
            value: Value::Null,
        }))
    })
    .unwrap_or(std::ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_uses_account(session:*const Session)->bool{!session.is_null()&&(*session).online}
/// Session is owned by the calling game thread; close exactly once.
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_close(session: *mut Session) {
    if !session.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| drop(Box::from_raw(session))));
    }
}
fn number(v: &Value, key: &str) -> Result<i64, ()> {
    v.get(key).and_then(Value::as_i64).ok_or(())
}
fn command(v: Value) -> Result<Command, ()> {
    let kind = v.get("type").and_then(Value::as_str).ok_or(())?;
    let string = |key: &str, max: usize| {
        v.get(key)
            .and_then(Value::as_str)
            .filter(|s| s.len() <= max)
            .map(str::to_owned)
            .ok_or(())
    };
    let boolean = |key: &str| v.get(key).and_then(Value::as_bool).ok_or(());
    let use_hand=||match v.get("hand"){None=>Ok(None),Some(value)=>match value.as_str(){Some("main")=>Ok(Some(false)),Some("off")=>Ok(Some(true)),_=>Err(())}};
    let menu_generation=||match v.get("menu_generation"){None=>Ok(None),Some(value)=>value.as_u64().map(Some).ok_or(())};
    Ok(match kind {
        "chat" => Command::Chat(string("text", 32767)?),
        "pick_ray" => {
            let epoch=v.get("epoch").and_then(Value::as_u64).ok_or(())?;
            let revision=v.get("revision").and_then(Value::as_u64).ok_or(())?;
            let vector=|key:&str|->Result<[f64;3],()>{let a=v.get(key).and_then(Value::as_array).filter(|a|a.len()==3).ok_or(())?;Ok([a[0].as_f64().ok_or(())?,a[1].as_f64().ok_or(())?,a[2].as_f64().ok_or(())?])};
            let offset=vector("offset")?;let direction=vector("direction")?;
            let enabled=boolean("enabled")?;
            if epoch==0||epoch>9007199254740991||network::native_java::valid_pick_ray(enabled,offset,direction,revision)==false{return Err(());}
            Command::PickRay{epoch,revision,enabled,offset,direction}
        },
        "look" => {
            let yaw = v.get("yaw").and_then(Value::as_f64).ok_or(())? as f32;
            let pitch = v.get("pitch").and_then(Value::as_f64).ok_or(())? as f32;
            if !yaw.is_finite() || !pitch.is_finite() || !(-90.0..=90.0).contains(&pitch) {
                return Err(());
            }
            Command::Look { yaw, pitch }
        }
        "flight" => {
            let vertical=v.get("vertical").and_then(Value::as_f64).ok_or(())?;
            if !vertical.is_finite()||!(-1.0..=1.0).contains(&vertical){return Err(());}
            Command::Flight{flying:boolean("flying")?,vertical:vertical as f32,toggle:match v.get("toggle"){None=>true,Some(value)=>value.as_bool().ok_or(())?}}
        },
        "walk" => {
            let forward = v.get("forward").and_then(Value::as_f64).ok_or(())?;
            let strafe = v.get("strafe").and_then(Value::as_f64).ok_or(())?;
            if !forward.is_finite() || !strafe.is_finite() || !(-1.0..=1.0).contains(&forward) || !(-1.0..=1.0).contains(&strafe) {
                return Err(());
            }
            Command::Walk {
                forward: forward as f32,
                strafe: strafe as f32,
                jump: boolean("jump")?,
                sneak: boolean("sneak")?,
                sprint: match v.get("sprint"){None=>false,Some(value)=>value.as_bool().ok_or(())?},
            }
        }
        "select_hotbar" => {
            let slot = number(&v, "slot")?;
            if !(0..=8).contains(&slot) {
                return Err(());
            }
            Command::SelectHotbar(slot as u8)
        }
        "click_slot" => Command::ClickSlot {
            window_id: i32::try_from(number(&v, "window_id")?).map_err(|_| ())?,
            slot: u16::try_from(number(&v, "slot")?).map_err(|_| ())?,
            right: boolean("right")?,
            generation:menu_generation()?,
        },
        "quick_move"=>Command::QuickMove{window_id:i32::try_from(number(&v,"window_id")?).map_err(|_|())?,slot:u16::try_from(number(&v,"slot")?).map_err(|_|())?,right:boolean("right")?,generation:menu_generation()?},
        "place_recipe"=>Command::PlaceRecipe{window_id:i32::try_from(number(&v,"window_id")?).map_err(|_|())?,recipe_id:u32::try_from(number(&v,"recipe_id")?).map_err(|_|())?,all:boolean("all")?,generation:v.get("menu_generation").and_then(Value::as_u64).ok_or(())?},
        "drop_selected" => Command::DropSelected(boolean("all")?),
        "swap_offhand" => Command::SwapOffhand,
        "drop_carried" => Command::DropCarried{window_id:i32::try_from(number(&v,"window_id")?).map_err(|_|())?,right:boolean("right")?,generation:menu_generation()?},
        "close_container" => {
            Command::CloseContainer{window_id:i32::try_from(number(&v, "window_id")?).map_err(|_| ())?,generation:menu_generation()?}
        }
        "use" => Command::UseItem(use_hand()?),
        "release_use" => Command::ReleaseUse,
        "mine" => Command::Mine(boolean("held")?),
        "interact" => Command::Interact{id:i32::try_from(number(&v,"entity_id")?).map_err(|_|())?,hand:use_hand()?},
        "attack" => Command::Attack(i32::try_from(number(&v,"entity_id")?).map_err(|_|())?),
        "dialog_click" => Command::DialogCustomClick(string("action", 32767)?),
        "resource_pack_loaded"=>Command::PackRendererLoaded{connection:u64::try_from(number(&v,"connection")?).map_err(|_|())?,generation:u64::try_from(number(&v,"generation")?).map_err(|_|())?,id:string("id",256)?},
        "load_pack_fonts"=>Command::LoadPackFonts{connection:u64::try_from(number(&v,"connection")?).map_err(|_|())?,generation:u64::try_from(number(&v,"generation")?).map_err(|_|())?,id:string("id",256)?},
        "respawn" => Command::Respawn,
        "disconnect" => Command::Disconnect,
        _ => return Err(()),
    })
}
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_command(
    session: *mut Session,
    data: *const c_char,
    len: usize,
) -> bool {
    if session.is_null() {
        return false;
    }
    catch_unwind(AssertUnwindSafe(|| {
        let Some(input) = text(data, len, 65536) else {
            return false;
        };
        let Ok(value) = serde_json::from_str(input) else {
            return false;
        };
        let Ok(command) = command(value) else {
            return false;
        };
        (*session).connection.commands.send(command).is_ok()
    }))
    .unwrap_or(false)
}
/// Returned buffers remain valid until next poll or close. No chunk height is cropped.
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_poll(session: *mut Session, event: *mut TcpeJavaEvent) -> i32 {
    if session.is_null() || event.is_null() {
        return -1;
    }
    catch_unwind(AssertUnwindSafe(|| {
        let session = &mut *session;
        let update = match session.connection.updates.try_recv() {
            Ok(update) => update,
            Err(std::sync::mpsc::TryRecvError::Empty) => return 0,
            Err(_) => return -1,
        };
        session.payload.clear();
        session.states.clear();
        let mut out = TcpeJavaEvent {
            kind: 0,
            x: 0,
            z: 0,
            min_y: 0,
            height: 0,
            payload: std::ptr::null(),
            payload_len: 0,
            states: std::ptr::null(),
            states_len: 0,
            sky_light:std::ptr::null(),sky_light_len:0,block_light:std::ptr::null(),block_light_len:0,light_sections:std::ptr::null(),light_sections_len:0,
        };
        session.sky_light.clear();session.block_light.clear();session.light_sections.clear();
        let (kind, value) = match update {
            Update::Login => (1, json!({})),
            Update::Spawn => (2, json!({})),
            Update::Position(position) => (3, json!({"position":position})),
            Update::RemovedEntities(ids)=>(23,json!({"removed_entities":ids})),
            Update::LookCorrection{yaw,pitch,relative_yaw,relative_pitch}=>(22,json!({"look":{"yaw":yaw,"pitch":pitch,"relative_yaw":relative_yaw,"relative_pitch":relative_pitch}})),
            Update::Chunk(chunk) => {
                out.x = chunk.x;
                out.z = chunk.z;
                out.min_y = chunk.min_y;
                out.height = chunk.height as u32;
                session.states = chunk.states;
                (4, json!({}))
            }
            Update::Light(light)=>{out.x=light.x;out.z=light.z;out.min_y=light.min_y;out.height=light.height as u32;session.sky_light=light.sky_light;session.block_light=light.block_light;session.light_sections=light.light_sections;(20,json!({}))},
            Update::UnloadChunk(x, z) => {
                out.x = x;
                out.z = z;
                (5, json!({}))
            }
            Update::Entities(entities) => (6, json!({"entities":entities})),
            Update::Skin(image) => (21, json!({"skin":image})),
            Update::Recipes(snapshot)=>(32,json!({"recipes":snapshot})),
            Update::VisualEffect(effect)=>(33,json!({"visual_effect":effect})),
            Update::Targeting(snapshot)=>(36,json!({"targeting":snapshot})),
            Update::Sound(sound)=>(35,json!({"sound":sound})),
            Update::Inventory(inventory) => (7, json!({"inventory":inventory})),
            Update::Scoreboard(update) => (27, json!({"scoreboard_update":update})),
            Update::ResourcePacks(snapshot) => (28,json!({"resource_packs":snapshot})),
            Update::PackFonts(fonts)=>(29,json!({"pack_fonts":fonts})),
            Update::PackFontError{connection,generation,id,message}=>(30,json!({"pack_font_error":{"connection":connection,"generation":generation,"id":id,"message":message}})),
            Update::Compass(snapshot)=>(31,json!({"compass":snapshot})),
            Update::Boss(update) => (26, json!({"boss_update":update})),
            Update::Title(update) => (25, json!({"title_update":update})),
            Update::ActionBar(text) => (24, json!({"text":text})),
            Update::Chat(text) => (8, json!({"text":text})),
            Update::SignedChat{text,signature} => (8,json!({"text":text,"signature":signature})),
            Update::DeleteChat(signature)=>(34,json!({"signature":signature})),
            Update::ChatActions(actions) => (9, json!({"actions":actions})),
            Update::ConfigurationPhase(phase) => (10, json!({"phase":phase})),
            Update::ServerDialog(dialog) => (11, dialog),
            Update::Disconnected(reason) => (12, json!({"reason":reason})),
            Update::Error(message) => (13, json!({"message":message})),
            Update::PlayerId(id) => (14, json!({"id":id})),
            Update::Time { day, ticking } => (15, json!({"day":day,"ticking":ticking})),
            Update::Vitals(vitals) => (16, json!({"vitals":vitals})),
            Update::Hurt { id, yaw } => (17, json!({"id":id,"yaw":yaw})),
            Update::Death(message) => (18, json!({"message":message})),
        };
        session.payload = serde_json::to_vec(&value).unwrap_or_default();
        session.value = value;
        out.kind = kind;
        out.payload = session.payload.as_ptr();
        out.payload_len = session.payload.len();
        out.states = session.states.as_ptr();
        out.states_len = session.states.len();
        out.sky_light=session.sky_light.as_ptr();out.sky_light_len=session.sky_light.len();out.block_light=session.block_light.as_ptr();out.block_light_len=session.block_light.len();out.light_sections=session.light_sections.as_ptr();out.light_sections_len=session.light_sections.len();
        *event = out;
        1
    }))
    .unwrap_or(-1)
}
#[cfg(test)]
mod tests {
    #[test] fn use_hand_command_validates_explicit_selection(){
        use super::*;
        assert!(matches!(command(serde_json::json!({"type":"use"})),Ok(Command::UseItem(None))));
        assert!(matches!(command(serde_json::json!({"type":"use","hand":"off"})),Ok(Command::UseItem(Some(true)))));
        assert!(matches!(command(serde_json::json!({"type":"use","hand":"main"})),Ok(Command::UseItem(Some(false)))));
        assert!(matches!(command(serde_json::json!({"type":"interact","entity_id":42,"hand":"off"})),Ok(Command::Interact{id:42,hand:Some(true)})));
        assert!(command(serde_json::json!({"type":"interact","entity_id":42,"hand":1})).is_err());
        for hand in [serde_json::json!("left"),serde_json::json!(1),serde_json::Value::Null]{assert!(command(serde_json::json!({"type":"use","hand":hand})).is_err());}
    }
    use super::*;
    #[test]
    fn rejects_invalid_commands_without_lossy_integer_casts() {
        for input in [
            json!({"type":"select_hotbar","slot":256}),
            json!({"type":"attack","entity_id":4294967296u64}),
            json!({"type":"interact","entity_id":4294967296u64}),
            json!({"type":"click_slot","window_id":0,"slot":-1,"right":false}),
            json!({"type":"quick_move","window_id":0,"slot":65536,"right":false}),
            json!({"type":"quick_move","window_id":0,"slot":-1,"right":false}),
            json!({"type":"walk","forward":2,"strafe":0,"jump":false,"sneak":false}),
            json!({"type":"look","yaw":0,"pitch":91}),
        ] {
            assert!(command(input).is_err());
        }
    }
    #[test]
    fn native_controls_accept_valid_actions() {
        for input in [
            json!({"type":"chat","text":"/help"}),
            json!({"type":"attack","entity_id":123}),
            json!({"type":"interact","entity_id":123}),
            json!({"type":"look","yaw":270,"pitch":-45}),
            json!({"type":"walk","forward":1,"strafe":-1,"jump":true,"sneak":false}),
            json!({"type":"click_slot","window_id":3,"slot":15,"right":false}),
            json!({"type":"quick_move","window_id":3,"slot":15,"right":false}),
        ] {
            assert!(command(input).is_ok());
        }
    }
    #[test]
    fn cursor_drop_command_preserves_window_and_click_kind(){
        assert!(matches!(command(json!({"type":"drop_carried","window_id":7,"right":true})).unwrap(),Command::DropCarried{window_id:7,right:true,generation:None}));
        assert!(command(json!({"type":"drop_carried","window_id":2147483648i64,"right":true})).is_err());
        assert!(command(json!({"type":"drop_carried","window_id":7,"right":"true"})).is_err());
    }
    #[test]fn inventory_generation_is_optional_strict_and_preserved(){
        for kind in["click_slot","quick_move","drop_carried","close_container"]{let mut input=json!({"type":kind,"window_id":7,"slot":0,"right":false});assert!(command(input.clone()).is_ok());for invalid in[json!(-1),json!(1.5),json!("1"),json!(null)]{input["menu_generation"]=invalid;assert!(command(input.clone()).is_err());}input["menu_generation"]=json!(u64::MAX);let generation=match command(input).unwrap(){Command::ClickSlot{generation,..}|Command::QuickMove{generation,..}|Command::DropCarried{generation,..}|Command::CloseContainer{generation,..}=>generation,_=>panic!("wrong command")};assert_eq!(generation,Some(u64::MAX));}
    }
    #[test]
    fn flight_command_rejects_invalid_vertical_and_requires_permission_state_request(){
      assert!(matches!(command(json!({"type":"flight","flying":true,"vertical":-1})).unwrap(),Command::Flight{flying:true,vertical:-1.,..}));
      assert!(command(json!({"type":"flight","flying":true,"vertical":2})).is_err());
      assert!(command(json!({"type":"flight","flying":1,"vertical":0})).is_err());
    }
    #[test]
    fn native_sprint_flag_is_optional_and_strict() {
        for (value,expected) in [(json!({"type":"walk","forward":1,"strafe":0,"jump":false,"sneak":false}),false),(json!({"type":"walk","forward":1,"strafe":0,"jump":false,"sneak":false,"sprint":true}),true)] {
            let Command::Walk{sprint,..}=command(value).unwrap() else{panic!("wrong command")};assert_eq!(sprint,expected);
        }
        assert!(command(json!({"type":"walk","forward":1,"strafe":0,"jump":false,"sneak":false,"sprint":1})).is_err());
    }
    #[test]
    fn native_left_right_match_camera_relative_java_physics(){
        use azalea_core::position::Vec3;
        use azalea_entity::{LookDirection,input_vector};
        for yaw in [0.,90.,180.,270.]{
            let direction=LookDirection::new(yaw,0.);
            let left=azalea_client::movement::AnalogMovement{forward:0.,left:1.}.vector();
            let right=azalea_client::movement::AnalogMovement{forward:0.,left:-1.}.vector();
            let a=input_vector(direction,1.,Vec3::new(left.x as f64,0.,left.y as f64));
            let b=input_vector(direction,1.,Vec3::new(right.x as f64,0.,right.y as f64));
            assert!((a.x+b.x).abs()<1e-6&&(a.z+b.z).abs()<1e-6);
            let forward=input_vector(direction,1.,Vec3::new(0.,0.,1.));
            assert!((a.x*forward.x+a.z*forward.z).abs()<1e-6);
            assert!(forward.z*a.x-forward.x*a.z>0.99);
        }
    }
    #[test]
    fn touch_impulses_survive_command_and_physics_boundary() {
        let cmd=command(json!({"type":"walk","forward":0.25,"strafe":-0.5,"jump":false,"sneak":false})).unwrap();
        let Command::Walk{forward,strafe,..}=cmd else {panic!("wrong command")};
        let v=azalea_client::movement::AnalogMovement{forward,left:strafe}.vector();
        assert_eq!(v.y,0.25); assert_eq!(v.x,-0.5);
        let diagonal=azalea_client::movement::AnalogMovement{forward:1.,left:1.}.vector();
        assert!((diagonal.x*diagonal.x+diagonal.y*diagonal.y-1.).abs()<1e-5);
        let zero=azalea_client::movement::AnalogMovement{forward:0.,left:0.}.vector();
        assert_eq!(zero.x,0.); assert_eq!(zero.y,0.);
    }
    #[test]
    fn null_abi_inputs_fail_cleanly() {
        unsafe {
            assert!(tcpe_java_connect(std::ptr::null(), 0, std::ptr::null(), 0, false).is_null());
            assert!(!tcpe_java_command(
                std::ptr::null_mut(),
                std::ptr::null(),
                0
            ));
            assert_eq!(
                tcpe_java_poll(std::ptr::null_mut(), std::ptr::null_mut()),
                -1
            );
            tcpe_java_close(std::ptr::null_mut());
        }
    }
}

fn field<'a>(root: &'a Value, path: &str) -> Option<&'a Value> {
    let mut value = root;
    for part in path.split('.') {
        value = if value.is_array() {
            value.get(part.parse::<usize>().ok()?)?
        } else {
            value.get(part)?
        };
    }
    Some(value)
}
#[repr(C)]
pub struct TcpeJavaText {
    pub data: *const u8,
    pub len: usize,
}
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_number(
    session: *const Session,
    path: *const c_char,
    len: usize,
    result: *mut f64,
) -> bool {
    if session.is_null() || result.is_null() {
        return false;
    }
    let Some(path) = text(path, len, 4096) else {
        return false;
    };
    let Some(value) = field(&(*session).value, path)
        .and_then(Value::as_f64)
        .filter(|v| v.is_finite())
    else {
        return false;
    };
    *result = value;
    true
}
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_boolean(
    session: *const Session,
    path: *const c_char,
    len: usize,
    result: *mut bool,
) -> bool {
    if session.is_null() || result.is_null() {
        return false;
    }
    let Some(path) = text(path, len, 4096) else {
        return false;
    };
    let Some(value) = field(&(*session).value, path).and_then(Value::as_bool) else {
        return false;
    };
    *result = value;
    true
}
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_text(
    session: *const Session,
    path: *const c_char,
    len: usize,
    result: *mut TcpeJavaText,
) -> bool {
    if session.is_null() || result.is_null() {
        return false;
    }
    let Some(path) = text(path, len, 4096) else {
        return false;
    };
    let Some(value) = field(&(*session).value, path).and_then(Value::as_str) else {
        return false;
    };
    *result = TcpeJavaText {
        data: value.as_ptr(),
        len: value.len(),
    };
    true
}
#[no_mangle]
pub unsafe extern "C" fn tcpe_java_array_size(
    session: *const Session,
    path: *const c_char,
    len: usize,
    result: *mut usize,
) -> bool {
    if session.is_null() || result.is_null() {
        return false;
    }
    let Some(path) = text(path, len, 4096) else {
        return false;
    };
    let Some(value) = field(&(*session).value, path).and_then(Value::as_array) else {
        return false;
    };
    *result = value.len();
    true
}

#[cfg(test)]
mod field_tests {
    use super::*;
    #[test]
    fn removed_entity_ids_cross_the_event_boundary_without_unsigned_conversion(){
        let (tx,updates)=std::sync::mpsc::channel();let (commands,_rx)=tokio::sync::mpsc::unbounded_channel();
        let mut session=Session{online:false,connection:Connection{commands,updates},payload:vec![],states:vec![],sky_light:vec![],block_light:vec![],light_sections:vec![],value:json!({})};
        tx.send(Update::RemovedEntities(vec![0,-1,i32::MAX])).unwrap();
        let mut event: TcpeJavaEvent=unsafe{std::mem::zeroed()};
        assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},1);assert_eq!(event.kind,23);
        assert_eq!(session.value["removed_entities"],json!([0,-1,i32::MAX]));assert!(event.payload_len>0);
    }
    #[test]
    fn rich_action_bar_and_empty_clear_cross_real_poll_boundary(){
        let (tx,updates)=std::sync::mpsc::channel();let (commands,_rx)=tokio::sync::mpsc::unbounded_channel();
        let mut session=Session{online:false,connection:Connection{commands,updates},payload:vec![],states:vec![],sky_light:vec![],block_light:vec![],light_sections:vec![],value:json!({})};
        let text="§x§1§2§a§b§e§fReady ✦ 雪";tx.send(Update::ActionBar(text.into())).unwrap();tx.send(Update::ActionBar(String::new())).unwrap();
        let mut event:TcpeJavaEvent=unsafe{std::mem::zeroed()};
        for expected in [text,""]{assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},1);assert_eq!(event.kind,24);let value:Value=serde_json::from_slice(unsafe{std::slice::from_raw_parts(event.payload,event.payload_len)}).unwrap();assert_eq!(value["text"],expected);}
        assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},0);
    }
    #[test]
    fn title_operations_cross_poll_in_order_with_signed_timing(){
        use network::native_java::TitleUpdate;
        let (tx,updates)=std::sync::mpsc::channel();let (commands,_rx)=tokio::sync::mpsc::unbounded_channel();
        let mut session=Session{online:false,connection:Connection{commands,updates},payload:vec![],states:vec![],sky_light:vec![],block_light:vec![],light_sections:vec![],value:json!({})};
        for update in [TitleUpdate::Subtitle{text:"雪 ✦".into()},TitleUpdate::Times{fade_in:-1,stay:70,fade_out:-1},TitleUpdate::Title{text:"§bFight!".into()},TitleUpdate::Clear{reset_times:true}]{tx.send(Update::Title(update)).unwrap();}
        let mut event:TcpeJavaEvent=unsafe{std::mem::zeroed()};
        for operation in ["subtitle","times","title","clear"]{assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},1);assert_eq!(event.kind,25);assert_eq!(session.value["title_update"]["operation"],operation);match operation{"subtitle"=>assert_eq!(session.value["title_update"]["text"],"雪 ✦"),"times"=>assert_eq!(session.value["title_update"]["fade_in"],-1),"clear"=>assert_eq!(session.value["title_update"]["reset_times"],true),_=>{}}}
    }
    #[test]
    fn boss_event_crosses_native_poll_boundary(){
        use network::native_java::BossUpdate;
        let (tx,updates)=std::sync::mpsc::channel();let (commands,_rx)=tokio::sync::mpsc::unbounded_channel();
        let mut session=Session{online:false,connection:Connection{commands,updates},payload:vec![],states:vec![],sky_light:vec![],block_light:vec![],light_sections:vec![],value:json!({})};
        tx.send(Update::Boss(BossUpdate::Add{id:"00000000-0000-0000-0000-000000000011".into(),text:"§b雪 ✦".into(),progress:0.5,color:5,overlay:3,darken_screen:true,play_music:false,create_world_fog:true})).unwrap();
        tx.send(Update::Boss(BossUpdate::Remove{id:"00000000-0000-0000-0000-000000000011".into()})).unwrap();
        let mut event:TcpeJavaEvent=unsafe{std::mem::zeroed()};
        assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},1);assert_eq!(event.kind,26);let value:Value=serde_json::from_slice(unsafe{std::slice::from_raw_parts(event.payload,event.payload_len)}).unwrap();assert_eq!(value["boss_update"]["text"],"§b雪 ✦");assert_eq!(value["boss_update"]["progress"],0.5);assert_eq!(value["boss_update"]["color"],5);assert_eq!(value["boss_update"]["overlay"],3);assert_eq!(value["boss_update"]["darken_screen"],true);
        assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},1);assert_eq!(event.kind,26);assert_eq!(session.value["boss_update"]["operation"],"remove");assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},0);
    }
    #[test]
    fn scoreboard_events_preserve_null_reset_and_signed_values_at_poll(){
        let (tx,updates)=std::sync::mpsc::channel();let (commands,_rx)=tokio::sync::mpsc::unbounded_channel();
        let mut session=Session{online:false,connection:Connection{commands,updates},payload:vec![],states:vec![],sky_light:vec![],block_light:vec![],light_sections:vec![],value:json!({})};
        for update in [json!({"operation":"score","objective":"main","owner":"雪","score":i32::MIN,"display":"§a✦"}),json!({"operation":"reset","objective":null,"owner":"雪"})]{tx.send(Update::Scoreboard(update)).unwrap();}
        let mut event:TcpeJavaEvent=unsafe{std::mem::zeroed()};assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},1);assert_eq!(event.kind,27);assert_eq!(session.value["scoreboard_update"]["score"],i32::MIN);assert_eq!(session.value["scoreboard_update"]["display"],"§a✦");assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},1);assert_eq!(event.kind,27);assert!(session.value["scoreboard_update"]["objective"].is_null());assert_eq!(unsafe{tcpe_java_poll(&mut session,&mut event)},0);
    }
    #[test]
    fn typed_event_fields_keep_utf8_arrays_and_types() {
        let (_tx, updates) = std::sync::mpsc::channel();
        let (commands, _rx) = tokio::sync::mpsc::unbounded_channel();
        let session = Session {
            online: false,
            connection: Connection { commands, updates },
            payload: vec![],
            states: vec![],sky_light:vec![],block_light:vec![],light_sections:vec![],
            value: json!({"position":[-16.5,319.25,0],"name":"TCPE \"native\"\n雪","alive":true}),
        };
        unsafe {
            let mut number = 0.;
            let path = "position.1";
            assert!(tcpe_java_number(
                &session,
                path.as_ptr().cast(),
                path.len(),
                &mut number
            ));
            assert_eq!(number, 319.25);
            let mut size = 0;
            let path = "position";
            assert!(tcpe_java_array_size(
                &session,
                path.as_ptr().cast(),
                path.len(),
                &mut size
            ));
            assert_eq!(size, 3);
            let mut alive = false;
            let path = "alive";
            assert!(tcpe_java_boolean(
                &session,
                path.as_ptr().cast(),
                path.len(),
                &mut alive
            ));
            assert!(alive);
            let mut value = TcpeJavaText {
                data: std::ptr::null(),
                len: 0,
            };
            let path = "name";
            assert!(tcpe_java_text(
                &session,
                path.as_ptr().cast(),
                path.len(),
                &mut value
            ));
            assert_eq!(
                std::str::from_utf8(std::slice::from_raw_parts(value.data, value.len)).unwrap(),
                "TCPE \"native\"\n雪"
            );
            for path in ["position.-1", "position.99", "name", "missing"] {
                assert!(!tcpe_java_number(
                    &session,
                    path.as_ptr().cast(),
                    path.len(),
                    &mut number
                ));
            }
        }
    }
}

#[cfg(test)] mod entity_click_coordinates_tests {
 #[test] fn world_hit_becomes_entity_relative_and_only_players_fall_back(){
  use azalea_core::position::Vec3;use azalea_client::interact::{relative_entity_interaction,player_item_fallback};use azalea_registry::builtin::EntityKind;
  let hit=relative_entity_interaction(Vec3::new(1000.25,65.25,1999.75),Vec3::new(1000.,64.,2000.));assert_eq!(hit,Vec3::new(0.25,1.25,-0.25));
  assert_eq!(relative_entity_interaction(Vec3::new(1.,2.,3.),Vec3::new(1.,2.,3.)),Vec3::ZERO);
  assert!(player_item_fallback(Some(EntityKind::Player),true));assert!(!player_item_fallback(Some(EntityKind::Player),false));assert!(!player_item_fallback(Some(EntityKind::ArmorStand),true));assert!(!player_item_fallback(Some(EntityKind::Cow),true));assert!(!player_item_fallback(None,true));
 }
}

#[cfg(test)]mod held_action_command_tests {
 use super::*;use serde_json::json;
 #[test]fn selected_drop_and_swap_validate_commands(){
  assert!(matches!(command(json!({"type":"drop_selected","all":false})),Ok(Command::DropSelected(false))));
  assert!(matches!(command(json!({"type":"drop_selected","all":true})),Ok(Command::DropSelected(true))));
  for value in[json!(1),json!("true"),json!(null)]{assert!(command(json!({"type":"drop_selected","all":value})).is_err());}
  assert!(command(json!({"type":"drop_selected"})).is_err());assert!(matches!(command(json!({"type":"swap_offhand"})),Ok(Command::SwapOffhand)));
 }
}
#[cfg(test)]mod recipe_command_tests{
 use super::*;
 #[test]fn recipe_command_requires_exact_numeric_id_boolean_and_current_generation(){
  let input=json!({"type":"place_recipe","window_id":0,"recipe_id":300,"all":true,"menu_generation":12});
  assert!(matches!(command(input.clone()).unwrap(),Command::PlaceRecipe{window_id:0,recipe_id:300,all:true,generation:12}));
  for field in["window_id","recipe_id","all","menu_generation"]{let mut v=input.clone();v.as_object_mut().unwrap().remove(field);assert!(command(v).is_err());}
  for invalid in[json!(-1),json!(1.2),json!("300"),json!(null)]{let mut v=input.clone();v["recipe_id"]=invalid;assert!(command(v).is_err());}
  for invalid in[json!(-1),json!(1.2),json!("12"),json!(null)]{let mut v=input.clone();v["menu_generation"]=invalid;assert!(command(v).is_err());}
 }
}
