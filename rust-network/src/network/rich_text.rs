//! Retain exact component colors through TCPE's native rich-text renderer.
use azalea_chat::{FormattedText,style::Style};
pub fn styled_text(message:&FormattedText)->String {styled_text_with_parent(message,&Style::default())}
pub fn styled_text_with_parent(message:&FormattedText,parent:&Style)->String {
 fn visit(message:&FormattedText,parent:&Style,out:&mut String,current_font:&mut String){
  let own=&message.get_base().style;let mut style=parent.clone();
  macro_rules! inherit {($($field:ident),*)=>{$(if own.$field.is_some(){style.$field=own.$field.clone();})*};}
  inherit!(color,bold,italic,underlined,strikethrough,obfuscated,font);
  let mut content=message.clone();content.get_base_mut().siblings.clear();let text=content.to_string();
  if !text.is_empty(){let font=style.font.as_deref().filter(|id|id.len()<=256&&id.contains(':')&&id.bytes().all(|c|c.is_ascii_lowercase()||c.is_ascii_digit()||b":/._-".contains(&c))).unwrap_or("minecraft:default");if current_font!=font{out.push_str("§z");out.push_str(font);out.push(';');*current_font=font.to_owned();}out.push_str("§r");if let Some(color)=&style.color{out.push_str("§x");for digit in format!("{:06x}",color.value&0xffffff).chars(){out.push('§');out.push(digit);}}
   for (enabled,code) in [(style.bold,'l'),(style.italic,'o'),(style.underlined,'n'),(style.strikethrough,'m'),(style.obfuscated,'k')]{if enabled==Some(true){out.push('§');out.push(code);}}
   out.push_str(&text);
  }
  for child in &message.get_base().siblings{visit(child,&style,out,current_font);}
 }
 let mut out=String::new();visit(message,parent,&mut out,&mut "minecraft:default".to_owned());out
}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn team_parent_color_inherits_but_explicit_child_style_wins(){
  let mut parent=Style::default();parent.color=azalea_chat::style::TextColor::parse("aqua");
  let message:FormattedText=serde_json::from_value(serde_json::json!({"text":"Prefix","extra":[{"text":"雪","color":"#123450","bold":true},{"text":"Suffix"}]})).unwrap();
  let text=styled_text_with_parent(&message,&parent);assert!(text.starts_with("§r§x§5§5§f§f§f§fPrefix"));assert!(text.contains("§r§x§1§2§3§4§5§0§l雪"));assert!(text.ends_with("§r§x§5§5§f§f§f§fSuffix"));
 }
 #[test]fn inherited_hex_colors_and_explicit_style_reset(){
  let message:FormattedText=serde_json::from_value(serde_json::json!({"text":"A","color":"#123456","bold":true,"extra":[{"text":"雪","color":"#abcdef","bold":false},{"text":"B"}]})).unwrap();
  let out=styled_text(&message);assert!(out.contains("§x§1§2§3§4§5§6§lA"));assert!(out.contains("§x§a§b§c§d§e§f雪"));assert!(out.ends_with("§x§1§2§3§4§5§6§lB"));
 }
}

#[cfg(test)]mod font_tests{use super::*;#[test]fn named_fonts_inherit_and_default_siblings_restore(){let message:FormattedText=serde_json::from_value(serde_json::json!({"text":"A","extra":[{"text":"B","font":"oraxen:shift","extra":[{"text":"C"}]},{"text":"D"}]})).unwrap();let text=styled_text(&message);assert!(text.contains("§zoraxen:shift;§rB§rC"));assert!(text.ends_with("§zminecraft:default;§rD"));}}
