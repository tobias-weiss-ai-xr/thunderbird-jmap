/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPMESSAGECHANNEL_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPMESSAGECHANNEL_H_

#include "nsHashPropertyBag.h"
#include "nsIMsgHdr.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIRequest.h"
#include "nsILoadInfo.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInputStreamPump.h"

class JmapMessageChannel : public nsHashPropertyBag,
                           public nsIChannel {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUEST

  explicit JmapMessageChannel(nsIURI* aURI, bool aConvert = false);

 private:
  ~JmapMessageChannel();

  nsresult StartMessageReadFromStore(nsIStreamListener* aListener);

  bool mConvert;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIRequest> mReadRequest;
  nsCOMPtr<nsILoadInfo> mLoadInfo;

  nsCString mContentType;
  nsCString mCharset;
  int64_t mContentLength;
  uint32_t mContentDisposition;
  nsLoadFlags mLoadFlags;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsIInterfaceRequestor> mNotificationCallbacks;

  nsCOMPtr<nsIMsgDBHdr> mHdr;
  bool mPending;
  nsresult mStatus;
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPMESSAGECHANNEL_H_
