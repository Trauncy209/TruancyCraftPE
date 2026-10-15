//! Background pack download/decoding. Renderer activation remains connection-scoped.
use super::{
    resource_pack_fonts::{decode_fonts_with_base, read_pack, PackGlyph},
    resource_packs::PackRequest,
};
use base64::{engine::general_purpose::STANDARD, Engine};
#[derive(Debug, serde::Serialize)]
pub struct PackFontTexture {
    pub width: u32,
    pub height: u32,
    pub rgba_base64: String,
}
#[derive(Debug, serde::Serialize)]
pub struct NamedPackFont {
    pub name: String,
    pub glyphs: Vec<PackGlyph>,
}
#[derive(Debug, serde::Serialize)]
pub struct TextureAnimationFrame {
    pub rgba_base64: String,
    pub time: u32,
}
#[derive(Debug, serde::Serialize)]
pub struct TextureAnimation {
    pub texture: i32,
    pub frames: Vec<TextureAnimationFrame>,
    pub interpolate: bool,
}
#[derive(Debug, serde::Serialize)]
pub struct DownloadedPackFonts {
    pub connection: u64,
    pub generation: u64,
    pub id: String,
    pub fonts: Vec<NamedPackFont>,
    pub textures: Vec<PackFontTexture>,
    pub models: Vec<super::resource_pack_models::PackItemModel>,
    pub model_errors: Vec<String>,
    pub animations: Vec<TextureAnimation>,
    pub sounds:super::resource_pack_sounds::PackSounds,
}
fn checked_url(raw: &str) -> Result<url::Url, String> {
    let url = url::Url::parse(raw).map_err(|_| "Invalid resource pack URL")?;
    if !matches!(url.scheme(), "http" | "https")
        || url.host_str().is_none()
        || !url.username().is_empty()
        || url.password().is_some()
    {
        return Err("Unsupported resource pack URL".into());
    }
    Ok(url)
}
#[derive(Debug)]
struct FetchFailure {
    message: String,
    retryable: bool,
}
impl FetchFailure {
    fn fatal(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
            retryable: false,
        }
    }
    fn transport(error: &reqwest::Error, message: &str) -> Self {
        // Error strings can contain signed URLs. Only expose our static message.
        Self {
            message: message.into(),
            retryable: error.is_timeout()
                || error.is_connect()
                || error.is_body()
                || (error.is_request() && !error.is_builder() && !error.is_redirect()),
        }
    }
}
async fn retry_fetch<F, Fut>(
    mut fetch: F,
    backoff: [std::time::Duration; 2],
) -> Result<Vec<u8>, String>
where
    F: FnMut() -> Fut,
    Fut: std::future::Future<Output = Result<Vec<u8>, FetchFailure>>,
{
    for attempt in 0..3 {
        match fetch().await {
            Ok(bytes) => return Ok(bytes),
            Err(error) if error.retryable && attempt < 2 => {
                tokio::time::sleep(backoff[attempt]).await
            }
            Err(error) => return Err(error.message),
        }
    }
    unreachable!()
}
async fn fetch_bytes(client: &reqwest::Client, url: &url::Url) -> Result<Vec<u8>, FetchFailure> {
    let mut response = client
        .get(url.clone())
        .send()
        .await
        .map_err(|e| FetchFailure::transport(&e, "Resource pack download failed"))?;
    if !response.status().is_success() {
        return Err(FetchFailure::fatal(format!(
            "Resource pack HTTP status {}",
            response.status()
        )));
    }
    const LIMIT: usize = 256 * 1024 * 1024;
    if response.content_length().is_some_and(|n| n > LIMIT as u64) {
        return Err(FetchFailure::fatal("Resource pack exceeds download limit"));
    }
    let mut bytes = Vec::new();
    while let Some(chunk) = response.chunk().await.map_err(|e| {
        let mut failure = FetchFailure::transport(&e, "Resource pack download interrupted");
        // Reqwest classifies premature EOF while streaming raw bytes as Decode.
        // This request enables no content decompression or JSON/body deserialization.
        failure.retryable |= e.is_decode();
        failure
    })? {
        if bytes
            .len()
            .checked_add(chunk.len())
            .is_none_or(|n| n > LIMIT)
        {
            return Err(FetchFailure::fatal("Resource pack exceeds download limit"));
        }
        bytes.extend_from_slice(&chunk);
    }
    Ok(bytes)
}
pub async fn download_fonts(
    request: PackRequest,
    connection: u64,
    generation: u64,
) -> Result<DownloadedPackFonts, String> {
    let url = checked_url(&request.url)?;
    let roots = rustls::RootCertStore::from_iter(webpki_roots::TLS_SERVER_ROOTS.iter().cloned());
    let tls = rustls::ClientConfig::builder_with_provider(std::sync::Arc::new(
        rustls::crypto::aws_lc_rs::default_provider(),
    ))
    .with_safe_default_protocol_versions()
    .map_err(|_| "Resource pack TLS initialization failed")?
    .with_root_certificates(roots)
    .with_no_client_auth();
    let client = reqwest::Client::builder()
        .tls_backend_preconfigured(tls)
        .user_agent("TCPE/0.5.22.1")
        .connect_timeout(std::time::Duration::from_secs(10))
        .timeout(std::time::Duration::from_secs(60))
        .redirect(reqwest::redirect::Policy::limited(5))
        .build()
        .map_err(|_| "Resource pack HTTP initialization failed")?;
    // Restart the bounded transfer from byte zero; decode/hash validation happens once.
    let bytes = retry_fetch(
        || fetch_bytes(&client, &url),
        [
            std::time::Duration::from_millis(250),
            std::time::Duration::from_millis(750),
        ],
    )
    .await?;
    tokio::task::spawn_blocking(move || {
        let files = read_pack(&bytes, &request.hash)?;
        let decoded = decode_fonts_with_base(&files, &|path| {
            (path == "assets/minecraft/textures/font/ascii.png").then(|| {
                include_bytes!("../../../java-assets/assets/minecraft/textures/font/ascii.png")
                    .to_vec()
            })
        })?;
        let models = super::resource_pack_models::decode_models(&files, decoded.textures.len())?;
        let mut textures = decoded.textures;
        textures.extend(models.textures);
        let animation_bytes = models
            .animations
            .iter()
            .flat_map(|a| &a.frames)
            .map(|f| f.rgba.len())
            .sum::<usize>();
        if textures
            .iter()
            .map(|t| t.rgba.len())
            .sum::<usize>()
            .checked_add(animation_bytes)
            .is_none_or(|n| n > 192 * 1024 * 1024)
        {
            return Err("Pack textures and animation payload exceed memory limit".into());
        }
        let animations = models
            .animations
            .into_iter()
            .map(|a| TextureAnimation {
                texture: a.texture,
                interpolate: a.interpolate,
                frames: a
                    .frames
                    .into_iter()
                    .map(|f| TextureAnimationFrame {
                        rgba_base64: STANDARD.encode(f.rgba),
                        time: f.time,
                    })
                    .collect(),
            })
            .collect();
        let sounds=super::resource_pack_sounds::decode(&files.0).unwrap_or_else(|error|super::resource_pack_sounds::PackSounds{errors:vec![error],..Default::default()});
        Ok(DownloadedPackFonts {
            connection,
            generation,
            id: request.id,
            fonts: decoded
                .fonts
                .into_iter()
                .map(|(name, glyphs)| NamedPackFont {
                    name,
                    glyphs: glyphs.into_values().collect(),
                })
                .collect(),
            models: models.models,
            model_errors: models.unsupported,
            animations,
            sounds,
            textures: textures
                .into_iter()
                .map(|texture| PackFontTexture {
                    width: texture.width,
                    height: texture.height,
                    rgba_base64: STANDARD.encode(texture.rgba),
                })
                .collect(),
        })
    })
    .await
    .map_err(|_| "Resource pack decoder task failed")?
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_non_http_and_embedded_credentials() {
        for raw in [
            "file:///tmp/pack.zip",
            "https://user:secret@example.com/p.zip",
            "data:application/zip,x",
        ] {
            assert!(checked_url(raw).is_err());
        }
        assert!(checked_url("https://example.com/pack.zip?token=test").is_ok());
    }
    #[tokio::test]
    async fn transport_retry_is_bounded_and_success_returns_only_complete_attempt() {
        let mut attempts = 0;
        let result = retry_fetch(
            || {
                attempts += 1;
                std::future::ready(if attempts < 3 {
                    Err(FetchFailure {
                        message: "Resource pack download interrupted".into(),
                        retryable: true,
                    })
                } else {
                    Ok(vec![1, 2, 3])
                })
            },
            [std::time::Duration::ZERO; 2],
        )
        .await
        .unwrap();
        assert_eq!(attempts, 3);
        assert_eq!(result, [1, 2, 3]);
        attempts = 0;
        let error = retry_fetch(
            || {
                attempts += 1;
                std::future::ready(Err(FetchFailure {
                    message: "Resource pack download interrupted".into(),
                    retryable: true,
                }))
            },
            [std::time::Duration::ZERO; 2],
        )
        .await
        .unwrap_err();
        assert_eq!(attempts, 3);
        assert_eq!(error, "Resource pack download interrupted");
    }
    #[tokio::test]
    async fn fatal_http_and_limit_failures_do_not_retry() {
        for message in [
            "Resource pack HTTP status 403 Forbidden",
            "Resource pack HTTP status 500 Internal Server Error",
            "Resource pack exceeds download limit",
        ] {
            let mut attempts = 0;
            let error = retry_fetch(
                || {
                    attempts += 1;
                    std::future::ready(Err(FetchFailure::fatal(message)))
                },
                [std::time::Duration::ZERO; 2],
            )
            .await
            .unwrap_err();
            assert_eq!(attempts, 1);
            assert_eq!(error, message);
        }
    }
    #[tokio::test]
    async fn interrupted_http_bodies_restart_without_partial_bytes() {
        use std::io::{Read, Write};
        let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let server = std::thread::spawn(move || {
            for attempt in 0..3 {
                let (mut socket, _) = listener.accept().unwrap();
                socket
                    .set_read_timeout(Some(std::time::Duration::from_secs(5)))
                    .unwrap();
                let mut request = [0; 2048];
                socket.read(&mut request).unwrap();
                socket
                    .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 8\r\nConnection: close\r\n\r\n")
                    .unwrap();
                socket
                    .write_all(if attempt < 2 { b"bad" } else { b"complete" })
                    .unwrap();
            }
        });
        let client = reqwest::Client::builder()
            .timeout(std::time::Duration::from_secs(5))
            .build()
            .unwrap();
        let url = url::Url::parse(&format!("http://{address}/pack?token=private-test")).unwrap();
        let bytes = retry_fetch(
            || fetch_bytes(&client, &url),
            [std::time::Duration::ZERO; 2],
        )
        .await
        .unwrap();
        assert_eq!(bytes, b"complete");
        server.join().unwrap();
    }
}
