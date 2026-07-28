/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapClient.h"
#include "mozilla/Logging.h"

mozilla::LazyLogModule gJmapLog("jmap");

#undef LOG
#define LOG(args) MOZ_LOG(gJmapLog, mozilla::LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(JmapClient, IJmapClient)

JmapClient::JmapClient() = default;

JmapClient::~JmapClient() = default;

NS_IMETHODIMP
JmapClient::Initialize(const nsACString& aEndpoint) {
  LOG(("JmapClient::Initialize endpoint=%s",
       PromiseFlatCString(aEndpoint).get()));
  mInitialized = true;
  return NS_OK;
}

NS_IMETHODIMP
JmapClient::Shutdown() {
  LOG(("JmapClient::Shutdown"));
  mInitialized = false;
  return NS_OK;
}

NS_IMETHODIMP
JmapClient::DiscoverSession() {
  LOG(("JmapClient::DiscoverSession"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
JmapClient::GetSessionState(nsACString& aSessionState) {
  aSessionState.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
JmapClient::GetAccountId(nsACString& aAccountId) {
  aAccountId.Truncate();
  return NS_OK;
}
