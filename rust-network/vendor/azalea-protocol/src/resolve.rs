//! Resolve Minecraft addresses through the platform's actual DNS configuration.
use std::net::{IpAddr,SocketAddr};
#[cfg(not(target_os="android"))]
use std::sync::LazyLock;
pub use hickory_resolver::ResolveError;
use hickory_resolver::Name;
#[cfg(not(target_os="android"))]
use hickory_resolver::{TokioResolver,name_server::TokioConnectionProvider};
use crate::address::ServerAddr;
#[cfg(target_os="android")]
#[path="resolve_android.rs"]
mod android;
#[doc(hidden)]
#[deprecated(note="Renamed to ResolveError")]
pub type ResolverError=ResolveError;
// A missing desktop resolver configuration is an error, never a poisoned panic.
#[cfg(not(target_os="android"))]
static RESOLVER:LazyLock<Result<TokioResolver,ResolveError>>=LazyLock::new(||TokioResolver::builder(TokioConnectionProvider::default()).map(|builder|builder.build()));
#[cfg(not(target_os="android"))]
fn resolver()->Result<&'static TokioResolver,ResolveError>{RESOLVER.as_ref().map_err(Clone::clone)}
#[doc(hidden)]
pub fn needs_srv_lookup(address:&ServerAddr)->bool{address.port==25565&&address.host.parse::<IpAddr>().is_err()}
/// IP literals bypass DNS; an explicit port bypasses Minecraft's optional SRV redirect.
pub async fn resolve_address(address:&ServerAddr)->Result<SocketAddr,ResolveError>{
 resolve_with_lookups(address.clone(),|a|async move{resolve_srv_redirect(&a).await},|a|async move{resolve_ip_without_redirects(&a).await}).await
}
#[doc(hidden)]
pub async fn resolve_with_lookups<S,SF,I,IF>(address:ServerAddr,srv:S,ip:I)->Result<SocketAddr,ResolveError>
where S:FnOnce(ServerAddr)->SF,SF:std::future::Future<Output=Result<ServerAddr,ResolveError>>,I:FnOnce(ServerAddr)->IF,IF:std::future::Future<Output=Result<SocketAddr,ResolveError>>{
 if let Ok(literal)=address.host.parse::<IpAddr>(){return Ok(SocketAddr::new(literal,address.port));}
 let target=if needs_srv_lookup(&address){srv(address.clone()).await.unwrap_or(address)}else{address};ip(target).await
}
async fn resolve_ip_without_redirects(address:&ServerAddr)->Result<SocketAddr,ResolveError>{
 if let Ok(ip)=address.host.parse::<IpAddr>(){return Ok(SocketAddr::new(ip,address.port));}
 Name::from_ascii(&address.host)?;
 #[cfg(target_os="android")]
 {return tokio::net::lookup_host((address.host.as_str(),address.port)).await.map_err(ResolveError::from)?.next().ok_or_else(||ResolveError::from("No platform A/AAAA record found"));}
 #[cfg(not(target_os="android"))]
 {let lookup=resolver()?.lookup_ip(Name::from_ascii(&address.host)?).await?;let ip=lookup.iter().next().ok_or(ResolveError::from("No A/AAAA record found"))?;Ok(SocketAddr::new(ip,address.port))}
}
async fn resolve_srv_redirect(address:&ServerAddr)->Result<ServerAddr,ResolveError>{
 let query=format!("_minecraft._tcp.{}",address.host);
 #[cfg(target_os="android")]
 {let expected=query.clone();let bytes=tokio::task::spawn_blocking(move||android::query_srv(&query)).await.map_err(|_|ResolveError::from("Android DNS worker failed"))??;return decode_srv_response(&bytes,&expected);}
 #[cfg(not(target_os="android"))]
 {let records=resolver()?.srv_lookup(query).await?;let srv=records.iter().next().ok_or(ResolveError::from("No SRV record found"))?;Ok(ServerAddr{host:srv.target().to_ascii(),port:srv.port()})}
}
/// Validate the platform DNS wire response before following its server redirect.
#[doc(hidden)]
pub fn decode_srv_response(bytes:&[u8],expected:&str)->Result<ServerAddr,ResolveError>{
 use hickory_resolver::proto::{op::{Message,MessageType,ResponseCode},rr::{DNSClass,RData,RecordType}};
 if bytes.len()>65535{return Err("Oversized DNS response".into());}
 let message=Message::from_vec(bytes)?;let mut expected=Name::from_ascii(expected)?;expected.set_fqdn(true);
 if message.message_type()!=MessageType::Response||message.truncated()||message.response_code()!=ResponseCode::NoError{return Err("No successful complete SRV response".into());}
 if message.queries().len()!=1||message.queries()[0].name()!=&expected||message.queries()[0].query_type()!=RecordType::SRV||message.queries()[0].query_class()!=DNSClass::IN{return Err("Mismatched SRV response query".into());}
 let mut owners=vec![expected];
 // Follow only an answer CNAME chain rooted at the actual question, never an
 // unrelated SRV inserted elsewhere in the response. Bound work and cycles.
 for _ in 0..16{let mut added=false;for record in message.answers(){if record.dns_class()!=DNSClass::IN||!owners.contains(record.name()){continue;}if let RData::CNAME(target)=record.data(){if !owners.contains(&target.0){owners.push(target.0.clone());added=true;if owners.len()>17{return Err("SRV CNAME chain too long".into());}}}}if !added{break;}}
 let srv=message.answers().iter().filter(|record|record.dns_class()==DNSClass::IN&&owners.contains(record.name())).find_map(|record|match record.data(){RData::SRV(srv)=>Some(srv),_=>None}).ok_or(ResolveError::from("No SRV record found"))?;
 if srv.target().is_root()||srv.port()==0{return Err("Unavailable SRV service".into());}
 Ok(ServerAddr{host:srv.target().to_ascii(),port:srv.port()})
}
