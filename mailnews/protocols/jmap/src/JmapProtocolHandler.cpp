/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsIURI.h"

NS_IMPL_ISUPPORTS(JmapProtocolHandler, nsIProtocolHandler)

NS_IMETHODIMP JmapProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("x-moz-jmap");
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolHandler::GetDefaultPort(int32_t* aDefaultPort) {
  *aDefaultPort = 443;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolHandler::GetProtocolFlags(uint32_t* aProtocolFlags) {
  *aProtocolFlags =
      URI_NORELATIVE | URI_FORBIDS_AUTOMATIC_DOCUMENT_REPLACEMENT |
      URI_DANGEROUS_TO_LOAD | URI_FORBIDS_COOKIE_ACCESS | ORIGIN_IS_FULL_SPEC;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolHandler::NewChannel(nsIURI* aURI,
                                              nsILoadInfo* aLoadInfo,
                                              nsIChannel** _retval) {
  // TODO: Create a proper JMAP message channel
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP JmapProtocolHandler::NewChannel2(nsIURI* aURI,
                                                 nsILoadInfo* aLoadInfo,
                                                 nsIChannel** _retval) {
  return NewChannel(aURI, aLoadInfo, _retval);
}

NS_IMETHODIMP JmapProtocolHandler::AllowPort(int32_t port,
                                              const char* scheme,
                                              bool* _retval) {
  // Don't override any ports
  *_retval = false;
  return NS_OK;
}

nsresult JmapProtocolHandler::Create(nsISupports* aOuter, REFNSIID aIID,
                                      void** aResult) {
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  RefPtr<JmapProtocolHandler> handler = new JmapProtocolHandler();
  return handler->QueryInterface(aIID, aResult);
}
