/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! JMAP error values.

use nserror::nsresult;
use protocol_shared::error::ProtocolError;
use serde_json::Value;
use thiserror::Error;

/// Error types for JMAP operations.
#[derive(Debug, Error)]
pub(crate) enum JmapError {
    #[error(transparent)]
    Protocol(#[from] ProtocolError),

    #[error("invalid URL: {0}")]
    Url(#[from] url::ParseError),

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
    Json(#[from] serde_json::Error),

    #[error("unexpected JMAP response: {0}")]
    UnexpectedResponse(String),

    #[error("missing required field `{field}` in JMAP response")]
    MissingField { field: String },

    #[error("session not initialized")]
    NotInitialized,

    #[error("account not found")]
    AccountNotFound,

    #[error("UTF-8 error: {0}")]
    Utf8(#[from] std::str::Utf8Error),

    #[error("error in processing: {0}")]
    Processing(String),
}

impl From<&JmapError> for nsresult {
    fn from(value: &JmapError) -> Self {
        match value {
            JmapError::Protocol(err) => err.into(),
            JmapError::NotInitialized => nserror::NS_ERROR_NOT_INITIALIZED,
            JmapError::StateMismatch => nserror::NS_ERROR_FAILURE,
            JmapError::TooManyChanges => nserror::NS_ERROR_FAILURE,
            JmapError::AccountNotFound => nserror::NS_ERROR_FAILURE,

            _ => nserror::NS_ERROR_UNEXPECTED,
        }
    }
}

impl From<JmapError> for nsresult {
    fn from(value: JmapError) -> Self {
        (&value).into()
    }
}

impl From<nsresult> for JmapError {
    fn from(value: nsresult) -> Self {
        JmapError::Protocol(ProtocolError::XpCom(value))
    }
}

impl From<moz_http::Error> for JmapError {
    fn from(value: moz_http::Error) -> Self {
        JmapError::Protocol(ProtocolError::Http(value))
    }
}

impl<'a> TryFrom<&'a JmapError> for &'a moz_http::Error {
    type Error = ();

    fn try_from(value: &'a JmapError) -> Result<Self, Self::Error> {
        match value {
            JmapError::Protocol(ProtocolError::Http(err)) => Ok(err),
            _ => Err(()),
        }
    }
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
