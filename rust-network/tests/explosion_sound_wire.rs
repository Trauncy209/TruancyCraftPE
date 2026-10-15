use std::io::Cursor;
use azalea_buf::{AzaleaWriteVar,AzaleaRead,AzaleaWrite};
use azalea_core::{position::Vec3,sound::CustomSound};
use azalea_entity::particle::Particle;
use azalea_protocol::packets::game::ClientboundExplode;
use azalea_registry::{builtin::SoundEvent,Holder,Registry};
fn prefix()->Vec<u8>{let mut b=Vec::new();Vec3::new(1.,2.,3.).azalea_write(&mut b).unwrap();3f32.azalea_write(&mut b).unwrap();0i32.azalea_write(&mut b).unwrap();Option::<Vec3>::None.azalea_write(&mut b).unwrap();Particle::Gust.azalea_write(&mut b).unwrap();b}
#[test]fn official_explosion_reference_sound_is_registry_id_plus_one(){let mut bytes=prefix();let sound=SoundEvent::EntityWindChargeWindBurst;(sound.to_u32()+1).azalea_write_var(&mut bytes).unwrap();0u32.azalea_write_var(&mut bytes).unwrap();let mut cursor=Cursor::new(bytes.as_slice());let p=ClientboundExplode::azalea_read(&mut cursor).unwrap();assert_eq!(p.explosion_sound,Holder::Reference(sound));assert_eq!(p.center,Vec3::new(1.,2.,3.));assert!(p.block_particles.is_empty());assert_eq!(cursor.position(),bytes.len()as u64);}
#[test]fn official_explosion_inline_sound_consumes_identifier_and_optional_range(){let mut bytes=prefix();0u32.azalea_write_var(&mut bytes).unwrap();let sound=CustomSound{sound_id:"example:boss_wind".into(),range:Some(40.)};sound.azalea_write(&mut bytes).unwrap();0u32.azalea_write_var(&mut bytes).unwrap();let mut cursor=Cursor::new(bytes.as_slice());let p=ClientboundExplode::azalea_read(&mut cursor).unwrap();assert_eq!(p.explosion_sound,Holder::Direct(sound));assert!(p.block_particles.is_empty());assert_eq!(cursor.position(),bytes.len()as u64);let mut encoded=Vec::new();p.azalea_write(&mut encoded).unwrap();assert_eq!(encoded,bytes);}
