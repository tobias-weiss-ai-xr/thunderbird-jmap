/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! High-level JMAP operations built on top of the client.
//!
//! Each function maps to one or more JMAP method calls as defined in
//! RFC 8620/8621. They return parsed, typed results.

use log::{info, warn};
use serde_json::json;

use super::JmapClient;
use crate::error::{is_method_error, JmapError};
use crate::types::*;

const MAIL_CAP: &str = "urn:ietf:params:jmap:mail";
const SUBMISSION_CAP: &str = "urn:ietf:params:jmap:submission";
const CORE_CAP: &str = "urn:ietf:params:jmap:core";

// ---------------------------------------------------------------------------
// Mailbox operations (RFC 8621 §2)
// ---------------------------------------------------------------------------

/// Get mailboxes from the server.
///
/// Implements Mailbox/get (RFC 8621 §2.4).
pub async fn get_mailboxes(
    client: &JmapClient<impl protocol_shared::ServerType>,
    ids: Option<&[String]>,
    properties: Option<&[String]>,
) -> Result<GetResponse<Mailbox>, JmapError> {
    let account_id = client.account_id()?.to_string();

    let mut args = json!({
        "accountId": account_id,
    });

    if let Some(ids) = ids {
        args["ids"] = json!(ids);
    } else {
        args["ids"] = json!(null); // fetch all
    }

    if let Some(props) = properties {
        args["properties"] = json!(props);
    } else {
        args["properties"] = json!(["id", "name", "parentId", "role", "sortOrder",
            "totalEmails", "unreadEmails", "myRights"]);
    }

    let result = client.call_method("Mailbox/get", &args, &[CORE_CAP, MAIL_CAP]).await?;
    Ok(serde_json::from_value(result)?)
}

/// Get mailbox changes since the given state token.
///
/// Implements Mailbox/changes (RFC 8621 §2.5).
pub async fn get_mailbox_changes(
    client: &JmapClient<impl protocol_shared::ServerType>,
    since_state: &str,
    max_changes: Option<u32>,
) -> Result<ChangesResponse, JmapError> {
    let account_id = client.account_id()?.to_string();

    let mut args = json!({
        "accountId": account_id,
        "sinceState": since_state,
    });

    if let Some(max) = max_changes {
        args["maxChanges"] = json!(max);
    }

    let result = client.call_method("Mailbox/changes", &args, &[CORE_CAP, MAIL_CAP]).await?;

    let changes: ChangesResponse = serde_json::from_value(result)?;

    // Check for tooManyChanges
    if let Some(true) = changes.has_more_changes {
        // Server indicated more changes exist; we'll loop
    }

    Ok(changes)
}

/// Create a new mailbox.
///
/// Implements Mailbox/set with create (RFC 8621 §2.2).
pub async fn create_mailbox(
    client: &JmapClient<impl protocol_shared::ServerType>,
    name: &str,
    parent_id: Option<&str>,
    role: Option<&str>,
) -> Result<(String, Mailbox), JmapError> {
    let account_id = client.account_id()?.to_string();
    let creation_id = format!("tb_new_{}", uuid::Uuid::new_v4());

    let mut mailbox = json!({
        "name": name,
    });

    if let Some(pid) = parent_id {
        mailbox["parentId"] = json!(pid);
    }
    if let Some(r) = role {
        mailbox["role"] = json!(r);
    }

    let args = json!({
        "accountId": account_id,
        "create": {
            creation_id: mailbox,
        },
    });

    let result = client.call_method("Mailbox/set", &args, &[CORE_CAP, MAIL_CAP]).await?;

    let set_response: SetResponse<Mailbox> = serde_json::from_value(result)?;

    if let Some(not_created) = set_response.not_created {
        for (_, err) in not_created {
            let type_ = err.get("type").and_then(|v| v.as_str()).unwrap_or("unknown");
            return Err(JmapError::MethodError {
                method: "Mailbox/set".to_string(),
                type_: type_.to_string(),
                description: err
                    .get("description")
                    .and_then(|v| v.as_str())
                    .unwrap_or("mailbox creation failed")
                    .to_string(),
            });
        }
    }

    let (id, mailbox) = set_response
        .created
        .and_then(|c| c.into_iter().next())
        .ok_or(JmapError::UnexpectedResponse(
            "Mailbox/set returned no created mailbox".to_string(),
        ))?;

    Ok((id, mailbox))
}

/// Rename a mailbox.
pub async fn rename_mailbox(
    client: &JmapClient<impl protocol_shared::ServerType>,
    mailbox_id: &str,
    new_name: &str,
) -> Result<String, JmapError> {
    let account_id = client.account_id()?.to_string();

    let args = json!({
        "accountId": account_id,
        "update": {
            mailbox_id: {
                "name": new_name,
            }
        },
    });

    let result = client.call_method("Mailbox/set", &args, &[CORE_CAP, MAIL_CAP]).await?;
    let set_response: SetResponse<serde_json::Value> = serde_json::from_value(result)?;
    Ok(set_response.new_state.unwrap_or_default())
}

/// Delete a mailbox.
pub async fn delete_mailbox(
    client: &JmapClient<impl protocol_shared::ServerType>,
    mailbox_id: &str,
) -> Result<String, JmapError> {
    let account_id = client.account_id()?.to_string();

    let args = json!({
        "accountId": account_id,
        "destroy": [mailbox_id],
    });

    let result = client.call_method("Mailbox/set", &args, &[CORE_CAP, MAIL_CAP]).await?;
    let set_response: SetResponse<serde_json::Value> = serde_json::from_value(result)?;

    if let Some(not_destroyed) = set_response.not_destroyed {
        for (_, err) in not_destroyed {
            let type_ = err.get("type").and_then(|v| v.as_str()).unwrap_or("unknown");
            return Err(JmapError::MethodError {
                method: "Mailbox/set".to_string(),
                type_: type_.to_string(),
                description: format!("failed to destroy mailbox: {:?}", err),
            });
        }
    }

    Ok(set_response.new_state.unwrap_or_default())
}

// ---------------------------------------------------------------------------
// Email operations (RFC 8621 §4)
// ---------------------------------------------------------------------------

/// Get emails by ID.
///
/// Implements Email/get (RFC 8621 §4.6).
pub async fn get_emails(
    client: &JmapClient<impl protocol_shared::ServerType>,
    ids: &[String],
    properties: Option<&[String]>,
    body_properties: Option<&[String]>,
    fetch_text_body_values: bool,
    fetch_html_body_values: bool,
    fetch_all_body_values: bool,
) -> Result<GetResponse<Email>, JmapError> {
    let account_id = client.account_id()?.to_string();

    let mut args = json!({
        "accountId": account_id,
        "ids": ids,
    });

    let default_props = vec![
        "id", "blobId", "threadId", "mailboxIds", "keywords", "size",
        "receivedAt", "messageId", "inReplyTo", "references",
        "sender", "from", "to", "cc", "bcc", "replyTo",
        "subject", "sentAt", "preview", "hasAttachment", "headers",
    ];

    args["properties"] = if let Some(props) = properties {
        json!(props)
    } else {
        json!(default_props)
    };

    if let Some(bp) = body_properties {
        args["bodyProperties"] = json!(bp);
    }

    if fetch_text_body_values || fetch_html_body_values || fetch_all_body_values {
        args["fetchTextBodyValues"] = json!(fetch_text_body_values || fetch_all_body_values);
        args["fetchHTMLBodyValues"] = json!(fetch_html_body_values || fetch_all_body_values);
        args["fetchAllBodyValues"] = json!(fetch_all_body_values);
    }

    let result = client.call_method("Email/get", &args, &[CORE_CAP, MAIL_CAP]).await?;
    Ok(serde_json::from_value(result)?)
}

/// Get email changes since the given state token.
///
/// Implements Email/changes (RFC 8621 §4.10).
pub async fn get_email_changes(
    client: &JmapClient<impl protocol_shared::ServerType>,
    since_state: &str,
    max_changes: Option<u32>,
) -> Result<ChangesResponse, JmapError> {
    let account_id = client.account_id()?.to_string();

    let mut args = json!({
        "accountId": account_id,
        "sinceState": since_state,
    });

    if let Some(max) = max_changes {
        args["maxChanges"] = json!(max);
    }

    let result = client.call_method("Email/changes", &args, &[CORE_CAP, MAIL_CAP]).await?;
    let changes: ChangesResponse = serde_json::from_value(result)?;

    Ok(changes)
}

/// Query emails matching a filter.
///
/// Implements Email/query (RFC 8621 §4.5).
pub async fn query_emails(
    client: &JmapClient<impl protocol_shared::ServerType>,
    filter: &serde_json::Value,
    sort: Option<&[serde_json::Value]>,
    position: Option<u32>,
    limit: Option<u32>,
    collapse_threads: bool,
    calculate_total: bool,
) -> Result<QueryResponse, JmapError> {
    let account_id = client.account_id()?.to_string();

    let mut args = json!({
        "accountId": account_id,
        "filter": filter,
        "collapseThreads": collapse_threads,
    });

    if let Some(s) = sort {
        args["sort"] = json!(s);
    }
    if let Some(p) = position {
        args["position"] = json!(p);
    }
    if let Some(l) = limit {
        args["limit"] = json!(l);
    }
    if calculate_total {
        args["calculateTotal"] = json!(true);
    }

    let result = client.call_method("Email/query", &args, &[CORE_CAP, MAIL_CAP]).await?;
    Ok(serde_json::from_value(result)?)
}

/// Update email properties (read status, keywords, mailboxIds, etc.).
///
/// Implements Email/set (RFC 8621 §4.3).
pub async fn set_emails(
    client: &JmapClient<impl protocol_shared::ServerType>,
    update: &HashMap<String, serde_json::Value>,
    destroy: Option<&[String]>,
) -> Result<SetResponse<Email>, JmapError> {
    let account_id = client.account_id()?.to_string();

    let mut args = json!({
        "accountId": account_id,
        "update": update,
    });

    if let Some(ids) = destroy {
        args["destroy"] = json!(ids);
    }

    let result = client.call_method("Email/set", &args, &[CORE_CAP, MAIL_CAP]).await?;
    let response: SetResponse<Email> = serde_json::from_value(result)?;

    // Check for errors in not_updated/not_destroyed
    if let Some(not_updated) = &response.not_updated {
        for (_, err) in not_updated {
            warn!("Email/set not_updated: {:?}", err);
        }
    }

    Ok(response)
}

/// Mark emails as read/unread by setting/clearing the $seen keyword.
pub async fn set_read_status(
    client: &JmapClient<impl protocol_shared::ServerType>,
    email_ids: &[String],
    is_read: bool,
) -> Result<(), JmapError> {
    let mut update = HashMap::new();
    for id in email_ids {
        update.insert(id.clone(), json!({
            "keywords": {
                "$seen": is_read
            }
        }));
    }

    set_emails(client, &update, None).await?;
    Ok(())
}

/// Mark emails as flagged/unflagged.
pub async fn set_flag_status(
    client: &JmapClient<impl protocol_shared::ServerType>,
    email_ids: &[String],
    is_flagged: bool,
) -> Result<(), JmapError> {
    let mut update = HashMap::new();
    for id in email_ids {
        update.insert(id.clone(), json!({
            "keywords": {
                "$flagged": is_flagged
            }
        }));
    }

    set_emails(client, &update, None).await?;
    Ok(())
}

/// Move emails to a different mailbox.
pub async fn move_emails(
    client: &JmapClient<impl protocol_shared::ServerType>,
    email_ids: &[String],
    destination_mailbox_id: &str,
) -> Result<(), JmapError> {
    let mut update = HashMap::new();
    for id in email_ids {
        update.insert(id.clone(), json!({
            "mailboxIds": {
                destination_mailbox_id: true,
            }
        }));
    }

    set_emails(client, &update, None).await?;
    Ok(())
}

/// Delete emails permanently.
pub async fn delete_emails(
    client: &JmapClient<impl protocol_shared::ServerType>,
    email_ids: &[String],
) -> Result<(), JmapError> {
    set_emails(client, &HashMap::new(), Some(email_ids)).await?;
    Ok(())
}

/// Copy emails to another mailbox.
///
/// Implements Email/copy (RFC 8621 §4.8).
pub async fn copy_emails(
    client: &JmapClient<impl protocol_shared::ServerType>,
    email_ids: &[String],
    destination_mailbox_id: &str,
    on_success_create_mailbox_id: Option<&str>,
) -> Result<CopyResponse, JmapError> {
    let account_id = client.account_id()?.to_string();

    let mut args = json!({
        "accountId": account_id,
        "fromAccountId": account_id,
        "ifInMailboxIds": email_ids,
        "create": {},
    });

    if let Some(mailbox_id) = on_success_create_mailbox_id {
        args["onSuccessCreateMailbox"] = json!(mailbox_id);
    }

    // Set the destination mailbox for each copy
    if let Some(create) = args.get_mut("create").and_then(|v| v.as_object_mut()) {
        for id in email_ids {
            create.insert(id.clone(), json!({
                "mailboxIds": {
                    destination_mailbox_id: true,
                }
            }));
        }
    }

    let result = client.call_method("Email/copy", &args, &[CORE_CAP, MAIL_CAP]).await?;
    Ok(serde_json::from_value(result)?)
}

/// Import (upload) a message to the server.
///
/// Implements Email/import (RFC 8621 §4.9).
pub async fn import_email(
    client: &JmapClient<impl protocol_shared::ServerType>,
    message_bytes: &[u8],
    mailbox_id: &str,
    keywords: Option<&[String]>,
    received_at: Option<&str>,
) -> Result<(String, Email), JmapError> {
    let account_id = client.account_id()?.to_string();

    // Upload the blob first
    let upload_result = client
        .upload_blob(account_id, message_bytes, "message/rfc822")
        .await?;

    let mut email = json!({
        "blobId": upload_result.blob_id,
        "mailboxIds": { mailbox_id: true },
    });

    if let Some(kws) = keywords {
        let kw_map: serde_json::Map<String, serde_json::Value> = kws
            .iter()
            .map(|k| (k.to_string(), json!(true)))
            .collect();
        email["keywords"] = json!(kw_map);
    }

    if let Some(date) = received_at {
        email["receivedAt"] = json!(date);
    }

    let import_id = format!("tb_import_{}", uuid::Uuid::new_v4());

    let args = json!({
        "accountId": account_id,
        "ifInState": null,
        "import": {
            import_id: email,
        },
    });

    let result = client.call_method("Email/import", &args, &[CORE_CAP, MAIL_CAP]).await?;
    let import_response: ImportResponse = serde_json::from_value(result)?;

    if let Some(not_created) = import_response.not_created {
        for (_, err) = not_created {
            let type_ = err.get("type").and_then(|v| v.as_str()).unwrap_or("unknown");
            return Err(JmapError::MethodError {
                method: "Email/import".to_string(),
                type_: type_.to_string(),
                description: format!("import failed: {:?}", err),
            });
        }
    }

    let (id, email_obj) = import_response
        .created
        .and_then(|c| c.into_iter().next())
        .ok_or(JmapError::UnexpectedResponse(
            "Email/import returned no created email".to_string(),
        ))?;

    Ok((id, email_obj))
}

// ---------------------------------------------------------------------------
// EmailSubmission operations (RFC 8621 §5)
// ---------------------------------------------------------------------------

/// Submit an email for delivery.
///
/// Implements EmailSubmission/set (RFC 8621 §5.2).
pub async fn submit_email(
    client: &JmapClient<impl protocol_shared::ServerType>,
    email_id: &str,
    identity_id: &str,
) -> Result<(String, EmailSubmission), JmapError> {
    let account_id = client.account_id()?.to_string();
    let submission_id = format!("tb_sub_{}", uuid::Uuid::new_v4());

    let args = json!({
        "accountId": account_id,
        "create": {
            submission_id: {
                "emailId": email_id,
                "identityId": identity_id,
            }
        },
    });

    let result = client.call_method("EmailSubmission/set", &args, &[CORE_CAP, SUBMISSION_CAP]).await?;
    let response: SetResponse<EmailSubmission> = serde_json::from_value(result)?;

    if let Some(not_created) = response.not_created {
        for (_, err) in not_created {
            let type_ = err.get("type").and_then(|v| v.as_str()).unwrap_or("unknown");
            return Err(JmapError::MethodError {
                method: "EmailSubmission/set".to_string(),
                type_: type_.to_string(),
                description: format!("submission failed: {:?}", err),
            });
        }
    }

    let (id, submission) = response
        .created
        .and_then(|c| c.into_iter().next())
        .ok_or(JmapError::UnexpectedResponse(
            "EmailSubmission/set returned no created submission".to_string(),
        ))?;

    Ok((id, submission))
}

// ---------------------------------------------------------------------------
// Identity operations (RFC 8621 §7)
// ---------------------------------------------------------------------------

/// Get identities for the account.
///
/// Implements Identity/get (RFC 8621 §7.2).
pub async fn get_identities(
    client: &JmapClient<impl protocol_shared::ServerType>,
) -> Result<GetResponse<Identity>, JmapError> {
    let account_id = client.account_id()?.to_string();

    let args = json!({
        "accountId": account_id,
    });

    let result = client.call_method("Identity/get", &args, &[CORE_CAP, SUBMISSION_CAP]).await?;
    Ok(serde_json::from_value(result)?)
}

// ---------------------------------------------------------------------------
// High-level sync operations
// ---------------------------------------------------------------------------

/// Perform a full mailbox hierarchy sync.
///
/// Returns the new state token and all mailbox details.
pub async fn sync_all_mailboxes(
    client: &JmapClient<impl protocol_shared::ServerType>,
    since_state: Option<&str>,
) -> Result<(String, Vec<Mailbox>), JmapError> {
    let new_state;

    let mailboxes = if let Some(state) = since_state {
        // Delta sync
        info!("Syncing mailboxes since state: {}", state);
        let mut all_created = Vec::new();
        let mut all_updated_ids: Vec<String> = Vec::new();

        let mut current_state = state.to_string();
        loop {
            let changes = get_mailbox_changes(client, &current_state, None).await?;

            if let Some(created) = changes.created {
                all_created.extend(created);
            }
            if let Some(updated) = changes.updated {
                all_updated_ids.extend(updated);
            }

            current_state = changes.new_state.ok_or(JmapError::MissingField {
                field: "newState".to_string(),
            })?;

            if changes.has_more_changes != Some(true) {
                break;
            }
        }

        // Fetch details for created and updated mailboxes
        let mut ids_to_fetch: Vec<String> = all_created;
        ids_to_fetch.extend(all_updated_ids);

        new_state = current_state;

        if ids_to_fetch.is_empty() {
            Vec::new()
        } else {
            // Fetch in batches
            let mut result = Vec::new();
            for chunk in ids_to_fetch.chunks(50) {
                let resp = get_mailboxes(client, Some(chunk), None).await?;
                result.extend(resp.list);
            }
            result
        }
    } else {
        // Full sync
        info!("Full mailbox sync (no previous state)");
        let resp = get_mailboxes(client, None, None).await?;
        new_state = resp.state.ok_or(JmapError::MissingField {
            field: "state".to_string(),
        })?;
        resp.list
    };

    Ok((new_state, mailboxes))
}

/// Sync messages for a mailbox using delta sync.
///
/// Returns the new state token and lists of created/updated/destroyed IDs.
pub async fn sync_emails_for_mailbox(
    client: &JmapClient<impl protocol_shared::ServerType>,
    since_state: Option<&str>,
) -> Result<(String, Vec<String>, Vec<String>, Vec<String>), JmapError> {
    let new_state;

    let (mut created, mut updated, mut destroyed) = (Vec::new(), Vec::new(), Vec::new());

    if let Some(state) = since_state {
        info!("Syncing emails since state: {}", state);
        let mut current_state = state.to_string();
        loop {
            let changes = get_email_changes(client, &current_state, None).await?;

            if let Some(c) = changes.created {
                created.extend(c);
            }
            if let Some(u) = changes.updated {
                updated.extend(u);
            }
            if let Some(d) = changes.destroyed {
                destroyed.extend(d);
            }

            current_state = changes.new_state.ok_or(JmapError::MissingField {
                field: "newState".to_string(),
            })?;

            if changes.has_more_changes != Some(true) {
                break;
            }
        }
        new_state = current_state;
    } else {
        // No previous state — use Email/query to get all IDs
        info!("Full email sync (no previous state)");
        let query = query_emails(
            client,
            &json!({}),
            None,
            Some(0),
            None,
            true,
            true,
        )
        .await?;
        new_state = query.query_state.ok_or(JmapError::MissingField {
            field: "queryState".to_string(),
        })?;
        created = query.ids;
    };

    Ok((new_state, created, updated, destroyed))
}

/// Build a filter for emails in a specific mailbox.
pub fn mailbox_filter(mailbox_id: &str) -> serde_json::Value {
    json!({ "inMailbox": mailbox_id })
}

/// Build a filter for unread emails in a mailbox.
pub fn unread_in_mailbox_filter(mailbox_id: &str) -> serde_json::Value {
    json!({
        "inMailbox": mailbox_id,
        "notKeyword": "$seen"
    })
}

/// Common sort by received date, newest first.
pub fn default_email_sort() -> Vec<serde_json::Value> {
    vec![json!({
        "property": "receivedAt",
        "isAscending": false
    })]
}
