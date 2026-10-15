use super::*;

#[test]
fn custom_item_visual_metadata_preserves_indexed_values_and_base_fallback() {
    let item = ItemStack::new(azalea_registry::builtin::ItemKind::PlayerHead, 1)
        .with_component(components::ItemModel { resource_location: "oraxen:arrow_next".into() })
        .with_component(components::CustomModelData { floats: vec![1., -2.5], flags: vec![false, true], strings: vec!["variant_one".into(), "".into()], colors: vec![0x123456, -1] });
    let value = snapshot(&item).unwrap();
    assert_eq!(value.item_model.as_deref(), Some("oraxen:arrow_next"));
    let data = value.custom_model_data.as_ref().unwrap();
    assert_eq!(data.floats, [1., -2.5]);
    assert_eq!(data.flags, [false, true]);
    assert_eq!(data.strings, ["variant_one", ""]);
    assert_eq!(data.colors, [0x123456, -1]);
    let base = snapshot(&ItemStack::new(azalea_registry::builtin::ItemKind::PlayerHead, 1)).unwrap();
    assert_eq!((value.original_id, value.original_data), (base.original_id, base.original_data));
    let json = serde_json::to_value(&value).unwrap();
    assert_eq!(json["custom_model_data"]["colors"][1], -1);
    assert_eq!(json["item_model"], "oraxen:arrow_next");
}

#[test]
fn malformed_or_oversized_model_data_is_rejected_without_index_shifting() {
    let mut data = components::CustomModelData { floats: vec![0., f32::NAN, 2.], flags: vec![], strings: vec![], colors: vec![] };
    assert!(model_data(&data).is_none());
    data.floats = vec![f32::INFINITY];
    assert!(model_data(&data).is_none());
    data.floats = vec![0.; 1025];
    assert!(model_data(&data).is_none());
    data.floats.clear();
    data.strings = vec!["x".repeat(2049)];
    assert!(model_data(&data).is_none());
    data.strings = vec!["x".repeat(2048); 33];
    assert!(model_data(&data).is_none());
    assert!(model_identifier("oraxen:../bad").is_none());
    assert!(model_identifier("oraxen:Uppercase").is_none());
    assert!(model_identifier("oraxen:").is_none());
    assert!(model_identifier("oraxen:arrow_next").is_some());
}
