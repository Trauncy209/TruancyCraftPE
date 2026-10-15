use tcpe_java_network::network::resource_pack_fonts::{read_pack,decode_fonts_with_base};
fn main(){
 let args=std::env::args().collect::<Vec<_>>();
 let bytes=std::fs::read(args.get(1).expect("pack ZIP path required")).expect("read pack");
 let files=read_pack(&bytes,args.get(2).map(String::as_str).unwrap_or("")).expect("verify pack");
 let fonts=decode_fonts_with_base(&files,&|path|args.get(3).and_then(|root|std::fs::read(std::path::Path::new(root).join(path)).ok())).expect("decode fonts");
 let counts=fonts.fonts.iter().map(|(id,glyphs)|(id.clone(),glyphs.len())).collect::<std::collections::BTreeMap<_,_>>();
 let negative=fonts.fonts.values().flat_map(|v|v.values()).filter(|g|g.advance<0.).count();
 let max_height=fonts.fonts.values().flat_map(|v|v.values()).map(|g|g.height as f32*g.scale).fold(0f32,f32::max);
 println!("{}",serde_json::json!({"fonts":counts,"textures":fonts.textures.len(),"rgba_bytes":fonts.textures.iter().map(|v|v.rgba.len()).sum::<usize>(),"negative_spacing_glyphs":negative,"largest_glyph_height":max_height}));
}
