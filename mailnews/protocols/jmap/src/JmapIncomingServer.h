/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPINCOMINGSERVER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPINCOMINGSERVER_H_

#include "IJmapIncomingServer.h"
#include "IJmapClient.h"
#include "nsMsgIncomingServer.h"
#include "nsString.h"

class JmapIncomingServer : public nsMsgIncomingServer,
                           public IJmapIncomingServer {
 public:
  NS_DECL_IJMAPINCOMINGSERVER
  NS_DECL_ISUPPORTS_INHERITED

  JmapIncomingServer();

  // nsIMsgIncomingServer overrides
  NS_IMETHOD GetPort(int32_t* aPort) override;
  NS_IMETHOD GetLocalStoreType(nsACString& aLocalStoreType) override;
  NS_IMETHOD GetLocalDatabaseType(nsACString& aLocalDatabaseType) override;
  NS_IMETHOD GetCanBeDefaultServer(bool* canBeDefaultServer) override;
  NS_IMETHOD GetOfflineSupportLevel(int32_t* aSupportLevel) override;
  NS_IMETHOD GetNewMessages(nsIMsgFolder* aFolder, nsIMsgWindow* aMsgWindow,
                            nsIUrlListener* aUrlListener) override;
  NS_IMETHOD PerformBiff(nsIMsgWindow* aMsgWindow) override;
  NS_IMETHOD PerformExpand(nsIMsgWindow* aMsgWindow) override;
  NS_IMETHOD Shutdown() override;
  NS_IMETHOD VerifyLogon(nsIUrlListener* aUrlListener,
                         nsIMsgWindow* aMsgWindow,
                         nsIURI** _retval) override;

 protected:
  virtual ~JmapIncomingServer();

 private:
  nsCString mJmapUrl;
  nsCString mJmapAccountId;

  /** The JMAP client, created on first use via GetProtocolClient(). */
  nsCOMPtr<IJmapClient> mClient;
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPINCOMINGSERVER_H_
