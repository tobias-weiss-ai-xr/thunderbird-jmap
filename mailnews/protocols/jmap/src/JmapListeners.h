/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPLISTENERS_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPLISTENERS_H_

#include "IJmapClient.h"
#include "JmapIncomingServer.h"
#include "nsIUrlListener.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"

/**
 * FolderSyncListener handles the async callbacks from JmapClient during
 * mailbox hierarchy synchronization.
 *
 * Translates JMAP Mailbox/changes results into Thunderbird folder operations.
 */
class JmapFolderSyncListener final : public IJmapFolderListener {
 public:
  // Callback types for folder sync events
  using NewRootFolderCallback = std::function<nsresult(const nsACString&)>;
  using FolderCreatedCallback =
      std::function<nsresult(const nsACString&, const nsACString&,
                             const nsACString&, const nsACString&, uint32_t)>;
  using FolderUpdatedCallback =
      std::function<nsresult(const nsACString&, const nsACString&,
                             const nsACString&, const nsACString&)>;
  using FolderDeletedCallback = std::function<nsresult(const nsACString&)>;
  using SyncStateChangedCallback = std::function<nsresult(const nsACString&)>;
  using SuccessCallback = std::function<nsresult()>;
  using FailureCallback = std::function<nsresult(nsresult)>;

  JmapFolderSyncListener(
      NewRootFolderCallback&& onNewRootFolder,
      FolderCreatedCallback&& onFolderCreated,
      FolderUpdatedCallback&& onFolderUpdated,
      FolderDeletedCallback&& onFolderDeleted,
      SyncStateChangedCallback&& onSyncStateChanged,
      SuccessCallback&& onSuccess,
      FailureCallback&& onError);

  NS_DECL_ISUPPORTS
  NS_DECL_IJMAPFOLDERLISTENER

 protected:
  ~JmapFolderSyncListener();

 private:
  NewRootFolderCallback mOnNewRootFolder;
  FolderCreatedCallback mOnFolderCreated;
  FolderUpdatedCallback mOnFolderUpdated;
  FolderDeletedCallback mOnFolderDeleted;
  SyncStateChangedCallback mOnSyncStateChanged;
  SuccessCallback mOnSuccess;
  FailureCallback mOnError;
};

/**
 * Maps JMAP mailbox roles to nsMsgFolderFlags.
 *
 * JMAP roles (RFC 8621 Section 2.1):
 *   inbox, archive, drafts, junk, sent, trash
 *
 * Thunderbird folder flags:
 *   nsMsgFolderFlags::Inbox, Archive, Drafts, Junk, SentMail, Trash
 */
uint32_t JmapRoleToFolderFlags(const nsACString& aRole);

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPLISTENERS_H_
