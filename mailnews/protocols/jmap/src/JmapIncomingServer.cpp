/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapIncomingServer.h"

#include <utility>

#include "JmapClient.h"
#include "JmapFolder.h"
#include "IJmapClient.h"
#include "mozilla/Logging.h"
#include "nsIMsgFolderNotificationService.h"
#include "nsIMsgWindow.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgUtils.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "mozilla/Components.h"

using namespace mozilla;

static constexpr auto kDeleteModelPreferenceName = "delete_model";
static constexpr auto kPushEnabledPreferenceName = "push_enabled";
static constexpr auto kJmapAccountIdPreferenceName = "jmap_account_id";
static constexpr auto kMailboxSyncStateTokenProperty = "jmapMailboxSyncStateToken";
static constexpr auto kJmapIdProperty = "jmapId";

extern LazyLogModule gJmapLog;

NS_IMPL_ADDREF_INHERITED(JmapIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(JmapIncomingServer, nsMsgIncomingServer)
NS_IMPL_QUERY_HEAD(JmapIncomingServer)
NS_IMPL_QUERY_BODY(IJmapIncomingServer)
NS_IMPL_QUERY_TAIL_INHERITING(nsMsgIncomingServer)

JmapIncomingServer::JmapIncomingServer() = default;
JmapIncomingServer::~JmapIncomingServer() = default;

// ---------------------------------------------------------------------------
// Folder management
// ---------------------------------------------------------------------------

nsresult JmapIncomingServer::MaybeCreateFolderWithDetails(
    const nsACString& id, const nsACString& parentId, const nsACString& name,
    const nsACString& role, uint32_t flags) {
  // Check if folder with this JMAP ID already exists
  nsCOMPtr<nsIMsgFolder> existingFolder;
  nsresult rv = FindFolderWithId(id, getter_AddRefs(existingFolder));
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;  // Already exists
  }

  nsCOMPtr<nsIMsgFolder> parent;
  if (!parentId.IsEmpty()) {
    rv = FindFolderWithId(parentId, getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = GetRootFolder(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Check for duplicate names
  bool containsChild;
  rv = parent->ContainsChildNamed(name, &containsChild);
  NS_ENSURE_SUCCESS(rv, rv);
  if (containsChild) {
    return NS_MSG_CANT_CREATE_FOLDER;
  }

  // Create the folder in the local message store
  nsCOMPtr<nsIMsgPluggableStore> msgStore;
  rv = GetMsgStore(getter_AddRefs(msgStore));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFolder> newFolder;
  rv = msgStore->CreateFolder(parent, name, getter_AddRefs(newFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  // Store the JMAP ID
  rv = newFolder->SetStringProperty(kJmapIdProperty, id);
  NS_ENSURE_SUCCESS(rv, rv);

  // Store the JMAP role
  if (!role.IsEmpty()) {
    rv = newFolder->SetStringProperty("jmapRole", role);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = newFolder->SetFlags(flags);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = newFolder->SetName(name);
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify
  nsCOMPtr<nsIMsgFolderNotificationService> notifier =
      mozilla::components::FolderNotification::Service();
  if (notifier) {
    notifier->NotifyFolderAdded(newFolder);
  }

  rv = parent->NotifyFolderAdded(newFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult JmapIncomingServer::DeleteFolderWithId(const nsACString& id) {
  nsCOMPtr<nsIMsgFolder> folder;
  nsresult rv = FindFolderWithId(id, getter_AddRefs(folder));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIMsgFolder> parentFolder;
    rv = folder->GetParent(getter_AddRefs(parentFolder));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = parentFolder->PropagateDelete(folder, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult JmapIncomingServer::UpdateFolderWithDetails(
    const nsACString& id, const nsACString& parentId, const nsACString& name,
    const nsACString& role, nsIMsgWindow* msgWindow) {
  nsCOMPtr<nsIMsgFolder> folder;
  nsresult rv = FindFolderWithId(id, getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFolder> newParent;
  if (!parentId.IsEmpty()) {
    rv = FindFolderWithId(parentId, getter_AddRefs(newParent));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = GetRootFolder(getter_AddRefs(newParent));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Update role
  if (!role.IsEmpty()) {
    folder->SetStringProperty("jmapRole", role);
  }

  return LocalRenameOrReparentFolder(folder, newParent, name, msgWindow);
}

nsresult JmapIncomingServer::FindFolderWithId(const nsACString& id,
                                               nsIMsgFolder** _retval) {
  RefPtr<nsIMsgFolder> root;
  nsresult rv = GetRootFolder(getter_AddRefs(root));
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<RefPtr<nsIMsgFolder>> foldersToScan;
  foldersToScan.AppendElement(root);

  while (foldersToScan.Length() != 0) {
    nsTArray<RefPtr<nsIMsgFolder>> nextFoldersToScan;

    for (auto folder : foldersToScan) {
      nsCString folderId;
      rv = folder->GetStringProperty(kJmapIdProperty, folderId);

      if (NS_SUCCEEDED(rv) && folderId.Equals(id)) {
        folder.forget(_retval);
        return NS_OK;
      }

      nsTArray<RefPtr<nsIMsgFolder>> subfolders;
      rv = folder->GetSubFolders(subfolders);
      if (NS_SUCCEEDED(rv)) {
        nextFoldersToScan.AppendElements(subfolders);
      }
    }

    foldersToScan = std::move(nextFoldersToScan);
  }

  return NS_MSG_ERROR_FOLDER_MISSING;
}

// ---------------------------------------------------------------------------
// nsIMsgIncomingServer overrides
// ---------------------------------------------------------------------------

NS_IMETHODIMP JmapIncomingServer::GetPassword(nsAString& password) {
  // Ensure password is loaded from login manager
  nsAutoCString value;
  MOZ_TRY(mPasswordModule->GetCachedPassword(value));
  if (value.IsEmpty()) {
    MOZ_TRY(GetPasswordWithoutUI());
  }
  return nsMsgIncomingServer::GetPassword(password);
}

NS_IMETHODIMP JmapIncomingServer::GetPort(int32_t* aPort) {
  NS_ENSURE_ARG_POINTER(aPort);

  nsCString jmapUrl;
  nsresult rv = GetJmapUrl(jmapUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t port = -1;
  if (!jmapUrl.IsEmpty()) {
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), jmapUrl);
    if (NS_SUCCEEDED(rv)) {
      rv = uri->GetPort(&port);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (port < 0) {
    nsMsgSocketTypeValue socketType;
    rv = GetSocketType(&socketType);
    NS_ENSURE_SUCCESS(rv, rv);
    port = socketType == nsMsgSocketType::SSL ? 443 : 80;
  }

  *aPort = port;
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::GetLocalStoreType(
    nsACString& aLocalStoreType) {
  aLocalStoreType.AssignLiteral("jmap");
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::GetLocalDatabaseType(
    nsACString& aLocalDatabaseType) {
  aLocalDatabaseType.AssignLiteral("mailbox");
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::GetCanBeDefaultServer(
    bool* canBeDefaultServer) {
  NS_ENSURE_ARG_POINTER(canBeDefaultServer);
  *canBeDefaultServer = true;
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::GetOfflineSupportLevel(
    int32_t* aSupportLevel) {
  NS_ENSURE_ARG_POINTER(aSupportLevel);
  *aSupportLevel = OFFLINE_SUPPORT_LEVEL_NONE;
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::GetNewMessages(
    nsIMsgFolder* aFolder, nsIMsgWindow* aMsgWindow,
    nsIUrlListener* aUrlListener) {
  nsCOMPtr<nsIMsgFolder> folder = aFolder;
  nsCOMPtr<nsIMsgWindow> window = aMsgWindow;
  nsCOMPtr<nsIUrlListener> urlListener = aUrlListener;

  return SyncFolderList(aMsgWindow,
                         [self = RefPtr(this), folder, window, urlListener]() {
                           bool isServer;
                           nsresult rv = folder->GetIsServer(&isServer);
                           NS_ENSURE_SUCCESS(rv, rv);

                           if (isServer) {
                             return self->SyncAllFolders(window, urlListener);
                           }

                           return folder->GetNewMessages(window, urlListener);
                         });
}

NS_IMETHODIMP JmapIncomingServer::PerformBiff(nsIMsgWindow* aMsgWindow) {
  nsCOMPtr<nsIMsgWindow> window = aMsgWindow;
  nsresult rv = SetPerformingBiff(true);
  NS_ENSURE_SUCCESS(rv, rv);

  return SyncFolderList(aMsgWindow, [self = RefPtr(this), window]() {
    nsCOMPtr<nsIMsgFolder> rootFolder;
    nsresult rv = self->GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    nsTArray<RefPtr<nsIMsgFolder>> msgFolders;
    rv = rootFolder->GetDescendants(msgFolders);
    NS_ENSURE_SUCCESS(rv, rv);

    return self->SyncFolders(msgFolders, window, nullptr);
  });
}

NS_IMETHODIMP JmapIncomingServer::PerformExpand(nsIMsgWindow* aMsgWindow) {
  return SyncFolderList(aMsgWindow, []() { return NS_OK; });
}

NS_IMETHODIMP JmapIncomingServer::VerifyLogon(nsIUrlListener* aUrlListener,
                                              nsIMsgWindow* aMsgWindow,
                                              nsIURI** _retval) {
  NS_ENSURE_ARG_POINTER(aUrlListener);

  RefPtr<IJmapClient> client;
  MOZ_TRY(GetProtocolClient(getter_AddRefs(client)));
  return client->CheckConnectivity(aUrlListener, _retval);
}

NS_IMETHODIMP JmapIncomingServer::GetCanSearchMessages(
    bool* canSearchMessages) {
  NS_ENSURE_ARG_POINTER(canSearchMessages);
  *canSearchMessages = true;
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::Shutdown() {
  if (mClient) {
    mClient->Shutdown();
  }
  return nsMsgIncomingServer::Shutdown();
}

// ---------------------------------------------------------------------------
// IJmapIncomingServer implementation
// ---------------------------------------------------------------------------

NS_IMETHODIMP JmapIncomingServer::GetProtocolClient(IJmapClient** aClient) {
  NS_ENSURE_ARG_POINTER(aClient);

  if (!mClient) {
    mClient = new JmapClient();
    nsAutoCString endpoint;
    nsresult rv = GetJmapUrl(endpoint);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mClient->Initialize(endpoint, this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aClient = mClient);
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::GetTrashFolder(nsIMsgFolder** aTrashFolder) {
  NS_ENSURE_ARG_POINTER(aTrashFolder);
  *aTrashFolder = nullptr;

  // Find folder with role "trash"
  RefPtr<nsIMsgFolder> root;
  nsresult rv = GetRootFolder(getter_AddRefs(root));
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<RefPtr<nsIMsgFolder>> foldersToScan;
  foldersToScan.AppendElement(root);

  while (foldersToScan.Length() != 0) {
    nsTArray<RefPtr<nsIMsgFolder>> nextFoldersToScan;
    for (auto folder : foldersToScan) {
      nsCString role;
      rv = folder->GetStringProperty("jmapRole", role);
      if (NS_SUCCEEDED(rv) && role.EqualsLiteral("trash")) {
        folder.forget(aTrashFolder);
        return NS_OK;
      }
      nsTArray<RefPtr<nsIMsgFolder>> subfolders;
      rv = folder->GetSubFolders(subfolders);
      if (NS_SUCCEEDED(rv)) {
        nextFoldersToScan.AppendElements(subfolders);
      }
    }
    foldersToScan = std::move(nextFoldersToScan);
  }

  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::GetJmapUrl(nsACString& aUrl) {
  return GetStringValue("jmap_url", aUrl);
}

NS_IMETHODIMP JmapIncomingServer::SetJmapUrl(const nsACString& aUrl) {
  return SetStringValue("jmap_url", aUrl);
}

NS_IMETHODIMP JmapIncomingServer::GetJmapAccountId(nsACString& aAccountId) {
  return GetStringValue(kJmapAccountIdPreferenceName, aAccountId);
}

NS_IMETHODIMP JmapIncomingServer::SetJmapAccountId(
    const nsACString& aAccountId) {
  return SetStringValue(kJmapAccountIdPreferenceName, aAccountId);
}

NS_IMETHODIMP JmapIncomingServer::SyncMailboxHierarchy(
    IJmapSimpleOperationListener* aListener, nsIMsgWindow* aWindow) {
  return SyncFolderList(aWindow, [refListener = RefPtr{aListener}]() {
    nsTArray<nsCString> empty;
    return refListener->OnOperationSuccess(empty);
  });
}

NS_IMETHODIMP JmapIncomingServer::GetDeleteModel(
    IJmapIncomingServer::DeleteModel* aModel) {
  NS_ENSURE_ARG_POINTER(aModel);
  int32_t value;
  nsresult rv = GetIntValue(kDeleteModelPreferenceName, &value);
  NS_ENSURE_SUCCESS(rv, rv);
  *aModel = static_cast<DeleteModel>(value);
  return NS_OK;
}

NS_IMETHODIMP JmapIncomingServer::SetDeleteModel(
    IJmapIncomingServer::DeleteModel aModel) {
  return SetIntValue(kDeleteModelPreferenceName, static_cast<int32_t>(aModel));
}

NS_IMETHODIMP JmapIncomingServer::GetPushEnabled(bool* aEnabled) {
  NS_ENSURE_ARG_POINTER(aEnabled);
  return GetBoolValue(kPushEnabledPreferenceName, aEnabled);
}

NS_IMETHODIMP JmapIncomingServer::SetPushEnabled(bool aEnabled) {
  return SetBoolValue(kPushEnabledPreferenceName, aEnabled);
}

NS_IMETHODIMP JmapIncomingServer::GetProtocolClientRunning(bool* aRunning) {
  NS_ENSURE_ARG_POINTER(aRunning);
  if (!mClient) {
    *aRunning = false;
    return NS_OK;
  }
  return mClient->GetRunning(aRunning);
}

NS_IMETHODIMP JmapIncomingServer::GetProtocolClientIdle(bool* aIdle) {
  NS_ENSURE_ARG_POINTER(aIdle);
  if (!mClient) {
    *aIdle = false;
    return NS_OK;
  }
  return mClient->GetIdle(aIdle);
}

// ---------------------------------------------------------------------------
// Sync helpers
// ---------------------------------------------------------------------------

nsresult JmapIncomingServer::SyncFolderList(
    nsIMsgWindow* aMsgWindow, std::function<nsresult()> postSyncCallback) {
  nsCString syncStateToken;
  nsresult rv =
      GetStringValue(kMailboxSyncStateTokenProperty, syncStateToken);
  if (NS_FAILED(rv)) {
    syncStateToken = EmptyCString();
  }

  // TODO: Implement full async sync with JmapClient
  // For now, call the callback directly
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapIncomingServer::SyncFolderList: syncState=%s",
           syncStateToken.get()));

  return postSyncCallback();
}

nsresult JmapIncomingServer::SyncFolders(
    const nsTArray<RefPtr<nsIMsgFolder>>& folders, nsIMsgWindow* aMsgWindow,
    nsIUrlListener* urlListener) {
  for (const auto& folder : folders) {
    nsresult rv = folder->GetNewMessages(aMsgWindow, urlListener);
    if (NS_FAILED(rv)) {
      nsCString name;
      folder->GetName(name);
      NS_ERROR(nsPrintfCString("failed to get new messages for folder %s: %s",
                               name.get(), mozilla::GetStaticErrorName(rv))
                   .get());
    }
  }
  return NS_OK;
}

nsresult JmapIncomingServer::SyncAllFolders(nsIMsgWindow* aMsgWindow,
                                             nsIUrlListener* urlListener) {
  nsCOMPtr<nsIMsgFolder> rootFolder;
  MOZ_TRY(GetRootFolder(getter_AddRefs(rootFolder)));

  nsTArray<RefPtr<nsIMsgFolder>> msgFolders;
  MOZ_TRY(rootFolder->GetDescendants(msgFolders));

  return SyncFolders(msgFolders, aMsgWindow, urlListener);
}

nsresult JmapIncomingServer::UpdateTrashFolder() {
  nsCOMPtr<nsIMsgFolder> trashFolder;
  nsresult rv = GetTrashFolder(getter_AddRefs(trashFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (trashFolder) {
    rv = trashFolder->SetFlag(nsMsgFolderFlags::Trash);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}
