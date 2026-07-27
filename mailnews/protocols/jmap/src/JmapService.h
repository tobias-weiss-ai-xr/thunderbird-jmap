/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_

#include "nsIMsgMessageService.h"

/**
 * JmapService implements nsIMsgMessageService to provide message
 * access for JMAP accounts.
 */
class JmapService : public nsIMsgMessageService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGMESSAGESERVICE

  JmapService();

 protected:
  virtual ~JmapService();
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_
