/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_

#include "nsISupports.h"

class JmapService : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  JmapService();

 private:
  virtual ~JmapService();
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_
