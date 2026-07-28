/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapProtocolHandler.h"

NS_IMPL_ISUPPORTS(JmapProtocolHandler, nsIProtocolHandler)

JmapProtocolHandler::JmapProtocolHandler() = default;

JmapProtocolHandler::~JmapProtocolHandler() = default;

nsresult
JmapProtocolHandler::Create(REFNSIID aIID, void** aResult) {
  RefPtr<JmapProtocolHandler> handler = new JmapProtocolHandler();
  return handler->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
JmapProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("jmap");
  return NS_OK;
}

NS_IMETHODIMP
JmapProtocolHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                nsIChannel** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
JmapProtocolHandler::AllowPort(int32_t aPort, const char* aScheme,
                                bool* aAllow) {
  *aAllow = false;
  return NS_OK;
}
