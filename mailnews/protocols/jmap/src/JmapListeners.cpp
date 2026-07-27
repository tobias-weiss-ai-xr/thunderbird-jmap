/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapListeners.h"
#include "nsMsgFolderFlags.h"
#include "mozilla/Logging.h"

using namespace mozilla;

extern LazyLogModule gJmapLog;

// ---------------------------------------------------------------------------
// JmapFolderSyncListener
// ---------------------------------------------------------------------------

JmapFolderSyncListener::JmapFolderSyncListener(
    NewRootFolderCallback&& onNewRootFolder,
    FolderCreatedCallback&& onFolderCreated,
    FolderUpdatedCallback&& onFolderUpdated,
    FolderDeletedCallback&& onFolderDeleted,
    SyncStateChangedCallback&& onSyncStateChanged,
    SuccessCallback&& onSuccess, FailureCallback&& onError)
    : mOnNewRootFolder(std::move(onNewRootFolder)),
      mOnFolderCreated(std::move(onFolderCreated)),
      mOnFolderUpdated(std::move(onFolderUpdated)),
      mOnFolderDeleted(std::move(onFolderDeleted)),
      mOnSyncStateChanged(std::move(onSyncStateChanged)),
      mOnSuccess(std::move(onSuccess)),
      mOnError(std::move(onError)) {}

JmapFolderSyncListener::~JmapFolderSyncListener() = default;

NS_IMPL_ISUPPORTS(JmapFolderSyncListener, IJmapFolderListener)

NS_IMETHODIMP JmapFolderSyncListener::OnNewRootMailbox(const nsACString& aId) {
  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JmapFolderSyncListener::OnNewRootMailbox: id=%s",
           PromiseFlatCString(aId).get()));
  return mOnNewRootFolder(aId);
}

NS_IMETHODIMP JmapFolderSyncListener::OnMailboxCreated(
    const nsACString& aId, const nsACString& aParentId,
    const nsACString& aName, const nsACString& aRole, uint32_t aFlags) {
  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JmapFolderSyncListener::OnMailboxCreated: id=%s name=%s role=%s",
           PromiseFlatCString(aId).get(), PromiseFlatCString(aName).get(),
           PromiseFlatCString(aRole).get()));
  return mOnFolderCreated(aId, aParentId, aName, aRole, aFlags);
}

NS_IMETHODIMP JmapFolderSyncListener::OnMailboxUpdated(
    const nsACString& aId, const nsACString& aParentId,
    const nsACString& aName, const nsACString& aRole) {
  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JmapFolderSyncListener::OnMailboxUpdated: id=%s name=%s",
           PromiseFlatCString(aId).get(), PromiseFlatCString(aName).get()));
  return mOnFolderUpdated(aId, aParentId, aName, aRole);
}

NS_IMETHODIMP JmapFolderSyncListener::OnMailboxDeleted(const nsACString& aId) {
  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JmapFolderSyncListener::OnMailboxDeleted: id=%s",
           PromiseFlatCString(aId).get()));
  return mOnFolderDeleted(aId);
}

NS_IMETHODIMP JmapFolderSyncListener::OnSinceStateChanged(
    const nsACString& aSinceStateToken) {
  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JmapFolderSyncListener::OnSinceStateChanged: state=%s",
           PromiseFlatCString(aSinceStateToken).get()));
  return mOnSyncStateChanged(aSinceStateToken);
}

NS_IMETHODIMP JmapFolderSyncListener::OnSuccess() {
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapFolderSyncListener::OnSuccess: mailbox hierarchy sync complete"));
  return mOnSuccess();
}

NS_IMETHODIMP JmapFolderSyncListener::OnFailure(nsresult aStatus) {
  MOZ_LOG(gJmapLog, LogLevel::Error,
          ("JmapFolderSyncListener::OnFailure: status=0x%08x",
           static_cast<uint32_t>(aStatus)));
  return mOnError(aStatus);
}

// ---------------------------------------------------------------------------
// Utility: JMAP role -> Thunderbird folder flags
// ---------------------------------------------------------------------------

uint32_t JmapRoleToFolderFlags(const nsACString& aRole) {
  uint32_t flags = 0;

  if (aRole.EqualsLiteral("inbox")) {
    flags |= nsMsgFolderFlags::Inbox;
  } else if (aRole.EqualsLiteral("archive")) {
    flags |= nsMsgFolderFlags::Archive;
  } else if (aRole.EqualsLiteral("drafts")) {
    flags |= nsMsgFolderFlags::Drafts;
  } else if (aRole.EqualsLiteral("junk")) {
    flags |= nsMsgFolderFlags::Junk;
  } else if (aRole.EqualsLiteral("sent")) {
    flags |= nsMsgFolderFlags::SentMail;
  } else if (aRole.EqualsLiteral("trash")) {
    flags |= nsMsgFolderFlags::Trash;
  }

  return flags;
}
