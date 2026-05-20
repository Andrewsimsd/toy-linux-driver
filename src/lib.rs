//! User-space interface for sending fixed-width messages to `/dev/messagesender`.

use std::fmt::{Display, Formatter};
use std::fs::OpenOptions;
use std::io::{BufWriter, Write};
use std::num::ParseIntError;
use std::path::Path;
use std::thread;
use std::time::Duration;

/// Linux device path used by the example sender application.
pub const DEVICE_PATH: &str = "/dev/messagesender";
/// Number of bytes written for each message.
pub const MESSAGE_SIZE_BYTES: usize = 16;

/// Payload sent to the kernel driver.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Message {
    /// First 32-bit value.
    pub a: u32,
    /// Second 32-bit value.
    pub b: u32,
    /// Third 32-bit value.
    pub c: u32,
    /// Fourth 32-bit value.
    pub d: u32,
}

impl Message {
    /// Creates a message from a base counter value.
    #[must_use]
    pub fn from_base(base: u32) -> Self {
        Self {
            a: base,
            b: base.saturating_add(1),
            c: base.saturating_add(2),
            d: base.saturating_add(3),
        }
    }

    /// Serializes the message as little-endian bytes.
    #[must_use]
    pub fn as_le_bytes(self) -> [u8; MESSAGE_SIZE_BYTES] {
        let mut out = [0_u8; MESSAGE_SIZE_BYTES];
        out[0..4].copy_from_slice(&self.a.to_le_bytes());
        out[4..8].copy_from_slice(&self.b.to_le_bytes());
        out[8..12].copy_from_slice(&self.c.to_le_bytes());
        out[12..16].copy_from_slice(&self.d.to_le_bytes());
        out
    }
}

/// Runtime configuration for the sender loop.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct AppConfig {
    /// Delay between writes in milliseconds.
    pub delay_ms: u64,
    /// Optional iteration limit. If `None`, runs forever.
    pub iterations: Option<u32>,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            delay_ms: 100,
            iterations: None,
        }
    }
}

/// Errors returned by the sender application.
#[derive(Debug)]
pub enum AppError {
    /// Provided CLI argument could not be parsed.
    InvalidNumber {
        /// Argument name.
        argument: &'static str,
        /// Parser error.
        source: ParseIntError,
    },
    /// Device I/O failure.
    Io(std::io::Error),
}

impl Display for AppError {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidNumber { argument, source } => {
                write!(f, "invalid value for {argument}: {source}")
            }
            Self::Io(source) => write!(f, "i/o error: {source}"),
        }
    }
}

impl std::error::Error for AppError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::InvalidNumber { source, .. } => Some(source),
            Self::Io(source) => Some(source),
        }
    }
}

impl From<std::io::Error> for AppError {
    fn from(value: std::io::Error) -> Self {
        Self::Io(value)
    }
}

/// Parses command line arguments into [`AppConfig`].
///
/// Supported arguments:
///
/// - `--delay-ms <u64>`: delay between writes.
/// - `--iterations <u32>`: optional finite run count for testing/demo.
///
/// # Errors
///
/// Returns an error when arguments are unknown, missing required values,
/// or contain invalid numeric values.
pub fn parse_config(args: &[String]) -> Result<AppConfig, AppError> {
    let mut cfg = AppConfig::default();
    let mut index = 1;

    while index < args.len() {
        match args[index].as_str() {
            "--delay-ms" => {
                let value = args.get(index + 1).ok_or_else(|| {
                    AppError::Io(std::io::Error::new(
                        std::io::ErrorKind::InvalidInput,
                        "missing value for --delay-ms",
                    ))
                })?;
                cfg.delay_ms = value
                    .parse::<u64>()
                    .map_err(|source| AppError::InvalidNumber {
                        argument: "--delay-ms",
                        source,
                    })?;
                index += 2;
            }
            "--iterations" => {
                let value = args.get(index + 1).ok_or_else(|| {
                    AppError::Io(std::io::Error::new(
                        std::io::ErrorKind::InvalidInput,
                        "missing value for --iterations",
                    ))
                })?;
                cfg.iterations =
                    Some(
                        value
                            .parse::<u32>()
                            .map_err(|source| AppError::InvalidNumber {
                                argument: "--iterations",
                                source,
                            })?,
                    );
                index += 2;
            }
            unknown => {
                return Err(AppError::Io(std::io::Error::new(
                    std::io::ErrorKind::InvalidInput,
                    format!("unknown argument: {unknown}"),
                )));
            }
        }
    }

    Ok(cfg)
}

/// Runs the producer loop and writes messages to the given device path.
///
/// # Errors
///
/// Returns an error when the device cannot be opened or a write/flush
/// operation fails.
pub fn run_loop<P: AsRef<Path>>(device_path: P, config: AppConfig) -> Result<(), AppError> {
    let file = OpenOptions::new().write(true).open(device_path)?;
    let mut writer = BufWriter::new(file);
    let delay = Duration::from_millis(config.delay_ms);

    let mut sent: u32 = 0;
    let mut base: u32 = 0;

    loop {
        if let Some(limit) = config.iterations
            && sent >= limit
        {
            break;
        }

        let message = Message::from_base(base);
        writer.write_all(&message.as_le_bytes())?;
        writer.flush()?;

        sent = sent.saturating_add(1);
        base = base.saturating_add(4);
        thread::sleep(delay);
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::{MESSAGE_SIZE_BYTES, Message, parse_config};

    #[test]
    fn message_to_bytes_serializes_all_fields_in_order() {
        let message = Message {
            a: 1,
            b: 2,
            c: 3,
            d: 4,
        };

        let raw = message.as_le_bytes();

        assert_eq!(raw.len(), MESSAGE_SIZE_BYTES);
        assert_eq!(&raw[0..4], &1_u32.to_le_bytes());
        assert_eq!(&raw[4..8], &2_u32.to_le_bytes());
        assert_eq!(&raw[8..12], &3_u32.to_le_bytes());
        assert_eq!(&raw[12..16], &4_u32.to_le_bytes());
    }

    #[test]
    fn parse_config_reads_delay_and_iterations() {
        let args = vec![
            "toy-linux-driver".to_owned(),
            "--delay-ms".to_owned(),
            "5".to_owned(),
            "--iterations".to_owned(),
            "10".to_owned(),
        ];

        let config = parse_config(&args).expect("config should parse");

        assert_eq!(config.delay_ms, 5);
        assert_eq!(config.iterations, Some(10));
    }
}
