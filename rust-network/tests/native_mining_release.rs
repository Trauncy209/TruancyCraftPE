//! Real client input helper, loaded chunks, pick system and wire packet observer.
use azalea_client::{test_utils::prelude::*, mining::{set_left_click_mine,Mining,MiningQueued,MineProgress,MineTicks,MineBlockPos},packet::game::SendGamePacketEvent,local_player::InstanceHolder};
use azalea_core::{position::{Vec3,BlockPos,ChunkPos},direction::Direction,aabb::Aabb};
use azalea_entity::{Position,Physics,LookDirection};
use azalea_protocol::packets::{ConnectionProtocol,game::{ClientboundBlockUpdate,ServerboundGamePacket,s_player_action::Action}};
use std::sync::{Arc,Mutex};
type Wire=Arc<Mutex<Vec<Action>>>;
fn setup()->(Simulation,Wire){
 let mut s=Simulation::new(ConnectionProtocol::Game);s.receive_packet(default_login_packet());s.receive_packet(make_basic_empty_chunk(ChunkPos::new(0,0),24));s.update();
 let log:azalea_block::BlockState=azalea_block::blocks::OakLog::default().into();
 s.receive_packet(ClientboundBlockUpdate{pos:BlockPos::new(8,65,10),block_state:log});
 s.receive_packet(ClientboundBlockUpdate{pos:BlockPos::new(8,63,8),block_state:azalea_block::BlockState::try_from(1u16).unwrap()});s.update();
 s.with_component_mut::<Position>(|p|**p=Vec3::new(8.5,64.,8.5));s.with_component_mut::<Physics>(|p|{p.bounding_box=Aabb{min:Vec3::new(8.2,64.,8.2),max:Vec3::new(8.8,65.8,8.8)};p.velocity=Vec3::ZERO;p.set_on_ground(true);});
 s.app.world_mut().entity_mut(s.entity).insert(LookDirection::new(0.,4.5));s.update();
 let packets=Arc::new(Mutex::new(Vec::new()));let out=packets.clone();s.app.world_mut().add_observer(move|e:bevy_ecs::prelude::On<SendGamePacketEvent>|if let ServerboundGamePacket::PlayerAction(p)=&e.packet{out.lock().unwrap().push(p.action);});(s,packets)
}
fn press(s:&mut Simulation){set_left_click_mine(s.app.world_mut(),s.entity,true);for _ in 0..3{s.tick();}assert!(s.has_component::<Mining>());assert!(!s.component::<Mining>().force);}
fn release(s:&mut Simulation){set_left_click_mine(s.app.world_mut(),s.entity,false);s.tick();}
#[test]fn released_log_aborts_once_never_finishes_or_breaks_after_ticks(){let(mut s,w)=setup();press(&mut s);release(&mut s);for _ in 0..160{s.tick();}let p=w.lock().unwrap();assert_eq!(p.iter().filter(|a|**a==Action::StartDestroyBlock).count(),1);assert_eq!(p.iter().filter(|a|**a==Action::AbortDestroyBlock).count(),1);assert!(!p.contains(&Action::StopDestroyBlock));assert!(!s.has_component::<Mining>());assert_eq!(s.component::<MineProgress>().0,0.);assert_eq!(s.component::<MineTicks>().0,0.);let h=s.component::<InstanceHolder>();assert!(!h.instance.read().get_block_state(BlockPos::new(8,65,10)).unwrap().is_air());}
#[test]fn held_log_requires_real_progress_then_completes(){let(mut s,w)=setup();press(&mut s);assert!(!w.lock().unwrap().contains(&Action::StopDestroyBlock));let mut ticks=3;while !w.lock().unwrap().contains(&Action::StopDestroyBlock)&&ticks<200{s.tick();ticks+=1;}assert!(ticks>20,"wood must not instant-break barehanded");assert!(ticks<200,"held mining never completed");assert_eq!(w.lock().unwrap().iter().filter(|a|**a==Action::StopDestroyBlock).count(),1);}
#[test]fn queued_release_and_repeated_release_without_active_position_are_safe(){let(mut s,w)=setup();s.app.world_mut().entity_mut(s.entity).insert(MiningQueued{position:BlockPos::new(8,65,10),direction:Direction::North,force:false});release(&mut s);release(&mut s);assert!(w.lock().unwrap().is_empty());assert!(!s.has_component::<MiningQueued>());s.app.world_mut().entity_mut(s.entity).insert((Mining{pos:BlockPos::new(8,65,10),dir:Direction::North,force:false},MineBlockPos(None)));release(&mut s);release(&mut s);assert!(w.lock().unwrap().is_empty());assert!(!s.has_component::<Mining>());}
#[test]fn held_lookaway_and_out_of_reach_abort_instead_of_finishing_old_block(){for distant in [false,true]{let(mut s,w)=setup();press(&mut s);if distant{s.with_component_mut::<Position>(|p|p.z=-10.);s.with_component_mut::<Physics>(|p|{p.bounding_box.min.z=-10.3;p.bounding_box.max.z=-9.7;});}else{s.app.world_mut().entity_mut(s.entity).insert(LookDirection::new(180.,0.));}for _ in 0..140{s.tick();}let p=w.lock().unwrap();assert!(p.contains(&Action::AbortDestroyBlock),"missing abort distant={distant}");assert!(!p.contains(&Action::StopDestroyBlock));assert!(!s.has_component::<Mining>());}}
