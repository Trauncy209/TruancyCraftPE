//! Explicit opt-in network/security checks. No account, token or login requests.
use std::{process::{Command,Stdio},time::Duration};
#[tokio::test]
#[ignore = "explicit public HTTPS transport probe; no authentication"]
async fn bundled_roots_validate_public_minecraft_service() {
    let client=azalea_auth::http_client::builder().no_proxy().timeout(Duration::from_secs(20)).build().unwrap();
    let response=client.get("https://api.minecraftservices.com/publickeys").send().await.unwrap();
    assert!(response.status().is_success());
}
#[tokio::test]
#[ignore = "explicit local TLS rejection probe requires openssl"]
async fn bundled_roots_reject_untrusted_local_certificate() {
    let directory=std::env::temp_dir().join(format!("tcpe-tls-probe-{}",uuid::Uuid::new_v4()));
    std::fs::create_dir(&directory).unwrap();
    let status=Command::new("openssl").args(["req","-x509","-newkey","rsa:2048","-nodes","-days","1","-subj","/CN=localhost","-addext","subjectAltName=DNS:localhost","-addext","basicConstraints=critical,CA:FALSE","-addext","keyUsage=digitalSignature,keyEncipherment","-addext","extendedKeyUsage=serverAuth","-keyout"])
      .arg(directory.join("key.pem")).arg("-out").arg(directory.join("certificate.pem")).stdout(Stdio::null()).stderr(Stdio::null()).status().unwrap();assert!(status.success());
    let reserve=std::net::TcpListener::bind("127.0.0.1:0").unwrap();let port=reserve.local_addr().unwrap().port();drop(reserve);
    let mut server=Command::new("openssl").args(["s_server","-quiet","-www","-accept"]).arg(format!("127.0.0.1:{port}")).arg("-cert").arg(directory.join("certificate.pem")).arg("-key").arg(directory.join("key.pem")).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null()).spawn().unwrap();
    let ready=(0..100).any(|_|{if std::net::TcpStream::connect(("127.0.0.1",port)).is_ok(){true}else{std::thread::sleep(Duration::from_millis(20));false}});
    let result=if ready{let client=azalea_auth::http_client::builder().no_proxy().timeout(Duration::from_secs(5)).build().unwrap();Some(client.get(format!("https://localhost:{port}/")).send().await)}else{None};
    let _=server.kill();let _=server.wait();std::fs::remove_dir_all(&directory).unwrap();
    assert!(ready,"local TLS test endpoint did not start");let error=result.unwrap().unwrap_err();let detail=format!("{error:?}");assert!(detail.contains("UnknownIssuer")||detail.contains("unknown issuer"),"unexpected failure: {detail}");
}
