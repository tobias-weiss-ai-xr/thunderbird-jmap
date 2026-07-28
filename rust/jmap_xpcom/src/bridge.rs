/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! XPCOM bridge for the JMAP client.
//!
//! This module provides `XpcomJmapBridge`, which implements `IJmapClient` via
//! the xpcom macro and delegates all operations to the internal `JmapClient`.

use std::cell::OnceCell;
use std::ffi::c_void;
use std::sync::Arc;

use log::info;
use nserror::{
    nsresult, NS_ERROR_ALREADY_INITIALIZED, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG,
    NS_ERROR_NOT_INITIALIZED, NS_OK,
};
use nsstring::{nsACString, nsAString, nsCString, nsString};
use thin_vec::ThinVec;
use url::Url;
use xpcom::{
    interfaces::{
        IJmapFolderListener, IJmapMessageListener, IJmapOperationListener,
        nsIURI, nsIUrlListener,
    },
    RefPtr, xpcom_method,
};

use crate::client::JmapClient;

// ---------------------------------------------------------------------------
// XpcomJmapBridge
// ---------------------------------------------------------------------------

/// XPCOM implementation of `IJmapClient`.
#[xpcom::xpcom(implement(IJmapClient), atomic)]
pub(crate) struct XpcomJmapBridge {
    client: OnceCell<Arc<JmapClient>>,
}

impl XpcomJmapBridge {
    // -----------------------------------------------------------------------
    // Simple getters
    // -----------------------------------------------------------------------

    xpcom_method!(get_running => GetRunning() -> bool);
    fn get_running(&self) -> Result<bool, nsresult> {
        Ok(self.client().is_ok())
    }

    xpcom_method!(get_idle => GetIdle() -> bool);
    fn get_idle(&self) -> Result<bool, nsresult> {
        Ok(true)
    }

    // -----------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------

    xpcom_method!(set_auth_token => SetAuthToken(token: *const nsACString));
    fn set_auth_token(&self, token: &nsACString) -> Result<(), nsresult> {
        let token_str = token.to_utf8();
        if let Ok(client) = self.client() {
            client.set_auth_token(&token_str);
        }
        Ok(())
    }

    xpcom_method!(initialize => Initialize(endpoint: *const nsACString));
    fn initialize(&self, endpoint: &nsACString) -> Result<(), nsresult> {
        let endpoint_str = endpoint.to_utf8();
        info!("JMAP: Initialize with endpoint={}", endpoint_str);

        let endpoint_url = Url::parse(&endpoint_str).or(Err(NS_ERROR_INVALID_ARG))?;
        let client = JmapClient::new(endpoint_url).map_err(|_| NS_ERROR_INVALID_ARG)?;

        self.client
            .set(Arc::new(client))
            .or(Err(NS_ERROR_ALREADY_INITIALIZED))?;

        Ok(())
    }

    xpcom_method!(shutdown => Shutdown());
    fn shutdown(&self) -> Result<(), nsresult> {
        info!("JMAP: Shutdown");
        // OnceCell doesn't support take() on &self, but dropping the
        // XpcomJmapBridge will clean up. For now just log.
        Ok(())
    }

    // -----------------------------------------------------------------------
    // Session discovery
    // -----------------------------------------------------------------------

    xpcom_method!(discover_session => DiscoverSession());
    fn discover_session(&self) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        moz_task::spawn_local("jmap_discover_session", async move {
            match client.discover_session_async().await {
                Ok(()) => info!("JMAP: session discovery complete"),
                Err(e) => log::error!("JMAP: session discovery failed: {e}"),
            }
        })
        .detach();

        Ok(())
    }

    // -----------------------------------------------------------------------
    // Session state / account ID getters (out-param style)
    // -----------------------------------------------------------------------

    xpcom_method!(get_session_state => GetSessionState() -> nsACString);
    fn get_session_state(&self) -> Result<nsCString, nsresult> {
        let client = match self.client() {
            Ok(c) => c,
            Err(err) if err == NS_ERROR_NOT_INITIALIZED => return Ok(nsCString::new()),
            Err(err) => return Err(err),
        };

        match client.session_state() {
            Ok(state) => Ok(nsCString::from(&state)),
            Err(_) => Ok(nsCString::new()),
        }
    }

    xpcom_method!(get_account_id => GetAccountId() -> nsACString);
    fn get_account_id(&self) -> Result<nsCString, nsresult> {
        let client = match self.client() {
            Ok(c) => c,
            Err(err) if err == NS_ERROR_NOT_INITIALIZED => return Ok(nsCString::new()),
            Err(err) => return Err(err),
        };

        match client.account_id() {
            Ok(id) => Ok(nsCString::from(&id)),
            Err(_) => Ok(nsCString::new()),
        }
    }

    // -----------------------------------------------------------------------
    // Folder operations
    // -----------------------------------------------------------------------

    xpcom_method!(sync_mailboxes => SyncMailboxes(
        listener: *const IJmapFolderListener,
        sync_state: *const nsACString
    ));
    fn sync_mailboxes(
        &self,
        listener: &IJmapFolderListener,
        sync_state: &nsACString,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let sync_state_opt = if sync_state.is_empty() {
            None
        } else {
            Some(sync_state.to_utf8().into_owned())
        };

        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_sync_mailboxes", async move {
            match crate::client::operations::sync_all_mailboxes(&client, sync_state_opt.as_deref()).await {
                Ok((state, mailboxes)) => {
                    // Serialize mailbox data as JSON so the C++ side can
                    // create local Thunderbird folders with full metadata.
                    let json = serde_json::to_string(&mailboxes).unwrap_or_default();
                    unsafe {
                        let state_ns = nsCString::from(&state);
                        let json_ns = nsCString::from(&json);
                        listener.OnFolderDiscoveryComplete(&*state_ns, &*json_ns);
                    }
                }
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnFolderDiscoveryError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(create_mailbox => CreateMailbox(
        listener: *const IJmapOperationListener,
        name: *const nsAString,
        parent_id: *const nsACString
    ));
    fn create_mailbox(
        &self,
        listener: &IJmapOperationListener,
        name: &nsAString,
        parent_id: &nsACString,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let name_str = name.to_string();
        let parent_opt = if parent_id.is_empty() {
            None
        } else {
            Some(parent_id.to_utf8().into_owned())
        };

        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_create_mailbox", async move {
            match crate::client::operations::create_mailbox(&client, &name_str, parent_opt.as_deref(), None).await {
                Ok((id, _mbox)) => {
                    info!("JMAP: created mailbox {id}");
                    unsafe { listener.OnOperationComplete(); }
                }
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnOperationError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(delete_mailbox => DeleteMailbox(
        listener: *const IJmapOperationListener,
        mailbox_id: *const nsACString
    ));
    fn delete_mailbox(
        &self,
        listener: &IJmapOperationListener,
        mailbox_id: &nsACString,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let id_str = mailbox_id.to_utf8().into_owned();
        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_delete_mailbox", async move {
            match crate::client::operations::delete_mailbox(&client, &id_str).await {
                Ok(()) => unsafe { listener.OnOperationComplete(); },
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnOperationError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    // -----------------------------------------------------------------------
    // Message operations
    // -----------------------------------------------------------------------

    xpcom_method!(sync_messages => SyncMessages(
        listener: *const IJmapMessageListener,
        mailbox_id: *const nsACString,
        sync_state: *const nsACString
    ));
    fn sync_messages(
        &self,
        listener: &IJmapMessageListener,
        mailbox_id: &nsACString,
        sync_state: &nsACString,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let sync_state_opt = if sync_state.is_empty() {
            None
        } else {
            Some(sync_state.to_utf8().into_owned())
        };

        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_sync_messages", async move {
            match crate::client::operations::sync_emails_for_mailbox(&client, sync_state_opt.as_deref()).await {
                Ok((state, ids, _, _)) => {
                    unsafe {
                        let state_ns = nsCString::from(&state);
                        let ids_ns: ThinVec<nsCString> =
                            ids.iter().map(|id| nsCString::from(id.as_str())).collect();
                        listener.OnMessagesFetched(&*state_ns, &ids_ns);
                    }
                }
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnMessageFetchError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(delete_messages => DeleteMessages(
        listener: *const IJmapOperationListener,
        message_ids: *const ThinVec<nsCString>
    ));
    fn delete_messages(
        &self,
        listener: &IJmapOperationListener,
        message_ids: &ThinVec<nsCString>,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let ids: Vec<String> = message_ids.iter().map(|s| s.to_string()).collect();
        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_delete_messages", async move {
            match crate::client::operations::delete_emails(&client, &ids).await {
                Ok(()) => unsafe { listener.OnOperationComplete(); },
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnOperationError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(set_read_status => SetReadStatus(
        listener: *const IJmapOperationListener,
        message_ids: *const ThinVec<nsCString>,
        is_read: bool
    ));
    fn set_read_status(
        &self,
        listener: &IJmapOperationListener,
        message_ids: &ThinVec<nsCString>,
        is_read: bool,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let ids: Vec<String> = message_ids.iter().map(|s| s.to_string()).collect();
        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_set_read_status", async move {
            match crate::client::operations::set_read_status(&client, &ids, is_read).await {
                Ok(()) => unsafe { listener.OnOperationComplete(); },
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnOperationError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(set_flag_status => SetFlagStatus(
        listener: *const IJmapOperationListener,
        message_ids: *const ThinVec<nsCString>,
        is_flagged: bool
    ));
    fn set_flag_status(
        &self,
        listener: &IJmapOperationListener,
        message_ids: &ThinVec<nsCString>,
        is_flagged: bool,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let ids: Vec<String> = message_ids.iter().map(|s| s.to_string()).collect();
        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_set_flag_status", async move {
            match crate::client::operations::set_flag_status(&client, &ids, is_flagged).await {
                Ok(()) => unsafe { listener.OnOperationComplete(); },
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnOperationError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    xpcom_method!(move_messages => MoveMessages(
        listener: *const IJmapOperationListener,
        message_ids: *const ThinVec<nsCString>,
        dest_mailbox_id: *const nsACString
    ));
    fn move_messages(
        &self,
        listener: &IJmapOperationListener,
        message_ids: &ThinVec<nsCString>,
        dest_mailbox_id: &nsACString,
    ) -> Result<(), nsresult> {
        let client = self.client()?;
        let client = Arc::clone(&client);

        let ids: Vec<String> = message_ids.iter().map(|s| s.to_string()).collect();
        let dest = dest_mailbox_id.to_utf8().into_owned();
        let listener = RefPtr::new(listener);

        moz_task::spawn_local("jmap_move_messages", async move {
            match crate::client::operations::move_emails(&client, &ids, &dest).await {
                Ok(()) => unsafe { listener.OnOperationComplete(); },
                Err(e) => {
                    let msg = format!("{e}");
                    unsafe {
                        let msg_ns = nsCString::from(&msg);
                        let ns_err: nsresult = (&e).into();
                        listener.OnOperationError(ns_err, &*msg_ns);
                    }
                }
            }
        })
        .detach();

        Ok(())
    }

    // -----------------------------------------------------------------------
    // Connectivity check
    // -----------------------------------------------------------------------

    xpcom_method!(check_connectivity => CheckConnectivity(
        url_listener: *const nsIUrlListener
    ) -> *const nsIURI);
    fn check_connectivity(
        &self,
        url_listener: &nsIUrlListener,
    ) -> Result<RefPtr<nsIURI>, nsresult> {
        let client = self.client()?;
        let url_str = client.endpoint().to_string();

        let io_service = xpcom::get_service::<xpcom::interfaces::nsIIOService>(
            c"@mozilla.org/network/io-service;1",
        )
        .ok_or(NS_ERROR_FAILURE)?;

        let mut uri_ptr: *const nsIURI = std::ptr::null();
        let uri_str = nsCString::from(&url_str);
        unsafe {
            io_service
                .NewURI(&*uri_str, std::ptr::null(), std::ptr::null(), &mut uri_ptr)
                .to_result()?;
        }
        let uri = unsafe { RefPtr::from_raw(uri_ptr as *mut nsIURI) }.ok_or(NS_ERROR_FAILURE)?;

        unsafe {
            url_listener.OnStartRunningUrl(uri.coerce());
            url_listener.OnStopRunningUrl(uri.coerce(), NS_OK);
        }

        Ok(uri)
    }

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    fn client(&self) -> Result<Arc<JmapClient>, nsresult> {
        self.client.get().cloned().ok_or(NS_ERROR_NOT_INITIALIZED)
    }
}

/// Creates a new instance of the XPCOM/JMAP bridge interface.
///
/// Called from components.conf via `legacy_constructor: "NS_CreateJmapClient"`.
///
/// # SAFETY
/// `iid` must be a reference to a valid `nsIID`, `result` must point to
/// valid memory, and `result` must not be used until the return value is checked.
#[allow(non_snake_case)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn NS_CreateJmapClient(iid: &xpcom::nsIID, result: *mut *mut c_void) -> nsresult {
    let instance = XpcomJmapBridge::allocate(InitXpcomJmapBridge {
        client: OnceCell::default(),
    });
    unsafe { instance.QueryInterface(iid, result) }
}
