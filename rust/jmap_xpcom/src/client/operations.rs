/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! High-level JMAP operations built on top of the client.

use crate::client::JmapClient;
use crate::error::JmapError;
use crate::types::{Mailbox, MailboxGetResponse, EmailGetResponse, EmailQueryResponse, IdentityGetResponse};
use log::info;

/// Sync all mailboxes.
pub async fn sync_all_mailboxes(
    client: &JmapClient,
    _sync_state: Option<&str>,
) -> Result<(String, Vec<Mailbox>), JmapError> {
    let account_id = client.account_id()?;
    let args = serde_json::json!({
        "accountId": account_id,
    });
    let resp = client.execute_single("Mailbox/get", args, "mbox_get").await?;
    let result: MailboxGetResponse = serde_json::from_value(resp)?;
    let state = result.state;
    let mailboxes = result.list;
    info!("JMAP: synced {} mailboxes, state={}", mailboxes.len(), state);
    Ok((state, mailboxes))
}

/// Sync emails for a specific mailbox.
pub async fn sync_emails_for_mailbox(
    client: &JmapClient,
    _sync_state: Option<&str>,
) -> Result<(String, Vec<String>, Vec<String>, Vec<String>), JmapError> {
    let account_id = client.account_id()?;
    let filter = serde_json::json!({
        "inMailbox": account_id,
    });
    let args = serde_json::json!({
        "accountId": account_id,
        "filter": filter,
        "limit": 100,
    });
    let resp = client.execute_single("Email/query", args, "email_query").await?;
    let result: EmailQueryResponse = serde_json::from_value(resp)?;
    let state = result.query_state;
    let ids = result.ids;
    info!("JMAP: synced {} email IDs, state={}", ids.len(), state);
    Ok((state, ids.clone(), Vec::new(), Vec::new()))
}

/// Get emails by ID.
pub async fn get_emails(
    client: &JmapClient,
    ids: &[String],
    properties: Option<&[&str]>,
    body_properties: Option<&[&str]>,
    fetch_all_body_values: bool,
) -> Result<EmailGetResponse, JmapError> {
    let account_id = client.account_id()?;
    let mut args = serde_json::json!({
        "accountId": account_id,
        "ids": ids,
    });
    if let Some(props) = properties {
        args["properties"] = serde_json::json!(props);
    }
    if let Some(bp) = body_properties {
        args["bodyProperties"] = serde_json::json!(bp);
    }
    if fetch_all_body_values {
        args["fetchAllBodyValues"] = serde_json::json!(true);
    }
    let resp = client.execute_single("Email/get", args, "email_get").await?;
    let result: EmailGetResponse = serde_json::from_value(resp)?;
    Ok(result)
}

/// Create a new mailbox.
pub async fn create_mailbox(
    client: &JmapClient,
    name: &str,
    parent_id: Option<&str>,
    role: Option<&str>,
) -> Result<(String, Mailbox), JmapError> {
    let account_id = client.account_id()?;
    let mut mailbox_obj = serde_json::json!({
        "name": name,
    });
    if let Some(pid) = parent_id {
        mailbox_obj["parentId"] = serde_json::json!(pid);
    }
    if let Some(r) = role {
        mailbox_obj["role"] = serde_json::json!(r);
    }

    let args = serde_json::json!({
        "accountId": account_id,
        "create": {
            "new_mbox": mailbox_obj
        }
    });

    let resp = client.execute_single("Mailbox/set", args, "mbox_create").await?;
    let result: crate::types::MailboxSetResponse = serde_json::from_value(resp)?;

    if let Some(created) = result.created {
        for (id, mbox_data) in created {
            if let Some(mbox) = mbox_data.get("created") {
                let mbox: Mailbox = serde_json::from_value(mbox.clone())
                    .unwrap_or(Mailbox { id: id.clone(), ..Default::default() });
                return Ok((id, mbox));
            }
        }
    }
    Err(JmapError::UnexpectedResponse(
        "create mailbox response missing created data".to_string(),
    ))
}

/// Delete a mailbox.
pub async fn delete_mailbox(client: &JmapClient, mailbox_id: &str) -> Result<(), JmapError> {
    let account_id = client.account_id()?;
    let args = serde_json::json!({
        "accountId": account_id,
        "destroy": [mailbox_id]
    });
    let _resp = client.execute_single("Mailbox/set", args, "mbox_delete").await?;
    Ok(())
}

/// Rename a mailbox.
pub async fn rename_mailbox(
    client: &JmapClient,
    mailbox_id: &str,
    new_name: &str,
) -> Result<(), JmapError> {
    let account_id = client.account_id()?;
    let args = serde_json::json!({
        "accountId": account_id,
        "update": {
            mailbox_id: { "name": new_name }
        }
    });
    let _resp = client.execute_single("Mailbox/set", args, "mbox_rename").await?;
    Ok(())
}

/// Delete emails.
pub async fn delete_emails(client: &JmapClient, email_ids: &[String]) -> Result<(), JmapError> {
    let account_id = client.account_id()?;
    let args = serde_json::json!({
        "accountId": account_id,
        "destroy": email_ids
    });
    let _resp = client.execute_single("Email/set", args, "email_delete").await?;
    Ok(())
}

/// Set read status on emails.
pub async fn set_read_status(
    client: &JmapClient,
    email_ids: &[String],
    is_read: bool,
) -> Result<(), JmapError> {
    let account_id = client.account_id()?;
    let mut update = serde_json::Map::new();
    for id in email_ids {
        update.insert(id.clone(), serde_json::json!({ "keywords": { "$seen": is_read } }));
    }
    let args = serde_json::json!({
        "accountId": account_id,
        "update": update
    });
    let _resp = client.execute_single("Email/set", args, "email_read").await?;
    Ok(())
}

/// Set flagged status on emails.
pub async fn set_flag_status(
    client: &JmapClient,
    email_ids: &[String],
    is_flagged: bool,
) -> Result<(), JmapError> {
    let account_id = client.account_id()?;
    let mut update = serde_json::Map::new();
    for id in email_ids {
        update.insert(id.clone(), serde_json::json!({ "keywords": { "$flagged": is_flagged } }));
    }
    let args = serde_json::json!({
        "accountId": account_id,
        "update": update
    });
    let _resp = client.execute_single("Email/set", args, "email_flag").await?;
    Ok(())
}

/// Move emails.
pub async fn move_emails(
    client: &JmapClient,
    email_ids: &[String],
    destination_mailbox_id: &str,
) -> Result<(), JmapError> {
    let account_id = client.account_id()?;
    let mut update = serde_json::Map::new();
    for id in email_ids {
        update.insert(id.clone(), serde_json::json!({
            "mailboxIds": { destination_mailbox_id: true }
        }));
    }
    let args = serde_json::json!({
        "accountId": account_id,
        "update": update
    });
    let _resp = client.execute_single("Email/set", args, "email_move").await?;
    Ok(())
}

/// Copy emails.
pub async fn copy_emails(
    client: &JmapClient,
    email_ids: &[String],
    destination_mailbox_id: &str,
    _on_success_create_ids: Option<&str>,
) -> Result<crate::types::EmailCopyResponse, JmapError> {
    let account_id = client.account_id()?;
    let mut create = serde_json::Map::new();
    for (i, id) in email_ids.iter().enumerate() {
        create.insert(format!("copy_{i}"), serde_json::json!({
            "fromAccountId": account_id,
            "fromEmailId": id,
            "mailboxIds": { destination_mailbox_id: true }
        }));
    }
    let args = serde_json::json!({
        "accountId": account_id,
        "create": create
    });
    let resp = client.execute_single("Email/copy", args, "email_copy").await?;
    let result: crate::types::EmailCopyResponse = serde_json::from_value(resp)?;
    Ok(result)
}

/// Submit an email.
pub async fn submit_email(
    client: &JmapClient,
    email_id: &str,
    identity_id: &str,
) -> Result<(), JmapError> {
    let account_id = client.account_id()?;
    let args = serde_json::json!({
        "accountId": account_id,
        "create": {
            "sub_1": {
                "emailId": email_id,
                "identityId": identity_id
            }
        }
    });
    let _resp = client.execute_single("EmailSubmission/set", args, "submit").await?;
    Ok(())
}

/// Get identities.
pub async fn get_identities(client: &JmapClient) -> Result<IdentityGetResponse, JmapError> {
    let account_id = client.account_id()?;
    let args = serde_json::json!({
        "accountId": account_id,
    });
    let resp = client.execute_single("Identity/get", args, "id_get").await?;
    let result: IdentityGetResponse = serde_json::from_value(resp)?;
    Ok(result)
}

/// Import an email from raw RFC 5322 data.
pub async fn import_email(
    client: &JmapClient,
    mailbox_id: &str,
    raw_email: &[u8],
) -> Result<String, JmapError> {
    let upload_result = client.upload_blob(raw_email, "message/rfc822").await?;
    let account_id = client.account_id()?;
    let args = serde_json::json!({
        "accountId": account_id,
        "create": {
            "import_1": {
                "mailboxIds": { mailbox_id: true },
                "blobId": upload_result.blob_id
            }
        }
    });
    let resp = client.execute_single("Email/set", args, "import").await?;
    let result: crate::types::EmailSetResponse = serde_json::from_value(resp)?;

    if let Some(created) = result.created {
        if let Some(id) = created.keys().next() {
            return Ok(id.clone());
        }
    }
    Err(JmapError::UnexpectedResponse(
        "import email response missing created data".to_string(),
    ))
}
