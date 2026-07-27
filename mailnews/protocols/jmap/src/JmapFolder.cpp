/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapFolder.h"

#include "JmapClient.h"
#include "IJmapClient.h"
#include "IJmapIncomingServer.h"
#include "mozilla/Logging.h"
#include "nsMsgDBFolder.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsMsgMessageFlags.h"

using namespace mozilla;

extern LazyLogModule gJmapLog;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

JmapFolder::JmapFolder() : mHasLoadedSubfolders(false) {}

JmapFolder::~JmapFolder() = default;

NS_IMPL_ADDREF_INHERITED(JmapFolder, nsMsgDBFolder)
NS_IMPL_RELEASE_INHERITED(JmapFolder, nsMsgDBFolder)
NS_IMPL_QUERY_HEAD(JmapFolder)
NS_IMPL_QUERY_BODY(IJmapFolder)
NS_IMPL_QUERY_TAIL_INHERITING(nsMsgDBFolder)

// ---------------------------------------------------------------------------
// IJmapFolder
// ---------------------------------------------------------------------------

NS_IMETHODIMP JmapFolder::GetJmapMailboxId(nsACString& aId) {
  return GetJmapMailboxIdInternal(aId);
}

NS_IMETHODIMP JmapFolder::GetJmapRole(nsACString& aRole) {
  return GetStringProperty(kJmapRoleProperty, aRole);
}

NS_IMETHODIMP JmapFolder::GetJmapSortOrder(int32_t* aSortOrder) {
  NS_ENSURE_ARG_POINTER(aSortOrder);
  // JMAP sort order can be stored as a custom property
  nsCString orderStr;
  nsresult rv = GetStringProperty("jmapSortOrder", orderStr);
  if (NS_SUCCEEDED(rv) && !orderStr.IsEmpty()) {
    int32_t order = orderStr.ToInteger(&rv);
    if (NS_SUCCEEDED(rv)) {
      *aSortOrder = order;
      return NS_OK;
    }
  }
  *aSortOrder = 0;
  return NS_OK;
}

NS_IMETHODIMP JmapFolder::GetEmailStateToken(nsACString& aToken) {
  return GetStringProperty(kJmapEmailStateTokenProperty, aToken);
}

NS_IMETHODIMP JmapFolder::SetEmailStateToken(const nsACString& aToken) {
  return SetStringProperty(kJmapEmailStateTokenProperty, aToken);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

nsresult JmapFolder::GetJmapMailboxIdInternal(nsACString& aId) {
  return GetStringProperty(kJmapIdProperty, aId);
}

nsresult JmapFolder::GetProtocolClient(IJmapClient** aClient) {
  nsCOMPtr<nsIMsgIncomingServer> server;
  MOZ_TRY(GetServer(getter_AddRefs(server)));

  nsCOMPtr<IJmapIncomingServer> jmapServer =
      do_QueryInterface(server);
  NS_ENSURE_TRUE(jmapServer, NS_ERROR_UNEXPECTED);

  return jmapServer->GetProtocolClient(aClient);
}

nsresult JmapFolder::GetTrashFolder(nsIMsgFolder** aResult) {
  nsCOMPtr<nsIMsgIncomingServer> server;
  MOZ_TRY(GetServer(getter_AddRefs(server)));

  nsCOMPtr<IJmapIncomingServer> jmapServer =
      do_QueryInterface(server);
  NS_ENSURE_TRUE(jmapServer, NS_ERROR_UNEXPECTED);

  return jmapServer->GetTrashFolder(aResult);
}

nsresult JmapFolder::GetHdrForJmapId(const nsACString& aJmapId,
                                       nsIMsgDBHdr** aHdr) {
  nsCOMPtr<nsIMsgDatabase> db;
  MOZ_TRY(GetDatabase());
  db.forget(aHdr);  // TODO: Implement proper lookup
  return NS_ERROR_NOT_AVAILABLE;
}

nsresult JmapFolder::SyncMessages(nsIMsgWindow* aWindow,
                                   nsIUrlListener* aListener) {
  nsCString mailboxId;
  nsresult rv = GetJmapMailboxIdInternal(mailboxId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString stateToken;
  rv = GetStringProperty(kJmapEmailStateTokenProperty, stateToken);

  RefPtr<IJmapClient> client;
  rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Create a JmapMessageSyncListener and pass it here
  // client->SyncMessagesForMailbox(listener, mailboxId, stateToken);

  return NS_OK;
}

nsresult JmapFolder::CreateChildrenFromStore() {
  // Load subfolders from the message store
  bool isServer;
  nsresult rv = GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isServer) {
    // For the root folder, we need to discover all mailboxes from the server
    // via SyncMailboxHierarchy. This will create local folders as needed.
    return NS_OK;
  }

  return nsMsgDBFolder::CreateChildrenFromStore();
}

// ---------------------------------------------------------------------------
// nsIMsgFolder overrides
// ---------------------------------------------------------------------------

NS_IMETHODIMP JmapFolder::SetStringProperty(const char* propertyName,
                                             const nsACString& propertyValue) {
  return nsMsgDBFolder::SetStringProperty(propertyName, propertyValue);
}

NS_IMETHODIMP JmapFolder::CreateStorageIfMissing(
    nsIUrlListener* urlListener) {
  return nsMsgDBFolder::CreateStorageIfMissing(urlListener);
}

NS_IMETHODIMP JmapFolder::CreateSubfolder(const nsACString& folderName,
                                           nsIMsgWindow* msgWindow) {
  nsCString parentId;
  nsresult rv = GetJmapMailboxIdInternal(parentId);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<IJmapClient> client;
  rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Implement with proper listener for async folder creation
  // client->CreateMailbox(listener, parentId, folderName);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::CopyFileMessage(
    nsIFile* aFile, nsIMsgDBHdr* msgToReplace, bool isDraftOrTemplate,
    uint32_t newMsgFlags, const nsACString& aNewMsgKeywords,
    nsIMsgWindow* msgWindow, nsIMsgCopyServiceListener* listener) {
  // TODO: Implement via JmapClient::CreateMessage (Email/import)
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::CopyMessages(
    nsIMsgFolder* srcFolder,
    nsTArray<RefPtr<nsIMsgDBHdr>> const& srcHdrs, bool isMove,
    nsIMsgWindow* msgWindow, nsIMsgCopyServiceListener* listener,
    bool isFolder, bool allowUndo) {
  // TODO: Implement via JmapClient::MoveMessages or CopyMessages
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::DeleteMessages(
    const nsTArray<RefPtr<nsIMsgDBHdr>>& msgHeaders, nsIMsgWindow* msgWindow,
    bool deleteStorage, bool isMove, nsIMsgCopyServiceListener* listener,
    bool allowUndo) {
  // TODO: Implement via JmapClient::DeleteMessages
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::EmptyTrash(nsIUrlListener* aListener) {
  // TODO: Implement via JmapClient::EmptyMailbox
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::CopyFolder(nsIMsgFolder* srcFolder, bool isMove,
                                       nsIMsgWindow* window,
                                       nsIMsgCopyServiceListener* listener) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::DeleteSelf(nsIMsgWindow* aWindow) {
  nsCString mailboxId;
  nsresult rv = GetJmapMailboxIdInternal(mailboxId);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<IJmapClient> client;
  rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Implement with proper async listener
  // client->DeleteMailbox(listener, mailboxId);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo** folderInfo,
                                                 nsIMsgDatabase** _retval) {
  return nsMsgDBFolder::GetDBFolderInfoAndDB(folderInfo, _retval);
}

NS_IMETHODIMP JmapFolder::GetSupportsOffline(bool* supportsOffline) {
  NS_ENSURE_ARG_POINTER(supportsOffline);
  *supportsOffline = false;
  return NS_OK;
}

NS_IMETHODIMP JmapFolder::GetDeletable(bool* deletable) {
  NS_ENSURE_ARG_POINTER(deletable);
  bool isServer;
  nsresult rv = GetIsServer(&isServer);
  NS_ENSURE_SUCCESS(rv, rv);
  *deletable = !isServer;
  return NS_OK;
}

NS_IMETHODIMP JmapFolder::GetIncomingServerType(
    nsACString& aIncomingServerType) {
  aIncomingServerType.AssignLiteral("jmap");
  return NS_OK;
}

NS_IMETHODIMP JmapFolder::GetNewMessages(nsIMsgWindow* aWindow,
                                          nsIUrlListener* aListener) {
  return SyncMessages(aWindow, aListener);
}

NS_IMETHODIMP JmapFolder::GetSubFolders(
    nsTArray<RefPtr<nsIMsgFolder>>& aSubFolders) {
  if (!mHasLoadedSubfolders) {
    nsresult rv = CreateChildrenFromStore();
    if (NS_SUCCEEDED(rv)) {
      mHasLoadedSubfolders = true;
    }
  }
  return nsMsgDBFolder::GetSubFolders(aSubFolders);
}

NS_IMETHODIMP JmapFolder::MarkMessagesRead(
    const nsTArray<RefPtr<nsIMsgDBHdr>>& messages, bool markRead) {
  nsTArray<nsCString> jmapIds;
  for (const auto& hdr : messages) {
    nsCString jmapId;
    nsresult rv = hdr->GetStringProperty(kJmapIdProperty, jmapId);
    if (NS_SUCCEEDED(rv) && !jmapId.IsEmpty()) {
      jmapIds.AppendElement(jmapId);
    }
  }

  if (!jmapIds.IsEmpty()) {
    RefPtr<IJmapClient> client;
    nsresult rv = GetProtocolClient(getter_AddRefs(client));
    if (NS_SUCCEEDED(rv)) {
      // TODO: client->ChangeReadStatus(listener, jmapIds, markRead);
    }
  }

  return nsMsgDBFolder::MarkMessagesRead(messages, markRead);
}

NS_IMETHODIMP JmapFolder::MarkAllMessagesRead(nsIMsgWindow* aMsgWindow) {
  // TODO: Use JMAP Email/set to clear $seen on all messages in this mailbox
  return nsMsgDBFolder::MarkAllMessagesRead(aMsgWindow);
}

NS_IMETHODIMP JmapFolder::Rename(const nsACString& aNewName,
                                  nsIMsgWindow* msgWindow) {
  nsCString mailboxId;
  nsresult rv = GetJmapMailboxIdInternal(mailboxId);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<IJmapClient> client;
  rv = GetProtocolClient(getter_AddRefs(client));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: client->RenameMailbox(listener, mailboxId, aNewName);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapFolder::UpdateFolder(nsIMsgWindow* aWindow) {
  return SyncMessages(aWindow, nullptr);
}

NS_IMETHODIMP JmapFolder::Compact(nsIUrlListener* aListener,
                                   nsIMsgWindow* aMsgWindow) {
  // JMAP folders don't need compacting (server manages storage)
  if (aListener) {
    aListener->OnStopRunningUrl(nullptr, NS_OK);
  }
  return NS_OK;
}

NS_IMETHODIMP JmapFolder::CompactAll(nsIUrlListener* aListener,
                                     nsIMsgWindow* aMsgWindow) {
  if (aListener) {
    aListener->OnStopRunningUrl(nullptr, NS_OK);
  }
  return NS_OK;
}

NS_IMETHODIMP JmapFolder::AddSubfolder(const nsACString& name,
                                        nsIMsgFolder** newFolder) {
  return nsMsgDBFolder::AddSubfolder(name, newFolder);
}

NS_IMETHODIMP JmapFolder::MarkMessagesFlagged(
    const nsTArray<RefPtr<nsIMsgDBHdr>>& messages, bool markFlagged) {
  nsTArray<nsCString> jmapIds;
  for (const auto& hdr : messages) {
    nsCString jmapId;
    nsresult rv = hdr->GetStringProperty(kJmapIdProperty, jmapId);
    if (NS_SUCCEEDED(rv) && !jmapId.IsEmpty()) {
      jmapIds.AppendElement(jmapId);
    }
  }

  if (!jmapIds.IsEmpty()) {
    RefPtr<IJmapClient> client;
    nsresult rv = GetProtocolClient(getter_AddRefs(client));
    if (NS_SUCCEEDED(rv)) {
      // TODO: client->ChangeFlagStatus(listener, jmapIds, markFlagged);
    }
  }

  return nsMsgDBFolder::MarkMessagesFlagged(messages, markFlagged);
}

// ---------------------------------------------------------------------------
// Overrides
// ---------------------------------------------------------------------------

nsresult JmapFolder::CreateBaseMessageURI(const nsACString& aURI) {
  nsCString uri(aURI);
  uri.AppendLiteral("-message");
  return SetUri(uri);
}

nsresult JmapFolder::GetDatabase() {
  return nsMsgDBFolder::GetDatabase();
}
