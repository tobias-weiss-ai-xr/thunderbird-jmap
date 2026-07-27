/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapProtocolInfo.h"

#include "JmapIncomingServer.h"
#include "nsMailDirServiceDefs.h"
#include "nsMsgUtils.h"

#define PREF_MAIL_ROOT_JMAP_REL "mail.root.jmap-rel"
#define PREF_MAIL_ROOT_JMAP "mail.root.jmap"

NS_IMPL_ISUPPORTS(JmapProtocolInfo, nsIMsgProtocolInfo)

JmapProtocolInfo::JmapProtocolInfo() = default;
JmapProtocolInfo::~JmapProtocolInfo() = default;

NS_IMETHODIMP JmapProtocolInfo::GetDefaultLocalPath(nsIFile** aDefaultLocalPath) {
  NS_ENSURE_ARG_POINTER(aDefaultLocalPath);
  *aDefaultLocalPath = nullptr;

  bool havePref;
  nsCOMPtr<nsIFile> localFile;
  nsresult rv = NS_GetPersistentFile(PREF_MAIL_ROOT_JMAP_REL, PREF_MAIL_ROOT_JMAP,
                                     NS_APP_MAIL_50_DIR, havePref,
                                     getter_AddRefs(localFile));
  if (NS_FAILED(rv)) return rv;

  bool exists;
  rv = localFile->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists) {
    rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
  }

  if (NS_FAILED(rv)) return rv;

  if (!havePref || !exists) {
    rv = NS_SetPersistentFile(PREF_MAIL_ROOT_JMAP_REL, PREF_MAIL_ROOT_JMAP,
                              localFile);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to set root dir pref.");
  }

  localFile.forget(aDefaultLocalPath);
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::SetDefaultLocalPath(nsIFile* aDefaultLocalPath) {
  NS_ENSURE_ARG(aDefaultLocalPath);
  return NS_SetPersistentFile(PREF_MAIL_ROOT_JMAP_REL, PREF_MAIL_ROOT_JMAP,
                              aDefaultLocalPath);
}

NS_IMETHODIMP JmapProtocolInfo::GetServerIID(nsIID& aServerIID) {
  aServerIID = NS_GET_IID(JmapIncomingServer);
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetRequiresUsername(bool* aRequiresUsername) {
  NS_ENSURE_ARG_POINTER(aRequiresUsername);
  *aRequiresUsername = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetPreflightPrettyNameWithEmailAddress(
    bool* aPreflightPrettyNameWithEmailAddress) {
  NS_ENSURE_ARG_POINTER(aPreflightPrettyNameWithEmailAddress);
  *aPreflightPrettyNameWithEmailAddress = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetCanDelete(bool* aCanDelete) {
  NS_ENSURE_ARG_POINTER(aCanDelete);
  *aCanDelete = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetCanLoginAtStartUp(bool* aCanLoginAtStartUp) {
  NS_ENSURE_ARG_POINTER(aCanLoginAtStartUp);
  *aCanLoginAtStartUp = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetCanDuplicate(bool* aCanDuplicate) {
  NS_ENSURE_ARG_POINTER(aCanDuplicate);
  *aCanDuplicate = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetDefaultServerPort(bool isSecure,
                                                     int32_t* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  // JMAP uses standard HTTP(S) ports.
  // Stalwart defaults to HTTPS on 443.
  if (isSecure) {
    *_retval = 443;
  } else {
    *_retval = 80;
  }

  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetCanGetMessages(bool* aCanGetMessages) {
  NS_ENSURE_ARG_POINTER(aCanGetMessages);
  *aCanGetMessages = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetCanGetIncomingMessages(
    bool* aCanGetIncomingMessages) {
  NS_ENSURE_ARG_POINTER(aCanGetIncomingMessages);
  *aCanGetIncomingMessages = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetDefaultDoBiff(bool* aDefaultDoBiff) {
  NS_ENSURE_ARG_POINTER(aDefaultDoBiff);
  *aDefaultDoBiff = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetShowComposeMsgLink(
    bool* aShowComposeMsgLink) {
  NS_ENSURE_ARG_POINTER(aShowComposeMsgLink);
  *aShowComposeMsgLink = true;
  return NS_OK;
}

NS_IMETHODIMP JmapProtocolInfo::GetFoldersCreatedAsync(
    bool* aFoldersCreatedAsync) {
  NS_ENSURE_ARG_POINTER(aFoldersCreatedAsync);
  *aFoldersCreatedAsync = true;
  return NS_OK;
}
