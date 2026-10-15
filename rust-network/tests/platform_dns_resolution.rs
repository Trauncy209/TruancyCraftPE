use azalea_protocol::{address::ServerAddr,resolve::{decode_srv_response,resolve_with_lookups,ResolveError}};
use std::net::SocketAddr;
fn name(value:&str)->Vec<u8>{if value=="."{return vec![0];}let mut bytes=Vec::new();for label in value.trim_end_matches('.').split('.'){bytes.push(label.len()as u8);bytes.extend(label.as_bytes());}bytes.push(0);bytes}
fn response(query:&str,target:Option<&str>,rcode:u8)->Vec<u8>{
 let mut b=vec![0x12,0x34,0x81,0x80|rcode,0,1,0,u8::from(target.is_some()),0,0,0,0];b.extend(name(query));b.extend([0,33,0,1]);if let Some(target)=target{let target=name(target);b.extend([0xc0,0x0c,0,33,0,1,0,0,0,60]);b.extend(((target.len()+6)as u16).to_be_bytes());b.extend([0,10,0,5,0x63,0xe9]);b.extend(target);}b
}
#[test]fn platform_srv_wire_preserves_target_and_port_and_rejects_malformed_or_unrelated_answers(){
 let query="_minecraft._tcp.example.test";let wire=response(query,Some("backend.example.test"),0);let address=decode_srv_response(&wire,query).unwrap();assert_eq!(address.host,"backend.example.test.");assert_eq!(address.port,25577);
 assert!(decode_srv_response(&wire,"_minecraft._tcp.different.test").is_err());assert!(decode_srv_response(&wire[..wire.len()-2],query).is_err());assert!(decode_srv_response(&response(query,None,3),query).is_err());assert!(decode_srv_response(&response(query,None,0),query).is_err());assert!(decode_srv_response(&response(query,Some("."),0),query).is_err());
 let owner_start=12+name(query).len()+4;let mut unrelated=wire[..owner_start].to_vec();unrelated.extend(name("foreign.example.test"));unrelated.extend_from_slice(&wire[owner_start+2..]);assert!(decode_srv_response(&unrelated,query).is_err());
 let alias="_minecraft._tcp.alias.example.test";let mut cname=response(query,None,0);cname[7]=2;cname.extend([0xc0,0x0c,0,5,0,1,0,0,0,60]);cname.extend((name(alias).len()as u16).to_be_bytes());cname.extend(name(alias));cname.extend(name(alias));cname.extend_from_slice(&wire[owner_start+2..]);assert_eq!(decode_srv_response(&cname,query).unwrap().host,"backend.example.test.");
 let mut truncated=wire.clone();truncated[2]|=2;assert!(decode_srv_response(&truncated,query).is_err());let mut wrongtype=wire;let qtype=12+name(query).len();wrongtype[qtype+1]=1;assert!(decode_srv_response(&wrongtype,query).is_err());
}
#[tokio::test]async fn custom_ports_and_ip_literals_skip_srv_and_missing_srv_uses_platform_host_lookup(){
 let expected:SocketAddr="127.0.0.1:25577".parse().unwrap();let custom=ServerAddr{host:"custom.example.test".into(),port:25577};let got=resolve_with_lookups(custom,|_|async{panic!("custom port must not query SRV");#[allow(unreachable_code)]Err(ResolveError::from("unexpected"))},|a|async move{assert_eq!(a.port,25577);assert_eq!(a.host,"custom.example.test");Ok(expected)}).await.unwrap();assert_eq!(got,expected);
 let literal=ServerAddr{host:"::1".into(),port:25565};let got=resolve_with_lookups(literal,|_|async{panic!("literal SRV");#[allow(unreachable_code)]Err(ResolveError::from("unexpected"))},|_|async{panic!("literal A/AAAA");#[allow(unreachable_code)]Err(ResolveError::from("unexpected"))}).await.unwrap();assert_eq!(got,"[::1]:25565".parse::<SocketAddr>().unwrap());
 let original=ServerAddr{host:"original.example.test".into(),port:25565};let got=resolve_with_lookups(original,|_|async{Err(ResolveError::from("No SRV record found"))},|a|async move{assert_eq!(a.host,"original.example.test");assert_eq!(a.port,25565);Ok(expected)}).await.unwrap();assert_eq!(got,expected);
}
#[tokio::test]async fn successful_srv_redirect_changes_only_socket_target(){
 let original=ServerAddr{host:"entry.example.test".into(),port:25565};let address=resolve_with_lookups(original.clone(),|a|async move{assert_eq!(a.host,"entry.example.test");Ok(ServerAddr{host:"backend.example.test".into(),port:25577})},|a|async move{assert_eq!(a.host,"backend.example.test");assert_eq!(a.port,25577);Ok("127.0.0.1:25577".parse().unwrap())}).await.unwrap();assert_eq!(address.port(),25577);assert_eq!(original.host,"entry.example.test");assert_eq!(original.port,25565);
}
