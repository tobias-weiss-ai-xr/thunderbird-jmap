/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDER_H_

#include "IJmapClient.h"
#include "IJmapFolder.h"
#include "nsMsgDBFolder.h"
#include "mozilla/HashTable.h"

constexpr auto kJmapIdProperty = "jmapId";
constexpr auto kJmapRoleProperty = "jmapRole";
constexpr auto kJmapEmailStateTokenProperty = "jmapEmailStateToken";

/**
 * The JMAP implementation of nsIMsgFolder, representing a mailbox
 * on a JMAP server.
 */
class JmapFolder : public nsMsgDBFolder, public IJmapFolder {
 public:
  NS_DECL_IJMAPFOLDER
  NS_DECL_ISUPPORTS_INHERITED

  JmapFolder();

  friend class JmapMessageSyncCallbacks;

 public:
  // nsIMsgFolder overrides
  NS_IMETHOD SetStringProperty(const char* propertyName,
                               const nsACString& propertyValue) override;
  NS_IMETHOD CreateStorageIfMissing(nsIUrlListener* urlListener) override;
  NS_IMETHOD CreateSubfolder(const nsACString& folderName,
                             nsIMsgWindow* msgWindow) override;
  NS_IMETHOD CopyFileMessage(nsIFile* aFile, nsIMsgDBHdr* msgToReplace,
                             bool isDraftOrTemplate, uint32_t newMsgFlags,
                             const nsACString& aNewMsgKeywords,
                             nsIMsgWindow* msgWindow,
                             nsIMsgCopyServiceListener* listener) override;
  NS_IMETHOD CopyMessages(nsIMsgFolder* srcFolder,
                          nsTArray<RefPtr<nsIMsgDBHdr>> const& srcHdrs,
                          bool isMove, nsIMsgWindow* msgWindow,
                          nsIMsgCopyServiceListener* listener, bool isFolder,
                          bool allowUndo) override;
  NS_IMETHOD DeleteMessages(const nsTArray<RefPtr<nsIMsgDBHdr>>& msgHeaders,
                            nsIMsgWindow* msgWindow, bool deleteStorage,
                            bool isMove, nsIMsgCopyServiceListener* listener,
                            bool allowUndo) override;
  NS_IMETHOD EmptyTrash(nsIUrlListener* aListener) override;
  NS_IMETHOD CopyFolder(nsIMsgFolder* srcFolder, bool isMoveFolder,
                        nsIMsgWindow* window,
                        nsIMsgCopyServiceListener* listener) override;
  NS_IMETHOD DeleteSelf(nsIMsgWindow* aWindow) override;
  NS_IMETHOD GetDBFolderInfoAndDB(nsIDBFolderInfo** folderInfo,
                                  nsIMsgDatabase** _retval) override;
  NS_IMETHOD GetSupportsOffline(bool* supportsOffline) override;
  NS_IMETHOD GetDeletable(bool* deletable) override;
  NS_IMETHOD GetIncomingServerType(nsACString& aIncomingServerType) override;
  NS_IMETHOD GetNewMessages(nsIMsgWindow* aWindow,
                            nsIUrlListener* aListener) override;
  NS_IMETHOD GetSubFolders(
      nsTArray<RefPtr<nsIMsgFolder>>& aSubFolders) override;
  NS_IMETHOD MarkMessagesRead(const nsTArray<RefPtr<nsIMsgDBHdr>>& messages,
                              bool markRead) override;
  NS_IMETHOD MarkAllMessagesRead(nsIMsgWindow* aMsgWindow) override;
  NS_IMETHOD Rename(const nsACString& aNewName,
                    nsIMsgWindow* msgWindow) override;
  NS_IMETHOD UpdateFolder(nsIMsgWindow* aWindow) override;
  NS_IMETHOD Compact(nsIUrlListener* aListener,
                     nsIMsgWindow* aMsgWindow) override;
  NS_IMETHOD CompactAll(nsIUrlListener* aListener,
                        nsIMsgWindow* aMsgWindow) override;
  NS_IMETHOD AddSubfolder(const nsACString& name,
                          nsIMsgFolder** newFolder) override;
  NS_IMETHOD MarkMessagesFlagged(const nsTArray<RefPtr<nsIMsgDBHdr>>& messages,
                                 bool markFlagged) override;

 protected:
  virtual ~JmapFolder();

  virtual nsresult CreateBaseMessageURI(const nsACString& aURI) override;
  virtual nsresult GetDatabase() override;

 private:
  nsresult CreateChildrenFromStore();
  bool mHasLoadedSubfolders;

  /**
   * Get the JMAP protocol client for this folder.
   */
  nsresult GetProtocolClient(IJmapClient** aClient);

  /**
   * Get the JMAP Mailbox ID for this folder.
   */
  nsresult GetJmapMailboxIdInternal(nsACString& aId);

  /**
   * Look up the trash folder for the current account.
   */
  nsresult GetTrashFolder(nsIMsgFolder** aResult);

  /**
   * Synchronize messages for this folder.
   */
  nsresult SyncMessages(nsIMsgWindow* aWindow, nsIUrlListener* aListener);

  /**
   * Look up the message database header matching a JMAP Email ID.
   */
  nsresult GetHdrForJmapId(const nsACString& aJmapId, nsIMsgDBHdr** aHdr);

  /**
   * Handle delete operation with soft/hard delete logic.
   */
  nsresult HandleDeleteOperation(
      bool forceHardDelete, std::function<nsresult()>&& onHardDelete,
      std::function<nsresult(IJmapFolder* trashFolder)>&& onSoftDelete);
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDER_H_
