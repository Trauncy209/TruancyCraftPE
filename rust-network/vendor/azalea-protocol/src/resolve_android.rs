//! Bionic device DNS: API29+ netd resolver, legacy libc fallback for API21-28.
use super::ResolveError;
use std::{ffi::{CString,c_char,c_int,c_uint,c_short,c_void},sync::LazyLock};
type Query=unsafe extern "C" fn(u64,*const c_char,c_int,c_int,u32)->c_int;
type ResultFn=unsafe extern "C" fn(c_int,*mut c_int,*mut u8,usize)->c_int;
type Cancel=unsafe extern "C" fn(c_int);
#[derive(Clone,Copy)]struct Api{query:Query,result:ResultFn,cancel:Cancel}
#[repr(C)]struct PollFd{fd:c_int,events:c_short,revents:c_short}
#[link(name="dl")]
unsafe extern "C"{fn dlopen(name:*const c_char,flags:c_int)->*mut c_void;fn dlsym(handle:*mut c_void,name:*const c_char)->*mut c_void;}
unsafe extern "C"{fn poll(fds:*mut PollFd,count:c_uint,timeout:c_int)->c_int;fn res_query(name:*const c_char,class:c_int,kind:c_int,answer:*mut u8,length:c_int)->c_int;}
static API:LazyLock<Option<Api>>=LazyLock::new(||unsafe{
 // Keep the library open for the lifetime of these function pointers. No API29
 // symbol is a hard ELF import, preserving the declared API21 minimum.
 let handle=dlopen(c"libandroid.so".as_ptr(),2);if handle.is_null(){return None;}
 let query=dlsym(handle,c"android_res_nquery".as_ptr());let result=dlsym(handle,c"android_res_nresult".as_ptr());let cancel=dlsym(handle,c"android_res_cancel".as_ptr());
 if query.is_null()||result.is_null()||cancel.is_null(){return None;}
 Some(Api{query:std::mem::transmute::<*mut c_void,Query>(query),result:std::mem::transmute::<*mut c_void,ResultFn>(result),cancel:std::mem::transmute::<*mut c_void,Cancel>(cancel)})
});
pub(super) fn query_srv(name:&str)->Result<Vec<u8>,ResolveError>{
 let name=CString::new(name).map_err(|_|ResolveError::from("Invalid Android DNS name"))?;let mut bytes=vec![0u8;65535];
 let length=if let Some(api)=*API{unsafe{
  // NETWORK_UNSPECIFIED=0 asks Android to choose the app's actual device/VPN
  // network, including its Private DNS policy. No public resolver is injected.
  let fd=(api.query)(0,name.as_ptr(),1,33,0);if fd<0{return Err("Android system SRV query failed".into());}
  let mut ready=PollFd{fd,events:1,revents:0};let polled=poll(&mut ready,1,3000);
  if polled<=0||(ready.revents&1)==0{(api.cancel)(fd);return Err("Android system SRV query timed out".into());}
  let mut rcode=0;let length=(api.result)(fd,&mut rcode,bytes.as_mut_ptr(),bytes.len()); // consumes fd
  if length<0||rcode!=0{return Err("Android system has no usable SRV answer".into());}length
 }}else{unsafe{res_query(name.as_ptr(),1,33,bytes.as_mut_ptr(),bytes.len()as c_int)}};
 if length<0||length as usize>bytes.len(){return Err("Android system SRV response failed".into());}
 bytes.truncate(length as usize);Ok(bytes)
}
