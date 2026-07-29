/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapMessageSyncListener.h"

#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
#include "nsMsgMessageFlags.h"
#include "nsMsgUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gJmapLog;

NS_IMPL_ISUPPORTS(JmapMessageSyncListener, IJmapMessageListener)

NS_IMETHODIMP
JmapMessageSyncListener::OnMessagesFetched(
    const nsACString& aState, const nsTArray<nsCString>& aMessageIds) {
  MOZ_LOG(gJmapLog, mozilla::LogLevel::Info,
          ("JMAP: messages fetched: count=%u, state=%s",
           static_cast<uint32_t>(aMessageIds.Length()),
           PromiseFlatCString(aState).get()));

  // Persist the sync state on the folder.
  nsresult rv = mFolder->SetStringProperty("jmapSyncState", aState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Process each message ID — add a placeholder header to the local database.
  return ProcessMessageIds(aMessageIds);
}

NS_IMETHODIMP
JmapMessageSyncListener::OnMessageFetchError(
    nsresult aStatus, const nsACString& aMessage) {
  MOZ_LOG(gJmapLog, mozilla::LogLevel::Error,
          ("JMAP: message fetch error: 0x%" PRIx32 " - %s",
           static_cast<uint32_t>(aStatus),
           PromiseFlatCString(aMessage).get()));
  return NS_OK;
}

nsresult
JmapMessageSyncListener::ProcessMessageIds(
    const nsTArray<nsCString>& aMessageIds) {
  // Get or create the local database for this folder.
  nsCOMPtr<nsIMsgDatabase> db;
  nsresult rv;
  nsCOMPtr<nsIMsgDBService> dbService =
      do_GetService("@mozilla.org/msgDatabase/msgDBService;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbService->OpenFolderDB(mFolder, false, getter_AddRefs(db));
  if (NS_FAILED(rv)) {
    rv = dbService->CreateNewDB(mFolder, getter_AddRefs(db));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  uint32_t addedCount = 0;

  for (uint32_t i = 0; i < aMessageIds.Length(); i++) {
    const nsCString& msgId = aMessageIds[i];

    // Parse the message key from the JMAP ID.
    nsMsgKey msgKey = msgKeyFromInt(ParseUint64Str(msgId.get()));

    // Check if we already have this header in the DB.
    nsCOMPtr<nsIMsgDBHdr> existingHdr;
    rv = db->GetMsgHdrForKey(msgKey, getter_AddRefs(existingHdr));
    if (NS_SUCCEEDED(rv) && existingHdr) {
      continue;  // Already present.
    }

    // Create a placeholder header in the database.
    nsCOMPtr<nsIMsgDBHdr> newHdr;
    rv = db->CreateNewHdrWithSpecificMsgKey(msgKey, getter_AddRefs(newHdr));
    if (NS_FAILED(rv)) {
      MOZ_LOG(gJmapLog, mozilla::LogLevel::Warning,
              ("JMAP: failed to create header for key %u", msgKey));
      continue;
    }

    // Store the JMAP message ID as a property on the header.
    newHdr->SetStringProperty("jmapMessageId", msgId);

    // Mark as offline (will be fetched on demand).
    uint32_t newFlags = 0;
    newHdr->OrFlags(nsMsgMessageFlags::Offline, &newFlags);

    rv = db->AddNewHdrToDB(newHdr, true);
    if (NS_SUCCEEDED(rv)) {
      addedCount++;
      MOZ_LOG(gJmapLog, mozilla::LogLevel::Debug,
              ("JMAP: added header for msgId=%s (key=%u)",
               msgId.get(), msgKey));
    }
  }

  // Commit changes.
  db->Commit(nsMsgDBCommitType::kSessionCommit);
  db->Close(true);

  // Notify folder that new messages arrived.
  if (addedCount > 0) {
    mFolder->NotifyFolderEvent("FolderLoaded"_ns);
    mFolder->NotifyFolderEvent("NewMessages"_ns);
  }

  return NS_OK;
}
