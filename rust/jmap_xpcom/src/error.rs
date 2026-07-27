/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fmt;

use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_UNEXPECTED};
use serde_json::Value;

/// Errors that can occur during JMAP operations.
#[derive(Debug, thiserror::Error)]
pub enum JmapError {
    #[error("JMAP request failed with error type `{type_}`: {detail}")]
    RequestError {
        type_: String,
        status: u32,
        detail: String,
    },

    #[error("JMAP method `{method}` failed: {description}")]
    MethodError {
        method: String,
        type_: String,
        description: String,
    },

    #[error("state mismatch: server state has changed since our last sync")]
    StateMismatch,

    #[error("too many changes: delta sync not possible, full sync required")]
    TooManyChanges,

    #[error("could not parse JMAP response as JSON: {0}")]
    JsonParse(#[from] serde_json::Error),

    #[error("could not parse JMAP response at path: {0}")]
    JsonPathParse(#[from] serde_path_to_error::Error<serde_json::Error>),

    #[error("unexpected JMAP response: {0}")]
    UnexpectedResponse(String),

    #[error("missing required field `{field}` in JMAP response")]
    MissingField { field: String },

    #[error("missing ID in response")]
    MissingIdInResponse,

    #[error("HTTP error: {0}")]
    Http(#[from] moz_http::Error),

    #[error("invalid URL: {0}")]
    InvalidUrl(#[from] url::ParseError),

    #[error("session not initialized")]
    NotInitialized,

    #[error("account not found")]
    AccountNotFound,

    #[error("JMAP capability not supported: {0}")]
    CapabilityNotSupported(String),

    #[error("transport security failure: {status}")]
    TransportSecurityFailure { status: nsresult },

    #[error("XPCOM operation failure: {0}")]
    XpComFailure(String),

    #[error("{0}")]
    Processing(String),
}

impl JmapError {
    /// Parse a JMAP "type" field from a method response error.
    pub fn from_method_error(method: &str, args: &Value) -> Self {
        let type_ = args
            .get("type")
            .and_then(|v| v.as_str())
            .unwrap_or("unknown");
        let description = args
            .get("description")
            .and_then(|v| v.as_str())
            .unwrap_or("no description");
        JmapError::MethodError {
            method: method.to_string(),
            type_: type_.to_string(),
            description: description.to_string(),
        }
    }

    /// Check if this is a "stateMismatch" error.
    pub fn is_state_mismatch(&self) -> bool {
        matches!(
            self,
            JmapError::MethodError { type_, .. } if type_ == "stateMismatch"
        )
    }
}

impl From<JmapError> for nsresult {
    fn from(err: JmapError) -> nsresult {
        match err {
            JmapError::NotInitialized => NS_ERROR_UNEXPECTED,
            JmapError::AccountNotFound => NS_ERROR_FAILURE,
            JmapError::StateMismatch => NS_ERROR_FAILURE,
            JmapError::TooManyChanges => NS_ERROR_FAILURE,
            JmapError::Http(e) => e.into(),
            JmapError::TransportSecurityFailure { status } => status,
            _ => NS_ERROR_FAILURE,
        }
    }
}

impl From<nsresult> for JmapError {
    fn from(rv: nsresult) -> JmapError {
        JmapError::XpComFailure(format!("nsresult: {rv}"))
    }
}

impl fmt::Display for nsresult {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "nsresult({})", self.0)
    }
}

/// Helper to extract the JMAP error type from method response arguments.
pub fn get_error_type(args: &Value) -> Option<&str> {
    args.get("type").and_then(|v| v.as_str())
}

/// Check if a method response indicates an error.
pub fn is_method_error(args: &Value) -> bool {
    matches!(get_error_type(args), Some(t) if t != "success")
}
