//! Extract signed argument ranges from the server-advertised command tree.
//! Message arguments preserve their exact raw range (including quotes/selectors).
use azalea_protocol::packets::game::c_commands::{ClientboundCommands,NodeType,BrigadierParser,BrigadierString};
#[derive(Debug,PartialEq)] pub enum ParseError { TooComplex, Invalid }
impl super::secure_state::SecureChatState {
 pub fn signed_arguments(&self,text:&str)->Result<Vec<(String,String)>,ParseError>{
  let Some(tree)=&self.commands else{return Ok(Vec::new())};
  let mut fuel=4096;let mut active=Vec::new();
  walk(tree,tree.root_index,text,0,Vec::new(),&mut fuel,&mut active).ok_or(ParseError::Invalid)
 }
}
fn walk(tree:&ClientboundCommands,index:u32,text:&str,pos:usize,args:Vec<(String,String)>,fuel:&mut usize,active:&mut Vec<(u32,usize)>)->Option<Vec<(String,String)>>{
 if *fuel==0||active.len()>128||active.contains(&(index,pos)){return None;}*fuel-=1;active.push((index,pos));
 let node=tree.entries.get(index as usize)?;
 let pos=skip(text,pos);
 if pos==text.len()&&node.is_executable{active.pop();return Some(args);}
 // Brigadier prioritizes an exact literal over argument alternatives.
 let exact=node.children.iter().copied().find(|id|tree.entries.get(*id as usize).is_some_and(|n|matches!(&n.node_type,NodeType::Literal{name} if text[pos..].starts_with(name)&&text.get(pos+name.len()..).is_some_and(|s|s.is_empty()||s.starts_with(' ')))));
 for child in exact.into_iter().chain(if exact.is_none(){node.children.clone()}else{Vec::new()}){
  let n=tree.entries.get(child as usize)?;let mut nextargs=args.clone();
  let next=match &n.node_type {
   NodeType::Root=>pos,
   NodeType::Literal{name}=>{if !text[pos..].starts_with(name){continue;}let end=pos+name.len();if end<text.len()&&!text[end..].starts_with(' '){continue;}end},
   NodeType::Argument{name,parser,..}=>{let Some(end)=parse(parser,text,pos) else {continue};if matches!(parser,BrigadierParser::Message){nextargs.push((name.clone(),text[pos..end].to_owned()));}end}
  };
  let target=n.redirect_node.unwrap_or(child);
  if let Some(found)=walk(tree,target,text,next,nextargs,fuel,active){active.pop();return Some(found);}
 }
 active.pop();None
}
fn skip(s:&str,mut p:usize)->usize{while s.as_bytes().get(p)==Some(&b' '){p+=1;}p}
fn token(s:&str,p:usize,quoted:bool)->Option<usize>{
 let first=*s.as_bytes().get(p)?;
 if quoted&&(first==b'"'||first==b'\'') {let mut escape=false;for (i,b) in s.as_bytes().iter().enumerate().skip(p+1){if escape{escape=false;}else if *b==b'\\'{escape=true;}else if *b==first{return Some(i+1);}}return None;}
 let mut brackets=Vec::new();let mut quote=0u8;let mut escape=false;
 for(i,b) in s.as_bytes().iter().enumerate().skip(p){
  if quote!=0{if escape{escape=false;}else if *b==b'\\'{escape=true;}else if *b==quote{quote=0;}continue;}
  match *b {b'"'|b'\'' if !brackets.is_empty()=>quote=*b,b'['=>brackets.push(b']'),b'{'=>brackets.push(b'}'),b'('=>brackets.push(b')'),b']'|b'}'|b')'=>{if brackets.pop()!=Some(*b){return None;}},b' ' if brackets.is_empty()=>return Some(i),_=>{}}
 }
 (quote==0&&brackets.is_empty()).then_some(s.len())
}
fn parse(parser:&BrigadierParser,s:&str,p:usize)->Option<usize>{
 use BrigadierParser::*;
 match parser {
  Message|String(BrigadierString::GreedyPhrase)=>Some(s.len()),
  String(BrigadierString::QuotablePhrase)=>token(s,p,true),
  BlockPos|Vec3=>coords(s,p,3),ColumnPos|Vec2|Rotation=>coords(s,p,2),
  Bool=>{let e=token(s,p,false)?;matches!(&s[p..e],"true"|"false").then_some(e)},
  Integer(n)=>{let e=token(s,p,false)?;let v=s[p..e].parse::<i32>().ok()?;(n.min.is_none_or(|m|v>=m)&&n.max.is_none_or(|m|v<=m)).then_some(e)},
  Long(n)=>{let e=token(s,p,false)?;let v=s[p..e].parse::<i64>().ok()?;(n.min.is_none_or(|m|v>=m)&&n.max.is_none_or(|m|v<=m)).then_some(e)},
  Float(n)=>{let e=token(s,p,false)?;let v=s[p..e].parse::<f32>().ok()?;(v.is_finite()&&n.min.is_none_or(|m|v>=m)&&n.max.is_none_or(|m|v<=m)).then_some(e)},
  Double(n)=>{let e=token(s,p,false)?;let v=s[p..e].parse::<f64>().ok()?;(v.is_finite()&&n.min.is_none_or(|m|v>=m)&&n.max.is_none_or(|m|v<=m)).then_some(e)},
  _=>token(s,p,false),
 }
}
fn coords(s:&str,mut p:usize,count:usize)->Option<usize>{for i in 0..count{p=token(s,skip(s,p),false)?;if i+1<count&&p==s.len(){return None;}}Some(p)}
#[cfg(test)]mod tests {
 use super::*;use azalea_protocol::packets::game::c_commands::BrigadierNodeStub;
 fn node(t:NodeType,children:Vec<u32>,executable:bool)->BrigadierNodeStub{BrigadierNodeStub{node_type:t,children,is_executable:executable,redirect_node:None,is_restricted:false}}
 #[test]fn tree_message_range_selector_quotes_alias(){let mut t=super::super::secure_state::SecureChatState::default();t.commands=Some(ClientboundCommands{root_index:0,entries:vec![node(NodeType::Root,vec![1,4],false),node(NodeType::Literal{name:"tell".into()},vec![2],false),node(NodeType::Argument{name:"targets".into(),parser:BrigadierParser::GameProfile,suggestions_type:None},vec![3],false),node(NodeType::Argument{name:"message".into(),parser:BrigadierParser::Message,suggestions_type:None},vec![],true),{let mut n=node(NodeType::Literal{name:"w".into()},vec![],false);n.redirect_node=Some(1);n}]});assert_eq!(t.signed_arguments("w @a[name=\"A B\"] hello \"raw\" §"),Ok(vec![("message".into(),"hello \"raw\" §".into())]));assert_eq!(t.signed_arguments("tell Player hello"),Ok(vec![("message".into(),"hello".into())]));}
}
