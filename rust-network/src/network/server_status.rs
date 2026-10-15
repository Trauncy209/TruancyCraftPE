//! Direct Java status requests, independent of the gameplay connection.
use std::{io::{Read,Write},net::{TcpStream,SocketAddr,IpAddr},time::{Duration,Instant},sync::{mpsc,atomic::{AtomicUsize,Ordering}}};
use serde_json::Value;
static ACTIVE:AtomicUsize=AtomicUsize::new(0);
pub struct Status { pub motd:String,pub icon:Vec<u8>,pub online:i32,pub max:i32,pub latency:i32 }
pub struct Request { rx:mpsc::Receiver<Result<Status,String>>, result:Option<Status> }
#[repr(C)]pub struct StatusView{pub motd:*const u8,pub motd_len:usize,pub icon:*const u8,pub icon_len:usize,pub online:i32,pub max:i32,pub latency:i32}
fn varint(mut n:u32,out:&mut Vec<u8>){loop{let b=(n&127) as u8;n>>=7;out.push(b|if n!=0{128}else{0});if n==0{break;}}}
fn read_varint(r:&mut impl Read)->Result<u32,String>{let mut n=0;for i in 0..5{let mut b=[0];r.read_exact(&mut b).map_err(|e|e.to_string())?;if i==4&&b[0]&0xf0!=0{return Err("Invalid VarInt".into());}n|=((b[0]&127)as u32)<<(i*7);if b[0]&128==0{return Ok(n)}}Err("Invalid VarInt".into())}
fn packet(s:&mut TcpStream,data:&[u8])->Result<(),String>{let mut out=Vec::new();varint(data.len() as u32,&mut out);out.extend_from_slice(data);s.write_all(&out).map_err(|e|e.to_string())}
fn parse(value:Value,latency:i32)->Result<Status,String>{
 let description=value.get("description").cloned().unwrap_or(Value::String(String::new()));let component:azalea_chat::FormattedText=serde_json::from_value(description).map_err(|e|e.to_string())?;
 let icon=value["favicon"].as_str().filter(|s|s.len()<200000).and_then(|s|s.strip_prefix("data:image/png;base64,")).and_then(|s|base64::Engine::decode(&base64::engine::general_purpose::STANDARD,s).ok()).and_then(|bytes|super::player_textures::decode_skin_png(&bytes).ok()).filter(|(w,h,_)|*w==64&&*h==64).map(|(_,_,data)|data).unwrap_or_default();
 Ok(Status{motd:super::rich_text::styled_text(&component),icon,online:value["players"]["online"].as_i64().unwrap_or(0).clamp(0,i32::MAX as i64)as i32,max:value["players"]["max"].as_i64().unwrap_or(0).clamp(0,i32::MAX as i64)as i32,latency})
}
fn resolve_status(host:&str,port:u16)->Result<SocketAddr,String>{
 if let Ok(ip)=host.parse::<IpAddr>(){return Ok(SocketAddr::new(ip,port));}
 static RUNTIME:std::sync::OnceLock<Option<tokio::runtime::Runtime>>=std::sync::OnceLock::new();
 let runtime=RUNTIME.get_or_init(||tokio::runtime::Builder::new_multi_thread().worker_threads(1).enable_all().build().ok()).as_ref().ok_or("Status DNS runtime unavailable")?;
 runtime.block_on(async{tokio::time::timeout(Duration::from_secs(4),azalea_protocol::resolve::resolve_address(&azalea_protocol::address::ServerAddr{host:host.to_owned(),port})).await.map_err(|_|"Status DNS timed out".to_string())?.map_err(|e|e.to_string())})
}
fn fetch(host:&str,port:u16)->Result<Status,String>{
 let address=resolve_status(host,port)?;fetch_at(host,port,address)
}
// Socket target may be redirected; the handshake retains the user's original host/port.
fn fetch_at(host:&str,port:u16,address:SocketAddr)->Result<Status,String>{
 let mut s=TcpStream::connect_timeout(&address,Duration::from_secs(3)).map_err(|e|e.to_string())?;s.set_read_timeout(Some(Duration::from_secs(3))).map_err(|e|e.to_string())?;s.set_write_timeout(Some(Duration::from_secs(3))).map_err(|e|e.to_string())?;
 let mut hello=vec![0];varint(774,&mut hello);varint(host.len()as u32,&mut hello);hello.extend_from_slice(host.as_bytes());hello.extend_from_slice(&port.to_be_bytes());hello.push(1);packet(&mut s,&hello)?;packet(&mut s,&[0])?;
 let length=read_varint(&mut s)? as usize;if length>1024*1024{return Err("Status too large".into());}let mut bytes=vec![0;length];s.read_exact(&mut bytes).map_err(|e|e.to_string())?;let mut data=std::io::Cursor::new(bytes);if read_varint(&mut data)?!=0{return Err("Unexpected status packet".into());}let len=read_varint(&mut data)?as usize;if len>length||data.position()as usize+len!=length{return Err("Invalid status length".into());}let mut json=vec![0;len];data.read_exact(&mut json).map_err(|e|e.to_string())?;
 let mut ping=vec![1];ping.extend_from_slice(&0x54435045_i64.to_be_bytes());let ping_start=Instant::now();packet(&mut s,&ping)?;let pong_len=read_varint(&mut s)?;if pong_len!=9{return Err("Invalid pong".into());}let mut pong=[0;9];s.read_exact(&mut pong).map_err(|e|e.to_string())?;if pong!=ping.as_slice(){return Err("Invalid pong token".into());}parse(serde_json::from_slice(&json).map_err(|e|e.to_string())?,ping_start.elapsed().as_millis().min(i32::MAX as u128)as i32)
}
#[no_mangle]pub unsafe extern "C" fn tcpe_java_status_start(host:*const std::ffi::c_char,len:usize,port:u16)->*mut Request{
 let Some(host)=super::super::text(host,len,253).filter(|s|!s.is_empty()&&!s.contains('\0'))else{return std::ptr::null_mut();};if port==0{return std::ptr::null_mut();}if ACTIVE.fetch_add(1,Ordering::SeqCst)>=4{ACTIVE.fetch_sub(1,Ordering::SeqCst);return std::ptr::null_mut();}let host=host.to_owned();let(tx,rx)=mpsc::channel();std::thread::spawn(move||{let result=std::panic::catch_unwind(||fetch(&host,port)).unwrap_or_else(|_|Err("Status failed".into()));let _=tx.send(result);ACTIVE.fetch_sub(1,Ordering::SeqCst);});Box::into_raw(Box::new(Request{rx,result:None}))
}
#[no_mangle]pub unsafe extern "C" fn tcpe_java_status_poll(request:*mut Request,out:*mut StatusView)->i32{
 let(Some(request),Some(out))=(request.as_mut(),out.as_mut())else{return -1;};if request.result.is_none(){match request.rx.try_recv(){Ok(Ok(value))=>request.result=Some(value),Ok(Err(_))|Err(mpsc::TryRecvError::Disconnected)=>return -1,Err(mpsc::TryRecvError::Empty)=>return 0}}
 let value=request.result.as_ref().unwrap();*out=StatusView{motd:value.motd.as_ptr(),motd_len:value.motd.len(),icon:value.icon.as_ptr(),icon_len:value.icon.len(),online:value.online,max:value.max,latency:value.latency};1
}
#[no_mangle]pub unsafe extern "C" fn tcpe_java_status_free(request:*mut Request){if !request.is_null(){drop(Box::from_raw(request));}}
#[cfg(test)]mod tests{use super::*;
 #[test]fn formatted_status_retains_rgb_lines_counts_and_ignores_bad_icon(){let value=parse(serde_json::json!({"description":{"text":"Zeus","color":"#123450","extra":[{"text":"\nEvents","color":"gold"}]},"players":{"online":4,"max":400},"favicon":"bad"}),15).unwrap();assert!(value.motd.contains("§x§1§2§3§4§5§0Zeus"));assert!(value.motd.contains("\nEvents"));assert_eq!((value.online,value.max,value.latency),(4,400,15));assert!(value.icon.is_empty());}
 #[test]fn direct_status_exchange_checks_handshake_and_ping_token(){
  let listener=std::net::TcpListener::bind(("127.0.0.1",0)).unwrap();let port=listener.local_addr().unwrap().port();
  let server=std::thread::spawn(move||{let(mut socket,_)=listener.accept().unwrap();socket.set_read_timeout(Some(Duration::from_secs(3))).unwrap();
   let length=read_varint(&mut socket).unwrap();let mut bytes=vec![0;length as usize];socket.read_exact(&mut bytes).unwrap();let mut hello=std::io::Cursor::new(bytes);assert_eq!(read_varint(&mut hello).unwrap(),0);assert_eq!(read_varint(&mut hello).unwrap(),774);let host_length=read_varint(&mut hello).unwrap();let mut host=vec![0;host_length as usize];hello.read_exact(&mut host).unwrap();assert_eq!(host,b"127.0.0.1");let mut tail=[0;3];hello.read_exact(&mut tail).unwrap();assert_eq!(&tail[..2],&port.to_be_bytes());assert_eq!(tail[2],1);assert_eq!(read_varint(&mut socket).unwrap(),1);let mut request=[1];socket.read_exact(&mut request).unwrap();assert_eq!(request,[0]);
   let json=br##"{"description":{"text":"First\nSecond","color":"#abcdef"},"players":{"online":3,"max":20}}"##;let mut response=vec![0];varint(json.len()as u32,&mut response);response.extend_from_slice(json);packet(&mut socket,&response).unwrap();assert_eq!(read_varint(&mut socket).unwrap(),9);let mut ping=[0;9];socket.read_exact(&mut ping).unwrap();assert_eq!(ping[0],1);packet(&mut socket,&ping).unwrap();
  });let result=fetch("127.0.0.1",port).unwrap();assert_eq!((result.online,result.max),(3,20));assert!(result.motd.contains("First\nSecond"));assert!(result.motd.contains("§x§a§b§c§d§e§f"));server.join().unwrap();
 }
 #[test]fn numeric_status_endpoints_keep_custom_port(){for host in ["127.0.0.1","::1"]{let address=resolve_status(host,23456).unwrap();assert_eq!(address.ip(),host.parse::<IpAddr>().unwrap());assert_eq!(address.port(),23456);}}
 #[test]fn varints_reject_overflow(){assert!(read_varint(&mut &b"\xff\xff\xff\xff\x7f"[..]).is_err());for n in [0,127,128,774,1048576]{let mut bytes=vec![];varint(n,&mut bytes);assert_eq!(read_varint(&mut bytes.as_slice()).unwrap(),n);}}
}
