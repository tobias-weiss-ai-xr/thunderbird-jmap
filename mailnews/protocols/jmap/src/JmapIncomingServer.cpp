/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapIncomingServer.h"

#include "IJmapClient.h"
#include "JmapClient.h"
#include "JmapFolder.h"
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

  // Get the sync state token (empty for first sync).
  nsCString syncState;
  // TODO: Persist and retrieve sync state from folder properties.

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
          ("JMAP: GetNewMessages requested"));

  // For now, just sync folder hierarchy.
  // TODO: sync message list for the specific folder.
  RefPtr<IJmapClient> client;
  nsresult rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Implement proper per-folder message sync.
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::PerformBiff(nsIMsgWindow* aMsgWindow) {
  MOZ_LOG(gJmapLog, LogLevel::Debug, ("JMAP: PerformBiff"));

  // Sync folder hierarchy on biff.
  RefPtr<IJmapClient> client;
  nsresult rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Full biff implementation with message sync.
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::PerformExpand(nsIMsgWindow* aMsgWindow) {
  MOZ_LOG(gJmapLog, LogLevel::Debug, ("JMAP: PerformExpand"));

  RefPtr<IJmapClient> client;
  nsresult rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Sync folder list and expand tree.
  return NS_OK;
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
