//! Consistent verified HTTPS without Android platform-verifier JNI setup.
//! Uses the same Mozilla trust anchors as TCPE skin and resource-pack clients.
pub fn builder() -> reqwest::ClientBuilder {
    let roots = rustls::RootCertStore::from_iter(webpki_roots::TLS_SERVER_ROOTS.iter().cloned());
    let tls = rustls::ClientConfig::builder_with_provider(std::sync::Arc::new(
        rustls::crypto::aws_lc_rs::default_provider(),
    ))
    .with_safe_default_protocol_versions()
    .expect("the bundled TLS provider supports its default protocol versions")
    .with_root_certificates(roots)
    .with_no_client_auth();
    reqwest::Client::builder().tls_backend_preconfigured(tls)
}
