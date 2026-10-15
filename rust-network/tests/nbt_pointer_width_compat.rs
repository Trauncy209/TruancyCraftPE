use simdnbt::owned::{Nbt,NbtCompound,NbtList,NbtTag};
use std::io::Cursor;
#[test]
fn owned_and_borrowed_roundtrip_all_tape_pointer_variants_on_actual_pointer_width(){
 let nested=NbtCompound::from_values(vec![("inner".into(),NbtTag::Long(i64::MIN+17))]);
 let values=vec![("byte".into(),NbtTag::Byte(-7)),("short".into(),NbtTag::Short(-1234)),("int".into(),NbtTag::Int(i32::MIN+3)),("long".into(),NbtTag::Long(i64::MAX-11)),("float".into(),NbtTag::Float(1.25)),("double".into(),NbtTag::Double(-12345.75)),("string".into(),NbtTag::String("hello \0 世界".into())),("bytes".into(),NbtTag::ByteArray(vec![1,2,255,128])),("ints".into(),NbtTag::IntArray(vec![i32::MIN,0,i32::MAX])),("longs".into(),NbtTag::LongArray(vec![i64::MIN,-1,0,i64::MAX])),("compound".into(),NbtTag::Compound(nested.clone())),("list".into(),NbtTag::List(NbtList::Compound(vec![nested.clone(),nested])))];
 let original=Nbt::new("root".into(),NbtCompound::from_values(values));let expected=original.clone().unwrap().as_compound();let mut bytes=Vec::new();original.write(&mut bytes);
 let owned=simdnbt::owned::read(&mut Cursor::new(bytes.as_slice())).unwrap().unwrap();assert_eq!(owned.as_compound(),expected);
 let borrowed=simdnbt::borrow::read(&mut Cursor::new(bytes.as_slice())).unwrap().unwrap();assert_eq!(borrowed.as_compound().to_owned(),expected);
 // Force pointer-backed tape elements and list iterators after other allocations.
 let another=vec![123u8;16384];assert_eq!(borrowed.as_compound().long("long"),Some(i64::MAX-11));assert_eq!(borrowed.as_compound().double("double"),Some(-12345.75));assert_eq!(borrowed.as_compound().long_array("longs").unwrap(),vec![i64::MIN,-1,0,i64::MAX]);assert_eq!(another.len(),16384);
 println!("actual pointer width {}: owned/borrowed/tape roundtrip",usize::BITS);
}
#[test]
fn oversized_array_lengths_reject_without_wrapping_or_out_of_bounds_tape(){
 for tag in [7u8,11,12]{for length in [0x2000_0000u32,0x4000_0000,0x8000_0000,u32::MAX]{let mut bytes=vec![10,0,0,tag,0,1,b'a'];bytes.extend(length.to_be_bytes());bytes.push(0);assert!(simdnbt::owned::read(&mut Cursor::new(bytes.as_slice())).is_err());assert!(simdnbt::borrow::read(&mut Cursor::new(bytes.as_slice())).is_err());}}
}
#[test]
fn truncated_scalars_names_lists_and_arrays_never_produce_valid_borrowed_pointers(){
 for bytes in [vec![10,0,0,4,0,1,b'x',1,2,3],vec![10,0,0,8,0,1,b'x',255,255,1],vec![10,0,0,9,0,1,b'x',4,0,0,0,2,1],vec![10,0,0,12,0,1,b'x',0,0,0,2,0]] {assert!(simdnbt::owned::read(&mut Cursor::new(bytes.as_slice())).is_err());assert!(simdnbt::borrow::read(&mut Cursor::new(bytes.as_slice())).is_err());}
}
