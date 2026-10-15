//! Decode server pack fonts without modifying the original TCPE assets.
use std::{collections::BTreeMap, io::{Cursor, Read}};
use sha1::{Digest, Sha1};

pub struct PackFiles(pub BTreeMap<String, Vec<u8>>);
#[derive(Debug)]
pub struct FontTexture {pub path:String,pub width:u32,pub height:u32,pub rgba:Vec<u8>}
#[derive(Clone,Debug,serde::Serialize)]
pub struct PackGlyph {
 pub codepoint:u32,
 #[serde(serialize_with="serialize_texture_index")]
 pub texture:Option<usize>,pub x:u32,pub y:u32,pub width:u32,pub height:u32,
 pub scale:f32,pub ascent:f32,pub advance:f32,
}
fn serialize_texture_index<S:serde::Serializer>(texture:&Option<usize>,serializer:S)->Result<S::Ok,S::Error>{serializer.serialize_i64(texture.map(|v|v as i64).unwrap_or(-1))}
#[cfg(test)]mod handoff_tests{use super::*;#[test]fn spacing_texture_serializes_as_native_signed_sentinel(){let glyph=PackGlyph{codepoint:0xe000,texture:None,x:0,y:0,width:0,height:0,scale:1.,ascent:0.,advance:-3.};let value=serde_json::to_value(&glyph).unwrap();assert_eq!(value["texture"],-1);assert_eq!(value["advance"],-3.);let bitmap=PackGlyph{texture:Some(2),..glyph};assert_eq!(serde_json::to_value(bitmap).unwrap()["texture"],2);}}
#[derive(Debug)]
pub struct PackFonts {pub fonts:BTreeMap<String,BTreeMap<u32,PackGlyph>>,pub textures:Vec<FontTexture>}
const MAX_ARCHIVE:u64=256*1024*1024;
const MAX_EXPANDED:u64=512*1024*1024;
const MAX_ENTRY:u64=32*1024*1024;

pub fn read_pack(bytes:&[u8],expected_sha1:&str)->Result<PackFiles,String>{
 if bytes.len() as u64>MAX_ARCHIVE{return Err("Resource pack exceeds size limit".into());}
 if !expected_sha1.is_empty(){
  if expected_sha1.len()!=40||!expected_sha1.bytes().all(|v|v.is_ascii_hexdigit()){return Err("Invalid resource pack SHA-1".into());}
  let actual=Sha1::digest(bytes).iter().map(|v|format!("{v:02x}")).collect::<String>();
  if !actual.eq_ignore_ascii_case(expected_sha1){return Err("Resource pack SHA-1 mismatch".into());}
 }
 let mut zip=zip::ZipArchive::new(Cursor::new(bytes)).map_err(|e|e.to_string())?;
 if zip.len()>32768{return Err("Too many resource pack entries".into());}
 let mut files=BTreeMap::new();let mut total=0u64;
 for i in 0..zip.len(){
  let entry=zip.by_index(i).map_err(|e|e.to_string())?;
  if entry.is_dir(){continue;}
  let name=entry.name().to_owned();
  if entry.enclosed_name().is_none()||name.contains('\\')||name.split('/').any(|v|v==".."){return Err("Invalid resource pack path".into());}
  if entry.size()>MAX_ENTRY{return Err("Resource pack entry exceeds size limit".into());}
  total=total.checked_add(entry.size()).ok_or("Resource pack size overflow")?;
  if total>MAX_EXPANDED{return Err("Expanded resource pack exceeds size limit".into());}
  let mut data=Vec::new();entry.take(MAX_ENTRY+1).read_to_end(&mut data).map_err(|e|e.to_string())?;
  if data.len() as u64>MAX_ENTRY{return Err("Resource pack entry exceeds size limit".into());}
  if files.insert(name,data).is_some(){return Err("Duplicate resource pack entry".into());}
 }
 if !files.contains_key("pack.mcmeta"){return Err("Resource pack metadata missing".into());}
 if files["pack.mcmeta"].len()>2*1024*1024{return Err("Resource pack metadata exceeds size limit".into());}
 let metadata:serde_json::Value=serde_json::from_slice(&files["pack.mcmeta"]).map_err(|e|e.to_string())?;
 if metadata.get("pack").and_then(|v|v.as_object()).is_none(){return Err("Invalid resource pack metadata".into());}
 apply_overlays(&mut files,&metadata,JAVA_RESOURCE_FORMAT)?;
 Ok(PackFiles(files))
}
// Mojang 1.21.11 release notes: resource pack version 75.0.
// https://www.minecraft.net/en-us/article/minecraft-java-edition-1-21-11
const JAVA_RESOURCE_FORMAT:(u32,u32)=(75,0);
fn format_bound(value:&serde_json::Value,maximum:bool)->Result<(u32,u32),String>{
 let number=|v:&serde_json::Value|v.as_u64().filter(|n|*n<=i32::MAX as u64).map(|n|n as u32).ok_or_else(||"Invalid overlay format version".to_string());
 let unspecified_minor=if maximum{i32::MAX as u32}else{0};
 if value.is_number(){return Ok((number(value)?,unspecified_minor));}
 let values=value.as_array().ok_or("Invalid overlay format version")?;
 match values.as_slice(){[major]=>Ok((number(major)?,unspecified_minor)),[major,minor]=>Ok((number(major)?,number(minor)?)),_=>Err("Invalid overlay format version".into())}
}
fn apply_overlays(files:&mut BTreeMap<String,Vec<u8>>,metadata:&serde_json::Value,target:(u32,u32))->Result<(),String>{
 let Some(overlays)=metadata.get("overlays") else{return Ok(());};
 let entries=overlays.get("entries").and_then(|v|v.as_array()).ok_or("Invalid resource pack overlays")?;
 if entries.len()>1024{return Err("Too many resource pack overlays".into());}
 let mut directories=Vec::new();let mut selected=Vec::new();
 for entry in entries{
  let directory=entry.get("directory").and_then(|v|v.as_str()).ok_or("Overlay directory missing")?;
  // A single relative directory component, matching the documented overlay grammar.
  if directory.is_empty()||directory.len()>256||!directory.bytes().all(|c|c.is_ascii_lowercase()||c.is_ascii_digit()||b"_-".contains(&c)){return Err("Invalid resource pack overlay directory".into());}
  // Modern (1.21.9+) fields govern selection; legacy `formats` must not override them.
  // Scalar/list-one maximum includes every minor version of that major.
  let minimum=format_bound(entry.get("min_format").ok_or("Overlay min_format missing")?,false)?;
  let maximum=format_bound(entry.get("max_format").ok_or("Overlay max_format missing")?,true)?;
  if minimum>maximum{return Err("Reversed resource pack overlay format range".into());}
  directories.push(format!("{directory}/"));
  if minimum<=target&&target<=maximum{selected.push(format!("{directory}/"));}
 }
 // Move selected resources rather than duplicating the expanded archive in memory.
 // List order is bottom-to-top: each later overlay replaces preceding resources.
 for prefix in selected{
  let keys=files.range(prefix.clone()..).take_while(|(name,_)|name.starts_with(&prefix)).map(|(name,_)|name.clone()).collect::<Vec<_>>();
  for key in keys{
   let relative=&key[prefix.len()..];
   if !relative.starts_with("assets/")&&!relative.starts_with("data/"){continue;}
   let value=files.remove(&key).expect("Enumerated overlay entry exists");
   files.insert(relative.to_owned(),value);
  }
 }
 files.retain(|name,_|!directories.iter().any(|prefix|name.starts_with(prefix)));
 Ok(())
}
fn texture_path(id:&str)->Result<String,String>{
 let (namespace,path)=id.split_once(':').unwrap_or(("minecraft",id));
 if namespace.is_empty()||path.is_empty()||!namespace.bytes().all(|c|c.is_ascii_lowercase()||c.is_ascii_digit()||b"_.-".contains(&c))||path.starts_with('/')||path.contains('\\')||path.split('/').any(|v|v==".."||v.is_empty()){return Err("Invalid font texture identifier".into());}
 Ok(format!("assets/{namespace}/textures/{path}"))
}
fn decode_texture(path:&str,bytes:&[u8])->Result<FontTexture,String>{
 let mut decoder=png::Decoder::new(Cursor::new(bytes));
 decoder.set_transformations(png::Transformations::EXPAND|png::Transformations::STRIP_16);
 let mut reader=decoder.read_info().map_err(|e|e.to_string())?;
 let info=reader.info();
 if info.width==0||info.height==0||info.width>8192||info.height>8192||(info.width as u64*info.height as u64*4)>64*1024*1024{return Err("Font texture dimensions exceed limit".into());}
 let mut pixels=vec![0;reader.output_buffer_size().ok_or("Invalid PNG output size")?];
 let frame=reader.next_frame(&mut pixels).map_err(|e|e.to_string())?;let pixels=&pixels[..frame.buffer_size()];
 let channels=match frame.color_type{png::ColorType::Rgba=>4,png::ColorType::Rgb=>3,png::ColorType::GrayscaleAlpha=>2,png::ColorType::Grayscale=>1,_=>return Err("Unsupported font PNG color type".into())};
 let mut rgba=Vec::with_capacity(frame.width as usize*frame.height as usize*4);
 for p in pixels.chunks_exact(channels){match channels{4=>rgba.extend_from_slice(p),3=>rgba.extend_from_slice(&[p[0],p[1],p[2],255]),2=>rgba.extend_from_slice(&[p[0],p[0],p[0],p[1]]),_=>rgba.extend_from_slice(&[p[0],p[0],p[0],255])}}
 Ok(FontTexture{path:path.into(),width:frame.width,height:frame.height,rgba})
}
fn append_texture(result:&mut PackFonts,path:&str,bytes:&[u8])->Result<(),String>{
 let texture=decode_texture(path,bytes)?;
 if result.textures.iter().map(|t|t.rgba.len()).sum::<usize>()+texture.rgba.len()>256*1024*1024{return Err("Decoded font textures exceed limit".into());}
 result.textures.push(texture);Ok(())
}
pub fn decode_fonts(pack:&PackFiles)->Result<PackFonts,String>{
 decode_fonts_with_base(pack,&|_|None)
}
pub fn decode_fonts_with_base(pack:&PackFiles,base:&impl Fn(&str)->Option<Vec<u8>>)->Result<PackFonts,String>{
 let mut result=PackFonts{fonts:BTreeMap::new(),textures:Vec::new()};let mut texture_ids=BTreeMap::new();
 for (name,data) in &pack.0{
  let Some((namespace,path))=name.strip_prefix("assets/").and_then(|s|s.split_once("/font/")) else{continue;};
  let Some(path)=path.strip_suffix(".json") else{continue;};
  if result.fonts.len()>=512{return Err("Too many pack fonts".into());}
  if data.len()>2*1024*1024{return Err("Font manifest exceeds size limit".into());}
  let json:serde_json::Value=serde_json::from_slice(data).map_err(|e|e.to_string())?;
  let providers=json.get("providers").and_then(|v|v.as_array()).ok_or("Font providers missing")?;
  let mut glyphs=BTreeMap::new();
  for provider in providers{
   if glyphs.len()>131072{return Err("Too many glyphs in pack font".into());}
   match provider.get("type").and_then(|v|v.as_str()).unwrap_or(""){
    "space"|"minecraft:space"=>{
     let advances=provider.get("advances").and_then(|v|v.as_object()).ok_or("Space advances missing")?;
     if advances.len()>131072{return Err("Too many space glyphs".into());}
     for (character,value) in advances{let mut chars=character.chars();let cp=chars.next().ok_or("Empty space glyph")?;if chars.next().is_some(){return Err("Space key must be one codepoint".into());}let advance=value.as_f64().ok_or("Invalid space advance")? as f32;if !advance.is_finite()||advance.abs()>65536.{return Err("Space advance exceeds limit".into());}glyphs.entry(cp as u32).or_insert(PackGlyph{codepoint:cp as u32,texture:None,x:0,y:0,width:0,height:0,scale:1.,ascent:0.,advance});}
    },
    "bitmap"|"minecraft:bitmap"=>{
     let path=texture_path(provider.get("file").and_then(|v|v.as_str()).ok_or("Bitmap file missing")?)?;
     let texture=if let Some(id)=texture_ids.get(&path){*id}else{let id=result.textures.len();let fallback;if let Some(bytes)=pack.0.get(&path){append_texture(&mut result,&path,bytes)?;}else{fallback=base(&path).ok_or_else(||format!("Font texture missing: {path}"))?;append_texture(&mut result,&path,&fallback)?;}texture_ids.insert(path,id);id};
     let image=&result.textures[texture];
     let rows=provider.get("chars").and_then(|v|v.as_array()).ok_or("Bitmap character rows missing")?;
     let rows=rows.iter().map(|v|v.as_str().map(|s|s.chars().collect::<Vec<_>>()).ok_or("Invalid bitmap character row")).collect::<Result<Vec<_>,_>>()?;
     let columns=rows.first().map_or(0,Vec::len);if columns==0||rows.iter().any(|r|r.len()!=columns)||image.width as usize%columns!=0||image.height as usize%rows.len()!=0{return Err("Invalid bitmap character grid".into());}
     if columns.checked_mul(rows.len()).is_none_or(|cells|cells>131072){return Err("Bitmap glyph grid exceeds limit".into());}
     let height=provider.get("height").and_then(|v|v.as_f64()).unwrap_or(8.) as f32;
     let ascent=provider.get("ascent").and_then(|v|v.as_f64()).ok_or("Bitmap ascent missing")? as f32;
     if !height.is_finite()||height<=0.||height>8192.||!ascent.is_finite()||ascent.abs()>8192.{return Err("Bitmap metrics exceed limit".into());}
     let cell_width=image.width/columns as u32;let cell_height=image.height/rows.len() as u32;let scale=height/cell_height as f32;
     for (row,chars) in rows.iter().enumerate(){for (column,&cp) in chars.iter().enumerate(){if cp=='\0'{continue;}let x=column as u32*cell_width;let y=row as u32*cell_height;let mut opaque_width=0;
      for px in (0..cell_width).rev(){if (0..cell_height).any(|py|image.rgba[(((y+py)*image.width+x+px)*4+3) as usize]!=0){opaque_width=px+1;break;}}
      let advance=(opaque_width as f32*scale+0.5).floor()+1.;glyphs.entry(cp as u32).or_insert(PackGlyph{codepoint:cp as u32,texture:Some(texture),x,y,width:cell_width,height:cell_height,scale,ascent,advance});
     }}
    },
    kind=>return Err(format!("Unsupported font provider: {kind}")),
   }
  }
  if result.fonts.values().map(BTreeMap::len).sum::<usize>()+glyphs.len()>1048576{return Err("Total pack glyphs exceed limit".into());}
  result.fonts.insert(format!("{namespace}:{path}"),glyphs);
 }
 Ok(result)
}

#[cfg(test)] mod tests{
 use super::*;
 #[test] fn negative_space_metrics_are_preserved_and_invalid_paths_rejected(){
  let mut files=BTreeMap::new();files.insert("assets/oraxen/font/shift.json".into(),br#"{"providers":[{"type":"space","advances":{"\uf80b":-1,"\uf815":-1024}}]}"#.to_vec());
  let fonts=decode_fonts(&PackFiles(files)).unwrap();assert_eq!(fonts.fonts["oraxen:shift"][&0xf815].advance,-1024.);assert!(texture_path("minecraft:../secret.png").is_err());
 }
 #[test] fn checksum_is_checked_before_zip_parsing(){assert_eq!(read_pack(b"bad",&"0".repeat(40)).err().unwrap(),"Resource pack SHA-1 mismatch");}
 #[test] fn archive_rejects_parent_paths_and_accepts_valid_metadata(){
  use std::io::Write;
  let build=|unsafe_path:bool|{let mut zip=zip::ZipWriter::new(Cursor::new(Vec::new()));zip.start_file("pack.mcmeta",zip::write::SimpleFileOptions::default()).unwrap();zip.write_all(br#"{"pack":{"pack_format":46,"description":"Test"}}"#).unwrap();if unsafe_path{zip.start_file("../escape",zip::write::SimpleFileOptions::default()).unwrap();zip.write_all(b"bad").unwrap();}zip.finish().unwrap().into_inner()};
  assert!(read_pack(&build(false),"").is_ok());assert_eq!(read_pack(&build(true),"").err().unwrap(),"Invalid resource pack path");
 }
 #[test] fn bitmap_metrics_use_alpha_width_and_preserve_negative_ascent(){
  let mut png_bytes=Vec::new();{let mut encoder=png::Encoder::new(&mut png_bytes,2,1);encoder.set_color(png::ColorType::Rgba);encoder.set_depth(png::BitDepth::Eight);let mut writer=encoder.write_header().unwrap();writer.write_image_data(&[255,255,255,255,0,0,0,0]).unwrap();}
  let mut files=BTreeMap::new();files.insert("assets/minecraft/textures/test.png".into(),png_bytes);files.insert("assets/minecraft/font/default.json".into(),br#"{"providers":[{"type":"bitmap","file":"minecraft:test.png","height":8,"ascent":-3,"chars":["\ue000"]}]}"#.to_vec());
  let fonts=decode_fonts(&PackFiles(files.clone())).unwrap();let glyph=&fonts.fonts["minecraft:default"][&0xe000];assert_eq!(glyph.advance,9.);assert_eq!(glyph.ascent,-3.);assert_eq!(glyph.scale,8.);assert_eq!(glyph.width,2);assert_eq!(fonts.textures[0].rgba.len(),8);
  let base=files.remove("assets/minecraft/textures/test.png").unwrap();let files=PackFiles(files);assert!(decode_fonts(&files).is_err());let fallback=decode_fonts_with_base(&files,&|path|(path=="assets/minecraft/textures/test.png").then(||base.clone())).unwrap();assert_eq!(fallback.fonts["minecraft:default"][&0xe000].advance,9.);
 }
 fn overlay_zip(metadata:serde_json::Value,entries:&[(&str,&[u8])])->Vec<u8>{
  use std::io::Write;
  let mut zip=zip::ZipWriter::new(Cursor::new(Vec::new()));
  zip.start_file("pack.mcmeta",zip::write::SimpleFileOptions::default()).unwrap();zip.write_all(&serde_json::to_vec(&metadata).unwrap()).unwrap();
  for (name,data) in entries{zip.start_file(*name,zip::write::SimpleFileOptions::default()).unwrap();zip.write_all(data).unwrap();}
  zip.finish().unwrap().into_inner()
 }
 #[test] fn current_java_overlay_selection_and_later_priority(){
  let metadata=serde_json::json!({"pack":{"pack_format":75},"overlays":{"entries":[
   {"directory":"older","min_format":74,"max_format":[74,99]},
   {"directory":"boss_modern","min_format":75,"max_format":88},
   {"directory":"later","min_format":[75],"max_format":[75,0]},
   {"directory":"future","min_format":[75,1],"max_format":88}
  ]}});
  let bytes=overlay_zip(metadata,&[
   ("assets/test/font/default.json",br#"{"providers":[{"type":"space","advances":{"A":1}}]}"#),
   ("boss_modern/assets/test/font/default.json",br#"{"providers":[{"type":"space","advances":{"A":2}}]}"#),
   ("later/assets/test/font/default.json",br#"{"providers":[{"type":"space","advances":{"A":3}}]}"#),
   ("future/assets/test/font/default.json",b"must not decode"),
   ("older/assets/test/font/default.json",b"must not decode"),
   ("boss_modern/assets/test/new.txt",b"added"),
   ("assets/test/unchanged.txt",b"base"),
   ("later/pack.mcmeta",b"ignored")
  ]);
  let pack=read_pack(&bytes,"").unwrap();assert_eq!(decode_fonts(&pack).unwrap().fonts["test:default"][&65].advance,3.);
  assert_eq!(pack.0["assets/test/new.txt"],b"added");assert_eq!(pack.0["assets/test/unchanged.txt"],b"base");assert!(!pack.0.keys().any(|name|name.starts_with("boss_modern/")));
 }
 #[test] fn overlay_format_minor_bounds_are_inclusive(){
  assert_eq!(format_bound(&serde_json::json!(75),false).unwrap(),(75,0));
  assert_eq!(format_bound(&serde_json::json!([75]),true).unwrap(),(75,i32::MAX as u32));
  assert_eq!(format_bound(&serde_json::json!([75,1]),true).unwrap(),(75,1));
  for bad in [serde_json::json!(-1),serde_json::json!(75.1),serde_json::json!([]),serde_json::json!([75,0,0]),serde_json::json!([75,-1])]{assert!(format_bound(&bad,false).is_err());}
  let mut files=BTreeMap::from([("assets/test/value".into(),b"base".to_vec()),("overlay/assets/test/value".into(),b"override".to_vec())]);
  let metadata=serde_json::json!({"overlays":{"entries":[{"directory":"overlay","min_format":[75,1],"max_format":[75,2]}]}});
  apply_overlays(&mut files,&metadata,(75,2)).unwrap();assert_eq!(files["assets/test/value"],b"override");
 }
 #[test] fn overlay_directories_and_ranges_are_validated_even_when_inactive(){
  for directory in ["","..","../bad","/absolute","foo/bar","foo\\bar","UPPER"]{
   let bytes=overlay_zip(serde_json::json!({"pack":{},"overlays":{"entries":[{"directory":directory,"min_format":99,"max_format":100}]}}),&[]);
   assert_eq!(read_pack(&bytes,"").err().unwrap(),"Invalid resource pack overlay directory");
  }
  let reversed=overlay_zip(serde_json::json!({"pack":{},"overlays":{"entries":[{"directory":"valid","min_format":88,"max_format":75}]}}),&[]);
  assert!(read_pack(&reversed,"").err().unwrap().contains("Reversed"));
  let missing=overlay_zip(serde_json::json!({"pack":{},"overlays":{"entries":[{"directory":"valid","formats":75}]}}),&[]);
  assert_eq!(read_pack(&missing,"").err().unwrap(),"Overlay min_format missing");
 }

}
