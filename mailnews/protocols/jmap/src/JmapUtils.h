/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPUTILS_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPUTILS_H_

#include "nsISupports.h"

/// JMAP protocol utility class (currently empty).
class JmapUtils : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  JmapUtils();

 private:
  virtual ~JmapUtils();
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPUTILS_H_
