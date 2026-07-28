/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! JMAP data types as defined in RFC 8620 and RFC 8621.
//!
//! All types use serde for JSON serialization/deserialization.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use time::OffsetDateTime;

// ---------------------------------------------------------------------------
// Core JMAP types (RFC 8620)
// ---------------------------------------------------------------------------

/// A JMAP request envelope.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JmapRequest {
    pub using: Vec<String>,
    pub method_calls: Vec<JmapMethodCall>,
}

/// A single JMAP method call: `[method, args, callId]`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JmapMethodCall {
    // Serialized as a JSON array of 3 elements
    pub method: String,
    pub args: serde_json::Value,
    pub call_id: String,
}

impl JmapMethodCall {
    pub fn new(method: &str, args: serde_json::Value, call_id: &str) -> Self {
        Self {
            method: method.to_string(),
            args,
            call_id: call_id.to_string(),
        }
    }
}

/// A JMAP response envelope.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JmapResponse {
    pub method_responses: Vec<JmapMethodResponse>,
}

/// A single JMAP method response: `[method, args, callId]`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JmapMethodResponse {
    pub method: String,
    pub args: serde_json::Value,
    pub call_id: String,
}

// ---------------------------------------------------------------------------
// Session object (RFC 8620)
// ---------------------------------------------------------------------------

/// JMAP Session resource.
#[derive(Debug, Clone, Deserialize)]
pub struct Session {
    pub capabilities: serde_json::Value,
    pub accounts: HashMap<String, Account>,
    #[serde(default)]
    pub primary_accounts: HashMap<String, String>,
    pub username: String,
    #[serde(default)]
    pub api_url: Option<String>,
    #[serde(default)]
    pub download_url: Option<String>,
    #[serde(default)]
    pub upload_url: Option<String>,
    #[serde(default)]
    pub event_source_url: Option<String>,
    #[serde(default)]
    pub state: Option<String>,
}

/// A JMAP account.
#[derive(Debug, Clone, Deserialize)]
pub struct Account {
    pub name: String,
    pub is_personal: bool,
    pub is_read_only: bool,
    pub account_capabilities: serde_json::Value,
}

// ---------------------------------------------------------------------------
// Mailbox types (RFC 8621 §2)
// ---------------------------------------------------------------------------

/// A JMAP Mailbox.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Mailbox {
    pub id: String,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub parent_id: Option<String>,
    #[serde(default)]
    pub role: Option<String>,
    #[serde(default)]
    pub sort_order: u32,
    #[serde(default)]
    pub total_emails: u32,
    #[serde(default)]
    pub unread_emails: u32,
    #[serde(default)]
    pub total_threads: u32,
    #[serde(default)]
    pub unread_threads: u32,
    #[serde(default)]
    pub my_rights: MyRights,
    #[serde(default)]
    pub is_subscribed: bool,
}

/// Rights the user has on a Mailbox.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct MyRights {
    #[serde(default)]
    pub may_read_items: bool,
    #[serde(default)]
    pub may_add_items: bool,
    #[serde(default)]
    pub may_remove_items: bool,
    #[serde(default)]
    pub may_set_seen: bool,
    #[serde(default)]
    pub may_set_keywords: bool,
    #[serde(default)]
    pub may_create_child: bool,
    #[serde(default)]
    pub may_rename: bool,
    #[serde(default)]
    pub may_delete: bool,
    #[serde(default)]
    pub may_submit: bool,
}

// ---------------------------------------------------------------------------
// Email types (RFC 8621 §4)
// ---------------------------------------------------------------------------

/// A JMAP Email.
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
    pub keywords: JmapKeywords,
    #[serde(default)]
    pub size: Option<u64>,
    #[serde(default)]
    pub message_id: Option<Option<String>>,
    #[serde(default)]
    pub in_reply_to: Option<Option<String>>,
    #[serde(default)]
    pub references: Option<Vec<String>>,
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
    pub sent_at: Option<OffsetDateTime>,
    #[serde(default)]
    pub received_at: Option<OffsetDateTime>,
    #[serde(default)]
    pub preview: Option<String>,
    #[serde(default)]
    pub has_attachment: Option<bool>,
    #[serde(default)]
    pub headers: Option<Vec<Header>>,
    #[serde(default)]
    pub body_structure: Option<BodyStructure>,
    #[serde(default)]
    pub body_values: Option<HashMap<String, BodyValue>>,
    #[serde(default)]
    pub text_body: Option<Vec<BodyPart>>,
    #[serde(default)]
    pub html_body: Option<Vec<BodyPart>>,
}

/// An email address.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailAddress {
    pub email: String,
    pub name: Option<String>,
}

/// An email header.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Header {
    pub name: String,
    pub value: String,
}

/// JMAP keywords (special: `$seen`, `$flagged`, `$answered`, `$draft`, `$forwarded`, `$phishing`, `$junk`, `$notjunk`, `$important`).
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct JmapKeywords {
    #[serde(default)]
    seen: bool,
    #[serde(default)]
    flagged: bool,
    #[serde(default)]
    answered: bool,
    #[serde(default)]
    draft: bool,
    #[serde(default)]
    forwarded: bool,
    #[serde(default)]
    phishing: bool,
    #[serde(default)]
    junk: bool,
    #[serde(default)]
    notjunk: bool,
    #[serde(default)]
    important: bool,
}

impl JmapKeywords {
    pub fn is_seen(&self) -> bool {
        self.seen
    }

    pub fn is_flagged(&self) -> bool {
        self.flagged
    }

    pub fn is_draft(&self) -> bool {
        self.draft
    }

    pub fn set_seen(&mut self, seen: bool) {
        self.seen = seen;
    }

    pub fn set_flagged(&mut self, flagged: bool) {
        self.flagged = flagged;
    }
}

/// A reference to a body part.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BodyPart {
    pub part_id: String,
    pub blob_id: Option<String>,
    pub size: Option<u64>,
    #[serde(rename = "type")]
    pub content_type: Option<String>,
    pub charset: Option<String>,
    #[serde(default)]
    pub disposition: Option<String>,
    #[serde(default)]
    pub cid: Option<String>,
    #[serde(default)]
    pub language: Option<Vec<String>>,
    #[serde(default)]
    pub location: Option<String>,
    #[serde(default)]
    pub headers: Option<Vec<Header>>,
}

/// Body structure (recursive).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BodyStructure {
    #[serde(rename = "type")]
    pub content_type: String,
    #[serde(default)]
    pub subtype: Option<String>,
    #[serde(default)]
    pub parts: Option<Vec<BodyStructure>>,
    #[serde(default)]
    pub part_id: Option<String>,
    #[serde(default)]
    pub blob_id: Option<String>,
    #[serde(default)]
    pub size: Option<u64>,
    #[serde(default)]
    pub headers: Option<Vec<Header>>,
    #[serde(default)]
    pub name: Option<String>,
    #[serde(default)]
    pub charset: Option<String>,
    #[serde(default)]
    pub disposition: Option<String>,
    #[serde(default)]
    pub cid: Option<String>,
    #[serde(default)]
    pub language: Option<Vec<String>>,
    #[serde(default)]
    pub location: Option<String>,
}

/// A fetched body value.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BodyValue {
    pub value: String,
    pub is_encoding_problem: Option<bool>,
    pub is_truncated: Option<bool>,
}

// ---------------------------------------------------------------------------
// Identity types (RFC 8621 §7)
// ---------------------------------------------------------------------------

/// A JMAP Identity.
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
    pub text_signature: Option<String>,
    #[serde(default)]
    pub html_signature: Option<String>,
    #[serde(default)]
    pub may_delete: bool,
}

// ---------------------------------------------------------------------------
// Method argument/response types
// ---------------------------------------------------------------------------

/// Request args for `Mailbox/get`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MailboxGetRequest {
    #[serde(default)]
    pub account_id: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub ids: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub properties: Vec<String>,
}

/// Response args for `Mailbox/get`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MailboxGetResponse {
    pub account_id: String,
    pub state: String,
    pub list: Vec<Mailbox>,
    #[serde(default)]
    pub not_found: Vec<String>,
}

/// Request args for `Email/get`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailGetRequest {
    #[serde(default)]
    pub account_id: String,
    pub ids: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub properties: Vec<String>,
    #[serde(default)]
    pub body_properties: Option<Vec<String>>,
    #[serde(default)]
    pub fetch_text_body_values: Option<bool>,
    #[serde(default)]
    pub fetch_html_body_values: Option<bool>,
    #[serde(default)]
    pub fetch_all_body_values: Option<bool>,
    #[serde(default)]
    pub max_body_value_bytes: Option<u64>,
}

/// Response args for `Email/get`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailGetResponse {
    pub account_id: String,
    pub state: String,
    pub list: Vec<Email>,
    #[serde(default)]
    pub not_found: Vec<String>,
}

/// Request args for `Email/query`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailQueryRequest {
    pub account_id: String,
    #[serde(default)]
    pub filter: Option<serde_json::Value>,
    #[serde(default)]
    pub sort: Option<Vec<SortComparator>>,
    #[serde(default)]
    pub collapse_threads: Option<bool>,
    #[serde(default)]
    pub position: Option<u32>,
    #[serde(default)]
    pub anchor: Option<String>,
    #[serde(default)]
    pub anchor_offset: Option<u32>,
    #[serde(default)]
    pub limit: Option<u32>,
}

/// A sort comparator.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SortComparator {
    pub property: String,
    #[serde(default)]
    pub is_ascending: Option<bool>,
    #[serde(rename = "collation")]
    #[serde(default)]
    pub collation: Option<String>,
}

/// Response args for `Email/query`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailQueryResponse {
    pub account_id: String,
    pub query_state: String,
    pub can_collapse_changes: bool,
    pub position: u32,
    pub ids: Vec<String>,
}

/// Request args for `Email/set`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailSetRequest {
    pub account_id: String,
    #[serde(default)]
    pub if_in_state: Option<String>,
    #[serde(default)]
    pub create: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub update: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub destroy: Option<Vec<String>>,
}

/// Response args for `Email/set`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailSetResponse {
    pub account_id: String,
    #[serde(default)]
    pub old_state: Option<String>,
    #[serde(default)]
    pub new_state: Option<String>,
    #[serde(default)]
    pub created: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub updated: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub destroyed: Option<Vec<String>>,
    #[serde(default)]
    pub not_created: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub not_updated: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub not_destroyed: Option<HashMap<String, serde_json::Value>>,
}

/// Request args for `Email/copy`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailCopyRequest {
    pub account_id: String,
    #[serde(default)]
    pub if_from_in_state: Option<String>,
    #[serde(default)]
    pub create: Option<HashMap<String, serde_json::Value>>,
}

/// Response args for `Email/copy`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailCopyResponse {
    pub account_id: String,
    #[serde(default)]
    pub from_state: Option<String>,
    #[serde(default)]
    pub new_state: Option<String>,
    #[serde(default)]
    pub created: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub not_created: Option<HashMap<String, serde_json::Value>>,
}

/// Request args for `Mailbox/set`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MailboxSetRequest {
    pub account_id: String,
    #[serde(default)]
    pub if_in_state: Option<String>,
    #[serde(default)]
    pub create: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub update: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub destroy: Option<Vec<String>>,
}

/// Response args for `Mailbox/set`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MailboxSetResponse {
    pub account_id: String,
    #[serde(default)]
    pub old_state: Option<String>,
    #[serde(default)]
    pub new_state: Option<String>,
    #[serde(default)]
    pub created: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub updated: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub destroyed: Option<Vec<String>>,
    #[serde(default)]
    pub not_created: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub not_updated: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub not_destroyed: Option<HashMap<String, serde_json::Value>>,
}

/// Request args for `EmailSubmission/set`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailSubmissionSetRequest {
    pub account_id: String,
    #[serde(default)]
    pub if_in_state: Option<String>,
    #[serde(default)]
    pub create: Option<HashMap<String, serde_json::Value>>,
}

/// Response args for `EmailSubmission/set`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EmailSubmissionSetResponse {
    pub account_id: String,
    #[serde(default)]
    pub old_state: Option<String>,
    #[serde(default)]
    pub new_state: Option<String>,
    #[serde(default)]
    pub created: Option<HashMap<String, serde_json::Value>>,
    #[serde(default)]
    pub not_created: Option<HashMap<String, serde_json::Value>>,
}

/// Request args for `Identity/get`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IdentityGetRequest {
    pub account_id: String,
    #[serde(default)]
    pub ids: Vec<String>,
}

/// Response args for `Identity/get`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IdentityGetResponse {
    pub account_id: String,
    pub state: String,
    pub list: Vec<Identity>,
    #[serde(default)]
    pub not_found: Vec<String>,
}

/// Response from blob upload.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UploadResult {
    pub account_id: String,
    pub blob_id: String,
    pub size: u64,
    #[serde(rename = "type")]
    pub content_type: String,
}
