/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_
#define COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_

#include "nsIMsgMessageService.h"

class JmapService : public nsIMsgMessageService,
                    public nsIMsgMessageFetchPartService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGMESSAGESERVICE
  NS_DECL_NSIMSGMESSAGEFETCHPARTSERVICE

  JmapService();

 protected:
  virtual ~JmapService();

 private:
  /**
   * Retrieves the message at the given URI, downloading it first if requested,
   * then optionally converting it to the desired output format.
   */
  nsresult FetchMessage(nsIURI* uri, nsIStreamListener* streamListener);

  /**
   * Extracts the message key as a string from a message URI.
   * JMAP message URIs follow the form:
   *   jmap-message://{user}@{server}/{Path/To/Folder}#{MessageKey}
   */
  nsresult MsgKeyStringFromMessageURI(nsIURI* uri, nsACString& msgKey);

  /**
   * Extracts the message key as a string from a JMAP channel URI.
   * Channel URIs follow the form:
   *   x-moz-jmap://{user}@{server}/{Path/To/Folder}/{MessageKey}
   */
  nsresult MsgKeyStringFromChannelURI(nsIURI* uri, nsACString& msgKey,
                                      nsACString& folderURIPath);

  /**
   * Retrieves the message header matching the provided URI.
   */
  nsresult MsgHdrFromUri(nsIURI* uri, nsIMsgDBHdr** _retval);
};

#endif  // COMM_MAILNEWS_PROTOCOLS_JMAP_SRC_JMAPSERVICE_H_
