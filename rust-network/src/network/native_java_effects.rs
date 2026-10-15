use azalea_core::position::Vec3;
use azalea_entity::particle::Particle;
#[derive(Clone,Debug,serde::Serialize)]
pub struct VisualEffect{pub kind:&'static str,pub position:[f64;3],pub count:u32,pub offset:[f32;3],#[serde(skip_serializing_if="Option::is_none")]pub firework:Option<FireworkPayload>}
#[derive(Clone,Debug,serde::Serialize)]pub struct FireworkPayload{pub velocity:[f64;3],pub explosions:Vec<azalea_inventory::components::FireworkExplosion>}
pub fn gust(particle:&Particle,position:Vec3)->Option<VisualEffect>{
 let kind=match particle{Particle::Gust=>"gust",Particle::SmallGust=>"small_gust",Particle::GustEmitterLarge=>"gust_emitter_large",Particle::GustEmitterSmall=>"gust_emitter_small",_=>return None};
 let position=[position.x,position.y,position.z];
 if position.iter().any(|v|!v.is_finite()||v.abs()>30_000_000.){return None}
 Some(VisualEffect{kind,position,count:1,offset:[0.;3],firework:None})
}
pub fn gust_batch(packet:&azalea_protocol::packets::game::ClientboundLevelParticles)->Option<VisualEffect>{
 let mut effect=gust(&packet.particle,packet.pos)?;
 if packet.count>256{return None}
 let offset=[packet.x_dist,packet.y_dist,packet.z_dist];
 if offset.iter().any(|v|!v.is_finite()||v.abs()>1024.){return None}
 effect.count=packet.count.max(1);
 // Count zero specifies one particle; gust ignores velocity, so the packet's
 // distance vector must not become positional jitter in this special case.
 effect.offset=if packet.count==0{[0.;3]}else{offset};
 Some(effect)
}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn gust_batch_count_zero_has_no_position_jitter_and_large_invalid_batches_are_rejected(){let mut p=azalea_protocol::packets::game::ClientboundLevelParticles{override_limiter:false,always_show:false,pos:Vec3::new(1.,2.,3.),x_dist:2.,y_dist:3.,z_dist:4.,max_speed:1.,count:0,particle:Particle::Gust};let e=gust_batch(&p).unwrap();assert_eq!(e.count,1);assert_eq!(e.offset,[0.;3]);p.count=4;let e=gust_batch(&p).unwrap();assert_eq!(e.count,4);assert_eq!(e.offset,[2.,3.,4.]);p.count=257;assert!(gust_batch(&p).is_none());p.count=1;p.y_dist=f32::NAN;assert!(gust_batch(&p).is_none());}
 #[test]fn only_actual_server_gust_kinds_are_forwarded(){for(particle,kind)in[(Particle::Gust,"gust"),(Particle::SmallGust,"small_gust"),(Particle::GustEmitterLarge,"gust_emitter_large"),(Particle::GustEmitterSmall,"gust_emitter_small")]{let effect=gust(&particle,Vec3::new(1.,-10.,3.)).unwrap();assert_eq!(effect.kind,kind);assert_eq!(effect.position,[1.,-10.,3.]);}assert!(gust(&Particle::Flame,Vec3::default()).is_none());assert!(gust(&Particle::Gust,Vec3::new(f64::NAN,0.,0.)).is_none());}
}
