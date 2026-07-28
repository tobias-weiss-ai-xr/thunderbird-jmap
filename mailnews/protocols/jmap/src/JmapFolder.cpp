/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapFolder.h"

NS_IMPL_ISUPPORTS_INHERITED(JmapFolder, nsMsgDBFolder, IJmapFolder)

JmapFolder::JmapFolder() = default;

JmapFolder::~JmapFolder() = default;

NS_IMETHODIMP
JmapFolder::GetFolderId(nsACString& aFolderId) {
  aFolderId.Truncate();
  return NS_OK;
}

nsresult
JmapFolder::GetDatabase() {
  return NS_ERROR_NOT_IMPLEMENTED;
}
