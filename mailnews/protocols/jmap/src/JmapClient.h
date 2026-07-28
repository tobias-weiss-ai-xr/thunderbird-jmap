/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_

#include "nsID.h"
#include "mozilla/Logging.h"

// JMAP logging module. Used by both C++ and Rust (via log crate).
extern mozilla::LazyLogModule gJmapLog;

extern "C" {
// Instantiates a new IJmapClient (implemented in Rust via jmap_xpcom).
MOZ_EXPORT nsresult NS_CreateJmapClient(REFNSIID aIID, void** aResult);
}

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPCLIENT_H_
