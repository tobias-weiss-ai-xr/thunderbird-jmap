/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapProtocolHandler.h"

#include "JmapMessageChannel.h"

nsresult
NS_CreateJmapProtocolHandler(REFNSIID aIID, void** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  RefPtr<JmapProtocolHandler> instance(new JmapProtocolHandler());
  return instance->QueryInterface(aIID, aResult);
}

nsresult
JmapProtocolHandler::Create(REFNSIID aIID, void** aResult) {
  return NS_CreateJmapProtocolHandler(aIID, aResult);
}

NS_IMPL_ISUPPORTS(JmapProtocolHandler, nsIProtocolHandler)

JmapProtocolHandler::JmapProtocolHandler() = default;

JmapProtocolHandler::~JmapProtocolHandler() = default;

NS_IMETHODIMP
JmapProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("x-moz-jmap");
  return NS_OK;
}

NS_IMETHODIMP
JmapProtocolHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                nsIChannel** _retval) {
  nsCString spec;
  MOZ_TRY(aURI->GetSpec(spec));

  bool convert = false;
  if (spec.Find("part=") != kNotFound ||
      spec.Find("convert=true") != kNotFound) {
    convert = true;
  }

  RefPtr<JmapMessageChannel> channel = new JmapMessageChannel(aURI, convert);
  MOZ_TRY(channel->SetLoadInfo(aLoadInfo));

  // Add the attachment disposition for non-message parts.
  if (spec.Find("part=") >= 0 && spec.Find("type=message/rfc822") < 0 &&
      spec.Find("type=application/x-message-display") < 0 &&
      spec.Find("type=application/pdf") < 0) {
    MOZ_TRY(channel->SetContentDisposition(nsIChannel::DISPOSITION_ATTACHMENT));
  }

  channel.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
JmapProtocolHandler::AllowPort(int32_t aPort, const char* aScheme,
                                bool* aAllow) {
  MOZ_ASSERT_UNREACHABLE("call to AllowPort on internal protocol");
  NS_ENSURE_ARG_POINTER(aAllow);
  *aAllow = false;
  return NS_OK;
}
