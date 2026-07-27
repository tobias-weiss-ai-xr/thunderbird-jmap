/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod client;
mod error;
mod types;

use std::sync::Arc;

use client::operations;
use error::JmapError;
use log::{error, info, warn};
use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_NOT_INITIALIZED};
use nsstring::nsCString;
use protocol_shared::{
    ServerType,
    client::{DoOperation, ProtocolClient},
    safe_xpcom::{
        SafeExchangeFolderListener, SafeExchangeMessageCreateListener,
        SafeExchangeMessageFetchListener, SafeExchangeMessageSyncListener,
        SafeExchangeSimpleOperationListener, SafeUri, SafeUrlListener,
    },
};
use thin_vec::ThinVec;
use types::Mailbox;
use url::Url;
use xpcom::{RefPtr, XpCom, interfaces::nsIUrlListener};

// Re-export for XPOM
pub use client::JmapClient;
pub use error::JmapError;
pub use types;

// ---------------------------------------------------------------------------
// JMAP ID ↔ nsMsgFolderFlags mapping
// ---------------------------------------------------------------------------

/// Map JMAP mailbox roles to nsMsgFolderFlags values.
pub fn jmap_role_to_tb_flags(role: &str) -> u32 {
    // These match nsMsgFolderFlags values used in Thunderbird's C++ code.
    match role {
        "inbox" => 0x0001,         // nsMsgFolderFlags::Inbox
        "drafts" => 0x00004000,    // nsMsgFolderFlags::Drafts
        "sent" => 0x00000200,      // nsMsgFolderFlags::SentMail
        "trash" => 0x00000100,     // nsMsgFolderFlags::Trash
        "archive" => 0x00001000,   // nsMsgFolderFlags::Archive
        "junk" => 0x00040000,     // nsMsgFolderFlags::Junk
        _ => 0x00000000,           // No special flags
    }
}

/// Map JMAP keywords to read/flagged state.
pub fn jmap_keywords_to_flags(keywords: &types::HashSet) -> (bool, bool) {
    (keywords.is_seen(), keywords.is_flagged())
}

// ---------------------------------------------------------------------------
// XPCOM Client wrapper
// ---------------------------------------------------------------------------

/// The XPCOM-accessible JMAP client.
pub struct XpComJmapClient<ServerT: ServerType + 'static> {
    inner: Arc<client::JmapClient<ServerT>>,
}

impl<ServerT: ServerType + 'static> XpComJmapClient<ServerT> {
    pub fn new(endpoint: Url, server: RefPtr<ServerT>) -> Result<Self, JmapError> {
        let inner = client::JmapClient::new(endpoint, server)?;
        Ok(Self {
            inner: Arc::new(inner),
        })
    }

    pub fn running(&self) -> bool {
        self.inner.running()
    }

    pub fn idle(&self) -> bool {
        self.inner.idle()
    }
}

impl<ServerT: ServerType + 'static> ProtocolClient for XpComJmapClient<ServerT> {
    fn protocol_identifier(&self) -> String {
        String::from("jmap")
    }

    async fn shutdown(self: Arc<Self>) {
        Arc::try_unwrap(self.inner)
            .unwrap_or_else(|arc| {
                // If there are other references, we can't unwrap.
                // In practice this shouldn't happen.
                error!("JMAP client has multiple Arc references at shutdown");
                // Create a new Arc to satisfy the type system
                unsafe { Arc::from_raw(Arc::into_raw(arc)) }
            })
            .shutdown()
            .await;
    }
}

// ---------------------------------------------------------------------------
// DoOperation implementations
// ---------------------------------------------------------------------------

struct DoCheckConnectivity<'a> {
    listener: &'a SafeUrlListener,
    uri: SafeUri,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError> for DoCheckConnectivity<'_> {
    const NAME: &'static str = "check_connectivity";
    type Okay = ();
    type Listener = SafeUrlListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        self.listener.on_start_running_url(self.uri.clone()).to_result()?;

        // Discover session and check connectivity
        let _api_url = client.inner.check_connectivity().await?;

        Ok(())
    }

    fn into_success_arg(self, _ok: Self::Okay) -> SafeUri {
        self.uri
    }

    fn into_failure_arg(self) -> SafeUri {
        self.uri
    }
}

struct DoSyncMailboxHierarchy<'a> {
    listener: &'a SafeExchangeFolderListener,
    sync_state_token: Option<String>,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoSyncMailboxHierarchy<'_>
{
    const NAME: &'static str = "sync_mailbox_hierarchy";
    type Okay = ();
    type Listener = SafeExchangeFolderListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        let (new_state, mailboxes) =
            operations::sync_all_mailboxes(client.inner.as_ref(), self.sync_state_token.as_deref())
                .await?;

        // Push state token
        self.listener.on_sync_state_token_changed(&new_state)?;

        // Find the root account ID for the root folder
        let account_id = client.inner.account_id()?;

        // Notify about the root mailbox (account ID IS the root in JMAP)
        self.listener.on_new_root_folder(account_id.to_string())?;

        // Notify about each mailbox
        for mailbox in &mailboxes {
            let role = mailbox.role.as_deref().unwrap_or("");
            let flags = jmap_role_to_tb_flags(role);
            let parent_id = mailbox.parent_id.as_deref().unwrap_or(account_id);

            self.listener.on_folder_created(
                Some(mailbox.id.clone()),
                Some(parent_id.to_string()),
                mailbox.name.clone(),
                flags,
            )?;
        }

        self.listener.on_success(ThinVec::new(), false)?;
        Ok(())
    }

    fn into_success_arg(self, _ok: Self::Okay) {}
    fn into_failure_arg(self) {}
}

struct DoSyncMessagesForFolder<'a> {
    listener: &'a SafeExchangeMessageSyncListener,
    folder_id: String,
    sync_state_token: Option<String>,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoSyncMessagesForFolder<'_>
{
    const NAME: &'static str = "sync_messages_for_folder";
    type Okay = ();
    type Listener = SafeExchangeMessageSyncListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        let (new_state, created_ids, updated_ids, destroyed_ids) =
            operations::sync_emails_for_mailbox(
                client.inner.as_ref(),
                self.sync_state_token.as_deref(),
            )
            .await?;

        // Fetch details for created messages
        if !created_ids.is_empty() {
            let chunk_size = 50;
            for chunk in created_ids.chunks(chunk_size) {
                let resp =
                    operations::get_emails(client.inner.as_ref(), chunk, None, None, false, false, false)
                        .await?;

                for email in &resp.list {
                    let headers_json = serde_json::to_string(&email.headers).unwrap_or_default();
                    let (is_read, is_flagged) = jmap_keywords_to_flags(&email.keywords);
                    let size = email.size.unwrap_or(0) as u32;
                    let preview = email.preview.as_deref().unwrap_or("");

                    // Create an IHeaderBlock from the email headers
                    let headers = create_header_block_from_email(&email);

                    self.listener.on_message_created(
                        &email.id,
                        headers,
                        size,
                        is_read,
                        is_flagged,
                        preview,
                    )?;
                }
            }
        }

        // Fetch details for updated messages
        if !updated_ids.is_empty() {
            let chunk_size = 50;
            for chunk in updated_ids.chunks(chunk_size) {
                let resp =
                    operations::get_emails(client.inner.as_ref(), chunk, None, None, false, false, false)
                        .await?;

                for email in &resp.list {
                    let headers_json = serde_json::to_string(&email.headers).unwrap_or_default();
                    let (is_read, is_flagged) = jmap_keywords_to_flags(&email.keywords);
                    let size = email.size.unwrap_or(0) as u32;
                    let preview = email.preview.as_deref().unwrap_or("");

                    let headers = create_header_block_from_email(&email);

                    let result = self.listener.on_message_updated(
                        &email.id,
                        headers,
                        size,
                        is_read,
                        is_flagged,
                        preview,
                    );
                    if let Err(rv) = result {
                        if rv != nserror::NS_MSG_MESSAGE_NOT_FOUND {
                            result?;
                        }
                        // Message not found locally — create it
                        self.listener.on_message_created(
                            &email.id,
                            headers,
                            size,
                            is_read,
                            is_flagged,
                            preview,
                        )?;
                    }
                }
            }
        }

        // Handle destroyed messages
        for id in &destroyed_ids {
            self.listener.on_message_deleted(id)?;
        }

        // Push new state token
        self.listener.on_sync_state_token_changed(&new_state)?;
        self.listener.on_sync_complete()?;
        Ok(())
    }

    fn into_success_arg(self, _ok: Self::Okay) {}
    fn into_failure_arg(self) {}
}

struct DoCreateFolder<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    parent_id: String,
    name: String,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError> for DoCreateFolder<'_> {
    const NAME: &'static str = "create_folder";
    type Okay = String;
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<String, JmapError> {
        let (id, _mailbox) = operations::create_mailbox(
            client.inner.as_ref(),
            &self.name,
            Some(&self.parent_id),
            None,
        )
        .await?;
        Ok(id)
    }

    fn into_success_arg(self, ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::from([nsCString::from(ok)])
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoDeleteMessages<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    message_ids: Vec<String>,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoDeleteMessages<'_>
{
    const NAME: &'static str = "delete_messages";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        operations::delete_emails(client.inner.as_ref(), &self.message_ids).await
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoChangeReadStatus<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    message_ids: Vec<String>,
    is_read: bool,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoChangeReadStatus<'_>
{
    const NAME: &'static str = "change_read_status";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        operations::set_read_status(client.inner.as_ref(), &self.message_ids, self.is_read).await
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoChangeFlagStatus<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    message_ids: Vec<String>,
    is_flagged: bool,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoChangeFlagStatus<'_>
{
    const NAME: &'static str = "change_flag_status";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        operations::set_flag_status(client.inner.as_ref(), &self.message_ids, self.is_flagged).await
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoCreateMessage<'a> {
    listener: &'a SafeExchangeMessageCreateListener,
    folder_id: String,
    is_draft: bool,
    is_read: bool,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoCreateMessage<'_>
{
    const NAME: &'static str = "create_message";
    type Okay = String;
    type Listener = SafeExchangeMessageCreateListener;

    async fn do_operation(&mut self, _client: &XpComJmapClient<ServerT>) -> Result<String, JmapError> {
        // This is a placeholder — actual implementation would read from the
        // input stream and call import_email. The stream handling requires
        // additional XPCOM bridging.
        warn!("DoCreateMessage not fully implemented — needs stream bridge");
        Ok(String::new())
    }

    fn into_success_arg(self, ok: Self::Okay) -> (nsresult, nsCString) {
        (nsresult(NS_OK), nsCString::from(ok))
    }

    fn into_failure_arg(self) -> (nsresult, nsCString) {
        (NS_ERROR_FAILURE, nsCString::new())
    }
}

struct DoGetMessage<'a> {
    listener: &'a SafeExchangeMessageFetchListener,
    email_id: String,
    blob_id: String,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError> for DoGetMessage<'_> {
    const NAME: &'static str = "get_message";
    type Okay = ();
    type Listener = SafeExchangeMessageFetchListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        self.listener.on_fetch_start()?;

        let account_id = client.inner.account_id()?.to_string();
        let blob_id = if self.blob_id.is_empty() {
            // Fetch the email to get the blobId first
            let resp = operations::get_emails(
                client.inner.as_ref(),
                &[self.email_id.clone()],
                Some(&["id", "blobId"]),
                None,
                false,
                false,
                true, // fetch all body values for full message
            )
            .await?;

            if let Some(email) = resp.list.first() {
                email.blob_id.clone().ok_or(JmapError::MissingField {
                    field: "blobId".to_string(),
                })?
            } else {
                return Err(JmapError::UnexpectedResponse("email not found".to_string()));
            }
        } else {
            self.blob_id.clone()
        };

        // Download the full message blob
        let data = client
            .inner
            .download_blob(&account_id, &blob_id, "message.eml")
            .await?;

        // Pass the data to the listener
        self.listener.on_fetched_data_available(&data)?;
        self.listener.on_fetch_stop(nsresult(NS_OK))?;

        Ok(())
    }

    fn into_success_arg(self, _ok: Self::Okay) {}
    fn into_failure_arg(self) {}
}

struct DoMoveMessages<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    destination_id: String,
    message_ids: Vec<String>,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError> for DoMoveMessages<'_> {
    const NAME: &'static str = "move_messages";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        operations::move_emails(client.inner.as_ref(), &self.message_ids, &self.destination_id).await
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoCopyMessages<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    destination_id: String,
    message_ids: Vec<String>,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError> for DoCopyMessages<'_> {
    const NAME: &'static str = "copy_messages";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        let _ = operations::copy_emails(
            client.inner.as_ref(),
            &self.message_ids,
            &self.destination_id,
            None,
        )
        .await?;
        Ok(())
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoRenameMailbox<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    mailbox_id: String,
    new_name: String,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoRenameMailbox<'_>
{
    const NAME: &'static str = "rename_mailbox";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        operations::rename_mailbox(client.inner.as_ref(), &self.mailbox_id, &self.new_name).await
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoDeleteMailbox<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    mailbox_id: String,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError>
    for DoDeleteMailbox<'_>
{
    const NAME: &'static str = "delete_mailbox";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        operations::delete_mailbox(client.inner.as_ref(), &self.mailbox_id).await
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

struct DoSubmitMessage<'a> {
    listener: &'a SafeExchangeSimpleOperationListener,
    email_id: String,
}

impl<ServerT: ServerType> DoOperation<XpComJmapClient<ServerT>, JmapError> for DoSubmitMessage<'_> {
    const NAME: &'static str = "submit_message";
    type Okay = ();
    type Listener = SafeExchangeSimpleOperationListener;

    async fn do_operation(&mut self, client: &XpComJmapClient<ServerT>) -> Result<(), JmapError> {
        let identities = operations::get_identities(client.inner.as_ref()).await?;
        let identity_id = identities
            .list
            .first()
            .and_then(|id| id.id.clone())
            .ok_or(JmapError::UnexpectedResponse("no identity found".to_string()))?;

        operations::submit_email(client.inner.as_ref(), &self.email_id, &identity_id).await?;
        Ok(())
    }

    fn into_success_arg(self, _ok: Self::Okay) -> ThinVec<nsCString> {
        ThinVec::new()
    }

    fn into_failure_arg(self) -> ThinVec<nsCString> {
        ThinVec::new()
    }
}

// ---------------------------------------------------------------------------
// Helper: create IHeaderBlock from email headers
// ---------------------------------------------------------------------------

fn create_header_block_from_email(email: &types::Email) -> RefPtr<xpcom::interfaces::IHeaderBlock> {
    use protocol_shared::headerblock_xpcom::HeaderBlock;

    let mut block = HeaderBlock::new();

    if let Some(headers) = &email.headers {
        for header in headers {
            block.add_header(&header.name, &header.value);
        }
    }

    // Add derived headers from structured fields
    if let Some(from) = &email.from {
        for addr in from {
            let value = if let Some(name) = &addr.name {
                format!("{} <{}>", name, addr.email)
            } else {
                addr.email.clone()
            };
            block.add_header("From", &value);
        }
    }

    if let Some(subject) = &email.subject {
        if let Some(s) = subject {
            block.add_header("Subject", s);
        }
    }

    if let Some(date) = &email.received_at {
        block.add_header("Date", date);
    }

    if let Some(mid) = &email.message_id {
        if let Some(m) = mid {
            block.add_header("Message-ID", m);
        }
    }

    block.query_interface::<xpcom::interfaces::IHeaderBlock>()
        .expect("HeaderBlock should implement IHeaderBlock")
}
