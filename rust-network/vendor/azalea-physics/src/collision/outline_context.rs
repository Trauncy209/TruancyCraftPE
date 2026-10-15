//! Verified Java1.21.11 context-dependent outlines and interaction-face shapes.
//! These are exact official voxel unions, not bounding-box approximations.
use std::sync::LazyLock;
use azalea_block::{BlockState,properties::Facing};
use azalea_registry::builtin::{BlockKind,ItemKind};
use super::{VoxelShape,Shapes,box_shape,BLOCK_SHAPE,EMPTY_SHAPE,checked_outline_shape};
#[derive(Clone,Copy,Debug,Default,PartialEq,Eq)]
pub struct OutlineContext {pub held_main:Option<ItemKind>}
impl OutlineContext {
 pub fn from_main(stack:&azalea_inventory::ItemStack)->Self{Self{held_main:match stack{azalea_inventory::ItemStack::Present(item)=>Some(item.kind),_=>None}}}
}
pub fn checked_outline_shape_with_context(state:BlockState,context:OutlineContext)->Option<&'static VoxelShape>{
 match BlockKind::from(state){
  BlockKind::Scaffolding if context.held_main==Some(ItemKind::Scaffolding)=>Some(&BLOCK_SHAPE),
  BlockKind::Light=>Some(if context.held_main==Some(ItemKind::Light){&BLOCK_SHAPE}else{&EMPTY_SHAPE}),
  _=>checked_outline_shape(state),
 }
}
static CAULDRON_INTERIOR:LazyLock<VoxelShape>=LazyLock::new(||box_shape(0.125,0.25,0.125,0.875,1.0,0.875));
static HOPPER_DOWN:LazyLock<VoxelShape>=LazyLock::new(||{
 let mut shape=EMPTY_SHAPE.clone();
 shape=Shapes::or(shape,box_shape(0.125,0.6875,0.125,0.875,1.0,0.875));
 shape
});
static HOPPER_NORTH:LazyLock<VoxelShape>=LazyLock::new(||{
 let mut shape=EMPTY_SHAPE.clone();
 shape=Shapes::or(shape,box_shape(0.375,0.5,0.0,0.625,0.625,0.25));
 shape=Shapes::or(shape,box_shape(0.125,0.6875,0.125,0.875,1.0,0.875));
 shape
});
static HOPPER_SOUTH:LazyLock<VoxelShape>=LazyLock::new(||{
 let mut shape=EMPTY_SHAPE.clone();
 shape=Shapes::or(shape,box_shape(0.375,0.5,0.75,0.625,0.625,1.0));
 shape=Shapes::or(shape,box_shape(0.125,0.6875,0.125,0.875,1.0,0.875));
 shape
});
static HOPPER_WEST:LazyLock<VoxelShape>=LazyLock::new(||{
 let mut shape=EMPTY_SHAPE.clone();
 shape=Shapes::or(shape,box_shape(0.0,0.5,0.375,0.25,0.625,0.625));
 shape=Shapes::or(shape,box_shape(0.125,0.6875,0.125,0.875,1.0,0.875));
 shape
});
static HOPPER_EAST:LazyLock<VoxelShape>=LazyLock::new(||{
 let mut shape=EMPTY_SHAPE.clone();
 shape=Shapes::or(shape,box_shape(0.75,0.5,0.375,1.0,0.625,0.625));
 shape=Shapes::or(shape,box_shape(0.125,0.6875,0.125,0.875,1.0,0.875));
 shape
});
pub fn interaction_shape(state:BlockState)->&'static VoxelShape{
 match BlockKind::from(state){
  BlockKind::Cauldron|BlockKind::WaterCauldron|BlockKind::LavaCauldron|BlockKind::PowderSnowCauldron=>&CAULDRON_INTERIOR,
  BlockKind::Composter|BlockKind::Scaffolding=>&BLOCK_SHAPE,
  BlockKind::Hopper=>match state.property::<Facing>(){
   Some(Facing::Down)=>&HOPPER_DOWN,Some(Facing::North)=>&HOPPER_NORTH,Some(Facing::South)=>&HOPPER_SOUTH,Some(Facing::West)=>&HOPPER_WEST,Some(Facing::East)=>&HOPPER_EAST,None=>&EMPTY_SHAPE,
  },
  _=>&EMPTY_SHAPE,
 }
}
