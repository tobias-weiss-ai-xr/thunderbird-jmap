/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapService.h"
#include "mozilla/Logging.h"

using namespace mozilla;

static LazyLogModule gJmapLog("jmap");

NS_IMPL_ISUPPORTS(JmapService, nsIMsgMessageService)

JmapService::JmapService() = default;
JmapService::~JmapService() = default;

// TODO: Implement nsIMsgMessageService methods for JMAP message access.
// Key methods:
// - MessageURIToMsgHdr: Convert URI to message header
// - LoadMessage: Load message content for display
// - DisplayMessage: Display message in a window
// - OpenMessage: Open message in new window/tab
// - CopyMessage: Copy message
// - SaveMessageToDisk: Save to file

NS_IMETHODIMP JmapService::MessageURIToMsgHdr(const nsACString& aUri,
                                               nsIMsgDBHdr** aMsgHdr) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::LoadMessage(const nsACString& aUri,
                                       nsISupports* aDisplayConsumer,
                                       nsIMsgWindow* aMsgWindow,
                                       nsIUrlListener* aUrlListener,
                                       nsIURI** aURL) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::DisplayMessage(
    const nsACString& aUri, nsISupports* aDisplayConsumer,
    nsIMsgWindow* aMsgWindow, nsIUrlListener* aUrlListener,
    const nsACString& aCharsetOverride, nsIURI** aURL) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::OpenMessage(const nsACString& aUri,
                                       nsISupports* aDisplayConsumer,
                                       nsIMsgWindow* aMsgWindow,
                                       nsIUrlListener* aUrlListener,
                                       const nsACString& aCharsetOverride,
                                       nsIURI** aURL) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::CopyMessage(const nsACString& aUri,
                                       nsIStreamListener* aMailboxCopyHandler,
                                       bool aMoveMessage,
                                       nsIUrlListener* aUrlListener,
                                       nsIMsgWindow* aMsgWindow) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::SaveMessageToDisk(const nsACString& aUri,
                                             nsIFile* aFile, bool aAddDummyEnvelope,
                                             nsIUrlListener* aUrlListener,
                                             nsIMsgWindow* aMsgWindow,
                                             bool aCanLeaflet) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::GetUrlForUri(const nsACString& aUri,
                                         nsIMsgWindow* aMsgWindow,
                                         nsIURI** aURL) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::Search(nsIMsgSearchSession* aSearchSession,
                                   nsIMsgWindow* aMsgWindow,
                                   nsIMsgFolder* aMsgFolder,
                                   const nsACString& aSearchUri) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::StreamMessage(
    const nsACString& aUri, nsIStreamListener* aConsumer,
    nsIMsgWindow* aMsgWindow, nsIUrlListener* aUrlListener,
    bool aConvertData, const nsACString& aAdditionalHeader,
    bool aLocalOnly, nsIURI** aURL) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::StreamHeaders(const nsACString& aMessageHeader,
                                          nsIStreamListener* aConsumer,
                                          nsIUrlListener* aUrlListener,
                                          bool* aAbort) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapService::IsMultipartRelated(nsIURI* aUrl, bool* aIsMultipart) {
  NS_ENSURE_ARG_POINTER(aIsMultipart);
  *aIsMultipart = false;
  return NS_OK;
}
