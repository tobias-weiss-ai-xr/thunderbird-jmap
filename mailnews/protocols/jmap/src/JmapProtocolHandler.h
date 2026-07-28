/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPPROTOCOLHANDLER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPPROTOCOLHANDLER_H_

#include "nsIProtocolHandler.h"

/**
 * Protocol handler for the x-moz-jmap scheme.
 *
 * JMAP message URIs come in two forms:
 *   jmap-message://user@server/Path/To/Folder#MessageKey
 *   x-moz-jmap://user@server/Path/To/Folder/MessageKey
 *
 * The former is used internally by the message service; the latter is the
 * channel URI that necko loads via this protocol handler.
 */
class JmapProtocolHandler : public nsIProtocolHandler {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  explicit JmapProtocolHandler();

  static nsresult Create(REFNSIID aIID, void** aResult);

 protected:
  virtual ~JmapProtocolHandler();
};

extern "C" {
nsresult NS_CreateJmapProtocolHandler(REFNSIID aIID, void** aResult);
}

// Also export the static Create method used by components.conf.
MOZ_EXPORT nsresult JmapProtocolHandler_Create(REFNSIID aIID, void** aResult);

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPPROTOCOLHANDLER_H_
