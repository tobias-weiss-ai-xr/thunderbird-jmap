/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPMESSAGESYNCLISTENER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPMESSAGESYNCLISTENER_H_

#include "IJmapClient.h"   // Generated: IJmapMessageListener
#include "nsIMsgFolder.h"
#include "nsIMsgWindow.h"
#include "nsString.h"

/**
 * Listens for JMAP message fetch results and populates the local database
 * with message headers.
 */
class JmapMessageSyncListener final : public IJmapMessageListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IJMAPMESSAGELISTENER

  JmapMessageSyncListener(nsIMsgFolder* aFolder, nsIMsgWindow* aMsgWindow)
      : mFolder(aFolder), mMsgWindow(aMsgWindow) {}

 private:
  ~JmapMessageSyncListener() = default;

  nsresult ProcessMessageIds(const nsTArray<nsCString>& aMessageIds);

  nsCOMPtr<nsIMsgFolder> mFolder;
  nsCOMPtr<nsIMsgWindow> mMsgWindow;
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPMESSAGESYNCLISTENER_H_
