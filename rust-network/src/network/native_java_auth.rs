//! Account credentials stay in Rust. Public snapshots contain display metadata only.
use std::{fs,io::Write,path::{Path,PathBuf},sync::{Arc,Mutex,OnceLock},time::{SystemTime,UNIX_EPOCH}};
use serde::{Serialize,Deserialize};
use azalea_client::Account;
use azalea_auth::{cache::ExpiringValue,AccessTokenResponse};
use uuid::Uuid;
const MAX_CACHE:usize=1024*1024;
fn now()->u64{SystemTime::now().duration_since(UNIX_EPOCH).map(|t|t.as_secs()).unwrap_or(0)}
#[derive(Default,Clone,Serialize)]
pub struct View {pub revision:u64,pub phase:u32,pub available:bool,pub username:String,pub message:String,pub user_code:String,pub verification_uri:String,pub expires_at:u64}
#[derive(Serialize,Deserialize)]
struct Stored {username:String,uuid:Uuid,minecraft_token:String,minecraft_expires:u64,msa:Option<ExpiringValue<AccessTokenResponse>>}
struct State {view:View,generation:u64,account:Option<Stored>,loaded:bool}
static STATE:OnceLock<Mutex<State>>=OnceLock::new();
fn state()->&'static Mutex<State>{STATE.get_or_init(||Mutex::new(State{view:View::default(),generation:0,account:None,loaded:false}))}
fn cache_path()->Result<PathBuf,&'static str>{
 if let Some(dir)=std::env::var_os("TCPE_JAVA_AUTH_DIR"){return Ok(PathBuf::from(dir).join("account.json"));}
 #[cfg(target_os="android")]return Err("Private Android account storage is unavailable");
 #[cfg(not(target_os="android"))]{let root=std::env::var_os("XDG_DATA_HOME").map(PathBuf::from).or_else(||std::env::var_os("HOME").map(|h|PathBuf::from(h).join(".local/share"))).ok_or("Private account storage is unavailable")?;Ok(root.join("truancycraftpe/accounts/account.json"))}
}
fn valid_name(name:&str)->bool{!name.is_empty()&&name.len()<=16&&name.bytes().all(|c|c.is_ascii_alphanumeric()||c==b'_')}
fn validate(s:&Stored)->Result<(),&'static str>{if !valid_name(&s.username)||s.uuid.is_nil()||s.minecraft_token.is_empty()||s.minecraft_token.len()>32768{return Err("Saved Minecraft account is invalid");}Ok(())}
fn read_private(path:&Path)->Result<Stored,&'static str>{
 let info=fs::symlink_metadata(path).map_err(|_|"No saved Minecraft account")?;if !info.is_file()||info.file_type().is_symlink()||info.len() as usize>MAX_CACHE{return Err("Saved account file is invalid");}
 #[cfg(unix)]{use std::os::unix::fs::PermissionsExt;if info.permissions().mode()&0o077!=0{return Err("Saved account permissions must be private");}}
 let data=fs::read(path).map_err(|_|"Could not read saved account")?;let s:Stored=serde_json::from_slice(&data).map_err(|_|"Saved account file is invalid")?;validate(&s)?;Ok(s)
}
fn save_private(path:&Path,s:&Stored)->Result<(),&'static str>{
 let dir=path.parent().ok_or("Invalid account storage")?;if fs::symlink_metadata(dir).map(|m|m.file_type().is_symlink()).unwrap_or(false){return Err("Account storage must be a private directory");}fs::create_dir_all(dir).map_err(|_|"Could not create private account storage")?;
 #[cfg(unix)]{use std::os::unix::fs::PermissionsExt;fs::set_permissions(dir,fs::Permissions::from_mode(0o700)).map_err(|_|"Could not protect account storage")?;}
 let temporary=path.with_extension(format!("{}.tmp",std::process::id()));let mut options=fs::OpenOptions::new();options.write(true).create_new(true);
 #[cfg(unix)]{use std::os::unix::fs::OpenOptionsExt;options.mode(0o600);}
 let mut file=options.open(&temporary).map_err(|_|"Could not create private account file")?;let bytes=serde_json::to_vec(s).map_err(|_|"Could not save account")?;
 let result=(||{file.write_all(&bytes)?;file.sync_all()?;fs::rename(&temporary,path)})();if result.is_err(){let _=fs::remove_file(&temporary);return Err("Could not save private account");}Ok(())
}
fn publish(s:&mut State,message:&str){s.view.revision=s.view.revision.wrapping_add(1);s.view.user_code.clear();s.view.verification_uri.clear();s.view.message=message.into();if let Some(account)=&s.account{s.view.username=account.username.clone();s.view.expires_at=account.minecraft_expires;s.view.available=account.minecraft_expires>now()||account.msa.is_some();s.view.phase=if s.view.available{2}else{3};}else{s.view.username.clear();s.view.expires_at=0;s.view.available=false;s.view.phase=0;}}
fn ensure_loaded(s:&mut State){if s.loaded{return;}s.loaded=true;if let Ok(path)=cache_path(){if let Ok(account)=read_private(&path){s.account=Some(account);publish(s,"Saved Minecraft account");}}}
pub fn snapshot()->View{let mut s=state().lock().unwrap();ensure_loaded(&mut s);if s.view.available&&s.account.as_ref().map(|a|a.msa.is_none()&&a.minecraft_expires<=now()).unwrap_or(false){publish(&mut s,"Minecraft session expired. Refresh it in your launcher or sign in");}s.view.clone()}
pub fn cancel(){let mut s=state().lock().unwrap();s.generation=s.generation.wrapping_add(1);publish(&mut s,"Sign-in cancelled");}
pub fn sign_out(){let mut s=state().lock().unwrap();s.generation=s.generation.wrapping_add(1);s.account=None;s.loaded=true;let removed=cache_path().and_then(|p|match fs::remove_file(p){Ok(())=>Ok(()),Err(e)if e.kind()==std::io::ErrorKind::NotFound=>Ok(()),Err(_)=>Err("Could not remove saved account")});publish(&mut s,removed.err().unwrap_or("Signed out of TCPE"));}
#[derive(Deserialize)]struct Prism {accounts:Vec<PrismAccount>}
#[derive(Deserialize)]struct PrismAccount {#[serde(rename="type")]kind:String,#[serde(default)]active:bool,profile:PrismProfile,#[serde(default)]ygg:PrismToken}
#[derive(Deserialize)]struct PrismProfile {name:String,id:String}
#[derive(Deserialize,Default)]struct PrismToken {#[serde(default)]token:String,#[serde(default)]exp:u64}
fn parse_prism(data:&[u8],timestamp:u64)->Result<Stored,&'static str>{if data.len()>MAX_CACHE{return Err("Launcher account file is too large");}let p:Prism=serde_json::from_slice(data).map_err(|_|"Launcher account file is invalid")?;let a=p.accounts.into_iter().find(|a|a.active&&a.kind=="MSA").ok_or("No active Minecraft account in Prism")?;let s=Stored{username:a.profile.name,uuid:Uuid::parse_str(&a.profile.id).map_err(|_|"Launcher profile is invalid")?,minecraft_token:a.ygg.token,minecraft_expires:a.ygg.exp,msa:None};validate(&s)?;if s.minecraft_expires<=timestamp{return Err("Launcher session expired. Refresh it in Prism first");}Ok(s)}
pub fn import_prism()->Result<(),&'static str>{
 #[cfg(not(target_os="linux"))]return Err("Prism session import is available on Linux only");
 #[cfg(target_os="linux")]{let home=std::env::var_os("HOME").map(PathBuf::from).ok_or("Launcher account storage is unavailable")?;let paths=[home.join(".var/app/org.prismlauncher.PrismLauncher/data/PrismLauncher/accounts.json"),home.join(".local/share/PrismLauncher/accounts.json")];let path=paths.iter().find(|p|p.is_file()).ok_or("No Prism account cache found")?;let info=fs::metadata(path).map_err(|_|"Could not read launcher account")?;if info.len() as usize>MAX_CACHE{return Err("Launcher account file is too large");}let data=fs::read(path).map_err(|_|"Could not read launcher account")?;let account=parse_prism(&data,now())?;save_private(&cache_path()?,&account)?;let mut s=state().lock().unwrap();s.generation=s.generation.wrapping_add(1);s.account=Some(account);s.loaded=true;publish(&mut s,"Existing Minecraft session imported. Choose this profile to use it");Ok(())}
}
fn fail(generation:u64,message:&str){let mut s=state().lock().unwrap();if s.generation!=generation{return;}publish(&mut s,message);s.view.phase=3;}
fn current(generation:u64)->bool{state().lock().unwrap().generation==generation}
fn client()->Result<reqwest::Client,&'static str>{azalea_auth::http_client::builder().timeout(std::time::Duration::from_secs(30)).build().map_err(|_|"Could not start secure sign-in")}
// Device-code errors are handled explicitly; never log OAuth responses or tokens.
fn poll_action(error:&str,interval:&mut u64)->Result<bool,&'static str>{match error{"authorization_pending"=>Ok(true),"slow_down"=>{*interval=interval.saturating_add(5);Ok(true)},"authorization_declined"|"access_denied"=>Err("Microsoft sign-in was declined"),"expired_token"=>Err("Sign-in code expired. Start sign-in again"),_=>Err("Microsoft sign-in failed. Start sign-in again")}}
async fn device_login(generation:u64)->Result<Stored,&'static str>{
 let client=client()?;let code=azalea_auth::get_ms_link_code(&client,None,None).await.map_err(|_|"Could not get Microsoft sign-in code")?;
 if code.user_code.is_empty()||code.user_code.len()>32||!(code.verification_uri.starts_with("https://www.microsoft.com/")||code.verification_uri.starts_with("https://microsoft.com/")||code.verification_uri.starts_with("https://login.live.com/")||code.verification_uri.starts_with("https://aka.ms/")){return Err("Microsoft returned an invalid sign-in page");}
 let deadline=now().saturating_add(code.expires_in.min(1800));let mut interval=code.interval.clamp(1,60);
 {let mut s=state().lock().unwrap();if s.generation!=generation{return Err("Sign-in cancelled");}s.view.phase=1;s.view.user_code=code.user_code.clone();s.view.verification_uri=code.verification_uri.clone();s.view.expires_at=deadline;s.view.message="Open the Microsoft page and enter this code".into();s.view.revision=s.view.revision.wrapping_add(1);}
 let msa=loop{if !current(generation){return Err("Sign-in cancelled");}if now()>=deadline{return Err("Sign-in code expired. Start sign-in again");}for _ in 0..interval{tokio::time::sleep(std::time::Duration::from_secs(1)).await;if !current(generation){return Err("Sign-in cancelled");}if now()>=deadline{return Err("Sign-in code expired. Start sign-in again");}}
  let response:serde_json::Value=client.post("https://login.live.com/oauth20_token.srf").form(&[("client_id","00000000441cc96b"),("device_code",code.device_code.as_str()),("grant_type","urn:ietf:params:oauth:grant-type:device_code")]).send().await.map_err(|_|"Could not contact Microsoft sign-in")?.json().await.map_err(|_|"Microsoft sign-in response was invalid")?;
  if let Some(error)=response.get("error").and_then(|v|v.as_str()){poll_action(error,&mut interval)?;continue;}
  let token:AccessTokenResponse=serde_json::from_value(response).map_err(|_|"Microsoft sign-in response was invalid")?;let expires_at=now().saturating_add(token.expires_in);break ExpiringValue{data:token,expires_at};
 };
 if !current(generation){return Err("Sign-in cancelled");}
 let minecraft=azalea_auth::get_minecraft_token(&client,&msa.data.access_token).await.map_err(|_|"Microsoft account could not access Minecraft")?;
 if !current(generation){return Err("Sign-in cancelled");}
 let profile=azalea_auth::get_profile(&client,&minecraft.minecraft_access_token).await.map_err(|_|"This account has no playable Java Edition profile")?;
 let s=Stored{username:profile.name,uuid:profile.id,minecraft_token:minecraft.minecraft_access_token,minecraft_expires:minecraft.mca.expires_at,msa:Some(msa)};validate(&s)?;Ok(s)
}
pub fn begin_sign_in(){let generation={let mut s=state().lock().unwrap();ensure_loaded(&mut s);s.generation=s.generation.wrapping_add(1);s.view.phase=1;s.view.message="Requesting Microsoft sign-in code...".into();s.view.user_code.clear();s.view.verification_uri.clear();s.view.revision=s.view.revision.wrapping_add(1);s.generation};std::thread::spawn(move||{let runtime=match tokio::runtime::Builder::new_current_thread().enable_all().build(){Ok(r)=>r,Err(_)=>{fail(generation,"Could not start sign-in");return;}};match runtime.block_on(device_login(generation)){Ok(account)=>{let mut s=state().lock().unwrap();if s.generation!=generation{return;}let saved=cache_path().and_then(|p|save_private(&p,&account));s.account=Some(account);s.loaded=true;publish(&mut s,if saved.is_ok(){"Signed in. Choose this Minecraft profile to use it"}else{"Signed in for this session. Private account storage could not be saved"});},Err(message)=>fail(generation,message)}});}
fn refresh_target<'a>(s:&'a mut State,generation:u64,uuid:Uuid,username:&str)->Result<&'a mut Stored,&'static str>{
 if s.generation!=generation{return Err("Account changed. Choose your profile again in Settings");}
 s.account.as_mut().filter(|a|a.uuid==uuid&&a.username==username).ok_or("Account changed. Choose your profile again in Settings")
}
pub async fn account(expected_name:&str)->Result<Account,&'static str>{
 let (generation,username,uuid,mut token,expires,msa)={let mut s=state().lock().unwrap();ensure_loaded(&mut s);let a=s.account.as_ref().ok_or("No Minecraft account selected. Sign in in Settings")?;if a.username!=expected_name{return Err("Account changed. Choose the Minecraft profile again in Settings");}(s.generation,a.username.clone(),a.uuid,a.minecraft_token.clone(),a.minecraft_expires,a.msa.clone())};
 if expires<=now(){if let Some(msa)=msa{let client=client()?;let refreshed=if msa.expires_at<=now(){azalea_auth::refresh_ms_auth_token(&client,&msa.data.refresh_token,None,None).await.map_err(|_|"Minecraft session expired. Sign in again in Settings")?}else{msa};let minecraft=azalea_auth::get_minecraft_token(&client,&refreshed.data.access_token).await.map_err(|_|"Minecraft session expired. Sign in again in Settings")?;let profile=azalea_auth::get_profile(&client,&minecraft.minecraft_access_token).await.map_err(|_|"Could not validate Minecraft profile")?;if profile.id!=uuid||profile.name!=username{return Err("Minecraft profile changed. Choose it again in Settings");}token=minecraft.minecraft_access_token.clone();let mut s=state().lock().unwrap();let a=refresh_target(&mut s,generation,uuid,&username)?;a.minecraft_token=minecraft.minecraft_access_token;a.minecraft_expires=minecraft.mca.expires_at;a.msa=Some(refreshed);let _=cache_path().and_then(|p|save_private(&p,a));}else{return Err("Existing Minecraft session expired. Refresh it in your launcher or sign in in Settings");}}
 if !current(generation){return Err("Account changed. Choose your profile again in Settings");}
 let mut account=Account::offline(&username);account.uuid=Some(uuid);account.access_token=Some(Arc::new(parking_lot::Mutex::new(token)));Ok(account)
}
#[cfg(test)]mod tests{use super::*;#[test]fn device_poll_errors_are_bounded(){let mut interval=5;assert!(poll_action("authorization_pending",&mut interval).unwrap());assert!(poll_action("slow_down",&mut interval).unwrap());assert_eq!(interval,10);assert!(poll_action("access_denied",&mut interval).is_err());assert!(poll_action("expired_token",&mut interval).is_err());}
#[test]fn launcher_import_uses_only_minecraft_token_and_exact_profile(){let data=br#"{"accounts":[{"type":"MSA","active":true,"profile":{"name":"Tester","id":"00112233-4455-6677-8899-aabbccddeeff"},"msa":{"token":"never-import-this"},"ygg":{"token":"minecraft-only-test","exp":200}}]}"#;let a=parse_prism(data,100).unwrap();assert_eq!(a.minecraft_token,"minecraft-only-test");assert!(a.msa.is_none());assert!(parse_prism(data,200).is_err());}
#[test]fn account_debug_redacts_token(){let mut a=Account::offline("Tester");a.access_token=Some(Arc::new(parking_lot::Mutex::new("test-private-token".into())));assert!(!format!("{a:?}").contains("test-private-token"));}
#[test]fn public_view_never_serializes_credentials(){let v=View{username:"Tester".into(),..Default::default()};let text=serde_json::to_string(&v).unwrap();assert!(!text.contains("token")&&!text.contains("uuid"));}
#[test]fn cache_permissions_and_atomic_replacement(){let path=std::env::temp_dir().join(format!("tcpe-auth-test-{}",std::process::id())).join("account.json");let a=Stored{username:"Tester".into(),uuid:Uuid::from_u128(7),minecraft_token:"test-private-token".into(),minecraft_expires:now()+100,msa:None};save_private(&path,&a).unwrap();assert_eq!(read_private(&path).unwrap().username,"Tester");save_private(&path,&a).unwrap();#[cfg(unix)]{use std::os::unix::fs::PermissionsExt;assert_eq!(fs::metadata(&path).unwrap().permissions().mode()&0o777,0o600);}fs::remove_dir_all(path.parent().unwrap()).unwrap();}}

#[cfg(test)]mod refresh_race_tests {use super::*;
 #[test]fn same_profile_new_generation_rejects_stale_refresh_before_mutation(){
  let uuid=Uuid::from_u128(7);let mut s=State{view:View::default(),generation:2,loaded:true,account:Some(Stored{username:"Tester".into(),uuid,minecraft_token:"new-session-token".into(),minecraft_expires:900,msa:None})};
  if let Ok(a)=refresh_target(&mut s,1,uuid,"Tester"){a.minecraft_token="stale-session-token".into();}
  assert_eq!(s.account.as_ref().unwrap().minecraft_token,"new-session-token");
  assert!(refresh_target(&mut s,2,Uuid::from_u128(8),"Tester").is_err());
  refresh_target(&mut s,2,uuid,"Tester").unwrap().minecraft_token="current-refresh".into();
  assert_eq!(s.account.as_ref().unwrap().minecraft_token,"current-refresh");s.account=None;assert!(refresh_target(&mut s,2,uuid,"Tester").is_err());
 }
}
