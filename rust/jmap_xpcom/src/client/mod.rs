/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! JMAP client implementation using moz_http for HTTP transport.

pub mod operations;

use crate::error::JmapError;
use crate::types::{
    EmailGetResponse, EmailQueryResponse, IdentityGetResponse, JmapMethodCall, JmapMethodResponse,
    MailboxGetResponse, Session, UploadResult,
};
use log::info;
use moz_http::Client;
use std::sync::Mutex;
use url::Url;

/// The JMAP client.
///
/// Handles session discovery, API communication, and blob download/upload.
/// Uses `moz_http::Client` for HTTP transport.
pub struct JmapClient {
    http_client: Client,
    endpoint: Url,
    session: Mutex<Option<SessionData>>,
}

/// Cached session information.
struct SessionData {
    api_url: Url,
    download_url: String,
    upload_url: String,
    account_id: String,
}

impl JmapClient {
    /// Create a new JMAP client targeting the given endpoint URL.
    pub fn new(endpoint: Url) -> Result<Self, JmapError> {
        let http_client = Client::new();
        Ok(Self {
            http_client,
            endpoint,
            session: Mutex::new(None),
        })
    }

    /// Discover the JMAP session and cache it.
    pub fn discover_session(&self) -> Result<(), JmapError> {
        // Synchronous session discovery using the XPCOM main thread.
        // The session URL is constructed from the endpoint.
        let session_url_str = format!(
            "{}/.well-known/jmap/session",
            self.endpoint.as_str().trim_end_matches('/')
        );
        info!("JMAP: will discover session at {}", session_url_str);
        // The actual HTTP call is done asynchronously in initialize().
        Ok(())
    }

    /// Initialize the session from a previously fetched JSON body.
    pub fn init_session_from_json(&self, body: &str) -> Result<(), JmapError> {
        let session: Session = serde_json::from_str(body)?;

        // Find the primary account
        let account_id = session
            .primary_accounts
            .iter()
            .find(|(_, uri)| *uri == "urn:ietf:params:jmap:mail" || uri.starts_with("urn:ietf:params:jmap:"))
            .map(|(id, _)| id.clone())
            .or_else(|| session.accounts.keys().next().cloned())
            .ok_or(JmapError::AccountNotFound)?;

        // Get API URL from session or construct from endpoint
        let api_url = if let Some(ref api) = session.api_url {
            Url::parse(api)?
        } else {
            let mut base = self.endpoint.clone();
            base.set_path("jmap");
            base
        };

        let download_url = session.download_url.unwrap_or_else(|| {
            format!(
                "{}/jmap/download/{{accountId}}/{{blobId}}/{{name}}",
                self.endpoint.as_str().trim_end_matches('/')
            )
        });

        let upload_url = session.upload_url.unwrap_or_else(|| {
            format!(
                "{}/jmap/upload/{{accountId}}",
                self.endpoint.as_str().trim_end_matches('/')
            )
        });

        let mut session_guard = self.session.lock().map_err(|e| {
            JmapError::Processing(format!("session lock poisoned: {e}"))
        })?;

        *session_guard = Some(SessionData {
            api_url,
            download_url,
            upload_url,
            account_id: account_id.clone(),
        });

        info!("JMAP: session initialized, account_id={}", account_id);
        Ok(())
    }

    /// Get the current account ID.
    pub fn account_id(&self) -> Result<String, JmapError> {
        let guard = self.session.lock().map_err(|e| {
            JmapError::Processing(format!("session lock poisoned: {e}"))
        })?;
        guard
            .as_ref()
            .map(|s| s.account_id.clone())
            .ok_or(JmapError::NotInitialized)
    }

    /// Get the API URL.
    fn api_url(&self) -> Result<Url, JmapError> {
        let guard = self.session.lock().map_err(|e| {
            JmapError::Processing(format!("session lock poisoned: {e}"))
        })?;
        guard
            .as_ref()
            .map(|s| s.api_url.clone())
            .ok_or(JmapError::NotInitialized)
    }

    /// Get the endpoint URL.
    pub fn endpoint(&self) -> &Url {
        &self.endpoint
    }

    /// Get a reference to the HTTP client.
    pub fn http_client(&self) -> &Client {
        &self.http_client
    }

    /// Execute a JMAP request and return the method responses.
    pub async fn execute(
        &self,
        method_calls: Vec<JmapMethodCall>,
    ) -> Result<Vec<JmapMethodResponse>, JmapError> {
        let api_url = self.api_url()?;

        // Build the request body
        let calls_json: Vec<serde_json::Value> = method_calls
            .iter()
            .map(|mc| serde_json::json!([&mc.method, &mc.args, &mc.call_id]))
            .collect();

        let body = serde_json::json!({
            "using": ["urn:ietf:params:jmap:core", "urn:ietf:params:jmap:mail"],
            "methodCalls": calls_json
        });

        let json = serde_json::to_string(&body)?;
        info!("JMAP: POST to {} ({} bytes)", api_url, json.len());

        let response = self
            .http_client
            .post(&api_url)?
            .header("Content-Type", "application/json; charset=utf-8")
            .header("Accept", "application/json")
            .body(json.as_str(), "application/json; charset=utf-8")
            .send()
            .await?;

        let resp_body = std::str::from_utf8(response.body())?;

        // Parse Stalwart-style response: {"methodResponses": [...]}
        let jmap_response: crate::types::JmapResponse = serde_json::from_str(resp_body)?;
        Ok(jmap_response.method_responses)
    }

    /// Execute a single JMAP method call.
    pub async fn execute_single(
        &self,
        method: &str,
        args: serde_json::Value,
        call_id: &str,
    ) -> Result<serde_json::Value, JmapError> {
        let method_call = JmapMethodCall::new(method, args, call_id);
        let responses = self.execute(vec![method_call]).await?;

        if let Some(resp) = responses.into_iter().next() {
            // Check if the response indicates an error
            if let Some(type_) = resp.args.get("type").and_then(|v| v.as_str()) {
                if !is_response_type(type_) {
                    return Err(JmapError::from_method_error(method, &resp.args));
                }
            }
            Ok(resp.args)
        } else {
            Err(JmapError::UnexpectedResponse(
                "no response received".to_string(),
            ))
        }
    }

    /// Download a blob from the server.
    pub async fn download_blob(
        &self,
        account_id: &str,
        blob_id: &str,
        name: &str,
    ) -> Result<Vec<u8>, JmapError> {
        let guard = self.session.lock().map_err(|e| {
            JmapError::Processing(format!("session lock poisoned: {e}"))
        })?;
        let session = guard.as_ref().ok_or(JmapError::NotInitialized)?;

        let url_str = session
            .download_url
            .replace("{accountId}", account_id)
            .replace("{blobId}", blob_id)
            .replace("{name}", name);

        let url = Url::parse(&url_str)?;
        let response = self
            .http_client
            .get(&url)?
            .header("Accept", "message/rfc822, application/octet-stream")
            .send()
            .await?;

        Ok(response.body().to_vec())
    }

    /// Upload a blob to the server.
    pub async fn upload_blob(
        &self,
        data: &[u8],
        content_type: &str,
    ) -> Result<UploadResult, JmapError> {
        let guard = self.session.lock().map_err(|e| {
            JmapError::Processing(format!("session lock poisoned: {e}"))
        })?;
        let session = guard.as_ref().ok_or(JmapError::NotInitialized)?;

        let url_str = session.upload_url.replace("{accountId}", &session.account_id);
        let url = Url::parse(&url_str)?;

        let response = self
            .http_client
            .post(&url)?
            .header("Content-Type", content_type)
            .header("Accept", "application/json")
            .body(data, content_type)
            .send()
            .await?;

        let body = std::str::from_utf8(response.body())?;
        let result: UploadResult = serde_json::from_str(body)?;
        Ok(result)
    }
}

/// Check if a response type is a success type (not an error).
fn is_response_type(type_: &str) -> bool {
    matches!(
        type_,
        "Mailbox/get"
            | "Email/get"
            | "Email/query"
            | "Email/set"
            | "Mailbox/set"
            | "Email/copy"
            | "EmailSubmission/set"
            | "Identity/get"
            | "Blob/upload"
            | "Blob/get"
    )
}
