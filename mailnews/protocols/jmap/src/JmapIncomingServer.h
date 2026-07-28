/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPINCOMINGSERVER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPINCOMINGSERVER_H_

#include "IJmapIncomingServer.h"
#include "nsMsgIncomingServer.h"
#include "nsString.h"

/**
 * JMAP incoming server implementation.
 */
class JmapIncomingServer : public nsMsgIncomingServer,
                           public IJmapIncomingServer {
 public:
  NS_DECL_IJMAPINCOMINGSERVER
  NS_DECL_ISUPPORTS_INHERITED

  JmapIncomingServer();

 protected:
  virtual ~JmapIncomingServer();

 private:
  nsCString mJmapUrl;
  nsCString mJmapAccountId;
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPINCOMINGSERVER_H_
