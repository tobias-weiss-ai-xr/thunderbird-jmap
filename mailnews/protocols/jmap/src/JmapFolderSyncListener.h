/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDERSYNCLISTENER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDERSYNCLISTENER_H_

#include "IJmapClient.h"  // Generated: IJmapFolderListener
#include "JmapIncomingServer.h"
#include "nsString.h"

class JmapFolderSyncListener final : public IJmapFolderListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IJMAPFOLDERLISTENER

  JmapFolderSyncListener(
      JmapIncomingServer* aServer,
      std::function<nsresult()> aPostSyncCallback = []() { return NS_OK; })
      : mServer(aServer), mPostSyncCallback(std::move(aPostSyncCallback)) {}

 private:
  ~JmapFolderSyncListener() = default;

  /** Parse the JSON array of mailboxes and create local folders. */
  nsresult ParseAndCreateFolders(const nsACString& aJSON);

  nsresult FindFolderByJmapId(const nsACString& aJmapId,
                               nsIMsgFolder** aResult);

  nsresult CreateFolder(const nsACString& aJmapId,
                        const nsACString& aParentJmapId,
                        const nsAString& aName,
                        const nsACString& aRole,
                        int32_t aSortOrder,
                        int32_t aTotalEmails,
                        int32_t aUnreadEmails);

  static uint32_t RoleToFlags(const nsACString& aRole);

  RefPtr<JmapIncomingServer> mServer;
  std::function<nsresult()> mPostSyncCallback;
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDERSYNCLISTENER_H_
