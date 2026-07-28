/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapIncomingServer.h"

#include "IJmapClient.h"
#include "JmapClient.h"
#include "JmapFolder.h"
#include "JmapFolderSyncListener.h"
#include "JmapMessageSyncListener.h"
#include "mozilla/Logging.h"
#include "nsComponentManagerUtils.h"
#include "nsIMsgFolder.h"
#include "nsIMsgWindow.h"
#include "nsMsgFolderFlags.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

extern mozilla::LazyLogModule gJmapLog;

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED(JmapIncomingServer, nsMsgIncomingServer,
                             IJmapIncomingServer)

JmapIncomingServer::JmapIncomingServer() = default;

JmapIncomingServer::~JmapIncomingServer() {}

// ---------------------------------------------------------------------------
// IJmapIncomingServer
// ---------------------------------------------------------------------------

NS_IMETHODIMP
JmapIncomingServer::GetJmapUrl(nsACString& aJmapUrl) {
  aJmapUrl = mJmapUrl;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::SetJmapUrl(const nsACString& aJmapUrl) {
  mJmapUrl = aJmapUrl;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetJmapAccountId(nsACString& aAccountId) {
  aAccountId = mJmapAccountId;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::SetJmapAccountId(const nsACString& aAccountId) {
  mJmapAccountId = aAccountId;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetProtocolClient(IJmapClient** aClient) {
  NS_ENSURE_ARG_POINTER(aClient);

  // Reuse the existing client if we have one.
  if (mClient) {
    NS_ADDREF(*aClient = mClient);
    return NS_OK;
  }

  nsresult rv;

  // Create a new JMAP client via the XPCOM component.
  mClient = do_CreateInstance("@mozilla.org/messenger/jmap-client;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize with the server's endpoint URL.
  rv = mClient->Initialize(mJmapUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  // Trigger session discovery (async — results will be available shortly).
  rv = mClient->DiscoverSession();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aClient = mClient);
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetProtocolClientRunning(bool* aRunning) {
  NS_ENSURE_ARG_POINTER(aRunning);

  if (!mClient) {
    *aRunning = false;
    return NS_OK;
  }

  return mClient->GetRunning(aRunning);
}

NS_IMETHODIMP
JmapIncomingServer::GetProtocolClientIdle(bool* aIdle) {
  NS_ENSURE_ARG_POINTER(aIdle);

  if (!mClient) {
    *aIdle = true;
    return NS_OK;
  }

  return mClient->GetIdle(aIdle);
}

NS_IMETHODIMP
JmapIncomingServer::SyncFolderHierarchy(IJmapFolderListener* aListener,
                                         nsIMsgWindow* aMsgWindow) {
  NS_ENSURE_ARG_POINTER(aListener);

  RefPtr<IJmapClient> client;
  MOZ_TRY(GetProtocolClient(getter_AddRefs(client)));

  // Retrieve the persisted sync state from the root folder.
  nsCString syncState;
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if (NS_SUCCEEDED(rv) && rootFolder) {
    nsAutoCString storedState;
    rv = rootFolder->GetStringProperty("jmapMailboxState", storedState);
    if (NS_SUCCEEDED(rv)) {
      syncState = storedState;
    }
  }

  return client->SyncMailboxes(aListener, syncState);
}

// ---------------------------------------------------------------------------
// nsIMsgIncomingServer overrides
// ---------------------------------------------------------------------------

NS_IMETHODIMP
JmapIncomingServer::GetPort(int32_t* aPort) {
  NS_ENSURE_ARG_POINTER(aPort);

  // Parse the port from the endpoint URL.
  int32_t port = -1;

  if (!mJmapUrl.IsEmpty()) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), mJmapUrl);
    if (NS_SUCCEEDED(rv)) {
      rv = uri->GetPort(&port);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (port < 0) {
    // Default ports: 443 for SSL/TLS, 80 for plain.
    nsMsgSocketTypeValue socketType;
    nsresult rv = GetSocketType(&socketType);
    NS_ENSURE_SUCCESS(rv, rv);
    port = (socketType == nsMsgSocketType::SSL) ? 443 : 80;
  }

  *aPort = port;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetLocalStoreType(nsACString& aLocalStoreType) {
  aLocalStoreType.AssignLiteral("jmap");
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetLocalDatabaseType(nsACString& aLocalDatabaseType) {
  aLocalDatabaseType.AssignLiteral("mailbox");
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetCanBeDefaultServer(bool* canBeDefaultServer) {
  NS_ENSURE_ARG_POINTER(canBeDefaultServer);
  *canBeDefaultServer = true;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetOfflineSupportLevel(int32_t* aSupportLevel) {
  NS_ENSURE_ARG_POINTER(aSupportLevel);
  *aSupportLevel = OFFLINE_SUPPORT_LEVEL_NONE;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetNewMessages(nsIMsgFolder* aFolder,
                                   nsIMsgWindow* aMsgWindow,
                                   nsIUrlListener* aUrlListener) {
  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JMAP: GetNewMessages for folder"));

  NS_ENSURE_ARG_POINTER(aFolder);

  RefPtr<IJmapClient> client;
  nsresult rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // Retrieve the JMAP mailbox ID from the folder's property.
  nsAutoCString mailboxId;
  rv = aFolder->GetStringProperty("jmapMailboxId", mailboxId);
  if (NS_FAILED(rv) || mailboxId.IsEmpty()) {
    MOZ_LOG(gJmapLog, LogLevel::Warning,
            ("JMAP: GetNewMessages — folder has no jmapMailboxId"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Retrieve the persisted sync state for this folder.
  nsAutoCString syncState;
  rv = aFolder->GetStringProperty("jmapSyncState", syncState);
  if (NS_FAILED(rv)) {
    syncState.Truncate();
  }

  // Create a message sync listener that will populate the local database
  // with fetched message IDs.
  RefPtr<JmapMessageSyncListener> msgListener =
      new JmapMessageSyncListener(aFolder, aMsgWindow);
  rv = client->SyncMessages(msgListener, mailboxId, syncState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify the URL listener that we're done.
  if (aUrlListener) {
    nsCOMPtr<nsIURI> uri;
    rv = client->CheckConnectivity(aUrlListener, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::PerformBiff(nsIMsgWindow* aMsgWindow) {
  MOZ_LOG(gJmapLog, LogLevel::Debug, ("JMAP: PerformBiff"));

  // Biff checks for new messages. For JMAP, we sync folder hierarchy
  // to discover any new mailboxes, then trigger message sync.
  RefPtr<IJmapClient> client;
  nsresult rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a folder sync listener that will trigger message sync
  // after folder discovery completes.
  RefPtr<JmapFolderSyncListener> folderListener =
      new JmapFolderSyncListener(this, [this, aMsgWindow]() -> nsresult {
        // After folder sync, trigger message sync on all JMAP folders.
        nsCOMPtr<nsIMsgFolder> rootFolder;
        nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
        NS_ENSURE_SUCCESS(rv, rv);

        nsTArray<RefPtr<nsIMsgFolder>> toScan;
        toScan.AppendElement(rootFolder);

        while (!toScan.IsEmpty()) {
          nsTArray<RefPtr<nsIMsgFolder>> nextScan;
          for (auto& folder : toScan) {
            nsAutoCString mailboxId;
            rv = folder->GetStringProperty("jmapMailboxId", mailboxId);
            if (NS_SUCCEEDED(rv) && !mailboxId.IsEmpty()) {
              GetNewMessages(folder, aMsgWindow, nullptr);
            }

            nsTArray<RefPtr<nsIMsgFolder>> children;
            rv = folder->GetSubFolders(children);
            if (NS_SUCCEEDED(rv)) {
              nextScan.AppendElements(children);
            }
          }
          toScan = std::move(nextScan);
        }
        return NS_OK;
      });

  return SyncFolderHierarchy(folderListener, aMsgWindow);
}

NS_IMETHODIMP
JmapIncomingServer::PerformExpand(nsIMsgWindow* aMsgWindow) {
  MOZ_LOG(gJmapLog, LogLevel::Debug, ("JMAP: PerformExpand"));

  // PerformExpand is called when the user expands the folder tree.
  // We sync the folder hierarchy to ensure all mailboxes are visible.
  RefPtr<JmapFolderSyncListener> folderListener =
      new JmapFolderSyncListener(this);

  return SyncFolderHierarchy(folderListener, aMsgWindow);
}

NS_IMETHODIMP
JmapIncomingServer::Shutdown() {
  MOZ_LOG(gJmapLog, LogLevel::Debug, ("JMAP: IncomingServer Shutdown"));

  if (mClient) {
    nsresult rv = mClient->Shutdown();
    NS_ENSURE_SUCCESS(rv, rv);
    mClient = nullptr;
  }

  return nsMsgIncomingServer::Shutdown();
}

NS_IMETHODIMP
JmapIncomingServer::VerifyLogon(nsIUrlListener* aUrlListener,
                                nsIMsgWindow* aMsgWindow,
                                nsIURI** _retval) {
  NS_ENSURE_ARG_POINTER(aUrlListener);

  MOZ_LOG(gJmapLog, LogLevel::Debug, ("JMAP: VerifyLogon"));

  RefPtr<IJmapClient> client;
  nsresult rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  return client->CheckConnectivity(aUrlListener, _retval);
}
