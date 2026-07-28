/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapProtocolInfo.h"

NS_IMPL_ISUPPORTS(JmapProtocolInfo, nsIMsgProtocolInfo)

JmapProtocolInfo::JmapProtocolInfo() = default;

JmapProtocolInfo::~JmapProtocolInfo() = default;

NS_IMETHODIMP
JmapProtocolInfo::GetDefaultServerPort(bool isSecure, int32_t* aPort) {
  NS_ENSURE_ARG_POINTER(aPort);
  *aPort = isSecure ? 993 : 443;
  return NS_OK;
}

NS_IMETHODIMP
JmapProtocolInfo::GetDefaultDoBiff(bool* aDoBiff) {
  NS_ENSURE_ARG_POINTER(aDoBiff);
  *aDoBiff = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetRequiresUsername(bool* aVal) { *aVal = true; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetPreflightPrettyNameWithEmailAddress(bool* aVal) { *aVal = false; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetCanDelete(bool* aVal) { *aVal = false; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetCanLoginAtStartUp(bool* aVal) { *aVal = true; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetCanDuplicate(bool* aVal) { *aVal = false; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetCanGetMessages(bool* aVal) { *aVal = true; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetCanGetIncomingMessages(bool* aVal) { *aVal = true; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetShowComposeMsgLink(bool* aVal) { *aVal = false; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetFoldersCreatedAsync(bool* aVal) { *aVal = true; return NS_OK; }
NS_IMETHODIMP JmapProtocolInfo::GetServerIID(nsIID& aIID) { return NS_ERROR_NOT_IMPLEMENTED; }
NS_IMETHODIMP JmapProtocolInfo::GetDefaultLocalPath(nsIFile** aPath) { return NS_ERROR_NOT_IMPLEMENTED; }
NS_IMETHODIMP JmapProtocolInfo::SetDefaultLocalPath(nsIFile* aPath) { return NS_ERROR_NOT_IMPLEMENTED; }
