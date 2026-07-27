/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! JMAP HTTP client for communicating with a JMAP server.
//!
//! Handles session discovery, request batching, and response parsing.
//! All JMAP API calls go through POST to the apiUrl discovered in the session.

use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;

use http::Method;
use log::{debug, info, warn};
use moz_http::Client;
use protocol_shared::{
    ServerType,
    authentication::credentials::AuthenticationProvider,
    client::ProtocolClient,
    operation_sender::{
        OperationSender, OperationRequestOptions, ResponseProcessor, TransportSecFailureBehavior,
    },
};
use url::Url;

use crate::error::JmapError;
use crate::types::*;

static CALL_ID_COUNTER: AtomicU32 = AtomicU32::new(0);

/// Generate a unique JMAP call ID.
pub fn generate_call_id() -> String {
    format!("tb{}", CALL_ID_COUNTER.fetch_add(1, Ordering::SeqCst))
}

/// The main JMAP client that handles all communication with a JMAP server.
///
/// Uses the `moz_http` client (Necko) for HTTP and serde_json for JSON.
/// Follows the same pattern as the EWS XpComEwsClient but for JMAP's
/// JSON-over-HTTPS protocol.
pub struct JmapClient<ServerT: ServerType + 'static> {
    /// The HTTP client.
    http_client: Client,

    /// Discovered session information.
    session: once_cell::sync::OnceCell<Session>,

    /// The configured endpoint URL (before session discovery).
    configured_url: Url,

    /// The JMAP account ID for this client.
    account_id: once_cell::sync::OnceCell<String>,

    /// The server reference for authentication.
    server: RefPtr<ServerT>,

    /// Operation sender for authenticated requests.
    op_sender: Option<Arc<OperationSender<ServerT>>>,
}

impl<ServerT: ServerType + 'static> JmapClient<ServerT> {
    /// Create a new JMAP client.
    ///
    /// `configured_url` is the user-supplied JMAP endpoint. Session discovery
    /// will try `.well-known/jmap` first, then use this URL directly.
    pub fn new(configured_url: Url, server: RefPtr<ServerT>) -> Result<Self, JmapError> {
        Ok(Self {
            http_client: Client::new(),
            session: once_cell::sync::OnceCell::new(),
            configured_url,
            account_id: once_cell::sync::OnceCell::new(),
            server,
            op_sender: None,
        })
    }

    /// Get the JMAP account ID, discovering it via session if needed.
    pub fn account_id(&self) -> Result<&str, JmapError> {
        self.account_id.get().map(|s| s.as_str()).ok_or(JmapError::NotInitialized)
    }

    /// Get the API URL from the session.
    pub fn api_url(&self) -> Result<Url, JmapError> {
        let session = self.session.get().ok_or(JmapError::NotInitialized)?;
        session
            .urls
            .api_url
            .as_ref()
            .map(|u| Url::parse(u))
            .transpose()?
            .ok_or(JmapError::MissingField {
                field: "urls.apiUrl".to_string(),
            })
    }

    /// Discover and cache the JMAP session.
    ///
    /// Tries `/.well-known/jmap` on the configured host first. If that fails,
    /// uses the configured URL directly as the session endpoint.
    pub async fn discover_session(&self) -> Result<&Session, JmapError> {
        if let Some(session) = self.session.get() {
            return Ok(session);
        }

        // Try .well-known/jmap first (RFC 8620 §2)
        let well_known = self.build_well_known_url()?;
        info!("JMAP session discovery: trying {}", well_known);

        match self.fetch_session_json(&well_known).await {
            Ok(session) => {
                info!(
                    "JMAP session discovered via .well-known/jmap, api_url={:?}",
                    session.urls.api_url
                );
                return Ok(self.session.get_or_init(|| session));
            }
            Err(e) => {
                warn!("JMAP .well-known/jmap failed: {}, trying direct URL", e);
            }
        }

        // Fall back to the configured URL directly
        info!("JMAP session discovery: trying {}", self.configured_url);
        let session = self.fetch_session_json(&self.configured_url).await?;
        info!(
            "JMAP session discovered via direct URL, api_url={:?}",
            session.urls.api_url
        );
        Ok(self.session.get_or_init(|| session))
    }

    /// Build the .well-known/jmap URL from the configured URL.
    fn build_well_known_url(&self) -> Result<Url, JmapError> {
        let mut well_known = self.configured_url.clone();

        // Strip any path from the configured URL
        well_known.set_path("");
        well_known.set_query(None);
        well_known.set_fragment(None);

        let path = format!(
            "{}://{}{}/.well-known/jmap/session",
            well_known.scheme(),
            well_known.host_str().unwrap_or(""),
            if let Some(port) = well_known.port() {
                format!(":{}", port)
            } else {
                String::new()
            }
        );

        Url::parse(&path).map_err(JmapError::InvalidUrl)
    }

    /// Fetch and parse the session JSON from a URL.
    async fn fetch_session_json(&self, url: &Url) -> Result<Session, JmapError> {
        let response = self
            .http_client
            .get(url)
            .and_then(|req| req.header("Accept", "application/json"))
            .send()
            .await?
            .error_from_status()?;

        let body = std::str::from_utf8(response.body())
            .map_err(|e| JmapError::UnexpectedResponse(format!("session response not UTF-8: {e}")))?;

        let session: Session = serde_json::from_str(body)?;
        Ok(session)
    }

    /// Set the primary account ID for mail operations.
    pub fn set_account_id(&self, account_id: String) -> Result<(), JmapError> {
        if self.account_id.set(account_id).is_err() {
            warn!("JMAP account ID already set, ignoring");
        }
        Ok(())
    }

    /// Resolve the primary mail account from the session.
    pub fn resolve_mail_account(&self) -> Result<String, JmapError> {
        let session = self.session.get().ok_or(JmapError::NotInitialized)?;

        let mail_urn = "urn:ietf:params:jmap:mail";
        let account_id = session
            .primary_accounts
            .get(mail_urn)
            .ok_or_else(|| {
                if session.accounts.is_empty() {
                    JmapError::AccountNotFound
                } else {
                    // Fallback: use the first account
                    session
                        .accounts
                        .keys()
                        .next()
                        .cloned()
                        .ok_or(JmapError::AccountNotFound)
                }
            })?;

        Ok(account_id.clone())
    }

    /// Check connectivity by performing a lightweight API request.
    pub async fn check_connectivity(&self) -> Result<Url, JmapError> {
        let session = self.discover_session().await?;
        let api_url = self.api_url()?;

        // Do a simple Ping/empty request to verify auth works
        let request = Request {
            using: vec!["urn:ietf:params:jmap:core".to_string()],
            method_calls: vec![],
        };

        let response_body = self
            .post_json(&api_url, &request)
            .await
            .map_err(|e| {
                if matches!(e, JmapError::Http(_)) {
                    JmapError::AccountNotFound
                } else {
                    e
                }
            })?;

        debug!("Connectivity check succeeded, response: {}", response_body);

        // Resolve and cache the mail account ID
        let acct_id = self.resolve_mail_account()?;
        self.set_account_id(acct_id)?;

        Ok(api_url)
    }

    /// Send a JMAP API request (POST with JSON body).
    ///
    /// This is the core method — all JMAP operations go through here.
    /// Handles authentication via the Authorization header.
    pub async fn post_json<T: serde::Serialize>(
        &self,
        url: &Url,
        body: &T,
    ) -> Result<String, JmapError> {
        let json = serde_json::to_string(body)?;

        debug!("JMAP POST to {}: {}", url, json);

        let response = self
            .http_client
            .post(url)
            .and_then(|req| {
                req.header("Content-Type", "application/json; charset=utf-8")
                    .header("Accept", "application/json")
                    .body(&json, "application/json; charset=utf-8")
            })
            .send()
            .await?
            .error_from_status()?;

        let body_str = std::str::from_utf8(response.body())
            .map_err(|e| JmapError::UnexpectedResponse(format!("response not UTF-8: {e}")))?;

        Ok(body_str.to_string())
    }

    /// Execute a single JMAP method call.
    ///
    /// Wraps the call in a proper JMAP Request, sends it, and returns
    /// the parsed response.
    pub async fn call_method(
        &self,
        method: &str,
        arguments: &serde_json::Value,
        capabilities: &[&str],
    ) -> Result<serde_json::Value, JmapError> {
        let api_url = self.api_url()?;

        let call_id = generate_call_id();
        let using: Vec<String> = capabilities.iter().map(|s| s.to_string()).collect();

        let request = Request {
            using,
            method_calls: vec![MethodCall::new(
                method,
                arguments.clone(),
                &call_id,
            )],
        };

        let response_str = self.post_json(&api_url, &request).await?;

        // Parse response
        let response: Response = match serde_json::from_str(&response_str) {
            Ok(r) => r,
            Err(e) => {
                // Try to parse as ProblemDetails first
                if let Ok(problem) = serde_json::from_str::<ProblemDetails>(&response_str) {
                    return Err(JmapError::RequestError {
                        type_: problem.error_type,
                        status: problem.status.unwrap_or(0),
                        detail: problem.detail.unwrap_or_default(),
                    });
                }
                return Err(e.into());
            }
        };

        // Find the matching response
        for method_resp in &response.method_responses {
            if method_resp.call_id == call_id {
                let args = &method_resp.arguments;
                if is_method_error(args) {
                    return Err(JmapError::from_method_error(method, args));
                }
                return Ok(args.clone());
            }
        }

        Err(JmapError::UnexpectedResponse(
            "no matching response for call ID".to_string(),
        ))
    }

    /// Execute multiple JMAP method calls in a single request (batching).
    pub async fn call_batch(
        &self,
        calls: Vec<(&str, serde_json::Value)>,
        capabilities: &[&str],
    ) -> Result<Vec<(String, serde_json::Value)>, JmapError> {
        let api_url = self.api_url()?;

        let using: Vec<String> = capabilities.iter().map(|s| s.to_string()).collect();
        let method_calls: Vec<MethodCall> = calls
            .into_iter()
            .map(|(method, args)| MethodCall::new(method, args, &generate_call_id()))
            .collect();

        let request = Request {
            using,
            method_calls,
        };

        let response_str = self.post_json(&api_url, &request).await?;
        let response: Response = serde_json::from_str(&response_str)?;

        Ok(response
            .method_responses
            .into_iter()
            .map(|mr| (mr.call_id, mr.arguments))
            .collect())
    }

    /// Download a blob from the JMAP server.
    ///
    /// Uses the downloadUrl template from the session.
    pub async fn download_blob(
        &self,
        account_id: &str,
        blob_id: &str,
        name: &str,
    ) -> Result<Vec<u8>, JmapError> {
        let session = self.discover_session().await?;
        let template = session
            .download_url
            .as_deref()
            .unwrap_or("/jmap/download/{accountId}/{blobId}/{name}");

        let url_str = template
            .replace("{accountId}", account_id)
            .replace("{blobId}", blob_id)
            .replace("{name}", name);

        let url = Url::parse(&url_str)?;
        let response = self.http_client.get(&url).send().await?.error_from_status()?;

        Ok(response.body().to_vec())
    }

    /// Upload a blob to the JMAP server.
    ///
    /// Uses the uploadUrl template from the session.
    pub async fn upload_blob(
        &self,
        account_id: &str,
        data: &[u8],
        content_type: &str,
    ) -> Result<UploadResult, JmapError> {
        let session = self.discover_session().await?;
        let template = session
            .upload_url
            .as_deref()
            .unwrap_or("/jmap/upload/{accountId}");

        let url_str = template.replace("{accountId}", account_id);
        let url = Url::parse(&url_str)?;

        let response = self
            .http_client
            .post(&url)
            .and_then(|req| {
                req.header("Content-Type", content_type)
                    .body(data, content_type)
            })
            .send()
            .await?
            .error_from_status()?;

        let body = std::str::from_utf8(response.body())?;
        Ok(serde_json::from_str(body)?)
    }

    pub fn running(&self) -> bool {
        self.session.get().is_some()
    }

    pub fn idle(&self) -> bool {
        true // TODO: track pending operations
    }
}

/// Result of a blob upload.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UploadResult {
    pub account_id: String,
    pub id: String,
    pub blob_id: String,
    pub size: u64,
    #[serde(rename = "type")]
    pub content_type: String,
}

impl<ServerT: ServerType + 'static> ProtocolClient for JmapClient<ServerT> {
    fn protocol_identifier(&self) -> String {
        String::from("jmap")
    }

    async fn shutdown(self: Arc<Self>) {
        info!("JMAP client shutting down");
    }
}

use xpcom::RefPtr;
