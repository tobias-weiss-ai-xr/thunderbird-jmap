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
#include "nsString.h"

#include <functional>

using namespace mozilla;

extern LazyLogModule gJmapLog;

NS_IMPL_ISUPPORTS(JmapFolderSyncListener, IJmapFolderListener)

// A lightweight representation of a JMAP mailbox parsed from JSON.
struct ParsedMailbox {
  nsCString id;
  nsString name;
  nsCString parentId;
  nsCString role;
  uint32_t sortOrder = 0;
  uint32_t totalEmails = 0;
  uint32_t unreadEmails = 0;
};

NS_IMETHODIMP
JmapFolderSyncListener::OnFolderDiscoveryComplete(
    const nsACString& aState, const nsACString& aMailboxesJSON) {
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JMAP: folder discovery complete: state=%s, JSON=%d bytes",
           PromiseFlatCString(aState).get(),
           aMailboxesJSON.Length()));

  // Persist the sync state on the root folder.
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = mServer->GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = rootFolder->SetStringProperty("jmapMailboxState", aState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Parse the mailbox JSON array using a simple JSON iterator.
  // We avoid full JSON parser dependencies; instead we use a lightweight
  // scan: the JSON is a serialized array of objects with known keys.
  rv = ParseAndCreateFolders(aMailboxesJSON);
  NS_ENSURE_SUCCESS(rv, rv);

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
           static_cast<uint32_t>(aStatus),
           PromiseFlatCString(aMessage).get()));

  if (mPostSyncCallback) {
    return mPostSyncCallback();
  }

  return NS_OK;
}

nsresult
JmapFolderSyncListener::ParseAndCreateFolders(const nsACString& aJSON) {
  // We parse the JSON manually without external dependencies.
  // The JSON is an array of objects: [{"id":"...","name":"...",...},...]
  //
  // For each object we extract: id, name, parentId, role, sortOrder,
  // totalEmails, unreadEmails.

  nsresult rv;
  NS_ConvertUTF8toUTF16 json16(aJSON);
  const char16_t* cur = json16.BeginReading();
  const char16_t* end = json16.EndReading();

  // Skip whitespace and outer array bracket.
  while (cur < end && *cur <= ' ') ++cur;
  if (cur >= end || *cur != '[') return NS_ERROR_FAILURE;
  ++cur;  // skip '['

  while (cur < end) {
    // Skip whitespace and commas
    while (cur < end && (*cur <= ' ' || *cur == ',')) ++cur;
    if (cur >= end || *cur == ']') break;
    if (*cur != '{') return NS_ERROR_FAILURE;
    ++cur;  // skip '{'

    nsAutoCString id;
    nsAutoString name;
    nsAutoCString parentId;
    nsAutoCString role;
    int32_t sortOrder = 0;
    int32_t totalEmails = 0;
    int32_t unreadEmails = 0;

    // Parse key-value pairs inside the object.
    while (cur < end && *cur != '}') {
      // Skip whitespace and commas
      while (cur < end && (*cur <= ' ' || *cur == ',')) ++cur;
      if (cur >= end || *cur == '}') break;

      // Read quoted key
      if (*cur != '"') return NS_ERROR_FAILURE;
      ++cur;  // skip opening quote
      nsAutoCString key;
      while (cur < end && *cur != '"') {
        if (*cur == '\\') ++cur;  // skip escape
        if (cur < end) key.Append(char16_t(*cur));
        ++cur;
      }
      if (cur >= end) return NS_ERROR_FAILURE;
      ++cur;  // skip closing quote

      // Skip colon
      while (cur < end && *cur <= ' ') ++cur;
      if (cur >= end || *cur != ':') return NS_ERROR_FAILURE;
      ++cur;
      while (cur < end && *cur <= ' ') ++cur;

      // Read value
      if (cur < end && *cur == '"') {
        // String value
        ++cur;  // skip opening quote
        nsAutoCString strVal;
        while (cur < end && *cur != '"') {
          if (*cur == '\\') ++cur;
          if (cur < end) strVal.Append(char16_t(*cur));
          ++cur;
        }
        if (cur >= end) return NS_ERROR_FAILURE;
        ++cur;  // skip closing quote

        if (key.EqualsLiteral("id")) {
          id = strVal;
        } else if (key.EqualsLiteral("name")) {
          CopyUTF8toUTF16(strVal, name);
        } else if (key.EqualsLiteral("parentId")) {
          parentId = strVal;
        } else if (key.EqualsLiteral("role")) {
          role = strVal;
        }
      } else if (cur < end && (*cur == '-' || (*cur >= '0' && *cur <= '9') || *cur == 't' || *cur == 'f' || *cur == 'n')) {
        // Number or boolean or null
        nsAutoCString numStr;
        if (*cur == 't' || *cur == 'f' || *cur == 'n') {
          // Skip true/false/null
          while (cur < end && ((*cur >= 'a' && *cur <= 'z') || (*cur >= 'A' && *cur <= 'Z'))) {
            numStr.Append(char16_t(*cur));
            ++cur;
          }
        } else {
          while (cur < end && (*cur == '-' || *cur == '+' || *cur == '.' || (*cur >= '0' && *cur <= '9') || *cur == 'e' || *cur == 'E')) {
            numStr.Append(char16_t(*cur));
            ++cur;
          }
        }

        if (key.EqualsLiteral("sortOrder")) {
          sortOrder = atoi(numStr.get());
        } else if (key.EqualsLiteral("totalEmails")) {
          totalEmails = atoi(numStr.get());
        } else if (key.EqualsLiteral("unreadEmails")) {
          unreadEmails = atoi(numStr.get());
        }
      } else if (cur < end && *cur == '[') {
        // Skip arrays (not used for folder creation)
        int depth = 1;
        ++cur;
        while (cur < end && depth > 0) {
          if (*cur == '[') ++depth;
          else if (*cur == ']') --depth;
          ++cur;
        }
      } else if (cur < end && *cur == '{') {
        // Skip nested objects (not used for folder creation)
        int depth = 1;
        ++cur;
        while (cur < end && depth > 0) {
          if (*cur == '{') ++depth;
          else if (*cur == '}') --depth;
          ++cur;
        }
      }
    }
    if (cur < end && *cur == '}') ++cur;  // skip '}'

    // Create the folder if we have an id.
    if (!id.IsEmpty()) {
      rv = CreateFolder(id, parentId, name, role, sortOrder,
                        totalEmails, unreadEmails);
      if (NS_FAILED(rv)) {
        MOZ_LOG(gJmapLog, LogLevel::Warning,
                ("JMAP: failed to create folder (id=%s): 0x%x",
                 id.get(), static_cast<uint32_t>(rv)));
      }
    }
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
                                      int32_t aSortOrder,
                                      int32_t aTotalEmails,
                                      int32_t aUnreadEmails) {
  // Check if folder already exists.
  nsCOMPtr<nsIMsgFolder> existing;
  if (NS_SUCCEEDED(FindFolderByJmapId(aJmapId, getter_AddRefs(existing)))) {
    MOZ_LOG(gJmapLog, LogLevel::Debug,
            ("JMAP: folder %s already exists", PromiseFlatCString(aJmapId).get()));
    // Update metadata in case it changed.
    nsresult rv = existing->SetStringProperty("jmapMailboxId", aJmapId);
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t flags = RoleToFlags(aRole);
    rv = existing->SetFlags(flags);
    NS_ENSURE_SUCCESS(rv, rv);
    nsPrintfCString sortOrderStr("%d", aSortOrder);
    rv = existing->SetStringProperty("jmapSortOrder", sortOrderStr);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aRole.IsEmpty()) {
      rv = existing->SetStringProperty("jmapRole", aRole);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    nsPrintfCString totalStr("%d", aTotalEmails);
    existing->SetStringProperty("jmapTotalMessages", totalStr);
    nsPrintfCString unreadStr("%d", aUnreadEmails);
    existing->SetStringProperty("jmapUnreadMessages", unreadStr);
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
    if (NS_FAILED(rv)) {
      // Parent not found yet — create this folder under root as fallback.
      MOZ_LOG(gJmapLog, LogLevel::Warning,
              ("JMAP: parent %s not found for %s, attaching to root",
               PromiseFlatCString(aParentJmapId).get(),
               PromiseFlatCString(aJmapId).get()));
      rv = mServer->GetRootFolder(getter_AddRefs(parent));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // CreateFolder takes AUTF8String, so convert the nsAString name.
  nsAutoCString nameUtf8;
  CopyUTF16toUTF8(aName, nameUtf8);

  nsCOMPtr<nsIMsgPluggableStore> msgStore;
  rv = mServer->GetMsgStore(getter_AddRefs(msgStore));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFolder> newFolder;
  rv = msgStore->CreateFolder(parent, nameUtf8, getter_AddRefs(newFolder));
  if (NS_FAILED(rv)) {
    // Some stores (e.g. maildir) may fail if folder exists; try to find it.
    MOZ_LOG(gJmapLog, LogLevel::Warning,
            ("JMAP: CreateFolder failed for '%s' (0x%x), trying lookup",
             nameUtf8.get(), static_cast<uint32_t>(rv)));
    rv = FindFolderByJmapId(aJmapId, getter_AddRefs(newFolder));
    NS_ENSURE_SUCCESS(rv, rv);
    existing = newFolder;
    if (existing) {
      rv = existing->SetStringProperty("jmapMailboxId", aJmapId);
      NS_ENSURE_SUCCESS(rv, rv);
      uint32_t flags = RoleToFlags(aRole);
      rv = existing->SetFlags(flags);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }

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

  nsPrintfCString totalStr("%d", aTotalEmails);
  newFolder->SetStringProperty("jmapTotalMessages", totalStr);
  nsPrintfCString unreadStr("%d", aUnreadEmails);
  newFolder->SetStringProperty("jmapUnreadMessages", unreadStr);

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
