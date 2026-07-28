/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapFolderSyncListener.h"
#include "IJmapClient.h"
#include "JmapIncomingServer.h"
#include "JmapClient.h"
#include "mozilla/Logging.h"
#include "nsIMsgFolderNotificationService.h"
#include "nsComponentManagerUtils.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgUtils.h"
#include "nsReadableUtils.h"

#include <functional>

using namespace mozilla;

extern LazyLogModule gJmapLog;

NS_IMPL_ISUPPORTS(JmapFolderSyncListener, IJmapFolderListener)

NS_IMETHODIMP
JmapFolderSyncListener::OnFolderDiscoveryComplete(
    const nsACString& aState,
    const nsTArray<RefPtr<IJmapMailbox>>& aMailboxes) {
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JMAP: folder discovery complete: state=%s, count=%u",
           PromiseFlatCString(aState).get(),
           static_cast<uint32_t>(aMailboxes.Length())));

  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = mServer->GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = rootFolder->SetStringProperty("jmapMailboxState", aState);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < aMailboxes.Length(); i++) {
    RefPtr<IJmapMailbox> mbox = aMailboxes[i];

    nsAutoCString id, parentId, role;
    nsString name;
    int32_t sortOrder = 0;

    mbox->GetId(id);
    mbox->GetName(name);
    mbox->GetParentId(parentId);
    mbox->GetRole(role);
    mbox->GetSortOrder(&sortOrder);

    rv = CreateFolder(id, parentId, name, role, sortOrder);
    if (NS_FAILED(rv)) {
      MOZ_LOG(gJmapLog, LogLevel::Warning,
              ("JMAP: failed to create folder (id=%s): 0x%x",
               PromiseFlatCString(id).get(),
               static_cast<uint32_t>(rv)));
    }
  }

  if (mPostSyncCallback) {
    return mPostSyncCallback();
  }

  return NS_OK;
}

NS_IMETHODIMP
JmapFolderSyncListener::OnFolderDiscoveryError(nsresult aStatus,
                                                const nsACString& aMessage) {
  MOZ_LOG(gJmapLog, LogLevel::Error,
          ("JMAP: folder discovery error: 0x%" PRIx32 " - %s",
           static_cast<uint32_t>(aStatus), PromiseFlatCString(aMessage).get()));

  if (mPostSyncCallback) {
    return mPostSyncCallback();
  }

  return NS_OK;
}

nsresult
JmapFolderSyncListener::FindFolderByJmapId(const nsACString& aJmapId,
                                            nsIMsgFolder** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = mServer->GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<RefPtr<nsIMsgFolder>> toScan;
  toScan.AppendElement(rootFolder);

  while (!toScan.IsEmpty()) {
    nsTArray<RefPtr<nsIMsgFolder>> nextScan;

    for (auto& folder : toScan) {
      nsAutoCString id;
      rv = folder->GetStringProperty("jmapMailboxId", id);
      if (NS_SUCCEEDED(rv) && id.Equals(aJmapId)) {
        folder.forget(aResult);
        return NS_OK;
      }

      nsTArray<RefPtr<nsIMsgFolder>> children;
      rv = folder->GetSubFolders(children);
      if (NS_SUCCEEDED(rv)) {
        nextScan.AppendElements(children);
      }
    }

    toScan = std::move(nextScan);
  }

  return NS_MSG_ERROR_FOLDER_MISSING;
}

nsresult
JmapFolderSyncListener::CreateFolder(const nsACString& aJmapId,
                                      const nsACString& aParentJmapId,
                                      const nsAString& aName,
                                      const nsACString& aRole,
                                      int32_t aSortOrder) {
  // Check if folder already exists.
  nsCOMPtr<nsIMsgFolder> existing;
  if (NS_SUCCEEDED(FindFolderByJmapId(aJmapId, getter_AddRefs(existing)))) {
    MOZ_LOG(gJmapLog, LogLevel::Debug,
            ("JMAP: folder %s already exists", PromiseFlatCString(aJmapId).get()));
    return NS_OK;
  }

  // Find parent folder.
  nsCOMPtr<nsIMsgFolder> parent;
  nsresult rv;
  if (aParentJmapId.IsEmpty()) {
    rv = mServer->GetRootFolder(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = FindFolderByJmapId(aParentJmapId, getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // CreateFolder takes AUTF8String, so convert the nsAString name.
  nsAutoCString nameUtf8;
  CopyUTF16toUTF8(aName, nameUtf8);

  nsCOMPtr<nsIMsgPluggableStore> msgStore;
  rv = mServer->GetMsgStore(getter_AddRefs(msgStore));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFolder> newFolder;
  rv = msgStore->CreateFolder(parent, nameUtf8, getter_AddRefs(newFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = newFolder->SetStringProperty("jmapMailboxId", aJmapId);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t flags = RoleToFlags(aRole);
  rv = newFolder->SetFlags(flags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPrintfCString sortOrderStr("%d", aSortOrder);
  rv = newFolder->SetStringProperty("jmapSortOrder", sortOrderStr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aRole.IsEmpty()) {
    rv = newFolder->SetStringProperty("jmapRole", aRole);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JMAP: created folder '%s' (id=%s, role=%s)",
           nameUtf8.get(),
           PromiseFlatCString(aJmapId).get(),
           PromiseFlatCString(aRole).get()));

  nsCOMPtr<nsIMsgFolderNotificationService> notifier =
      do_GetService("@mozilla.org/messenger/notificationservice;1");
  if (notifier) {
    notifier->NotifyFolderAdded(newFolder);
  }

  rv = parent->NotifyFolderAdded(newFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

uint32_t
JmapFolderSyncListener::RoleToFlags(const nsACString& aRole) {
  if (aRole.EqualsLiteral("inbox")) {
    return nsMsgFolderFlags::Inbox | nsMsgFolderFlags::Mail;
  }
  if (aRole.EqualsLiteral("archive")) {
    return nsMsgFolderFlags::Archive | nsMsgFolderFlags::Mail;
  }
  if (aRole.EqualsLiteral("drafts")) {
    return nsMsgFolderFlags::Drafts | nsMsgFolderFlags::Mail;
  }
  if (aRole.EqualsLiteral("sent")) {
    return nsMsgFolderFlags::SentMail | nsMsgFolderFlags::Mail;
  }
  if (aRole.EqualsLiteral("trash")) {
    return nsMsgFolderFlags::Trash | nsMsgFolderFlags::Mail;
  }
  if (aRole.EqualsLiteral("junk")) {
    return nsMsgFolderFlags::Junk | nsMsgFolderFlags::Mail;
  }
  return nsMsgFolderFlags::Mail;
}
