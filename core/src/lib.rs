//! Local, deterministic orchestration kernel. No network, shell execution or model calls.
//! Caller identities must come from a trusted host; this crate is not an authentication service.
mod catalog;
mod engine;
mod environment;
mod lineage;
mod model;
mod permissions;
mod tasks;

pub use catalog::*;
pub use engine::{Clock, Engine, SystemClock};
pub use environment::*;
pub use lineage::*;
pub use model::*;
pub use permissions::{Grant, GrantRequest};
pub use tasks::*;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("invalid contract: {0}")]
    Invalid(String),
    #[error("access denied: {0}")]
    Denied(String),
    #[error("conflict: {0}")]
    Conflict(String),
    #[error("not found: {0}")]
    NotFound(String),
    #[error(transparent)]
    Sql(#[from] rusqlite::Error),
    #[error(transparent)]
    Json(#[from] serde_json::Error),
    #[error(transparent)]
    Io(#[from] std::io::Error),
}
pub type Result<T> = std::result::Result<T, Error>;
