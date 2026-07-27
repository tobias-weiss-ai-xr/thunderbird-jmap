/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Core JMAP data types defined by RFC 8620 (JMAP Core) and RFC 8621 (JMAP Mail).
//!
//! All types use serde for JSON serialization/deserialization. These map
//! directly to the JMAP JSON structures.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

// ---------------------------------------------------------------------------
// RFC 8620 §2 — Session
// ---------------------------------------------------------------------------

/// A JMAP Session resource, returned by GET to the session URL.
///
/// ```json
/// {
///   "capabilities": { ... },
///   "accounts": { ... },
///   "primaryAccounts": { ... },
///   "urls": { ... },
///   ...
/// }
/// ```
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Session {
    #[serde(default)]
    pub capabilities: HashMap<String, serde_json::Value>,

    pub accounts: HashMap<String, Account>,

    #[serde(default)]
    pub primary_accounts: HashMap<String, String>,

    pub urls: SessionUrls,

    #[serde(default)]
    pub download_url: Option<String>,

    #[serde(default)]
    pub upload_url: Option<String>,

    #[serde(default)]
    pub event_source_url: Option<String>,

    #[serde(default)]
    pub max_concurrent_upload: Option<u32>,

    #[serde(default)]
    pub max_concurrent_requests: Option<u32>,

    #[serde(default)]
    pub max_size_upload: Option<u64>,

    #[serde(default)]
    pub max_requests_in_batch: Option<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Account {
    pub name: String,

    #[serde(default)]
    pub is_personal: Option<bool>,

    #[serde(default)]
    pub is_read_only: Option<bool>,

    #[serde(default)]
    pub account_capabilities: HashMap<String, serde_json::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SessionUrls {
    #[serde(rename = "apiUrl")]
    pub api_url: Option<String>,
}

// ---------------------------------------------------------------------------
// RFC 8620 §3.2 — Request / Response
// ---------------------------------------------------------------------------

/// A JMAP API Request.
///
/// ```json
/// [
///   ["using", ["urn:ietf:params:jmap:mail", ...]],
///   [ [ "Mailbox/get", {...}, "c1" ], [ "Email/query", {...}, "c2" ] ]
/// ]
/// ```
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Request {
    #[serde(rename = "using")]
    pub using: Vec<String>,

    pub method_calls: Vec<MethodCall>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MethodCall {
    /// e.g. "Mailbox/get", "Email/changes"
    pub name: String,

    /// Method-specific arguments as a JSON object
    #[serde(rename = "arguments")]
    pub arguments: serde_json::Value,

    /// Client-generated call ID for referencing responses
    #[serde(rename = "callId")]
    pub call_id: String,
}

impl MethodCall {
    pub fn new(name: &str, arguments: serde_json::Value, call_id: &str) -> Self {
        Self {
            name: name.to_string(),
            arguments,
            call_id: call_id.to_string(),
        }
    }
}

/// A JMAP API Response.
///
/// ```json
/// [
///   "urn:ietf:params:jmap:mail",
///   [ [ "Mailbox/get", {...}, "c1" ], ... ],
///   [ { "type": "unknownMethod", ... }, ... ]
/// ]
/// ```
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Response {
    #[serde(rename = "using")]
    pub using: Vec<String>,

    pub method_responses: Vec<MethodResponse>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MethodResponse {
    /// e.g. "Mailbox/get", "Email/changes"
    pub name: String,

    /// Method-specific response as JSON value
    #[serde(rename = "arguments")]
    pub arguments: serde_json::Value,

    /// Matches the request callId
    #[serde(rename = "callId")]
    pub call_id: String,
}

// ---------------------------------------------------------------------------
// RFC 8620 §3.6 — Problem Details
// ---------------------------------------------------------------------------

/// JMAP-specific problem details returned on errors.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProblemDetails {
    #[serde(rename = "type")]
    pub error_type: String,

    #[serde(default)]
    pub title: Option<String>,

    #[serde(default)]
    pub status: Option<u32>,

    #[serde(default)]
    pub detail: Option<String>,

    #[serde(default)]
    pub method_calls: Vec<serde_json::Value>,
}

// ---------------------------------------------------------------------------
// RFC 8621 §2 — Mailbox
// ---------------------------------------------------------------------------

/// A JMAP Mailbox object (RFC 8621 §2).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Mailbox {
    pub id: String,

    pub name: String,

    #[serde(default)]
    pub parent_id: Option<String>,

    #[serde(default)]
    pub role: Option<String>,

    #[serde(default)]
    pub sort_order: Option<u32>,

    #[serde(default)]
    pub total_emails: Option<u64>,

    #[serde(default)]
    pub unread_emails: Option<u64>,

    #[serde(default)]
    pub total_threads: Option<u64>,

    #[serde(default)]
    pub unread_threads: Option<u64>,

    #[serde(default)]
    pub my_rights: Option<MailboxRights>,

    #[serde(default)]
    pub is_subscribed: Option<bool>,

    #[serde(default)]
    pub quarantine: Option<bool>,

    #[serde(rename = "x-stalwart-jmap quotas", default)]
    pub stalwart_jmap_quotas: Option<serde_json::Value>,

    #[serde(flatten)]
    pub extra: HashMap<String, serde_json::Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct MailboxRights {
    #[serde(default)]
    pub may_read_items: Option<bool>,

    #[serde(default)]
    pub may_add_items: Option<bool>,

    #[serde(default)]
    pub may_remove_items: Option<bool>,

    #[serde(default)]
    pub may_set_seen: Option<bool>,

    #[serde(default)]
    pub may_set_keywords: Option<bool>,

    #[serde(default)]
    pub may_create_child: Option<bool>,

    #[serde(default)]
    pub may_rename: Option<bool>,

    #[serde(default)]
    pub may_delete: Option<bool>,

    #[serde(default)]
    pub may_submit: Option<bool>,
}

// ---------------------------------------------------------------------------
// RFC 8621 §4 — Email
// ---------------------------------------------------------------------------

/// A JMAP Email object (RFC 8621 §4).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Email {
    pub id: String,

    #[serde(default)]
    pub blob_id: Option<String>,

    #[serde(default)]
    pub thread_id: Option<String>,

    #[serde(default)]
    pub mailbox_ids: HashMap<String, bool>,

    #[serde(default)]
    pub keywords: HashSet,

    #[serde(default)]
    pub size: Option<u64>,

    #[serde(default)]
    pub received_at: Option<String>,

    #[serde(default)]
    pub message_id: Option<Option<String>>,

    #[serde(default)]
    pub in_reply_to: Option<Option<String>>,

    #[serde(default)]
    pub references: Option<Option<String>>,

    #[serde(default)]
    pub sender: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub from: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub to: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub cc: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub bcc: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub reply_to: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub subject: Option<Option<String>>,

    #[serde(default)]
    pub sent_at: Option<String>,

    #[serde(default)]
    pub preview: Option<String>,

    #[serde(default)]
    pub has_attachment: Option<bool>,

    #[serde(default)]
    pub headers: Option<Vec<Header>>,

    #[serde(default)]
    pub body_structure: Option<BodyStructure>,

    #[serde(default)]
    pub text_body: Option<Vec<BodyPart>>,

    #[serde(default)]
    pub html_body: Option<Vec<BodyPart>>,

    #[serde(default)]
    pub attachments: Option<Vec<BodyPart>>,

    #[serde(flatten)]
    pub extra: HashMap<String, serde_json::Value>,
}

/// JMAP keywords (RFC 8621 §4.4).
///
/// Special keywords: $draft, $seen, $flagged, $answered, $forwarded,
/// $junk, $notjunk, $phishing, $recent, $important
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
#[serde(transparent)]
pub struct HashSet {
    #[serde(default)]
    pub values: std::collections::HashSet<String>,
}

impl HashSet {
    pub fn new(values: Vec<&str>) -> Self {
        Self {
            values: values.into_iter().map(String::from).collect(),
        }
    }

    pub fn contains(&self, keyword: &str) -> bool {
        self.values.contains(keyword)
    }

    pub fn is_seen(&self) -> bool {
        self.contains("$seen")
    }

    pub fn is_flagged(&self) -> bool {
        self.contains("$flagged")
    }

    pub fn is_draft(&self) -> bool {
        self.contains("$draft")
    }

    pub fn is_junk(&self) -> bool {
        self.contains("$junk")
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailAddress {
    pub name: Option<String>,
    pub email: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Header {
    pub name: String,
    pub value: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BodyPart {
    pub part_id: String,

    #[serde(default)]
    pub blob_id: Option<String>,

    #[serde(default)]
    pub size: Option<u64>,

    #[serde(default)]
    pub headers: Option<Vec<Header>>,

    #[serde(default)]
    pub type_: Option<String>,

    #[serde(default)]
    pub charset: Option<String>,

    #[serde(default)]
    pub disposition: Option<String>,

    #[serde(default)]
    pub cid: Option<String>,

    #[serde(default)]
    pub language: Option<String>,

    #[serde(default)]
    pub location: Option<String>,

    #[serde(default)]
    pub sub_parts: Option<Vec<BodyPart>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BodyStructure {
    #[serde(rename = "type")]
    pub type_: String,
    pub sub_type: String,

    #[serde(default)]
    pub parts: Option<Vec<BodyPart>>,

    #[serde(default)]
    pub body_parts: Option<BodyPart>,

    #[serde(default)]
    pub blob_id: Option<String>,

    #[serde(default)]
    pub size: Option<u64>,

    #[serde(default)]
    pub headers: Option<Vec<Header>>,

    #[serde(default)]
    pub disposition: Option<String>,

    #[serde(default)]
    pub language: Option<Vec<String>>,

    #[serde(default)]
    pub location: Option<String>,

    #[serde(default)]
    pub charset: Option<String>,

    #[serde(default)]
    pub cid: Option<String>,
}

// ---------------------------------------------------------------------------
// RFC 8621 §5 — EmailSubmission
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailSubmission {
    pub id: String,

    #[serde(default)]
    pub identity_id: Option<String>,

    #[serde(default)]
    pub email_id: Option<String>,

    #[serde(default)]
    pub thread_id: Option<String>,

    #[serde(default)]
    pub envelope: Option<Envelope>,

    #[serde(default)]
    pub send_at: Option<String>,

    #[serde(default)]
    pub undo_status: Option<String>,

    #[serde(default)]
    pub dsn_algorithm: Option<String>,

    #[serde(default)]
    pub mdn_algorithm: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Envelope {
    #[serde(default)]
    pub mail_from: Option<EmailAddress>,

    #[serde(default)]
    pub rcpt_to: Vec<EmailAddress>,
}

// ---------------------------------------------------------------------------
// RFC 8621 §7 — Identity
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Identity {
    pub id: String,

    #[serde(default)]
    pub name: Option<String>,

    #[serde(default)]
    pub email: Option<String>,

    #[serde(default)]
    pub reply_to: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub bcc: Option<Vec<EmailAddress>>,

    #[serde(default)]
    pub html_signature: Option<String>,

    #[serde(default)]
    pub text_signature: Option<String>,
}

// ---------------------------------------------------------------------------
// Method arguments / response shapes
// ---------------------------------------------------------------------------

/// Response for Mailbox/get, Mailbox/changes, etc.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GetResponse<T> {
    #[serde(default)]
    pub account_id: Option<String>,

    #[serde(default)]
    pub state: Option<String>,

    #[serde(default)]
    pub list: Vec<T>,

    #[serde(default)]
    pub not_found: Option<Vec<String>>,
}

/// Response for * /changes
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChangesResponse {
    #[serde(default)]
    pub account_id: Option<String>,

    #[serde(default)]
    pub old_state: Option<String>,

    #[serde(default)]
    pub new_state: Option<String>,

    #[serde(default)]
    pub has_more_changes: Option<bool>,

    #[serde(default)]
    pub created: Option<Vec<String>>,

    #[serde(default)]
    pub updated: Option<Vec<String>>,

    #[serde(default)]
    pub destroyed: Option<Vec<String>>,
}

/// Response for * /set
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SetResponse<T> {
    #[serde(default)]
    pub account_id: Option<String>,

    #[serde(default)]
    pub old_state: Option<String>,

    #[serde(default)]
    pub new_state: Option<String>,

    #[serde(default)]
    pub created: Option<HashMap<String, T>>,

    #[serde(default)]
    pub updated: Option<HashMap<String, Option<serde_json::Value>>>,

    #[serde(default)]
    pub destroyed: Option<Vec<String>>,

    #[serde(default)]
    pub not_created: Option<HashMap<String, serde_json::Value>>,

    #[serde(default)]
    pub not_updated: Option<HashMap<String, serde_json::Value>>,

    #[serde(default)]
    pub not_destroyed: Option<HashMap<String, serde_json::Value>>,
}

/// Response for * /query
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QueryResponse {
    #[serde(default)]
    pub account_id: Option<String>,

    #[serde(default)]
    pub query_state: Option<String>,

    #[serde(default)]
    pub can_calculate_changes: Option<bool>,

    #[serde(default)]
    pub position: Option<u32>,

    #[serde(default)]
    pub ids: Vec<String>,

    #[serde(default)]
    pub total: Option<u32>,

    #[serde(default)]
    pub limit: Option<u32>,
}

/// Response for Email/copy
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CopyResponse {
    #[serde(default)]
    pub account_id: Option<String>,

    #[serde(default)]
    pub from_account_id: Option<String>,

    #[serde(default)]
    pub old_state: Option<String>,

    #[serde(default)]
    pub new_state: Option<String>,

    #[serde(default)]
    pub created: Option<HashMap<String, Email>>,

    #[serde(default)]
    pub not_created: Option<HashMap<String, serde_json::Value>>,
}

/// Response for Email/import
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ImportResponse {
    #[serde(default)]
    pub account_id: Option<String>,

    #[serde(default)]
    pub old_state: Option<String>,

    #[serde(default)]
    pub new_state: Option<String>,

    #[serde(default)]
    pub created: Option<HashMap<String, Email>>,

    #[serde(default)]
    pub not_created: Option<HashMap<String, serde_json::Value>>,
}

// ---------------------------------------------------------------------------
// RFC 8620 §6 — Push (EventSource)
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PushStateChange {
    #[serde(default)]
    pub changed: Vec<HashMap<String, String>>,

    #[serde(default)]
    pub source: Option<String>,
}
