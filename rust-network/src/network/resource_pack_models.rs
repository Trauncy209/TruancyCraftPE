//! Connection-scoped Java item model meshes. No server assets are written to disk.
use super::resource_pack_fonts::{FontTexture, PackFiles};
use serde_json::Value;
use std::{collections::BTreeMap, io::Cursor};
#[derive(Debug, serde::Serialize)]
pub struct PackItemQuad {
    pub texture: i32,
    pub vertices: [[f32; 5]; 4],
    pub tint: i32,
}
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct GuiTransform {
    pub rotation: [f32; 3],
    pub translation: [f32; 3],
    pub scale: [f32; 3],
}
impl Default for GuiTransform {
    fn default() -> Self {
        Self {
            rotation: [0.; 3],
            translation: [0.; 3],
            scale: [1.; 3],
        }
    }
}
#[derive(Debug, Clone, PartialEq, serde::Serialize)]
pub struct PackItemTint {
    pub kind: String,
    pub color: i32,
    pub index: u32,
    pub default_color: i32,
}
#[derive(Debug, serde::Serialize)]
pub struct PackDisplayTransform {
    pub context: String,
    pub transform: GuiTransform,
}
#[derive(Debug, serde::Serialize)]
pub struct PackItemModel {
    pub id: String,
    pub quads: Vec<PackItemQuad>,
    pub gui: GuiTransform,
    pub display: Vec<PackDisplayTransform>,
    pub tints: Vec<PackItemTint>,
}
#[derive(Debug)]
pub struct ModelAnimationFrame {
    pub rgba: Vec<u8>,
    pub time: u32,
}
#[derive(Debug)]
pub struct ModelAnimation {
    pub texture: i32,
    pub frames: Vec<ModelAnimationFrame>,
    pub interpolate: bool,
}
pub struct DecodedModels {
    pub models: Vec<PackItemModel>,
    pub textures: Vec<FontTexture>,
    pub unsupported: Vec<String>,
    pub animations: Vec<ModelAnimation>,
}
// Vanilla tint colors are made opaque; store RGB separately from texture alpha.
fn tint_color(value: Option<&Value>) -> Result<i32, String> {
    let value = value.ok_or("Missing tint color")?;
    if let Some(color) = value.as_i64() {
        if (i32::MIN as i64..=u32::MAX as i64).contains(&color) {
            return Ok((color as u32 & 0xffffff) as i32);
        }
    }
    Err("Invalid tint color".into())
}
fn decode_tints(value: Option<&Value>) -> Result<(Vec<PackItemTint>, Vec<String>), String> {
    let Some(value) = value else {
        return Ok((Vec::new(), Vec::new()));
    };
    let definitions = value
        .as_array()
        .filter(|v| v.len() <= 256)
        .ok_or("Invalid item tint list")?;
    let mut tints = Vec::new();
    let mut errors = Vec::new();
    for (position, definition) in definitions.iter().enumerate() {
        let source = definition.get("type").and_then(Value::as_str).unwrap_or("");
        let decoded = match source {
            "minecraft:constant" => tint_color(definition.get("value")).map(|color| PackItemTint {
                kind: "constant".into(),
                color,
                index: 0,
                default_color: color,
            }),
            "minecraft:custom_model_data" => {
                let index = match definition.get("index") {
                    None => Ok(0),
                    Some(v) => v
                        .as_u64()
                        .filter(|v| *v <= 1023)
                        .map(|v| v as u32)
                        .ok_or("Invalid custom-model-data tint index".to_string()),
                };
                index.and_then(|index| {
                    tint_color(definition.get("default")).map(|default_color| PackItemTint {
                        kind: "custom_model_data".into(),
                        color: default_color,
                        index,
                        default_color,
                    })
                })
            }
            "minecraft:dye"|"minecraft:potion" => tint_color(definition.get("default")).map(|default_color|PackItemTint{kind:source.strip_prefix("minecraft:").unwrap().into(),color:default_color,index:0,default_color}),
            _ => Err(format!("Unsupported tint source {source}")),
        };
        match decoded {
            Ok(tint) => tints.push(tint),
            Err(reason) => {
                errors.push(format!("tint[{position}]: {reason}"));
                // Preserve the tint slot; never shift subsequent face indices.
                let color = tint_color(definition.get("default")).unwrap_or(0xffffff);
                tints.push(PackItemTint {
                    kind: "unsupported".into(),
                    color,
                    index: 0,
                    default_color: color,
                });
            }
        }
    }
    Ok((tints, errors))
}
fn identifier(id: &str) -> Result<(String, String), String> {
    let (ns, path) = id.split_once(':').unwrap_or(("minecraft", id));
    if ns.is_empty()
        || path.is_empty()
        || id.len() > 256
        || !ns
            .bytes()
            .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || b"_.-".contains(&c))
        || !path
            .bytes()
            .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || b"/_.-".contains(&c))
        || path.split('/').any(|v| v == "..")
    {
        return Err("Invalid model asset identifier".into());
    }
    Ok((ns.into(), path.into()))
}
fn asset(id: &str, folder: &str, suffix: &str) -> Result<String, String> {
    let (ns, path) = identifier(id)?;
    Ok(format!("assets/{ns}/{folder}/{path}{suffix}"))
}
fn json(pack: &PackFiles, path: &str) -> Result<Value, String> {
    let bytes = pack
        .0
        .get(path)
        .ok_or_else(|| format!("Missing model asset {path}"))?;
    if bytes.len() > 2 * 1024 * 1024 {
        return Err("Model JSON exceeds size limit".into());
    }
    serde_json::from_slice(bytes).map_err(|_| "Invalid model JSON".into())
}
fn vec3(v: Option<&Value>, default: [f32; 3], bound: f64) -> Result<[f32; 3], String> {
    let Some(v) = v else { return Ok(default) };
    let a = v
        .as_array()
        .filter(|a| a.len() == 3)
        .ok_or("Invalid model vector")?;
    let mut out = [0.; 3];
    for (i, v) in a.iter().enumerate() {
        let n = v
            .as_f64()
            .filter(|n| n.is_finite() && n.abs() <= bound)
            .ok_or("Invalid model vector value")?;
        out[i] = n as f32;
    }
    Ok(out)
}
fn inherited(pack: &PackFiles, id: &str, depth: usize) -> Result<Value, String> {
    if depth > 32 {
        return Err("Model parent recursion limit".into());
    }
    if matches!(
        id,
        "item/generated"
            | "minecraft:item/generated"
            | "builtin/generated"
            | "minecraft:builtin/generated"
    ) {
        return Ok(serde_json::json!({"generated":true}));
    }
    let mut child = json(pack, &asset(id, "models", ".json")?)?;
    if !child.is_object() {
        return Err("Model must be an object".into());
    }
    if let Some(parent) = child.get("parent").and_then(Value::as_str) {
        let mut base = inherited(pack, parent, depth + 1)?;
        let b = base.as_object_mut().ok_or("Invalid parent model")?;
        for (k, v) in child.as_object().unwrap() {
            if matches!(k.as_str(), "textures" | "display") {
                let current = b.entry(k.clone()).or_insert_with(|| serde_json::json!({}));
                let entries = v.as_object().ok_or("Invalid model mapping")?;
                let dest = current.as_object_mut().ok_or("Invalid inherited mapping")?;
                for (k, v) in entries {
                    dest.insert(k.clone(), v.clone());
                }
            } else if k != "parent" {
                b.insert(k.clone(), v.clone());
            }
        }
        child = base;
    }
    Ok(child)
}
fn texture_ref(value: &str, textures: &Value) -> Result<String, String> {
    let mut value = value.to_owned();
    for _ in 0..32 {
        if let Some(key) = value.strip_prefix('#').or_else(||textures.get(&value).map(|_|value.as_str())) {
            value = textures
                .get(key)
                .and_then(Value::as_str)
                .ok_or("Unresolved model texture variable")?
                .to_owned();
        } else {
            return asset(&value, "textures", ".png");
        }
    }
    Err("Texture variable recursion limit".into())
}
fn png_texture(pack: &PackFiles, path: &str) -> Result<FontTexture, String> {
    let bytes = pack
        .0
        .get(path)
        .ok_or_else(|| format!("Missing model texture {path}"))?;
    let mut decoder = png::Decoder::new(Cursor::new(bytes));
    decoder.set_transformations(png::Transformations::EXPAND | png::Transformations::STRIP_16);
    let mut reader = decoder
        .read_info()
        .map_err(|_| "Invalid model texture PNG")?;
    let info = reader.info();
    if info.width == 0 || info.height == 0 || info.width > 4096 || info.height > 4096 {
        return Err("Model texture dimensions exceed limit".into());
    }
    let mut data = vec![
        0;
        reader
            .output_buffer_size()
            .ok_or("Invalid PNG buffer size")?
    ];
    let output = reader
        .next_frame(&mut data)
        .map_err(|_| "Model PNG decode failed")?;
    let mut rgba = Vec::with_capacity(output.width as usize * output.height as usize * 4);
    let data = &data[..output.buffer_size()];
    match output.color_type {
        png::ColorType::Rgba => rgba.extend_from_slice(data),
        png::ColorType::Rgb => {
            for p in data.chunks_exact(3) {
                rgba.extend_from_slice(&[p[0], p[1], p[2], 255]);
            }
        }
        png::ColorType::Grayscale => {
            for p in data {
                rgba.extend_from_slice(&[*p, *p, *p, 255]);
            }
        }
        png::ColorType::GrayscaleAlpha => {
            for p in data.chunks_exact(2) {
                rgba.extend_from_slice(&[p[0], p[0], p[0], p[1]]);
            }
        }
        _ => return Err("Unsupported model PNG color type".into()),
    }
    Ok(FontTexture {
        path: path.into(),
        width: output.width,
        height: output.height,
        rgba,
    })
}
fn animation_frames(
    pack: &PackFiles,
    path: &str,
    sheet: &FontTexture,
    texture: i32,
) -> Result<Option<(FontTexture, ModelAnimation)>, String> {
    let Some(bytes) = pack.0.get(&format!("{path}.mcmeta")) else {
        return Ok(None);
    };
    if bytes.len() > 1024 * 1024 {
        return Err("Texture animation metadata exceeds limit".into());
    }
    let metadata: Value =
        serde_json::from_slice(bytes).map_err(|_| "Invalid texture animation metadata")?;
    let Some(animation) = metadata.get("animation") else {
        return Ok(None);
    };
    if !animation.is_object() {
        return Err("Invalid animation object".into());
    }
    let positive = |value: Option<&Value>, default: u32| -> Result<u32, String> {
        match value {
            None => Ok(default),
            Some(v) => v
                .as_u64()
                .filter(|n| *n > 0 && *n <= 1_000_000)
                .map(|n| n as u32)
                .ok_or("Invalid animation dimension/duration".into()),
        }
    };
    let (width, height) = match (animation.get("width"), animation.get("height")) {
        (None, None) => {
            let side = sheet.width.min(sheet.height);
            (side, side)
        }
        (Some(w), None) => (positive(Some(w), 0)?, sheet.height),
        (None, Some(h)) => (sheet.width, positive(Some(h), 0)?),
        (Some(w), Some(h)) => (positive(Some(w), 0)?, positive(Some(h), 0)?),
    };
    if width > sheet.width
        || height > sheet.height
        || sheet.width % width != 0
        || sheet.height % height != 0
    {
        return Err("Animation frame grid does not divide texture sheet".into());
    }
    let columns = sheet.width / width;
    let count = columns * (sheet.height / height);
    let default_time = positive(animation.get("frametime"), 1)?;
    let interpolate = match animation.get("interpolate") {
        None => false,
        Some(v) => v.as_bool().ok_or("Invalid animation interpolation flag")?,
    };
    let mut sequence = Vec::new();
    if let Some(frames) = animation.get("frames") {
        let frames = frames
            .as_array()
            .filter(|a| !a.is_empty() && a.len() <= 1024)
            .ok_or("Invalid animation frame sequence")?;
        for frame in frames {
            let (index, time) = if frame.is_number() {
                (
                    frame.as_u64().ok_or("Invalid animation frame index")?,
                    default_time,
                )
            } else if frame.is_object() {
                (
                    frame
                        .get("index")
                        .and_then(Value::as_u64)
                        .ok_or("Missing animation frame index")?,
                    positive(frame.get("time"), default_time)?,
                )
            } else {
                return Err("Invalid animation frame entry".into());
            };
            if index >= count as u64 {
                return Err("Animation frame index exceeds sheet grid".into());
            }
            sequence.push((index as u32, time));
        }
    } else {
        if count > 1024 {
            return Err("Animation sequence exceeds frame limit".into());
        }
        sequence.extend((0..count).map(|i| (i, default_time)));
    }
    let frame_bytes = width as usize * height as usize * 4;
    if frame_bytes
        .checked_mul(sequence.len())
        .is_none_or(|n| n > 128 * 1024 * 1024)
    {
        return Err("Animation frames exceed memory limit".into());
    }
    let mut frames = Vec::with_capacity(sequence.len());
    for (index, time) in sequence {
        let x = (index % columns) * width;
        let y = (index / columns) * height;
        let mut rgba = Vec::with_capacity(frame_bytes);
        for row in 0..height {
            let begin = ((y + row) as usize * sheet.width as usize + x as usize) * 4;
            rgba.extend_from_slice(&sheet.rgba[begin..begin + width as usize * 4]);
        }
        frames.push(ModelAnimationFrame { rgba, time });
    }
    let base = FontTexture {
        path: path.into(),
        width,
        height,
        rgba: frames[0].rgba.clone(),
    };
    Ok(Some((
        base,
        ModelAnimation {
            texture,
            frames,
            interpolate,
        },
    )))
}
struct Decoder<'a> {
    pack: &'a PackFiles,
    pages: Vec<FontTexture>,
    lookup: BTreeMap<String, usize>,
    offset: usize,
    total: usize,
    animations: Vec<ModelAnimation>,
    animation_bytes: usize,
}
impl Decoder<'_> {
    fn texture(&mut self, path: &str) -> Result<usize, String> {
        if let Some(i) = self.lookup.get(path) {
            return Ok(*i);
        }
        if self.pages.len() >= 512 {
            return Err("Too many model texture pages".into());
        }
        let sheet = png_texture(self.pack, path)?;
        let i = self.pages.len();
        let animated = animation_frames(self.pack, path, &sheet, (i + self.offset) as i32)?;
        let (page, animation) = match animated {
            Some((page, animation)) => (page, Some(animation)),
            None => (sheet, None),
        };
        let animation_bytes = animation
            .as_ref()
            .map(|a| a.frames.iter().map(|f| f.rgba.len()).sum::<usize>())
            .unwrap_or(0);
        if self.animation_bytes + animation_bytes > 128 * 1024 * 1024 {
            return Err("Pack animation frames exceed memory limit".into());
        }
        self.total += page.rgba.len() + animation_bytes;
        if self.total > 192 * 1024 * 1024 {
            return Err("Model textures exceed memory limit".into());
        }
        self.animation_bytes += animation_bytes;
        if let Some(animation) = animation {
            self.animations.push(animation);
        }
        self.pages.push(page);
        self.lookup.insert(path.into(), i);
        Ok(i)
    }
    fn quad(
        &self,
        page: usize,
        positions: [[f32; 3]; 4],
        uv: [[f32; 2]; 4],
        tint: i32,
    ) -> PackItemQuad {
        let mut vertices = [[0.; 5]; 4];
        for i in 0..4 {
            vertices[i] = [
                positions[i][0],
                positions[i][1],
                positions[i][2],
                uv[i][0],
                uv[i][1],
            ];
        }
        PackItemQuad {
            texture: (page + self.offset) as i32,
            vertices,
            tint,
        }
    }
    fn decode(&mut self, id: String, model: &Value) -> Result<PackItemModel, String> {
        let textures = model
            .get("textures")
            .cloned()
            .unwrap_or_else(|| serde_json::json!({}));
        let gui = model.pointer("/display/gui");
        let gui = GuiTransform {
            rotation: vec3(gui.and_then(|v| v.get("rotation")), [0.; 3], 36000.)?,
            translation: vec3(gui.and_then(|v| v.get("translation")), [0.; 3], 80.)?,
            scale: vec3(gui.and_then(|v| v.get("scale")), [1.; 3], 4.)?,
        };
        // Retain authored contexts independently; child display entries already
        // override the corresponding parent entry in inherited().
        let mut display = Vec::new();
        for context in ["thirdperson_righthand", "thirdperson_lefthand",
            "firstperson_righthand", "firstperson_lefthand", "head", "gui", "ground", "fixed", "on_shelf"] {
            if let Some(value) = model.pointer(&format!("/display/{context}")) {
                if !value.is_object() { return Err("Invalid display transform".into()); }
                display.push(PackDisplayTransform { context: context.into(), transform: GuiTransform {
                    rotation: vec3(value.get("rotation"), [0.; 3], 36000.)?,
                    translation: vec3(value.get("translation"), [0.; 3], 80.)?,
                    scale: vec3(value.get("scale"), [1.; 3], 4.)?,
                }});
            }
        }
        let mut out = PackItemModel {
            id,
            quads: Vec::new(),
            gui,
            display,
            tints: Vec::new(),
        };
        if let Some(elements) = model.get("elements") {
            let elements = elements
                .as_array()
                .filter(|v| v.len() <= 4096)
                .ok_or("Too many model elements")?;
            for e in elements {
                let from = vec3(e.get("from"), [0.; 3], 80.)?;
                let to = vec3(e.get("to"), [16.; 3], 80.)?;
                let faces = e
                    .get("faces")
                    .and_then(Value::as_object)
                    .ok_or("Missing element faces")?;
                for (direction, face) in faces {
                    let path = texture_ref(
                        face.get("texture")
                            .and_then(Value::as_str)
                            .ok_or("Missing face texture")?,
                        &textures,
                    )?;
                    let page = self.texture(&path)?;
                    let [x0, y0, z0] = from;
                    let [x1, y1, z1] = to;
                    let (mut positions, default_uv) = match direction.as_str() {
                        "north" => (
                            [[x1, y1, z0], [x1, y0, z0], [x0, y0, z0], [x0, y1, z0]],
                            [16. - x1, 16. - y1, 16. - x0, 16. - y0],
                        ),
                        "south" => (
                            [[x0, y1, z1], [x0, y0, z1], [x1, y0, z1], [x1, y1, z1]],
                            [x0, 16. - y1, x1, 16. - y0],
                        ),
                        "west" => (
                            [[x0, y1, z0], [x0, y0, z0], [x0, y0, z1], [x0, y1, z1]],
                            [z0, 16. - y1, z1, 16. - y0],
                        ),
                        "east" => (
                            [[x1, y1, z1], [x1, y0, z1], [x1, y0, z0], [x1, y1, z0]],
                            [16. - z1, 16. - y1, 16. - z0, 16. - y0],
                        ),
                        "up" => (
                            [[x0, y1, z0], [x0, y1, z1], [x1, y1, z1], [x1, y1, z0]],
                            [x0, z0, x1, z1],
                        ),
                        "down" => (
                            [[x0, y0, z1], [x0, y0, z0], [x1, y0, z0], [x1, y0, z1]],
                            [x0, 16. - z1, x1, 16. - z0],
                        ),
                        _ => return Err("Unsupported element face direction".into()),
                    };
                    let mut rect = default_uv;
                    if let Some(uv) = face.get("uv") {
                        let a = uv
                            .as_array()
                            .filter(|a| a.len() == 4)
                            .ok_or("Invalid face UV")?;
                        for i in 0..4 {
                            rect[i] = a[i]
                                .as_f64()
                                .filter(|v| v.is_finite() && v.abs() <= 65536.)
                                .ok_or("Invalid face UV coordinate")?
                                as f32;
                        }
                    }
                    let rotation = face.get("rotation").and_then(Value::as_u64).unwrap_or(0);
                    if ![0, 90, 180, 270].contains(&rotation) {
                        return Err("Invalid face UV rotation".into());
                    }
                    let mut uv = [
                        [rect[0] / 16., rect[1] / 16.],
                        [rect[0] / 16., rect[3] / 16.],
                        [rect[2] / 16., rect[3] / 16.],
                        [rect[2] / 16., rect[1] / 16.],
                    ];
                    uv.rotate_left(rotation as usize / 90);
                    if let Some(rot) = e.get("rotation") {
                        rotate(&mut positions, rot)?;
                    }
                    let tint = face.get("tintindex").and_then(Value::as_i64).unwrap_or(-1);
                    if !(-1..=255).contains(&tint) {
                        return Err("Invalid model tint index".into());
                    }
                    out.quads.push(self.quad(page, positions, uv, tint as i32));
                }
            }
        } else if model.get("generated").and_then(Value::as_bool) == Some(true) {
            for layer in 0..5 {
                let Some(value) = textures
                    .get(format!("layer{layer}"))
                    .and_then(Value::as_str)
                else {
                    break;
                };
                let path = texture_ref(value, &textures)?;
                let page = self.texture(&path)?;
                out.quads.push(self.quad(
                    page,
                    [
                        [0., 16., 8.5],
                        [0., 0., 8.5],
                        [16., 0., 8.5],
                        [16., 16., 8.5],
                    ],
                    [[0., 0.], [0., 1.], [1., 1.], [1., 0.]],
                    layer,
                ));
                out.quads.push(self.quad(
                    page,
                    [
                        [16., 16., 7.5],
                        [16., 0., 7.5],
                        [0., 0., 7.5],
                        [0., 16., 7.5],
                    ],
                    [[1., 0.], [1., 1.], [0., 1.], [0., 0.]],
                    layer,
                ));
                let texture = &self.pages[page];
                let (w, h) = (texture.width as usize, texture.height as usize);
                let frames: Vec<&[u8]> = match self
                    .animations
                    .iter()
                    .find(|a| a.texture == (page + self.offset) as i32)
                {
                    Some(a) => a.frames.iter().map(|f| f.rgba.as_slice()).collect(),
                    None => vec![texture.rgba.as_slice()],
                };
                let pixel = |frame: &[u8], x: usize, y: usize| frame[(y * w + x) * 4 + 3] != 0;
                let solid = |x: usize, y: usize| frames.iter().any(|frame| pixel(frame, x, y));
                let edge = |x: usize, y: usize, neighbor: Option<(usize, usize)>| {
                    frames.iter().any(|frame| {
                        pixel(frame, x, y) && neighbor.is_none_or(|(nx, ny)| !pixel(frame, nx, ny))
                    })
                };
                for y in 0..h {
                    for x in 0..w {
                        if !solid(x, y) {
                            continue;
                        }
                        let (x0, x1) = (16. * x as f32 / w as f32, 16. * (x + 1) as f32 / w as f32);
                        let (y0, y1) = (
                            16. - 16. * (y + 1) as f32 / h as f32,
                            16. - 16. * y as f32 / h as f32,
                        );
                        let (u, v) = ((x as f32 + 0.5) / w as f32, (y as f32 + 0.5) / h as f32);
                        for (side, edge) in [
                            (0, edge(x, y, y.checked_sub(1).map(|ny| (x, ny)))),
                            (1, edge(x, y, (y + 1 < h).then_some((x, y + 1)))),
                            (2, edge(x, y, x.checked_sub(1).map(|nx| (nx, y)))),
                            (3, edge(x, y, (x + 1 < w).then_some((x + 1, y)))),
                        ] {
                            if !edge {
                                continue;
                            }
                            let p = match side {
                                0 => [[x0, y1, 7.5], [x0, y1, 8.5], [x1, y1, 8.5], [x1, y1, 7.5]],
                                1 => [[x0, y0, 8.5], [x0, y0, 7.5], [x1, y0, 7.5], [x1, y0, 8.5]],
                                2 => [[x0, y1, 7.5], [x0, y0, 7.5], [x0, y0, 8.5], [x0, y1, 8.5]],
                                _ => [[x1, y1, 8.5], [x1, y0, 8.5], [x1, y0, 7.5], [x1, y1, 7.5]],
                            };
                            out.quads.push(self.quad(page, p, [[u, v]; 4], layer));
                            if out.quads.len() > 65536 {
                                return Err("Generated model exceeds quad limit".into());
                            }
                        }
                    }
                }
            }
        } else {
            return Err("Unsupported model without elements/generated parent".into());
        }
        if out.quads.len() > 65536 {
            return Err("Model exceeds quad limit".into());
        }
        Ok(out)
    }
}
fn rotate(positions: &mut [[f32; 3]; 4], rotation: &Value) -> Result<(), String> {
    let origin = vec3(rotation.get("origin"), [8.; 3], 80.)?;
    if rotation.get("angle").is_none() {
        return Ok(());
    }
    let axis = match rotation.get("axis").and_then(Value::as_str) {
        Some("x") => 0,
        Some("y") => 1,
        Some("z") => 2,
        _ => return Err("Invalid element rotation axis".into()),
    };
    let angle = rotation
        .get("angle")
        .map(|v| v.as_f64())
        .unwrap_or(Some(0.))
        .filter(|v| [-45., -22.5, 0., 22.5, 45.].contains(v))
        .ok_or("Unsupported element rotation angle")? as f32;
    let (s, c) = angle.to_radians().sin_cos();
    let scale = if rotation.get("rescale").and_then(Value::as_bool) == Some(true) {
        1. / c.abs()
    } else {
        1.
    };
    for p in positions {
        let mut v = [p[0] - origin[0], p[1] - origin[1], p[2] - origin[2]];
        let (a, b) = match axis {
            0 => (1, 2),
            1 => (2, 0),
            _ => (0, 1),
        };
        let (va, vb) = (v[a], v[b]);
        v[a] = (va * c - vb * s) * scale;
        v[b] = (va * s + vb * c) * scale;
        for i in 0..3 {
            p[i] = v[i] + origin[i];
        }
    }
    Ok(())
}
pub fn decode_models(pack: &PackFiles, texture_offset: usize) -> Result<DecodedModels, String> {
    let mut decoder = Decoder {
        pack,
        pages: Vec::new(),
        lookup: BTreeMap::new(),
        offset: texture_offset,
        total: 0,
        animations: Vec::new(),
        animation_bytes: 0,
    };
    let mut models = Vec::new();
    let mut unsupported = Vec::new();
    for path in pack.0.keys() {
        let Some(rest) = path.strip_prefix("assets/") else {
            continue;
        };
        let Some((ns, item)) = rest.split_once("/items/") else {
            continue;
        };
        let Some(item) = item.strip_suffix(".json") else {
            continue;
        };
        if models.len() + unsupported.len() >= 4096 {
            return Err("Too many item model definitions".into());
        }
        let id = format!("{ns}:{item}");
        let result: Result<(PackItemModel, Vec<String>), String> = (|| {
            let definition = json(pack, path)?;
            let model = definition
                .get("model")
                .ok_or("Missing item model definition")?;
            if model.get("type").and_then(Value::as_str) != Some("minecraft:model") {
                return Err("Unsupported item model selector".into());
            }
            let (tints, tint_errors) = decode_tints(model.get("tints"))?;
            let reference = model
                .get("model")
                .and_then(Value::as_str)
                .ok_or("Missing item model reference")?;
            let model = inherited(pack, reference, 0)?;
            let mut result = decoder.decode(id.clone(), &model)?;
            result.tints = tints;
            Ok((result, tint_errors))
        })();
        match result {
            Ok((model, tint_errors)) => {
                unsupported.extend(
                    tint_errors
                        .into_iter()
                        .map(|reason| format!("{id}: {reason}")),
                );
                if models
                    .iter()
                    .map(|m: &PackItemModel| m.quads.len())
                    .sum::<usize>()
                    + model.quads.len()
                    > 262144
                {
                    return Err("Pack model quad total exceeds limit".into());
                }
                models.push(model)
            }
            Err(reason) => unsupported.push(format!("{id}: {reason}")),
        }
    }
    Ok(DecodedModels {
        models,
        textures: decoder.pages,
        animations: decoder.animations,
        unsupported,
    })
}
#[cfg(test)]
mod tests {
    use super::*;
    fn image() -> Vec<u8> {
        let mut bytes = Vec::new();
        {
            let mut encoder = png::Encoder::new(&mut bytes, 2, 1);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().unwrap();
            writer
                .write_image_data(&[255, 255, 255, 255, 255, 255, 255, 0])
                .unwrap();
        }
        bytes
    }
    fn fixture(entries: &[(&str, Value)]) -> PackFiles {
        let mut files = BTreeMap::new();
        for (path, value) in entries {
            files.insert((*path).into(), serde_json::to_vec(value).unwrap());
        }
        files.insert("assets/test/textures/icon.png".into(), image());
        PackFiles(files)
    }
    fn animation_fixture(metadata: Value) -> PackFiles {
        PackFiles(BTreeMap::from([(
            "assets/test/textures/anim.png.mcmeta".into(),
            serde_json::to_vec(&serde_json::json!({"animation":metadata})).unwrap(),
        )]))
    }
    fn sheet(width: u32, height: u32) -> FontTexture {
        FontTexture {
            path: "assets/test/textures/anim.png".into(),
            width,
            height,
            rgba: (0..width * height)
                .flat_map(|n| [n as u8 + 1, 0, 0, 255])
                .collect(),
        }
    }
    #[test]
    fn animation_grid_defaults_explicit_sequence_and_frame_dwell() {
        let path = "assets/test/textures/anim.png";
        let source = sheet(2, 2);
        let pack = animation_fixture(
            serde_json::json!({"width":1,"height":1,"frametime":7,"frames":[2,{"index":1,"time":3},0,1],"interpolate":true}),
        );
        let (base, animation) = animation_frames(&pack, path, &source, 9).unwrap().unwrap();
        assert_eq!((base.width, base.height), (1, 1));
        assert_eq!(base.rgba[0], 3);
        assert_eq!(animation.texture, 9);
        assert!(animation.interpolate);
        assert_eq!(
            animation
                .frames
                .iter()
                .map(|f| f.rgba[0])
                .collect::<Vec<_>>(),
            [3, 2, 1, 2]
        );
        assert_eq!(
            animation.frames.iter().map(|f| f.time).collect::<Vec<_>>(),
            [7, 3, 7, 7]
        );
        let pack = animation_fixture(serde_json::json!({"width":1}));
        let (base, animation) = animation_frames(&pack, path, &source, 0).unwrap().unwrap();
        assert_eq!((base.width, base.height), (1, 2));
        assert_eq!(animation.frames.len(), 2);
        assert_eq!(&base.rgba[..], [1, 0, 0, 255, 3, 0, 0, 255]);
        assert_eq!(animation.frames[0].time, 1);
        let pack = animation_fixture(serde_json::json!({"height":1}));
        let (base, animation) = animation_frames(&pack, path, &source, 0).unwrap().unwrap();
        assert_eq!((base.width, base.height), (2, 1));
        assert_eq!(animation.frames.len(), 2);
        let source = sheet(2, 4);
        let pack = animation_fixture(serde_json::json!({}));
        let (base, animation) = animation_frames(&pack, path, &source, 0).unwrap().unwrap();
        assert_eq!((base.width, base.height), (2, 2));
        assert_eq!(animation.frames.len(), 2);
    }
    #[test]fn animation_cropping_preserves_all_rgba_channels_and_partial_alpha(){
        let source=FontTexture{path:"assets/test/textures/anim.png".into(),width:2,height:1,rgba:vec![1,2,3,4,5,6,7,8]};
        let pack=animation_fixture(serde_json::json!({"width":1,"height":1,"frames":[1,0]}));
        let(base,animation)=animation_frames(&pack,&source.path,&source,0).unwrap().unwrap();
        assert_eq!(base.rgba,[5,6,7,8]);assert_eq!(animation.frames[0].rgba,[5,6,7,8]);assert_eq!(animation.frames[1].rgba,[1,2,3,4]);
    }
    #[test]
    fn malformed_animation_never_falls_back_to_full_strip() {
        let source = sheet(2, 2);
        for metadata in [
            serde_json::json!({"width":0}),
            serde_json::json!({"width":3}),
            serde_json::json!({"width":1,"height":1,"frames":[4]}),
            serde_json::json!({"frames":[]}),
            serde_json::json!({"frames":[-1]}),
            serde_json::json!({"frametime":0}),
            serde_json::json!({"frames":[{"index":0,"time":0}]}),
            serde_json::json!({"frames":[{"time":1}]}),
            serde_json::json!({"interpolate":"yes"}),
        ] {
            assert!(
                animation_frames(&animation_fixture(metadata), &source.path, &source, 0).is_err()
            );
        }
    }
    #[test]
    fn generated_edges_union_each_animation_frame_boundaries() {
        let mut pack = fixture(&[
            (
                "assets/test/items/arrow.json",
                serde_json::json!({"model":{"type":"minecraft:model","model":"test:arrow"}}),
            ),
            (
                "assets/test/models/arrow.json",
                serde_json::json!({"parent":"item/generated","textures":{"layer0":"test:icon"}}),
            ),
            (
                "assets/test/textures/icon.png.mcmeta",
                serde_json::json!({"animation":{"width":2,"height":1,"frames":[0,1],"frametime":5}}),
            ),
        ]);
        let mut bytes = Vec::new();
        {
            let mut encoder = png::Encoder::new(&mut bytes, 2, 2);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().unwrap();
            writer
                .write_image_data(&[
                    255, 255, 255, 255, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 255,
                ])
                .unwrap();
        }
        pack.0.insert("assets/test/textures/icon.png".into(), bytes);
        let decoded = decode_models(&pack, 8).unwrap();
        assert!(decoded.unsupported.is_empty());
        assert_eq!(decoded.animations.len(), 1);
        assert_eq!(decoded.animations[0].texture, 8);
        assert_eq!(decoded.animations[0].frames.len(), 2);
        assert_eq!(
            (decoded.textures[0].width, decoded.textures[0].height),
            (2, 1)
        );
        assert_eq!(decoded.models[0].quads.len(), 10);
        assert!(decoded.models[0].quads[2..]
            .iter()
            .flat_map(|q| q.vertices)
            .any(|v| v[0] == 16.));
    }
    #[test]fn dye_and_potion_tint_sources_keep_authoritative_model_defaults(){let(tints,errors)=decode_tints(Some(&serde_json::json!([{"type":"minecraft:dye","default":16578808},{"type":"minecraft:potion","default":0x385dc6}]))).unwrap();assert!(errors.is_empty());assert_eq!(tints[0].kind,"dye");assert_eq!(tints[0].default_color,16578808);assert_eq!(tints[1].kind,"potion");assert_eq!(tints[1].default_color,0x385dc6);}
    #[test]
    fn tint_sources_preserve_indices_defaults_and_explicit_unsupported_slots() {
        let (tints, errors) = decode_tints(Some(&serde_json::json!([
            {"type":"minecraft:constant","value":0x123456},
            {"type":"minecraft:custom_model_data","index":2,"default":0xabcdef},
            {"type":"minecraft:future","default":16578808},
            {"type":"minecraft:custom_model_data","default":-1}
        ])))
        .unwrap();
        assert_eq!(tints.len(), 4);
        assert_eq!(errors.len(), 1);
        assert_eq!(tints[0].kind, "constant");
        assert_eq!(tints[0].color, 0x123456);
        assert_eq!(tints[1].kind, "custom_model_data");
        assert_eq!(tints[1].index, 2);
        assert_eq!(tints[1].default_color, 0xabcdef);
        assert_eq!(tints[2].kind, "unsupported");
        assert_eq!(tints[2].default_color, 16578808);
        assert_eq!(tints[3].index, 0);
        assert_eq!(tints[3].default_color, 0xffffff);
        assert!(decode_tints(None).unwrap().0.is_empty());
        let (invalid, errors) = decode_tints(Some(&serde_json::json!([
            {"type":"minecraft:custom_model_data","index":-1,"default":0},
            {"type":"minecraft:constant","value":"oops"}
        ])))
        .unwrap();
        assert_eq!(invalid.len(), 2);
        assert_eq!(errors.len(), 2);
    }
    #[test]
    fn generated_sprite_keeps_alpha_silhouette_layers_and_texture_offset() {
        let pack = fixture(&[
            (
                "assets/test/items/arrow.json",
                serde_json::json!({"model":{"type":"minecraft:model","model":"test:arrow"}}),
            ),
            (
                "assets/test/models/arrow.json",
                serde_json::json!({"parent":"minecraft:item/generated","textures":{"layer0":"test:icon"}}),
            ),
        ]);
        let decoded = decode_models(&pack, 8).unwrap();
        assert!(decoded.unsupported.is_empty());
        assert_eq!(decoded.models.len(), 1);
        assert_eq!(decoded.models[0].id, "test:arrow");
        assert_eq!(decoded.models[0].quads.len(), 6);
        assert!(decoded.models[0].quads.iter().all(|q| q.texture == 8));
        assert_eq!(decoded.textures[0].rgba[7], 0);
        assert!(decoded.models[0].quads[2..]
            .iter()
            .flat_map(|q| q.vertices)
            .all(|v| v[0] <= 8.));
    }
    #[test]
    fn inherited_elements_gui_uv_rotation_and_exporter_origins_are_preserved() {
        let pack = fixture(&[
            (
                "assets/test/items/child.json",
                serde_json::json!({"model":{"type":"minecraft:model","model":"test:child"}}),
            ),
            (
                "assets/test/models/parent.json",
                serde_json::json!({"textures":{"base":"test:icon","alias":"#base"},"display":{"gui":{"rotation":[30,45,0],"translation":[0,2,0],"scale":[0.5,0.5,0.5]}},"elements":[{"from":[-2,0,0],"to":[18,16,16],"rotation":{"origin":[8,8,8]},"faces":{"south":{"texture":"#alias","uv":[0,0,16,16],"rotation":90,"tintindex":2}}}]}),
            ),
            (
                "assets/test/models/child.json",
                serde_json::json!({"parent":"test:parent"}),
            ),
        ]);
        let decoded = decode_models(&pack, 0).unwrap();
        assert!(decoded.unsupported.is_empty());
        let m = &decoded.models[0];
        assert_eq!(m.gui.rotation, [30., 45., 0.]);
        assert_eq!(m.gui.translation, [0., 2., 0.]);
        assert_eq!(m.gui.scale, [0.5; 3]);
        assert_eq!(m.quads[0].tint, 2);
        assert_eq!(m.quads[0].vertices[0], [-2., 16., 16., 0., 1.]);
        assert_eq!(m.quads[0].vertices[2][0], 18.);
    }
    #[test]
    fn display_contexts_inherit_and_override_independently() {
        let pack = fixture(&[
            ("assets/test/items/child.json", serde_json::json!({"model":{"type":"minecraft:model","model":"test:child"}})),
            ("assets/test/models/parent.json", serde_json::json!({"parent":"minecraft:item/generated","textures":{"layer0":"test:icon"},"display":{
                "firstperson_righthand":{"rotation":[10,20,30],"translation":[1,2,3],"scale":[0.5,0.6,0.7]},
                "firstperson_lefthand":{"translation":[-1,0,0]},
                "ground":{"scale":[0.2,0.2,0.2]},"gui":{"rotation":[30,45,0]}}})),
            ("assets/test/models/child.json", serde_json::json!({"parent":"test:parent","display":{"firstperson_righthand":{"rotation":[5,6,7]}}})),
        ]);
        let decoded = decode_models(&pack, 0).unwrap();
        assert!(decoded.unsupported.is_empty());
        let model = &decoded.models[0];
        let context = |name: &str| &model.display.iter().find(|v| v.context == name).unwrap().transform;
        assert_eq!(context("firstperson_righthand").rotation, [5.,6.,7.]);
        assert_eq!(context("firstperson_righthand").translation, [0.;3]);
        assert_eq!(context("firstperson_righthand").scale, [1.;3]);
        assert_eq!(context("firstperson_lefthand").translation, [-1.,0.,0.]);
        assert_eq!(context("ground").scale, [0.2;3]);
        assert_eq!(context("gui"), &model.gui);
        let json = serde_json::to_value(model).unwrap();
        assert_eq!(json["display"].as_array().unwrap().len(), 4);
        assert!(!model.display.iter().any(|v| v.context == "fixed"));
    }
    #[test]
    fn element_rotation_rescale_and_unsupported_selectors_are_explicit() {
        let mut p = [[9., 8., 8.]; 4];
        rotate(
            &mut p,
            &serde_json::json!({"axis":"z","origin":[8,8,8],"angle":45,"rescale":true}),
        )
        .unwrap();
        assert!((p[0][0] - 9.).abs() < 0.0001 && (p[0][1] - 9.).abs() < 0.0001);
        let pack = fixture(&[
            (
                "assets/test/items/unsupported.json",
                serde_json::json!({"model":{"type":"minecraft:select"}}),
            ),
            (
                "assets/test/items/cycle.json",
                serde_json::json!({"model":{"type":"minecraft:model","model":"test:cycle"}}),
            ),
            (
                "assets/test/models/cycle.json",
                serde_json::json!({"parent":"test:cycle"}),
            ),
        ]);
        let decoded = decode_models(&pack, 0).unwrap();
        assert!(decoded.models.is_empty());
        assert_eq!(decoded.unsupported.len(), 2);
        assert!(decoded.unsupported.iter().any(|s| s.contains("recursion")));
        assert!(decoded.unsupported.iter().any(|s| s.contains("selector")));
    }
}

#[cfg(test)]mod official_plain_texture_alias_tests{
 use super::*;
 #[test]fn official_heavy_core_plain_alias_and_legacy_hash_alias_resolve_identically(){let textures=serde_json::json!({"all":"block/heavy_core","side":"#all"});assert_eq!(texture_ref("all",&textures).unwrap(),"assets/minecraft/textures/block/heavy_core.png");assert_eq!(texture_ref("#all",&textures).unwrap(),texture_ref("side",&textures).unwrap());assert_eq!(texture_ref("minecraft:block/stone",&textures).unwrap(),"assets/minecraft/textures/block/stone.png");assert!(texture_ref("all",&serde_json::json!({"all":"all"})).is_err());}
}
