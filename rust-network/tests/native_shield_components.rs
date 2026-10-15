//! Reference-derived shield Holder wire, including inline values that formerly desynchronized slots.
use azalea_buf::{AzaleaRead,AzaleaWrite};
use azalea_inventory::{ItemStack,components::{BannerPatterns,BannerPattern,BannerPatternData,BaseColor,DyeColor}};
use azalea_registry::{Holder,DataRegistry,builtin::ItemKind,data};
use azalea_protocol::packets::game::ClientboundContainerSetContent;
use std::io::Cursor;
fn patterns()->BannerPatterns{BannerPatterns{patterns:vec![BannerPattern{pattern:Holder::Direct(BannerPatternData{asset_id:"minecraft:stripe_top".parse().unwrap(),translation_key:"block.minecraft.banner.stripe_top".into()}),color:14},BannerPattern{pattern:Holder::Reference(data::BannerPattern::new_raw(345)),color:4}]}}
#[test]fn official_inline_zero_and_reference_plus_one_wire_roundtrip_preserve_following_bytes(){
 let asset="minecraft:stripe_top";let translation="block.minecraft.banner.stripe_top";
 let mut wire=vec![2,0,asset.len()as u8];wire.extend(asset.bytes());wire.push(translation.len()as u8);wire.extend(translation.bytes());wire.extend([14,0xda,0x02,4]);let end=wire.len();wire.push(0x55);
 let mut cursor=Cursor::new(wire.as_slice());let decoded=BannerPatterns::azalea_read(&mut cursor).unwrap();assert_eq!(cursor.position(),end as u64);assert_eq!(decoded,patterns());assert_eq!(u8::azalea_read(&mut cursor).unwrap(),0x55);
 let mut encoded=Vec::new();decoded.azalea_write(&mut encoded).unwrap();assert_eq!(encoded,wire[..end]);
}
#[test]fn actual_container_packet_inline_shield_keeps_next_slot_and_carried_item_aligned(){
 let shield=ItemStack::new(ItemKind::Shield,1).with_component(BaseColor{color:DyeColor::Blue}).with_component(patterns());
 let packet=ClientboundContainerSetContent{container_id:4,state_id:123,items:vec![shield,ItemStack::new(ItemKind::Diamond,3)],carried_item:ItemStack::new(ItemKind::IronIngot,2)};
 let mut wire=Vec::new();packet.write(&mut wire).unwrap();let mut cursor=Cursor::new(wire.as_slice());let azalea_protocol::packets::game::ClientboundGamePacket::ContainerSetContent(decoded)=ClientboundContainerSetContent::read(&mut cursor).unwrap()else{panic!("wrong packet")};assert_eq!(cursor.position(),wire.len()as u64);assert_eq!(decoded.container_id,4);assert_eq!(decoded.state_id,123);assert_eq!(decoded.items[1],ItemStack::new(ItemKind::Diamond,3));assert_eq!(decoded.carried_item,ItemStack::new(ItemKind::IronIngot,2));let ItemStack::Present(shield)=&decoded.items[0]else{panic!("shield missing")};assert_eq!(shield.get_component::<BannerPatterns>().unwrap().as_ref(),&patterns());assert_eq!(shield.get_component::<BaseColor>().unwrap().color,DyeColor::Blue);
}
