/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDER_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDER_H_

#include "IJmapFolder.h"
#include "nsMsgDBFolder.h"
#include "nsString.h"

class JmapFolder : public nsMsgDBFolder, public IJmapFolder {
 public:
  NS_DECL_IJMAPFOLDER
  NS_DECL_ISUPPORTS_INHERITED

  JmapFolder();

 protected:
  virtual ~JmapFolder();

  // nsMsgDBFolder override — pure virtual in nsMsgDBFolder.
  NS_IMETHOD GetDatabase() override;
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPFOLDER_H_
