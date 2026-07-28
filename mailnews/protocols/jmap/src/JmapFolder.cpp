/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapFolder.h"
#include "JmapClient.h"
#include "mozilla/Logging.h"
#include "nsPrintfCString.h"
#include <cstdlib>

extern mozilla::LazyLogModule gJmapLog;

NS_IMPL_ISUPPORTS_INHERITED(JmapFolder, nsMsgDBFolder, IJmapFolder)

JmapFolder::JmapFolder() = default;

JmapFolder::~JmapFolder() {}

NS_IMETHODIMP
JmapFolder::GetDatabase() {
  // For now, return NS_ERROR_NOT_INITIALIZED.
  // TODO: Open/create the local mbox database for this folder.
  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
JmapFolder::GetJmapMailboxId(nsACString& aMailboxId) {
  return GetStringProperty("jmapMailboxId", aMailboxId);
}

NS_IMETHODIMP
JmapFolder::SetJmapMailboxId(const nsACString& aMailboxId) {
  return SetStringProperty("jmapMailboxId", aMailboxId);
}

NS_IMETHODIMP
JmapFolder::GetJmapRole(nsACString& aRole) {
  return GetStringProperty("jmapRole", aRole);
}

NS_IMETHODIMP
JmapFolder::GetJmapSortOrder(int32_t* aSortOrder) {
  NS_ENSURE_ARG_POINTER(aSortOrder);
  nsAutoCString val;
  nsresult rv = GetStringProperty("jmapSortOrder", val);
  NS_ENSURE_SUCCESS(rv, rv);
  *aSortOrder = val.IsEmpty() ? 0 : atoi(val.get());
  return NS_OK;
}

NS_IMETHODIMP
JmapFolder::GetTotalMessages(int32_t* aTotal) {
  NS_ENSURE_ARG_POINTER(aTotal);
  nsAutoCString val;
  nsresult rv = GetStringProperty("jmapTotalMessages", val);
  NS_ENSURE_SUCCESS(rv, rv);
  *aTotal = val.IsEmpty() ? 0 : atoi(val.get());
  return NS_OK;
}

NS_IMETHODIMP
JmapFolder::SetTotalMessages(int32_t aTotal) {
  nsPrintfCString val("%d", aTotal);
  return SetStringProperty("jmapTotalMessages", val);
}

NS_IMETHODIMP
JmapFolder::GetUnreadMessages(int32_t* aUnread) {
  NS_ENSURE_ARG_POINTER(aUnread);
  nsAutoCString val;
  nsresult rv = GetStringProperty("jmapUnreadMessages", val);
  NS_ENSURE_SUCCESS(rv, rv);
  *aUnread = val.IsEmpty() ? 0 : atoi(val.get());
  return NS_OK;
}

NS_IMETHODIMP
JmapFolder::SetUnreadMessages(int32_t aUnread) {
  nsPrintfCString val("%d", aUnread);
  return SetStringProperty("jmapUnreadMessages", val);
}

NS_IMETHODIMP
JmapFolder::GetTotalThreads(int32_t* aTotal) {
  NS_ENSURE_ARG_POINTER(aTotal);
  nsAutoCString val;
  nsresult rv = GetStringProperty("jmapTotalThreads", val);
  NS_ENSURE_SUCCESS(rv, rv);
  *aTotal = val.IsEmpty() ? 0 : atoi(val.get());
  return NS_OK;
}

NS_IMETHODIMP
JmapFolder::SetTotalThreads(int32_t aTotal) {
  nsPrintfCString val("%d", aTotal);
  return SetStringProperty("jmapTotalThreads", val);
}

NS_IMETHODIMP
JmapFolder::GetUnreadThreads(int32_t* aUnread) {
  NS_ENSURE_ARG_POINTER(aUnread);
  nsAutoCString val;
  nsresult rv = GetStringProperty("jmapUnreadThreads", val);
  NS_ENSURE_SUCCESS(rv, rv);
  *aUnread = val.IsEmpty() ? 0 : atoi(val.get());
  return NS_OK;
}

NS_IMETHODIMP
JmapFolder::SetUnreadThreads(int32_t aUnread) {
  nsPrintfCString val("%d", aUnread);
  return SetStringProperty("jmapUnreadThreads", val);
}

NS_IMETHODIMP
JmapFolder::GetJmapMailboxState(nsACString& aState) {
  return GetStringProperty("jmapMailboxState", aState);
}
