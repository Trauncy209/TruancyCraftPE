//! Decode server profile texture properties. This does not verify signatures or fetch images.
use base64::{Engine, engine::general_purpose::STANDARD};
use serde::Serialize;
use serde_json::Value;
#[derive(Debug, Clone, PartialEq, Serialize)]
pub struct PlayerTextures {pub skin_url:Option<String>,pub slim:bool,pub cape_url:Option<String>}
fn texture_url(value:&Value)->Option<String>{
    let raw=value.get("url")?.as_str()?;
    let mut url=url::Url::parse(raw).ok()?;
    if !matches!(url.scheme(),"http"|"https") || url.host_str()!=Some("textures.minecraft.net") || !url.username().is_empty() || url.password().is_some() || url.port().is_some() || url.query().is_some() || url.fragment().is_some(){return None;}
    let hash=url.path().strip_prefix("/texture/")?;
    if hash.len()!=64 || !hash.bytes().all(|c|c.is_ascii_hexdigit()){return None;}
    url.set_scheme("https").ok()?;
    Some(url.into())
}
pub fn decode(encoded:&str)->Option<PlayerTextures>{
    if encoded.len()>32767{return None;}
    let data=STANDARD.decode(encoded).ok()?;
    let value:Value=serde_json::from_slice(&data).ok()?;
    let textures=value.get("textures")?.as_object()?;
    let skin=textures.get("SKIN");
    let skin_url=skin.and_then(texture_url);
    let slim=skin.and_then(|v|v.get("metadata")).and_then(|v|v.get("model")).and_then(Value::as_str)==Some("slim");
    Some(PlayerTextures{skin_url,slim,cape_url:textures.get("CAPE").and_then(texture_url)})
}
#[cfg(test)] mod tests{
 use super::*;
 fn property(value:Value)->String{STANDARD.encode(serde_json::to_vec(&value).unwrap())}
 fn address()->String{format!("http://textures.minecraft.net/texture/{}","a".repeat(64))}
 #[test] fn canonical_skin_cape_and_slim(){
  let result=decode(&property(serde_json::json!({"textures":{"SKIN":{"url":address(),"metadata":{"model":"slim"}},"CAPE":{"url":address()}}}))).unwrap();
  assert!(result.slim);assert_eq!(result.skin_url,Some(address().replacen("http:","https:",1)));assert_eq!(result.skin_url,result.cape_url);
 }
 #[test] fn absent_skin_stays_absent(){let v=decode(&property(serde_json::json!({"textures":{}}))).unwrap();assert_eq!(v.skin_url,None);assert!(!v.slim);}
 #[test] fn malformed_properties_rejected(){for v in ["not base64".to_owned(),STANDARD.encode(b"not json"),"a".repeat(32768)]{assert!(decode(&v).is_none());}}
 #[test] fn other_origins_and_invalid_paths_not_fetch_targets(){
  for url in ["https://example.com/texture/a","file:///tmp/skin.png","http://textures.minecraft.net.evil/texture/a","http://user@textures.minecraft.net/texture/a","https://textures.minecraft.net:444/texture/a","https://textures.minecraft.net/texture/short"]{
   let v=decode(&property(serde_json::json!({"textures":{"SKIN":{"url":url}}}))).unwrap();assert!(v.skin_url.is_none(),"{url}");
  }
 }
}

/// Decode bounded Java skin PNGs to RGBA for the original texture uploader.
/// Modern skins are 64x64; older profiles may supply 64x32 images.
pub fn decode_skin_png(bytes:&[u8])->Result<(u32,u32,Vec<u8>),String>{
    if bytes.len()>1024*1024{return Err("Skin PNG exceeds size limit".into());}
    let mut decoder=png::Decoder::new(std::io::Cursor::new(bytes));
    decoder.set_limits(png::Limits{bytes:1024*1024});
    decoder.set_transformations(png::Transformations::EXPAND|png::Transformations::STRIP_16);
    let mut reader=decoder.read_info().map_err(|e|e.to_string())?;
    let info=reader.info();
    if info.width!=64 || !matches!(info.height,32|64){return Err("Unsupported Java skin dimensions".into());}
    let mut buffer=vec![0;reader.output_buffer_size().ok_or("Missing PNG output size")?];
    let output=reader.next_frame(&mut buffer).map_err(|e|e.to_string())?;
    let data=&buffer[..output.buffer_size()];
    let mut rgba=Vec::with_capacity((output.width*output.height*4)as usize);
    match output.color_type {
        png::ColorType::Rgba=>rgba.extend_from_slice(data),
        png::ColorType::Rgb=>for pixel in data.chunks_exact(3){rgba.extend_from_slice(&[pixel[0],pixel[1],pixel[2],255]);},
        png::ColorType::Grayscale=>for &v in data{rgba.extend_from_slice(&[v,v,v,255]);},
        png::ColorType::GrayscaleAlpha=>for pixel in data.chunks_exact(2){rgba.extend_from_slice(&[pixel[0],pixel[0],pixel[0],pixel[1]]);},
        _=>return Err("Unsupported decoded skin color type".into()),
    }
    if rgba.len()!=(output.width*output.height*4)as usize{return Err("Incomplete skin pixels".into());}
    Ok((output.width,output.height,rgba))
}
#[cfg(test)] mod image_tests{
 use super::*;
 fn image(width:u32,height:u32)->Vec<u8>{
  let mut bytes=Vec::new();{let mut encoder=png::Encoder::new(&mut bytes,width,height);encoder.set_color(png::ColorType::Rgba);encoder.set_depth(png::BitDepth::Eight);let mut writer=encoder.write_header().unwrap();writer.write_image_data(&vec![127;(width*height*4)as usize]).unwrap();}bytes
 }
 #[test] fn skin_pixels_and_dimensions_survive(){for height in [32,64]{let (w,h,pixels)=decode_skin_png(&image(64,height)).unwrap();assert_eq!((w,h),(64,height));assert_eq!(pixels,vec![127;(64*height*4)as usize]);}}
 #[test] fn malformed_oversized_wrong_dimensions_rejected(){assert!(decode_skin_png(b"not png").is_err());assert!(decode_skin_png(&vec![0;1024*1024+1]).is_err());assert!(decode_skin_png(&image(32,32)).is_err());}
}

#[derive(Debug,serde::Serialize)]
pub struct SkinImage {pub url:String,pub width:u32,pub height:u32,pub rgba_base64:String}
pub fn download_client()->Result<reqwest::Client,String>{
 let roots=rustls::RootCertStore::from_iter(webpki_roots::TLS_SERVER_ROOTS.iter().cloned());
 let tls=rustls::ClientConfig::builder_with_provider(std::sync::Arc::new(rustls::crypto::aws_lc_rs::default_provider()))
  .with_safe_default_protocol_versions().map_err(|e|e.to_string())?.with_root_certificates(roots).with_no_client_auth();
 reqwest::Client::builder().tls_backend_preconfigured(tls).redirect(reqwest::redirect::Policy::none()).timeout(std::time::Duration::from_secs(8)).build().map_err(|e|e.to_string())
}
pub async fn download_skin(client:reqwest::Client,url:String)->Result<SkinImage,String>{
 if texture_url(&serde_json::json!({"url":url})).as_deref()!=Some(&url){return Err("Unsupported skin URL".into());}
 let mut response=client.get(&url).send().await.map_err(|e|e.to_string())?.error_for_status().map_err(|e|e.to_string())?;
 if response.content_length().is_some_and(|n|n>1024*1024){return Err("Skin response exceeds size limit".into());}
 let mut bytes=Vec::new();
 while let Some(chunk)=response.chunk().await.map_err(|e|e.to_string())?{if bytes.len()+chunk.len()>1024*1024{return Err("Skin response exceeds size limit".into());}bytes.extend_from_slice(&chunk);}
 let (width,height,rgba)=decode_skin_png(&bytes)?;
 Ok(SkinImage{url,width,height,rgba_base64:STANDARD.encode(rgba)})
}
#[cfg(test)] mod download_tests {
 use super::*;
 #[tokio::test] async fn invalid_origin_fails_before_any_request(){
  let client=download_client().expect("explicit TLS roots must initialize without a platform Java verifier");
  assert_eq!(download_skin(client,"https://example.com/skin.png".into()).await.unwrap_err(),"Unsupported skin URL");
 }
}

/// Cosmetic username fallback for genuine offline-mode player identities only.
/// Server-supplied properties always win, including NPC/custom texture properties.
pub fn username_fallback_candidate(kind:&str,profile:&Value)->Option<String>{
 if kind!="Player"||profile.get("textures").is_some_and(|v|!v.is_null()){return None}
 let name=profile.get("name")?.as_str()?;
 if !valid_username(name){return None}
 let uuid=profile.get("uuid")?.as_str()?;
 if uuid!=azalea_client::Account::offline(name).uuid_or_offline().to_string(){return None}
 Some(name.to_owned())
}
fn valid_username(name:&str)->bool{(1..=16).contains(&name.len())&&name.bytes().all(|c|c.is_ascii_alphanumeric()||c==b'_')}
fn official_profile_id(value:&Value,name:&str)->Option<String>{
 if !value.get("name")?.as_str()?.eq_ignore_ascii_case(name){return None}
 let id=value.get("id")?.as_str()?;if id.len()!=32||!id.bytes().all(|c|c.is_ascii_hexdigit()){return None}Some(id.to_ascii_lowercase())
}
fn official_profile_textures(value:&Value,id:&str)->Option<PlayerTextures>{
 if !value.get("id")?.as_str()?.eq_ignore_ascii_case(id){return None}
 value.get("properties")?.as_array()?.iter().take(32).find(|p|p.get("name").and_then(Value::as_str)==Some("textures")).and_then(|p|p.get("value").and_then(Value::as_str)).and_then(decode)
}
async fn bounded_profile_json(client:&reqwest::Client,address:String)->Result<Option<Value>,String>{
 let mut response=client.get(address).send().await.map_err(|e|e.to_string())?;
 if matches!(response.status().as_u16(),204|404){return Ok(None)}
 response=response.error_for_status().map_err(|e|e.to_string())?;
 if response.content_length().is_some_and(|n|n>65536){return Err("Profile response exceeds size limit".into())}
 let mut bytes=Vec::new();while let Some(chunk)=response.chunk().await.map_err(|e|e.to_string())?{if bytes.len()+chunk.len()>65536{return Err("Profile response exceeds size limit".into())}bytes.extend_from_slice(&chunk)}
 serde_json::from_slice(&bytes).map(Some).map_err(|e|e.to_string())
}
type ProfileCache=std::collections::BTreeMap<String,(std::time::Instant,Option<PlayerTextures>)>;
static PROFILE_CACHE:std::sync::OnceLock<std::sync::Mutex<ProfileCache>>=std::sync::OnceLock::new();
pub async fn lookup_username(client:reqwest::Client,name:String)->Result<(String,Option<PlayerTextures>),String>{
 if !valid_username(&name){return Err("Invalid profile username".into())}
 let key=name.to_ascii_lowercase();let cache=PROFILE_CACHE.get_or_init(Default::default);
 if let Ok(cache)=cache.lock(){if let Some((when,value))=cache.get(&key){let ttl=if value.is_some(){1800}else{300};if when.elapsed().as_secs()<ttl{return Ok((key,value.clone()))}}}
 let result=async{
  let Some(profile)=bounded_profile_json(&client,format!("https://api.mojang.com/users/profiles/minecraft/{name}")).await?else{return Ok(None)};
  let id=official_profile_id(&profile,&name).ok_or("Invalid official username profile")?;
  let Some(profile)=bounded_profile_json(&client,format!("https://sessionserver.mojang.com/session/minecraft/profile/{id}")).await?else{return Ok(None)};
  Ok::<_,String>(official_profile_textures(&profile,&id))
 }.await;
 // Short negative cache also bounds repeated requests after rate limits/network errors.
 let value=result.unwrap_or(None);if let Ok(mut cache)=cache.lock(){if cache.len()>=512&&!cache.contains_key(&key){if let Some(oldest)=cache.iter().min_by_key(|(_,v)|v.0).map(|(k,_)|k.clone()){cache.remove(&oldest);}}cache.insert(key.clone(),(std::time::Instant::now(),value.clone()));}
 Ok((key,value))
}
#[cfg(test)]mod username_profile_tests{
 use super::*;
 #[test]fn only_exact_offline_player_identity_without_server_texture_gets_fallback(){let name="Notch";let mut profile=serde_json::json!({"name":name,"uuid":azalea_client::Account::offline(name).uuid_or_offline().to_string(),"textures":null});assert_eq!(username_fallback_candidate("Player",&profile),Some(name.into()));assert!(username_fallback_candidate("Zombie",&profile).is_none());profile["textures"]=serde_json::json!("server custom/NPC property");assert!(username_fallback_candidate("Player",&profile).is_none());profile["textures"]=Value::Null;profile["uuid"]=serde_json::json!("00000000-0000-0000-0000-000000000000");assert!(username_fallback_candidate("Player",&profile).is_none());for name in["../Notch","https://example.com","","abcdefghijklmnopq"]{assert!(!valid_username(name));}}
 #[test]fn official_username_and_uuid_must_match_and_slim_model_survives(){let id="a".repeat(32);assert_eq!(official_profile_id(&serde_json::json!({"id":id,"name":"nOtCh"}),"Notch"),Some(id.clone()));assert!(official_profile_id(&serde_json::json!({"id":id,"name":"Other"}),"Notch").is_none());let encoded=STANDARD.encode(serde_json::to_vec(&serde_json::json!({"textures":{"SKIN":{"url":format!("http://textures.minecraft.net/texture/{}","b".repeat(64)),"metadata":{"model":"slim"}}}})).unwrap());let profile=serde_json::json!({"id":id,"properties":[{"name":"textures","value":encoded}]});let result=official_profile_textures(&profile,&id).unwrap();assert!(result.slim&&result.skin_url.is_some());assert!(official_profile_textures(&profile,&"c".repeat(32)).is_none());}
}
#[cfg(test)]mod official_live_profile_check{
 use super::*;
 #[tokio::test]#[ignore="Explicit live official API/PNG verification; not an offline unit test"]
 async fn official_username_profile_and_skin_png_download(){
  let client=download_client().unwrap();let(name,profile)=lookup_username(client.clone(),"Notch".into()).await.unwrap();assert_eq!(name,"notch");let profile=profile.expect("Known official profile must resolve");let url=profile.skin_url.expect("Official skin URL");let image=download_skin(client,url).await.unwrap();assert_eq!(image.width,64);assert!(matches!(image.height,32|64));assert_eq!(STANDARD.decode(image.rgba_base64).unwrap().len(),(image.width*image.height*4)as usize);println!("Official Notch lookup and validated {}x{} skin PNG decoded; slim={}",image.width,image.height,profile.slim);
 }
}
