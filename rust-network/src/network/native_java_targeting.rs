//! Bounded, exact outline geometry. Overflow suppresses the complete outline;
//! it never substitutes a full block or a bounding box.
use azalea_core::{direction::Axis, position::Vec3i};
use azalea_physics::collision::VoxelShape;
use serde::Serialize;

const MAX_BOXES: usize = 128;
const MAX_EDGES: usize = 2048;
const MAX_CELLS: usize = 65536;
#[derive(Debug, Default, Serialize)]
pub(super) struct Outline {
    pub complete: bool,
    pub boxes: Vec<[f64; 6]>,
    pub edges: Vec<[f64; 6]>,
}
pub(super) fn outline(shape: &VoxelShape) -> Outline {
    let axes = [Axis::X, Axis::Y, Axis::Z];
    let sizes = axes.map(|a| shape.shape().size(a) as usize);
    let coords = axes.map(|a| shape.get_coords(a));
    if sizes.iter().try_fold(1usize, |n, s| n.checked_mul(s + 1)).is_none_or(|n| n > MAX_CELLS)
        || coords.iter().zip(sizes).any(|(c, s)| c.len() != s + 1 || c.iter().any(|v| !v.is_finite() || v.abs() > 1024.)) {
        return Outline::default();
    }
    let mut result = Outline { complete: true, ..Default::default() };
    let mut overflow = false;
    shape.for_all_boxes(|x0,y0,z0,x1,y1,z1| {
        if result.boxes.len() < MAX_BOXES { result.boxes.push([x0,y0,z0,x1,y1,z1]); }
        else { overflow = true; }
    });
    if overflow { return Outline::default(); }
    // Official DiscreteVoxelShape.forAllAxisEdges: one/three occupied cells,
    // or two diagonally opposite cells, form an edge. Merge contiguous runs.
    // Enumerating the union's boundary avoids internal edges of merged AABBs.
    for along in 0..3 {
        let u = (along + 1) % 3;
        let v = (along + 2) % 3;
        for a in 0..=sizes[u] { for b in 0..=sizes[v] {
            let mut start = None;
            for k in 0..=sizes[along] {
                let mut count = 0;
                let mut parity = 0;
                if k < sizes[along] {
                    for da in 0..2 { for db in 0..2 {
                        let mut p = [0i32; 3];
                        p[along] = k as i32;
                        p[u] = a as i32 + da - 1;
                        p[v] = b as i32 + db - 1;
                        if shape.shape().is_full_wide(Vec3i::new(p[0],p[1],p[2])) {
                            count += 1; parity ^= da ^ db;
                        }
                    }}
                }
                let edge = count == 1 || count == 3 || (count == 2 && parity & 1 == 0);
                if edge && start.is_none() { start = Some(k); }
                if !edge { if let Some(begin) = start.take() {
                    if result.edges.len() == MAX_EDGES { return Outline::default(); }
                    let mut lo = [0.; 3];
                    lo[along] = coords[along][begin]; lo[u] = coords[u][a]; lo[v] = coords[v][b];
                    let mut hi = lo; hi[along] = coords[along][k];
                    result.edges.push([lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]]);
                }}
            }
        }}
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use azalea_physics::collision::{Shapes, box_shape};
    #[test]
    fn cube_has_twelve_exact_edges() {
        let o = outline(&box_shape(0.,0.,0.,1.,1.,1.));
        assert!(o.complete); assert_eq!(o.boxes.len(),1); assert_eq!(o.edges.len(),12);
        assert!(o.edges.iter().all(|e| e.iter().all(|v| *v == 0. || *v == 1.)));
    }
    #[test]
    fn adjacent_boxes_have_no_internal_seam() {
        let shape = Shapes::or(box_shape(0.,0.,0.,0.5,1.,1.),box_shape(0.5,0.,0.,1.,1.,1.));
        let o = outline(&shape);
        assert!(o.complete); assert_eq!(o.edges.len(),12);
        assert!(o.edges.iter().all(|e| !e.iter().any(|v| *v == 0.5)));
    }
    #[test]
    fn thin_shape_retains_actual_height() {
        let o = outline(&box_shape(0.,0.,0.,1.,0.0625,1.));
        assert!(o.complete); assert_eq!(o.edges.len(),12);
        assert!(o.boxes.iter().all(|b| b[4] == 0.0625));
    }
}

/// Shape identity is the actual static outline returned by the current shape
/// resolver, not a guessed state-to-cube mapping. The cache is bounded and scoped
/// to one connection; future contextual resolvers must include context identity.
#[derive(Default)]
pub(super) struct OutlineCache(std::collections::BTreeMap<usize, std::sync::Arc<Outline>>);
impl OutlineCache {
    pub fn get(&mut self, shape: &'static VoxelShape) -> std::sync::Arc<Outline> {
        let key = shape as *const VoxelShape as usize;
        if let Some(value) = self.0.get(&key) { return value.clone(); }
        let value = std::sync::Arc::new(outline(shape));
        if self.0.len() == 256 { self.0.clear(); }
        self.0.insert(key, value.clone()); value
    }
}

pub(super) fn refresh(world: &mut bevy_ecs::world::World) {
    use bevy_ecs::system::RunSystemOnce;
    let _ = world.run_system_once(azalea_client::interact::pick::update_hit_result_component);
}
/// Refresh the production pick only when its held-item context changed between
/// ticks. The emitted revision then belongs to a real pick with the new shape.
pub(super) fn fresh_snapshot(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,epoch:u64,cache:&mut OutlineCache)->Option<serde_json::Value>{
 let context=world.get::<azalea_entity::inventory::Inventory>(entity).map(|inv|azalea_physics::collision::OutlineContext::from_main(inv.held_item())).unwrap_or_default();
 if world.get::<azalea_client::interact::pick::NativePickStamp>(entity).is_none_or(|stamp|stamp.outline_context!=context){refresh(world);}
 snapshot(world,entity,epoch,cache)
}
pub(super) fn snapshot(world: &bevy_ecs::world::World, entity: bevy_ecs::entity::Entity,
    epoch: u64, cache: &mut OutlineCache) -> Option<serde_json::Value> {
    use azalea_client::{interact::pick::{HitResultComponent,NativePickStamp},local_player::InstanceHolder};
    use azalea_core::hit_result::HitResult;
    let stamp = world.get::<NativePickStamp>(entity)?;
    let hit = world.get::<HitResultComponent>(entity)?;
    let vec = |p: azalea_core::position::Vec3| [p.x,p.y,p.z];
    let mut result = serde_json::json!({"epoch":epoch,"revision":stamp.revision,
        "input_revision":stamp.input_revision,"eye":vec(stamp.eye),"kind":"miss"});
    let current_context=world.get::<azalea_entity::inventory::Inventory>(entity).map(|inv|azalea_physics::collision::OutlineContext::from_main(inv.held_item())).unwrap_or_default();
    if current_context!=stamp.outline_context{return Some(result);}
    if let Some(attack)=&stamp.attack_hit {
        if let Some(id)=world.get::<azalea_world::MinecraftEntityId>(attack.entity){
            result["attack_entity"]=id.0.into();result["attack_location"]=serde_json::json!(vec(attack.location));
        }
    }
    match &**hit {
        HitResult::Block(block) if !block.miss => {
            let holder = world.get::<InstanceHolder>(entity)?;
            let instance = holder.instance.read();
            // An unloaded/replaced target is not reused as a guessed cube.
            let state = instance.chunks.get_block_state(block.block_pos)?;
            if stamp.block_state != Some(state.id()) { return None; }
            let shape = azalea_physics::collision::checked_outline_shape_with_context(state,stamp.outline_context)?;
            result["kind"] = "block".into();
            result["block"] = serde_json::json!([block.block_pos.x,block.block_pos.y,block.block_pos.z]);
            result["state"] = state.id().into();
            result["face"] = (block.direction as u8).into();
            result["location"] = serde_json::json!(vec(block.location));
            result["inside"] = block.inside.into();
            result["outline"] = serde_json::to_value(&*cache.get(shape)).ok()?;
        },
        HitResult::Entity(hit) => {
            let id = world.get::<azalea_world::MinecraftEntityId>(hit.entity)?;
            result["kind"] = "entity".into(); result["entity"] = id.0.into();
            result["location"] = serde_json::json!(vec(hit.location));
        }, _ => {}
    }
    Some(result)
}

pub(super) fn ray(enabled: bool, offset: [f64;3], direction: [f64;3], revision: u64)
    -> Option<azalea_client::interact::pick::NativePickRay> {
    if revision == 0 || revision > 9007199254740991 || offset.iter().chain(direction.iter()).any(|v| !v.is_finite()) { return None; }
    let offset = azalea_core::position::Vec3::new(offset[0],offset[1],offset[2]);
    let d = azalea_core::position::Vec3::new(direction[0],direction[1],direction[2]);
    if offset.length() > 16. || !(0.999..=1.001).contains(&d.length()) { return None; }
    let yaw = (-d.x).atan2(d.z).to_degrees() as f32;
    let pitch = (-d.y).atan2((d.x*d.x+d.z*d.z).sqrt()).to_degrees() as f32;
    Some(azalea_client::interact::pick::NativePickRay {enabled, offset,
        look:azalea_entity::LookDirection::new(yaw,pitch),revision})
}

#[cfg(test)]
mod actual_pick_tests {
 use super::*;
 use azalea_client::{test_utils::prelude::*,interact::pick::{HitResultComponent,NativePickRay}};
 use azalea_protocol::packets::{ConnectionProtocol,game::ClientboundBlockUpdate};
 use azalea_core::position::{BlockPos,ChunkPos,Vec3};
 use azalea_entity::{Position,Physics};
 pub(super) fn setup(state:u16)->Simulation{
  let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
  sim.receive_packet(make_basic_empty_chunk(ChunkPos{x:-1,z:-1},24));sim.update();
  sim.receive_packet(ClientboundBlockUpdate{pos:BlockPos::new(-2,64,-2),block_state:azalea_block::BlockState::try_from(state).unwrap()});sim.update();
  sim.with_component_mut::<Position>(|p|**p=Vec3::new(-1.5,64.,-4.));
  let dim=sim.component::<azalea_entity::dimensions::EntityDimensions>();
  sim.with_component_mut::<Physics>(|p|p.bounding_box=azalea_core::aabb::Aabb{min:Vec3::new(-1.8,64.,-4.3),max:Vec3::new(-1.2,64.+f64::from(dim.height),-3.7)});
  sim
 }
 #[test]
 fn official_leaf_litter_outline_is_thin_and_not_fallback_cube(){
  let state=azalea_block::BlockState::try_from(27648u16).unwrap();
  let o=outline(azalea_physics::collision::checked_outline_shape(state).unwrap());
  assert!(o.complete);assert!(!o.boxes.is_empty());assert!(o.boxes.iter().all(|b|b[4]<1.));
 }
 #[test]
 fn actual_negative_cube_and_offcentre_ray_use_same_pick_component(){
  let mut sim=setup(1);
  // Native camera looks downward toward the selected cube, independent of the
  // centre LookDirection. Refresh invokes the actual production pick system.
  let d=Vec3::new(0.,-1.12,2.5);let n=d.length();
  let r=ray(true,[0.,0.,0.],[d.x/n,d.y/n,d.z/n],7).unwrap();
  sim.app.world_mut().entity_mut(sim.entity).insert(r);refresh(sim.app.world_mut());
  let hit=sim.component::<HitResultComponent>();let block=hit.as_block_hit_result_if_not_miss().expect("negative cube hit");
  assert_eq!(block.block_pos,BlockPos::new(-2,64,-2));
  let mut cache=OutlineCache::default();let payload=snapshot(sim.app.world(),sim.entity,3,&mut cache).unwrap();
  assert_eq!(payload["input_revision"],7);assert_eq!(payload["state"],1);assert_eq!(payload["outline"]["edges"].as_array().unwrap().len(),12);
  // A disabled touch ray cannot silently fall back to the centre camera.
  sim.app.world_mut().entity_mut(sim.entity).insert(NativePickRay{enabled:false,..r});refresh(sim.app.world_mut());
  assert!(sim.component::<HitResultComponent>().miss());
 }
 #[test]
 fn actual_entity_pick_and_camera_offset_cannot_extend_eye_reach(){
  let mut sim=setup(0);
  sim.receive_packet(make_basic_add_entity(azalea_registry::builtin::EntityKind::Cow,77,Vec3::new(-1.5,64.,-1.5)));sim.update();
  let d=Vec3::new(0.,-0.92,2.5);let n=d.length();sim.app.world_mut().entity_mut(sim.entity).insert(ray(true,[0.;3],[0.,d.y/n,d.z/n],8).unwrap());refresh(sim.app.world_mut());
  let hit=sim.component::<HitResultComponent>();let entity=hit.as_entity_hit_result().expect("actual living entity hit");assert_eq!(sim.app.world().get::<azalea_world::MinecraftEntityId>(entity.entity).unwrap().0,77);
  let mut sim=setup(1);sim.with_component_mut::<Position>(|p|p.z=-10.);
  let d=Vec3::new(0.,-1.12,2.5);let n=d.length();sim.app.world_mut().entity_mut(sim.entity).insert(ray(true,[0.,0.,6.],[0.,d.y/n,d.z/n],9).unwrap());refresh(sim.app.world_mut());
  assert!(sim.component::<HitResultComponent>().miss(),"displaced camera never grants extra player reach");
 }
 #[test]
 fn all_cube_faces_at_negative_coordinates_are_exact(){
  let sim=setup(1);let holder=sim.component::<azalea_client::local_player::InstanceHolder>();let world=holder.instance.read();
  for (delta,face) in [([0.,-2.,0.],0),([0.,2.,0.],1),([0.,0.,-2.],2),([0.,0.,2.],3),([-2.,0.,0.],4),([2.,0.,0.],5)] {
   let eye=Vec3::new(-1.5+delta[0],64.5+delta[1],-1.5+delta[2]);
   let r=ray(true,[0.;3],[-delta[0]/2.,-delta[1]/2.,-delta[2]/2.],1).unwrap();
   let hit=azalea_client::interact::pick::pick_block(r.look,eye,&world.chunks,4.5);
   assert!(!hit.miss);assert_eq!(hit.block_pos,BlockPos::new(-2,64,-2));assert_eq!(hit.direction as u8,face);
  }
 }
 #[test]
 fn geometry_overflow_suppresses_instead_of_approximating(){
  use azalea_physics::collision::{Shapes,box_shape};
  let mut shape=box_shape(0.,0.,0.,1.,1.,1.);
  for i in 1..130 {let x=f64::from(i)*2.;shape=Shapes::or(shape,box_shape(x,0.,0.,x+1.,1.,1.));}
  let result=outline(&shape);assert!(!result.complete);assert!(result.boxes.is_empty()&&result.edges.is_empty());
 }
 #[test]
 fn spear_attack_pick_is_extended_but_ordinary_use_pick_is_not(){
  use azalea_entity::inventory::Inventory;
  let mut sim=setup(0);sim.receive_packet(make_basic_add_entity(azalea_registry::builtin::EntityKind::Cow,78,Vec3::new(-1.5,64.,0.)));sim.update();
  sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(36).unwrap()=azalea_inventory::ItemStack::new(azalea_registry::builtin::ItemKind::IronSpear,1));
  let d=Vec3::new(0.,-0.92,4.);let n=d.length();sim.app.world_mut().entity_mut(sim.entity).insert(ray(true,[0.;3],[0.,d.y/n,d.z/n],10).unwrap());refresh(sim.app.world_mut());
  assert!(sim.component::<HitResultComponent>().as_entity_hit_result().is_none(),"ordinary interaction reach remains3");
  let stamp=sim.component::<azalea_client::interact::pick::NativePickStamp>();let attack=stamp.attack_hit.expect("spear target beyond ordinary reach");
  assert_eq!(sim.app.world().get::<azalea_world::MinecraftEntityId>(attack.entity).unwrap().0,78);
  let mut cache=OutlineCache::default();let payload=snapshot(sim.app.world(),sim.entity,3,&mut cache).unwrap();assert_eq!(payload["kind"],"miss");assert_eq!(payload["attack_entity"],78);
  // A solid block between the player and the weapon target occludes it.
  sim.receive_packet(ClientboundBlockUpdate{pos:BlockPos::new(-2,65,-2),block_state:azalea_block::BlockState::try_from(1u16).unwrap()});sim.update();refresh(sim.app.world_mut());
  assert!(sim.component::<azalea_client::interact::pick::NativePickStamp>().attack_hit.is_none());
 }
 #[test]
 fn legal_maximum_weapon_component_allows_sixty_four_plus_margin(){
  use azalea_entity::inventory::Inventory;
  use azalea_inventory::{ItemStack,components::AttackRange};
  let mut sim=setup(0);for z in 0..4{sim.receive_packet(make_basic_empty_chunk(ChunkPos{x:-1,z},24));sim.update();}
  sim.receive_packet(make_basic_add_entity(azalea_registry::builtin::EntityKind::Cow,79,Vec3::new(-1.5,64.,60.4)));sim.update();
  let stack=ItemStack::new(azalea_registry::builtin::ItemKind::IronSpear,1);
  let mut range=stack.get_component::<AttackRange>().unwrap().into_owned();range.min_reach=0.;range.max_reach=64.;range.min_creative_reach=0.;range.max_creative_reach=64.;range.hitbox_margin=1.;
  sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(36).unwrap()=stack.with_component(range));
  let d=Vec3::new(0.,-0.92,64.4);let n=d.length();sim.app.world_mut().entity_mut(sim.entity).insert(ray(true,[0.;3],[0.,d.y/n,d.z/n],11).unwrap());refresh(sim.app.world_mut());
  let stamp=sim.component::<azalea_client::interact::pick::NativePickStamp>();let attack=stamp.attack_hit.expect("official codec permits64 reach plus1margin");
  assert_eq!(sim.app.world().get::<azalea_world::MinecraftEntityId>(attack.entity).unwrap().0,79);
  assert!(sim.component::<HitResultComponent>().as_entity_hit_result().is_none());
 }
 #[test]
 fn ray_rejects_nonfinite_nonunit_and_unbounded_origins(){
  assert!(ray(true,[0.;3],[f64::NAN,0.,1.],1).is_none());
  assert!(ray(true,[17.,0.,0.],[0.,0.,1.],1).is_none());
  assert!(ray(true,[0.;3],[0.,0.,2.],1).is_none());
  assert!(ray(true,[0.;3],[0.,0.,1.],0).is_none());
 }
 #[test]
 fn overlapping_union_does_not_export_internal_box_edges(){
  use azalea_physics::collision::{Shapes,box_shape};
  let shape=Shapes::or(box_shape(0.,0.,0.,0.75,1.,1.),box_shape(0.25,0.,0.,1.,1.,1.));
  let out=outline(&shape);assert!(out.complete);assert_eq!(out.edges.len(),12);
  assert!(out.edges.iter().all(|e|!e.iter().any(|v|*v==0.25||*v==0.75)));
 }
}

#[cfg(test)]
mod packet_tests {
 use super::*;
 use azalea_client::{test_utils::prelude::*,interact::{pick::HitResultComponent,StartUseItemQueued},packet::game::SendGamePacketEvent};
 use azalea_protocol::packets::{ConnectionProtocol,game::{ClientboundBlockUpdate,ServerboundGamePacket,s_interact::InteractionHand}};
 use azalea_core::position::{BlockPos,ChunkPos,Vec3};
 use bevy_ecs::prelude::*;
 use std::sync::{Arc,Mutex};
 #[test]
 fn actual_thin_target_drives_mining_and_use_item_on_wire(){
  for mining in [false,true] {
   let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
   sim.receive_packet(make_basic_empty_chunk(ChunkPos{x:-1,z:-1},24));sim.update();
   let pos=BlockPos::new(-2,64,-2);
   sim.receive_packet(ClientboundBlockUpdate{pos,block_state:azalea_block::BlockState::try_from(27648u16).unwrap()});sim.update();
   sim.with_component_mut::<azalea_entity::Position>(|p|**p=Vec3::new(-1.5,64.,-4.));
   let direction=Vec3::new(0.,-1.59,2.5);let n=direction.length();
   sim.app.world_mut().entity_mut(sim.entity).insert(ray(true,[0.;3],[0.,direction.y/n,direction.z/n],1).unwrap());refresh(sim.app.world_mut());
   let hit=sim.component::<HitResultComponent>().as_block_hit_result_if_not_miss().cloned().expect("actual leaf litter hit");assert_eq!(hit.block_pos,pos);
   let packets=Arc::new(Mutex::new(Vec::new()));let out=packets.clone();
   sim.app.world_mut().add_observer(move |event:On<SendGamePacketEvent>|out.lock().unwrap().push(event.packet.clone()));
   if mining {sim.app.world_mut().entity_mut(sim.entity).insert(azalea_client::mining::LeftClickMine);sim.tick();sim.tick();}
   else {sim.app.world_mut().entity_mut(sim.entity).insert(StartUseItemQueued{hand:InteractionHand::MainHand,force_block:None});sim.tick();}
   let packets=packets.lock().unwrap();
   if mining {assert!(packets.iter().any(|p|matches!(p,ServerboundGamePacket::PlayerAction(p) if p.pos==pos&&p.direction==hit.direction&&p.action==azalea_protocol::packets::game::s_player_action::Action::StartDestroyBlock)),"mining must use actual selected thin block/face");}
   else {let p=packets.iter().find_map(|p|if let ServerboundGamePacket::UseItemOn(p)=p{Some(p)}else{None}).expect("use item on packet");assert_eq!(p.block_hit.block_pos,pos);assert_eq!(p.block_hit.direction,hit.direction);assert_eq!(p.block_hit.location,hit.location);}
  }
 }
}

pub(super) fn apply_ray(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,
 current_epoch:u64,epoch:u64,revision:u64,enabled:bool,offset:[f64;3],direction:[f64;3])->bool {
 if epoch!=current_epoch||epoch==0{return false;}
 let Some(ray)=ray(enabled,offset,direction,revision) else{return false;};
 if world.get::<azalea_client::interact::pick::NativePickRay>(entity).is_some_and(|old|revision<=old.revision){return false;}
 world.entity_mut(entity).insert(ray);refresh(world);update_use_aim(world,entity);true
}
#[cfg(test)]
mod lifecycle_tests {
 use super::*;
 #[test]
 fn actual_ray_mutation_rejects_old_epoch_or_revision(){
  let mut world=bevy_ecs::world::World::new();let entity=world.spawn_empty().id();
  // Invalid requests return before querying pick resources or changing state.
  assert!(!apply_ray(&mut world,entity,3,2,1,true,[0.;3],[0.,0.,1.]));
  assert!(world.get::<azalea_client::interact::pick::NativePickRay>(entity).is_none());
  world.entity_mut(entity).insert(ray(true,[0.;3],[0.,0.,1.],8).unwrap());
  assert!(!apply_ray(&mut world,entity,3,3,7,true,[0.;3],[1.,0.,0.]));
  assert!(!apply_ray(&mut world,entity,3,3,8,false,[0.;3],[1.,0.,0.]));
  assert_eq!(world.get::<azalea_client::interact::pick::NativePickRay>(entity).unwrap().revision,8);
 }
}

/// A charge belongs to its original stacks/world. Pointer-up disables picking,
/// so retain the last valid touch direction for that charge's release only.
#[derive(Clone,bevy_ecs::component::Component)]
pub(super) struct HeldAim {look:azalea_entity::LookDirection,main:azalea_inventory::ItemStack,off:azalea_inventory::ItemStack}
fn current_aim(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity)->Option<azalea_entity::LookDirection>{
 azalea_client::interact::native_aim::current(world.get::<azalea_client::interact::pick::NativePickRay>(entity),world.get::<azalea_client::interact::pick::NativePickStamp>(entity))
}
pub(super) fn capture_use_aim(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity){
 let aim=current_aim(world,entity).and_then(|look|world.get::<azalea_entity::inventory::Inventory>(entity).map(|inv|HeldAim{look,main:inv.held_item().clone(),off:inv.get_equipment(azalea_inventory::components::EquipmentSlot::Offhand).cloned().unwrap_or_default()}));
 world.entity_mut(entity).remove::<HeldAim>();if let Some(aim)=aim{world.entity_mut(entity).insert(aim);}
}
fn held_stack_matches(world:&bevy_ecs::world::World,entity:bevy_ecs::entity::Entity,aim:&HeldAim)->bool{
 use azalea_protocol::packets::game::s_interact::InteractionHand;
 let off=world.get::<azalea_client::interact::native_use::PredictedNativeUse>(entity).map(|p|p.hand==InteractionHand::OffHand).unwrap_or_else(||world.get::<azalea_entity::metadata::AbstractLivingUsingOffHand>(entity).is_some_and(|v|v.0));
 // Official LivingEntity.updatingUsingItem compares kind, then adopts the
 // current stack. Charged-projectile/durability/components may change in-place.
 world.get::<azalea_entity::inventory::Inventory>(entity).is_some_and(|inv|{
  let current=if off{inv.get_equipment(azalea_inventory::components::EquipmentSlot::Offhand)}else{Some(inv.held_item())};
  let original=if off{&aim.off}else{&aim.main};
  matches!((original,current),(azalea_inventory::ItemStack::Present(old),Some(azalea_inventory::ItemStack::Present(now))) if old.kind==now.kind)
 })
}
pub(super) fn update_use_aim(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity){
 if !super::super::native_java_use::busy(world,entity){return;}
 let Some(look)=current_aim(world,entity) else{return;};
 if !world.get::<HeldAim>(entity).is_some_and(|aim|held_stack_matches(world,entity,aim)){world.entity_mut(entity).remove::<HeldAim>();return;}
 if let Some(mut aim)=world.get_mut::<HeldAim>(entity){aim.look=look;}
}
pub(super) fn release_use(world:&mut bevy_ecs::world::World,entity:bevy_ecs::entity::Entity){
 use azalea_protocol::packets::game::{s_player_action::{Action,ServerboundPlayerAction},ServerboundMovePlayerRot};
 use azalea_client::packet::game::SendGamePacketEvent;
 let rotation=if world.get::<azalea_entity::metadata::Health>(entity).is_none_or(|health|health.0.is_finite()&&health.0>0.)&&super::super::native_java_use::busy(world,entity){world.get::<HeldAim>(entity).filter(|aim|held_stack_matches(world,entity,aim)).and_then(|aim|{
  let camera=*world.get::<azalea_entity::LookDirection>(entity)?;let flags=azalea_client::interact::native_aim::flags(world.get::<azalea_entity::Physics>(entity)?);
  azalea_client::interact::native_aim::differs(aim.look,camera).then_some((aim.look,camera,flags))
 })}else{None};
 // ReleaseUseItem carries no angles. Official server release reads its current
 // player rotation, so bracket release with rotation-only packets. Never write
 // local LookDirection, Position, PhysicsState, flags, or velocity.
 if let Some((aim,_,flags))=rotation{world.trigger(SendGamePacketEvent::new(entity,ServerboundMovePlayerRot{look_direction:aim,flags}));}
 world.trigger(SendGamePacketEvent::new(entity,ServerboundPlayerAction{action:Action::ReleaseUseItem,pos:Default::default(),direction:azalea_core::direction::Direction::Down,seq:0}));
 if let Some((_,camera,flags))=rotation{world.trigger(SendGamePacketEvent::new(entity,ServerboundMovePlayerRot{look_direction:camera,flags}));}
}

#[cfg(test)]
mod mobile_action_aim_tests {
 use super::*;
 use azalea_client::{test_utils::prelude::*,interact::{StartUseItemQueued,native_use::{NativeUseOptions,Outcome}},packet::game::SendGamePacketEvent};
 use azalea_entity::{LookDirection,Physics,inventory::Inventory};
 use azalea_inventory::ItemStack;
 use azalea_protocol::packets::{ConnectionProtocol,game::{ServerboundGamePacket,s_interact::InteractionHand,s_player_action::Action}};
 use azalea_registry::builtin::ItemKind;
 use bevy_ecs::prelude::*;
 use std::sync::{Arc,Mutex};
 fn charge(off:bool,enabled:bool,stale:bool)->(Simulation,Arc<Mutex<Vec<ServerboundGamePacket>>>) {
  let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
  let main=ItemStack::new(if off{ItemKind::IronSword}else{ItemKind::Bow},1);
  let off_stack=if off{ItemStack::new(ItemKind::Bow,1)}else{ItemStack::Empty};
  sim.with_component_mut::<Inventory>(|inv|{*inv.inventory_menu.slot_mut(36).unwrap()=main.clone();*inv.inventory_menu.slot_mut(45).unwrap()=off_stack.clone();*inv.inventory_menu.slot_mut(9).unwrap()=ItemStack::new(ItemKind::Arrow,64);});
  sim.app.world_mut().entity_mut(sim.entity).insert((LookDirection::new(0.,0.),ray(enabled,[0.;3],[-1.,0.,0.],13).unwrap()));refresh(sim.app.world_mut());
  if stale {sim.app.world_mut().get_mut::<azalea_client::interact::pick::NativePickStamp>(sim.entity).unwrap().input_revision=12;}
  capture_use_aim(sim.app.world_mut(),sim.entity);
  let packets=Arc::new(Mutex::new(Vec::new()));let out=packets.clone();sim.app.world_mut().add_observer(move |event:On<SendGamePacketEvent>|out.lock().unwrap().push(event.packet.clone()));
  sim.app.world_mut().entity_mut(sim.entity).insert((NativeUseOptions{automatic:true,main:if off{Outcome::Pass}else{Outcome::Consume{duration:72000}},off:if off{Outcome::Consume{duration:72000}}else{Outcome::Pass},main_stack:main,off_stack},StartUseItemQueued{hand:InteractionHand::MainHand,force_block:None}));{use bevy_ecs::system::RunSystemOnce;sim.app.world_mut().run_system_once(azalea_client::interact::handle_start_use_item_queued).unwrap();}
  (sim,packets)
 }
 fn rotations_and_release(packets:&[ServerboundGamePacket])->Vec<String>{packets.iter().filter_map(|p|match p{ServerboundGamePacket::UseItem(p)=>Some(format!("use:{:?}:{}",p.hand,p.y_rot)),ServerboundGamePacket::MovePlayerRot(p)=>Some(format!("rot:{}",p.look_direction.y_rot())),ServerboundGamePacket::PlayerAction(p) if p.action==Action::ReleaseUseItem=>Some("release".into()),_=>None}).collect()}
 #[test]
 fn offcentre_use_then_pointer_up_release_are_ordered_and_camera_physics_unchanged(){
  let (mut sim,packets)=charge(false,true,false);
  let initial=rotations_and_release(&packets.lock().unwrap());assert_eq!(initial,vec!["use:MainHand:90","rot:0"]);
  let direction=sim.component::<LookDirection>();let physics=sim.component::<Physics>();
  let current=*sim.app.world().get::<azalea_client::interact::pick::NativePickRay>(sim.entity).unwrap();
  sim.app.world_mut().entity_mut(sim.entity).insert(azalea_client::interact::pick::NativePickRay{enabled:false,revision:14,..current});refresh(sim.app.world_mut());update_use_aim(sim.app.world_mut(),sim.entity);
  packets.lock().unwrap().clear();release_use(sim.app.world_mut(),sim.entity);
  assert_eq!(rotations_and_release(&packets.lock().unwrap()),vec!["rot:90","release","rot:0"]);
  assert_eq!(sim.component::<LookDirection>(),direction);let after=sim.component::<Physics>();assert_eq!(after.velocity,physics.velocity);assert_eq!(after.on_ground(),physics.on_ground());assert_eq!(after.horizontal_collision,physics.horizontal_collision);
 }
 #[test]
 fn offhand_charge_tracks_latest_valid_touch_and_keeps_ordinary_camera(){
  let (mut sim,packets)=charge(true,true,false);
  assert!(rotations_and_release(&packets.lock().unwrap()).contains(&"use:OffHand:90".into()));
  assert!(apply_ray(sim.app.world_mut(),sim.entity,2,2,14,true,[0.;3],[1.,0.,0.]));
  packets.lock().unwrap().clear();release_use(sim.app.world_mut(),sim.entity);
  assert_eq!(rotations_and_release(&packets.lock().unwrap()),vec!["rot:-90","release","rot:0"]);assert_eq!(sim.component::<LookDirection>(),LookDirection::new(0.,0.));
 }
 #[test]
 fn stale_or_disabled_ray_never_changes_action_rotation(){
  for (enabled,stale) in [(true,true),(false,false)] {
   let (mut sim,packets)=charge(false,enabled,stale);
   assert_eq!(rotations_and_release(&packets.lock().unwrap()),vec!["use:MainHand:0"]);
   packets.lock().unwrap().clear();release_use(sim.app.world_mut(),sim.entity);assert_eq!(rotations_and_release(&packets.lock().unwrap()),vec!["release"]);
  }
 }
 #[test]
 fn same_item_component_update_retains_charged_touch_aim_like_official_use_lifecycle(){
  let (mut sim,packets)=charge(false,true,false);
  sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::new(ItemKind::Bow,1).with_component(azalea_inventory::components::Damage{amount:1}));
  packets.lock().unwrap().clear();release_use(sim.app.world_mut(),sim.entity);assert_eq!(rotations_and_release(&packets.lock().unwrap()),vec!["rot:90","release","rot:0"]);
 }
 #[test]
 fn item_replacement_and_real_use_cleanup_discard_charged_touch_aim(){
  for reset in 0..3 {
   let (mut sim,packets)=charge(false,true,false);
   if reset==1{super::super::clear_native_item_use(sim.app.world_mut(),sim.entity);}else if reset==2{sim.app.world_mut().entity_mut(sim.entity).insert(azalea_entity::metadata::Health(0.));}else{sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::new(ItemKind::Snowball,3));}
   packets.lock().unwrap().clear();release_use(sim.app.world_mut(),sim.entity);assert_eq!(rotations_and_release(&packets.lock().unwrap()),vec!["release"]);
  }
 }
}

#[cfg(test)]
mod contextual_outline_tests {
 use super::*;
 use azalea_core::position::{BlockPos,Vec3};
 use azalea_entity::{Position,inventory::Inventory};
 use azalea_inventory::ItemStack;
 use azalea_registry::builtin::{BlockKind,ItemKind};
 use azalea_physics::{clip::{clip_with_outline_context,ClipContext,BlockShapeType,FluidPickType},collision::{OutlineContext,checked_outline_shape_with_context,interaction_shape,Shapes,box_shape,EMPTY_SHAPE}};
 fn expected(boxes:&serde_json::Value)->VoxelShape{
  let mut out=EMPTY_SHAPE.clone();for row in boxes.as_array().unwrap(){let v:Vec<_>=row.as_array().unwrap().iter().map(|v|v.as_f64().unwrap()).collect();out=Shapes::or(out,box_shape(v[0],v[1],v[2],v[3],v[4],v[5]));}out
 }
 #[test]
 fn all_182_official_held_context_and_interaction_voxel_unions_match_exactly(){
  let reference:serde_json::Value=serde_json::from_str(include_str!("../../tests/fixtures/official_context_outline_1_21_11.json")).unwrap();let rows=reference["states"].as_array().unwrap();assert_eq!(rows.len(),182);
  for row in rows {
   let state=azalea_block::BlockState::try_from(row["id"].as_u64().unwrap() as u16).unwrap();
   let held=if row["holding_matching_item"].as_bool().unwrap(){Some(if BlockKind::from(state)==BlockKind::Light{ItemKind::Light}else{ItemKind::Scaffolding})}else{None};
   let shape=checked_outline_shape_with_context(state,OutlineContext{held_main:held}).unwrap();
   assert!(Shapes::join(shape.clone(),expected(&row["outline_boxes"]),|a,b|a!=b).is_empty(),"official outline state={} holding={held:?}",state.id());
   assert!(Shapes::join(interaction_shape(state).clone(),expected(&row["interaction_boxes"]),|a,b|a!=b).is_empty(),"official interaction state={}",state.id());
  }
 }
 fn horizontal_target(state:u16)->azalea_client::test_utils::prelude::Simulation{
  let mut sim=super::actual_pick_tests::setup(state);
  let eye_height=sim.component::<azalea_entity::dimensions::EntityDimensions>().eye_height;
  sim.with_component_mut::<Position>(|p|p.y=64.5-f64::from(eye_height));
  sim.app.world_mut().entity_mut(sim.entity).insert(ray(true,[0.;3],[0.,0.,1.],20).unwrap());refresh(sim.app.world_mut());sim
 }
 #[test]
 fn same_state_and_ray_held_context_refreshes_actual_pick_and_shape_cache(){
  let mut sim=horizontal_target(20504);let mut cache=OutlineCache::default();
  assert_eq!(fresh_snapshot(sim.app.world_mut(),sim.entity,2,&mut cache).unwrap()["kind"],"miss");
  sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::new(ItemKind::Scaffolding,1));
  let held=fresh_snapshot(sim.app.world_mut(),sim.entity,2,&mut cache).unwrap();assert_eq!(held["kind"],"block");assert_eq!(held["state"],20504);assert_eq!(held["outline"]["boxes"].as_array().unwrap().len(),1);assert_eq!(held["outline"]["edges"].as_array().unwrap().len(),12);
  let revision=held["revision"].as_u64().unwrap();let state=azalea_block::BlockState::try_from(20504u16).unwrap();
  let unheld=cache.get(checked_outline_shape_with_context(state,Default::default()).unwrap());let full=cache.get(checked_outline_shape_with_context(state,OutlineContext{held_main:Some(ItemKind::Scaffolding)}).unwrap());assert!(!std::sync::Arc::ptr_eq(&unheld,&full));
  sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::Empty);
  let changed=fresh_snapshot(sim.app.world_mut(),sim.entity,2,&mut cache).unwrap();assert_eq!(changed["kind"],"miss");assert!(changed["revision"].as_u64().unwrap()>revision);
 }
 #[test]
 fn light_outline_requires_actual_mainhand_not_offhand(){
  let mut sim=horizontal_target(12333);let mut cache=OutlineCache::default();
  sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(45).unwrap()=ItemStack::new(ItemKind::Light,1));
  assert_eq!(fresh_snapshot(sim.app.world_mut(),sim.entity,2,&mut cache).unwrap()["kind"],"miss");
  sim.with_component_mut::<Inventory>(|inv|*inv.inventory_menu.slot_mut(36).unwrap()=ItemStack::new(ItemKind::Light,1));
  let held=fresh_snapshot(sim.app.world_mut(),sim.entity,2,&mut cache).unwrap();assert_eq!(held["kind"],"block");assert_eq!(held["outline"]["edges"].as_array().unwrap().len(),12);
 }
 #[test]
 fn official_hollow_block_override_changes_only_face_at_negative_position(){
  for (state,from,to) in [(9259,[0.2,2.,0.5],[0.9,0.5,0.5]),(21541,[0.2,2.,0.5],[0.9,0.5,0.5]),(11111,[0.5,1.2,0.5],[0.95,0.8,0.5])]{
   let sim=super::actual_pick_tests::setup(state);let holder=sim.component::<azalea_client::local_player::InstanceHolder>();let world=holder.instance.read();let pos=BlockPos::new(-2,64,-2);
   let from=Vec3::new(from[0]-2.,from[1]+64.,from[2]-2.);let to=Vec3::new(to[0]-2.,to[1]+64.,to[2]-2.);
   let shape=checked_outline_shape_with_context(azalea_block::BlockState::try_from(state).unwrap(),Default::default()).unwrap();let plain=shape.clip(from,to,pos).unwrap();assert_eq!(plain.direction,azalea_core::direction::Direction::West);
   let hit=clip_with_outline_context(&world.chunks,ClipContext{from,to,block_shape_type:BlockShapeType::Outline,fluid_pick_type:FluidPickType::None},Default::default());
   assert!(!hit.miss);assert_eq!(hit.direction,azalea_core::direction::Direction::Up);assert_eq!(hit.location,plain.location);assert_eq!(hit.block_pos,plain.block_pos);assert_eq!(hit.inside,plain.inside);
  }
 }
}

#[cfg(test)]
mod context_packet_tests {
 use super::*;
 use azalea_client::{interact::{StartUseItemQueued,pick::HitResultComponent},packet::game::SendGamePacketEvent};
 use azalea_core::{position::Vec3,direction::Direction};
 use azalea_protocol::packets::game::{ServerboundGamePacket,s_interact::InteractionHand};
 use bevy_ecs::{prelude::*,system::RunSystemOnce};
 use std::sync::{Arc,Mutex};
 #[test]
 fn actual_cauldron_use_wire_keeps_outline_location_but_uses_official_override_face(){
  let mut sim=super::actual_pick_tests::setup(9259);let eye_height=sim.component::<azalea_entity::dimensions::EntityDimensions>().eye_height;
  sim.with_component_mut::<azalea_entity::Position>(|p|**p=Vec3::new(-1.8,66.-f64::from(eye_height),-1.5));
  let direction=Vec3::new(0.7,-1.5,0.);let n=direction.length();sim.app.world_mut().entity_mut(sim.entity).insert(ray(true,[0.;3],[direction.x/n,direction.y/n,0.],30).unwrap());refresh(sim.app.world_mut());
  let hit=sim.component::<HitResultComponent>().as_block_hit_result_if_not_miss().cloned().unwrap();assert_eq!(hit.direction,Direction::Up);assert!(hit.location.y<65.,"location remains inner-wall intersection, not fake top-plane point");
  let packets=Arc::new(Mutex::new(Vec::new()));let out=packets.clone();sim.app.world_mut().add_observer(move |event:On<SendGamePacketEvent>|out.lock().unwrap().push(event.packet.clone()));
  sim.app.world_mut().entity_mut(sim.entity).insert(StartUseItemQueued{hand:InteractionHand::MainHand,force_block:None});sim.app.world_mut().run_system_once(azalea_client::interact::handle_start_use_item_queued).unwrap();
  let packets=packets.lock().unwrap();let packet=packets.iter().find_map(|p|if let ServerboundGamePacket::UseItemOn(p)=p{Some(p)}else{None}).expect("real use-item-on wire packet");assert_eq!(packet.block_hit.direction,Direction::Up);assert_eq!(packet.block_hit.location,hit.location);assert_eq!(packet.block_hit.block_pos,hit.block_pos);
 }
}
