//! Bounded server-pack OGG/definition decoding; activation uses existing pack scope.
use std::{collections::{BTreeMap,BTreeSet},io::Cursor};
use serde::Serialize;
use sha1::{Digest,Sha1};
use base64::{engine::general_purpose::STANDARD,Engine};
#[derive(Debug,Serialize)]pub struct PackSoundVariant{pub name:String,pub kind:String,pub weight:u32,pub volume:f32,pub pitch:f32,pub attenuation_distance:f32,pub stream:bool}
#[derive(Debug,Serialize)]pub struct PackSoundDefinition{pub name:String,pub replace:bool,pub variants:Vec<PackSoundVariant>}
#[derive(Debug,Serialize)]pub struct PackSoundSample{pub name:String,pub file:String,pub channels:u8,pub rate:u32,pub pcm_base64:String}
#[derive(Debug,Default,Serialize)]pub struct PackSounds{pub definitions:Vec<PackSoundDefinition>,pub samples:Vec<PackSoundSample>,pub errors:Vec<String>,pub unavailable_files:Vec<String>}
fn id(raw:&str,namespace:&str)->Option<String>{let value=if raw.contains(':'){raw.to_owned()}else{format!("{namespace}:{raw}")};let(n,p)=value.split_once(':')?;if n.is_empty()||p.is_empty()||value.len()>256||!n.bytes().all(|c|c.is_ascii_lowercase()||c.is_ascii_digit()||b"_.-".contains(&c))||!p.bytes().all(|c|c.is_ascii_lowercase()||c.is_ascii_digit()||b"_/.-".contains(&c))||p.split('/').any(|v|v==".."||v.is_empty()){return None;}Some(value)}
fn number(v:&serde_json::Value,key:&str,default:f32)->Result<f32,String>{let Some(value)=v.get(key)else{return Ok(default)};let n=value.as_f64().ok_or("Invalid sound numeric attribute")?;if !n.is_finite()||n<0.||n>100000.{return Err("Invalid sound numeric range".into());}Ok(n as f32)}
pub fn decode(files:&BTreeMap<String,Vec<u8>>)->Result<PackSounds,String>{
 let mut result=PackSounds{definitions:Vec::new(),samples:Vec::new(),errors:Vec::new(),unavailable_files:Vec::new()};let mut requested=BTreeSet::new();let mut streaming=BTreeSet::new();let mut decoded_bytes=0usize;
 for(path,bytes)in files{let Some(ns)=path.strip_prefix("assets/").and_then(|s|s.strip_suffix("/sounds.json"))else{continue};if ns.contains('/')||bytes.len()>4*1024*1024{continue;}let json:serde_json::Value=serde_json::from_slice(bytes).map_err(|_|"Invalid sounds.json")?;let object=json.as_object().ok_or("Invalid sound event table")?;if object.len()>4096{return Err("Pack has too many sound events".into());}
 for(raw,event)in object{if raw.contains(':'){return Err("Invalid sound event key".into());}let name=id(raw,ns).ok_or("Invalid sound event identifier")?;let mut definition=PackSoundDefinition{name,replace:event.get("replace").and_then(|v|v.as_bool()).unwrap_or(false),variants:Vec::new()};let empty=Vec::new();let entries=match event.get("sounds"){None=>&empty,Some(value)=>value.as_array().ok_or("Invalid sound variant list")?};if entries.len()>512{return Err("Too many sound variants".into());}
 for(entry_idx,entry)in entries.iter().enumerate(){let raw=entry.as_str().or_else(||entry.get("name").and_then(|v|v.as_str())).ok_or("Missing sound name")?;let sound=id(raw,"minecraft").ok_or("Invalid sound variant identifier")?;let kind=entry.get("type").and_then(|v|v.as_str()).unwrap_or("file");if kind!="event"&&kind!="file"{return Err("Invalid sound variant type".into());}let stream=entry.get("stream").and_then(|v|v.as_bool()).unwrap_or(false);let weight=number(entry,"weight",1.)?;if weight<1.||weight.fract()!=0.{return Err("Invalid sound weight".into());}let volume=number(entry,"volume",1.)?;let pitch=number(entry,"pitch",1.)?;let attenuation=number(entry,"attenuation_distance",16.)?;if volume<=0.||pitch<=0.||attenuation<=0.{return Err("Invalid sound playback attributes".into());}
 if stream{if kind=="file"{streaming.insert(sound.clone());}if result.errors.len()<128{result.errors.push(format!("Streaming sound unsupported: {} variant {}",definition.name,entry_idx));}continue;}if kind=="file"{requested.insert(sound.clone());}definition.variants.push(PackSoundVariant{name:sound,kind:kind.to_owned(),weight:weight as u32,volume,pitch,attenuation_distance:attenuation,stream:false});}
 result.definitions.push(definition);}}
 // File resources can shadow existing canonical sounds without sounds.json.
 // Keep explicit streaming files excluded; all remaining decoding retains the
 // same per-sample/whole-pack limits as definition-referenced resources.
 for path in files.keys(){
  let Some(resource)=path.strip_prefix("assets/")else{continue};
  let Some((ns,resource))=resource.split_once("/sounds/")else{continue};
  let Some(resource)=resource.strip_suffix(".ogg")else{continue};
  let Some(name)=id(resource,ns)else{continue};
  if !streaming.contains(&name){requested.insert(name);}
 }
 if requested.len()>4096{return Err("Too many pack sound files".into());}
 for name in requested{
  let(ns,p)=name.split_once(':').unwrap();let path=format!("assets/{ns}/sounds/{p}.ogg");let Some(bytes)=files.get(&path)else{continue};
  match decode_sample(&name,bytes){
   Ok((sample,size))if size<=64*1024*1024-decoded_bytes=>{decoded_bytes+=size;result.samples.push(sample);},
   outcome=>{let reason=match outcome{Ok(_)=>"Pack decoded sound budget exceeded".to_owned(),Err(reason)=>reason};if result.errors.len()<128{result.errors.push(format!("{name}: {reason}"));}result.unavailable_files.push(name);}
  }
 }

 Ok(result)
}
fn decode_sample(name:&str,bytes:&[u8])->Result<(PackSoundSample,usize),String>{
 if bytes.len()>8*1024*1024{return Err("Pack sound OGG exceeds limit".into());}
 let mut reader=lewton::inside_ogg::OggStreamReader::new(Cursor::new(bytes)).map_err(|_|"Invalid OGG sound")?;
 let channels=reader.ident_hdr.audio_channels;let rate=reader.ident_hdr.audio_sample_rate;
 if !(channels==1||channels==2)||rate<8000||rate>192000{return Err("Unsupported OGG sound format".into());}
 let mut pcm=Vec::new();while let Some(packet)=reader.read_dec_packet_itl().map_err(|_|"OGG sound decode failed")?{
  if pcm.len().checked_add(packet.len()*2).is_none_or(|n|n>16*1024*1024){return Err("Decoded sound exceeds PCM limit".into());}
  for sample in packet{pcm.extend_from_slice(&sample.to_le_bytes());}
 }
 if pcm.is_empty(){return Err("Empty pack sound".into());}
 let size=pcm.len();let mut hash=Sha1::new();hash.update(name.as_bytes());hash.update([channels]);hash.update(rate.to_le_bytes());hash.update(&pcm);
 let file=format!("{}.wav",hash.finalize().iter().take(12).map(|v|format!("{v:02x}")).collect::<String>());
 Ok((PackSoundSample{name:name.to_owned(),file,channels,rate,pcm_base64:STANDARD.encode(pcm)},size))
}
#[cfg(test)]mod tests{use super::*;fn pack(json:&str)->BTreeMap<String,Vec<u8>>{BTreeMap::from([("assets/example/sounds.json".into(),json.as_bytes().to_vec())])}
 #[test]fn empty_replace_and_default_minecraft_file_namespace(){let result=decode(&pack(r#"{"silence":{"replace":true},"wood":{"sounds":["step/wood1"]}}"#)).unwrap();assert_eq!(result.definitions[0].name,"example:silence");assert!(result.definitions[0].replace&&result.definitions[0].variants.is_empty());assert_eq!(result.definitions[1].variants[0].name,"minecraft:step/wood1");assert!(result.samples.is_empty());}
 #[test]fn real_mace_ogg_keeps_original_rate_channels(){let mut files=pack(r#"{"mace":{"sounds":[{"name":"example:mace","volume":0.5,"pitch":1.25,"attenuation_distance":32}]}}"#);files.insert("assets/example/sounds/mace.ogg".into(),include_bytes!("../../../java-assets/audio-fixtures/mace-smash-heavy.ogg").to_vec());let result=decode(&files).unwrap();assert_eq!(result.samples.len(),1);let sample=&result.samples[0];assert_eq!(sample.channels,1);assert_eq!(sample.rate,48000);assert_eq!(sample.name,"example:mace");let pcm=STANDARD.decode(&sample.pcm_base64).unwrap();assert_eq!(pcm.len()%2,0);assert!(pcm.len()>100_000&&pcm.iter().any(|b|*b!=0));let variant=&result.definitions[0].variants[0];assert_eq!((variant.volume,variant.pitch,variant.attenuation_distance),(0.5,1.25,32.));}
 #[test]fn streaming_is_explicit_unsupported_and_never_fake_pcm(){let result=decode(&pack(r#"{"welcome":{"sounds":[{"name":"example:welcome","stream":true}]}}"#)).unwrap();assert!(result.definitions[0].variants.is_empty());assert!(result.samples.is_empty());assert_eq!(result.errors.len(),1);}
 #[test]fn file_only_override_decodes_without_inventing_definition(){let files=BTreeMap::from([("assets/minecraft/sounds/step/wood1.ogg".into(),include_bytes!("../../../java-assets/audio-fixtures/mace-smash-heavy.ogg").to_vec())]);let result=decode(&files).unwrap();assert!(result.definitions.is_empty());assert_eq!(result.samples.len(),1);assert_eq!(result.samples[0].name,"minecraft:step/wood1");assert_eq!(result.samples[0].rate,48000);}
 #[test]fn explicitly_streaming_file_remains_excluded(){let mut files=pack(r#"{"song":{"sounds":[{"name":"example:song","stream":true}]}}"#);files.insert("assets/example/sounds/song.ogg".into(),include_bytes!("../../../java-assets/audio-fixtures/mace-smash-heavy.ogg").to_vec());let result=decode(&files).unwrap();assert!(result.samples.is_empty());assert_eq!(result.errors.len(),1);}
 #[test]fn bad_file_does_not_remove_valid_sound_or_empty_replace(){let mut files=pack(r#"{"silence":{"replace":true},"bad":{"sounds":["example:bad"]},"good":{"sounds":["example:good"]}}"#);files.insert("assets/example/sounds/bad.ogg".into(),vec![0;128]);files.insert("assets/example/sounds/good.ogg".into(),include_bytes!("../../../java-assets/audio-fixtures/mace-smash-heavy.ogg").to_vec());let result=decode(&files).unwrap();assert_eq!(result.definitions.len(),3);assert!(result.definitions.iter().any(|d|d.name=="example:silence"&&d.replace&&d.variants.is_empty()));assert_eq!(result.samples.len(),1);assert_eq!(result.samples[0].name,"example:good");assert_eq!(result.unavailable_files,vec!["example:bad"]);assert_eq!(result.errors.len(),1);}
 #[test]fn invalid_canonical_file_override_is_explicitly_unavailable(){let files=BTreeMap::from([("assets/minecraft/sounds/step/wood1.ogg".into(),vec![0;128])]);let result=decode(&files).unwrap();assert!(result.definitions.is_empty()&&result.samples.is_empty());assert_eq!(result.unavailable_files,vec!["minecraft:step/wood1"]);}
 #[test]fn event_alias_weight_and_gain_are_preserved(){let result=decode(&pack(r#"{"alias":{"sounds":[{"name":"example:source","type":"event","weight":3,"volume":0.75,"pitch":1.5}]}}"#)).unwrap();let v=&result.definitions[0].variants[0];assert_eq!(v.kind,"event");assert_eq!(v.weight,3);assert_eq!(v.volume,0.75);assert_eq!(v.pitch,1.5);}
 #[test]fn malformed_and_path_escape_sound_definitions_rejected(){for json in [r#"{"x":{"sounds":["../escape"]}}"#,r#"{"x":{"sounds":[{"name":"ok","pitch":0}]}}"#,r#"{"x":{"sounds":[{"name":"ok","weight":1.5}]}}"#,r#"{"bad:key":{"sounds":[]}}"#]{assert!(decode(&pack(json)).is_err(),"{json}");}let mut p=pack(r#"{"bad":{"sounds":["example:bad"]}}"#);p.insert("assets/example/sounds/bad.ogg".into(),vec![0;1024]);let result=decode(&p).unwrap();assert!(result.samples.is_empty());assert_eq!(result.unavailable_files,vec!["example:bad"]);assert_eq!(result.errors.len(),1);}
}
