/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapIncomingServer.h"
#include "mozilla/Logging.h"

NS_IMPL_ISUPPORTS_INHERITED(JmapIncomingServer, nsMsgIncomingServer,
                             IJmapIncomingServer)

JmapIncomingServer::JmapIncomingServer() = default;

JmapIncomingServer::~JmapIncomingServer() = default;

NS_IMETHODIMP
JmapIncomingServer::GetJmapUrl(nsACString& aJmapUrl) {
  aJmapUrl = mJmapUrl;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::SetJmapUrl(const nsACString& aJmapUrl) {
  mJmapUrl = aJmapUrl;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::GetJmapAccountId(nsACString& aAccountId) {
  aAccountId = mJmapAccountId;
  return NS_OK;
}

NS_IMETHODIMP
JmapIncomingServer::SetJmapAccountId(const nsACString& aAccountId) {
  mJmapAccountId = aAccountId;
  return NS_OK;
}
