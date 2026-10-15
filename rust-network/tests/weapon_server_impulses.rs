use azalea_client::test_utils::prelude::*;
use azalea_core::position::Vec3;
use azalea_entity::{Physics,particle::Particle};
use azalea_protocol::packets::{ConnectionProtocol,game::ClientboundExplode};
use azalea_registry::builtin::SoundEvent;
#[test]
fn server_explosion_adds_impulse_without_replacing_existing_motion(){
 let mut sim=Simulation::new(ConnectionProtocol::Game);sim.receive_packet(default_login_packet());sim.update();
 let initial=Vec3::new(0.15,-0.25,0.3);let impulse=Vec3::new(0.2,0.8,-0.4);sim.with_component_mut::<Physics>(|p|p.velocity=initial);
 let packet=ClientboundExplode{center:Vec3::ZERO,radius:1.,block_count:0,player_knockback:Some(impulse),explosion_particle:Particle::Gust,explosion_sound:azalea_registry::Holder::Reference(SoundEvent::EntityGenericExplode),block_particles:vec![]};
 sim.receive_packet(packet);sim.update();let actual=sim.component::<Physics>().velocity;assert!((actual.x-0.35).abs()<1e-9);assert!((actual.y-0.55).abs()<1e-9);assert!((actual.z+0.1).abs()<1e-9);
 sim.receive_packet(ClientboundExplode{center:Vec3::ZERO,radius:1.,block_count:0,player_knockback:None,explosion_particle:Particle::Gust,explosion_sound:azalea_registry::Holder::Reference(SoundEvent::EntityGenericExplode),block_particles:vec![]});sim.update();assert_eq!(sim.component::<Physics>().velocity,actual);
}
