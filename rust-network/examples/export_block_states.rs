use azalea_block::{BlockState, BlockTrait};
fn main() {
    let states: Vec<_> = (0..=BlockState::MAX_STATE).map(|id| {
        let state = BlockState::try_from(id).unwrap();
        let block: Box<dyn BlockTrait> = Box::from(state);
        serde_json::json!({"id": id, "name": block.id(), "properties": block.property_map()})
    }).collect();
    println!("{}", serde_json::to_string(&states).unwrap());
}
