//! Contains a few ways to style numbers. At the time of writing, Minecraft only
//! uses this for rendering scoreboard objectives.

use std::io::{self, Cursor, Write};

#[cfg(feature = "azalea-buf")]
use azalea_buf::{AzaleaRead, AzaleaWrite};
use azalea_registry::builtin::NumberFormatKind;
use simdnbt::owned::Nbt;

use crate::FormattedText;

#[derive(Clone, Debug, PartialEq)]
pub enum NumberFormat {
    Blank,
    Styled { style: Nbt },
    Fixed { value: FormattedText },
}

#[cfg(feature = "azalea-buf")]
impl AzaleaRead for NumberFormat {
    fn azalea_read(buf: &mut Cursor<&[u8]>) -> Result<Self, azalea_buf::BufReadError> {
        let kind = NumberFormatKind::azalea_read(buf)?;
        match kind {
            NumberFormatKind::Blank => Ok(NumberFormat::Blank),
            NumberFormatKind::Styled => Ok(NumberFormat::Styled {
                style: simdnbt::owned::read(buf)?,
            }),
            NumberFormatKind::Fixed => Ok(NumberFormat::Fixed {
                value: FormattedText::azalea_read(buf)?,
            }),
        }
    }
}

#[cfg(feature = "azalea-buf")]
impl AzaleaWrite for NumberFormat {
    fn azalea_write(&self, buf: &mut impl Write) -> io::Result<()> {
        match self {
            NumberFormat::Blank => NumberFormatKind::Blank.azalea_write(buf)?,
            NumberFormat::Styled { style } => {
                NumberFormatKind::Styled.azalea_write(buf)?;
                style.azalea_write(buf)?;
            }
            NumberFormat::Fixed { value } => {
                NumberFormatKind::Fixed.azalea_write(buf)?;
                value.azalea_write(buf)?;
            }
        }
        Ok(())
    }
}

impl NumberFormat {
    /// Decode the protocol's compound style without losing RGB or explicit flags.
    pub fn styled_component(&self, text: &str) -> Option<FormattedText> {
        use simdnbt::Deserialize;
        let Self::Styled { style } = self else { return None; };
        let mut bytes = Vec::new();
        style.write(&mut bytes);
        let decoded = simdnbt::borrow::read(&mut Cursor::new(bytes.as_slice())).ok()?;
        let simdnbt::borrow::Nbt::Some(compound) = decoded else { return None; };
        let parsed = crate::style::Style::from_compound(compound.as_compound()).ok()?;
        let mut component = FormattedText::from(text);
        *component.get_base_mut().style = parsed;
        Some(component)
    }
}

#[cfg(test)]
mod tcpe_number_tests {
    use super::*;
    #[test]
    fn styled_score_preserves_hex_and_explicit_flags() {
        let mut compound = simdnbt::owned::NbtCompound::new();
        compound.insert("color", "#123450");
        compound.insert("bold", 1i8);
        compound.insert("italic", 0i8);
        let format = NumberFormat::Styled { style: Nbt::Some(simdnbt::owned::BaseNbt::new("", compound)) };
        let component = format.styled_component("-42").unwrap();
        assert_eq!(component.to_string(), "-42");
        assert_eq!(component.get_base().style.color.as_ref().unwrap().value, 0x123450);
        assert_eq!(component.get_base().style.bold, Some(true));
        assert_eq!(component.get_base().style.italic, Some(false));
        assert!(NumberFormat::Styled { style: Nbt::None }.styled_component("0").is_none());
    }
}
