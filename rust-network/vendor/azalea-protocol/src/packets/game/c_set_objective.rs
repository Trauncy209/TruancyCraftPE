use std::io::{self, Cursor, Write};

use azalea_buf::{AzBuf, AzaleaRead, AzaleaWrite};
use azalea_chat::{FormattedText, numbers::NumberFormat};
use azalea_core::objectives::ObjectiveCriteria;
use azalea_protocol_macros::ClientboundGamePacket;

#[derive(AzBuf, ClientboundGamePacket, Clone, Debug, PartialEq)]
pub struct ClientboundSetObjective {
    pub objective_name: String,
    pub method: Method,
}

#[derive(AzBuf, Clone, Copy, Debug, PartialEq)]
pub enum MethodKind {
    Add,
    Remove,
    Change,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Method {
    Add {
        display_name: FormattedText,
        render_type: ObjectiveCriteria,
        number_format: Option<NumberFormat>,
    },
    Remove,
    Change {
        display_name: FormattedText,
        render_type: ObjectiveCriteria,
        number_format: Option<NumberFormat>,
    },
}

impl AzaleaRead for Method {
    fn azalea_read(buf: &mut Cursor<&[u8]>) -> Result<Self, azalea_buf::BufReadError> {
        let kind = MethodKind::azalea_read(buf)?;
        match kind {
            MethodKind::Add => Ok(Method::Add {
                display_name: FormattedText::azalea_read(buf)?,
                render_type: ObjectiveCriteria::azalea_read(buf)?,
                number_format: Option::<NumberFormat>::azalea_read(buf)?,
            }),
            MethodKind::Remove => Ok(Method::Remove),
            MethodKind::Change => Ok(Method::Change {
                display_name: FormattedText::azalea_read(buf)?,
                render_type: ObjectiveCriteria::azalea_read(buf)?,
                number_format: Option::<NumberFormat>::azalea_read(buf)?,
            }),
        }
    }
}

impl AzaleaWrite for Method {
    fn azalea_write(&self, buf: &mut impl Write) -> io::Result<()> {
        match self {
            Method::Add {
                display_name,
                render_type,
                number_format,
            } => {
                MethodKind::Add.azalea_write(buf)?;
                display_name.azalea_write(buf)?;
                render_type.azalea_write(buf)?;
                number_format.azalea_write(buf)?;
            }
            Method::Remove => MethodKind::Remove.azalea_write(buf)?,
            Method::Change {
                display_name,
                render_type,
                number_format,
            } => {
                MethodKind::Change.azalea_write(buf)?;
                display_name.azalea_write(buf)?;
                render_type.azalea_write(buf)?;
                number_format.azalea_write(buf)?;
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod regression_tests {
    use super::*;
    #[test]
    fn optional_objective_number_format() {
        // Update objective `x`, display `x`, integer render, present fixed-format `0`.
        let encoded = [1, b'x', 2, 8, 0, 1, b'x', 0, 1, 2, 8, 0, 1, b'0'];
        let decoded=ClientboundSetObjective::azalea_read(&mut Cursor::new(encoded.as_slice())).unwrap();
        let mut out=Vec::new();decoded.azalea_write(&mut out).unwrap();
        assert_eq!(ClientboundSetObjective::azalea_read(&mut Cursor::new(out.as_slice())).unwrap(),decoded);
        let absent = [1, b'x', 2, 8, 0, 1, b'x', 0, 0];
        let decoded=ClientboundSetObjective::azalea_read(&mut Cursor::new(absent.as_slice())).unwrap();
        let mut out=Vec::new();decoded.azalea_write(&mut out).unwrap();
        assert_eq!(ClientboundSetObjective::azalea_read(&mut Cursor::new(out.as_slice())).unwrap(),decoded);
    }
}
