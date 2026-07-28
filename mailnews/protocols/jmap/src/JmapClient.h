/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_

#include "IJmapClient.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gJmapLog;

/**
 * JmapClient implements IJmapClient to communicate with a JMAP server.
 *
 * All actual protocol logic is implemented in Rust (rust/jmap_xpcom/).
 * This C++ class serves as a lightweight XPCOM entry point.
 */
class JmapClient final : public IJmapClient {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_IJMAPCLIENT

  JmapClient();

 private:
  ~JmapClient();

  bool mInitialized = false;
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_
