/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPPROTOCOLHANDLER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPPROTOCOLHANDLER_H_

#include "nsIProtocolHandler.h"

/**
 * JMAP protocol handler.
 */
class JmapProtocolHandler : public nsIProtocolHandler {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  static nsresult Create(REFNSIID aIID, void** aResult);

  JmapProtocolHandler();

 protected:
  virtual ~JmapProtocolHandler();
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPPROTOCOLHANDLER_H_
