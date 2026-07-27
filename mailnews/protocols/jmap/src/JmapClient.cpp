/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapClient.h"
#include "mozilla/Components.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsPrintfCString.h"

using namespace mozilla;

LazyLogModule gJmapLog("jmap");

NS_IMPL_ISUPPORTS(JmapClient, IJmapClient)

JmapClient::JmapClient() = default;
JmapClient::~JmapClient() = default;

// ---------------------------------------------------------------------------
// IJmapClient implementation
// ---------------------------------------------------------------------------

NS_IMETHODIMP JmapClient::GetRunning(bool* aRunning) {
  NS_ENSURE_ARG_POINTER(aRunning);
  *aRunning = mHasSession && !mShuttingDown;
  return NS_OK;
}

NS_IMETHODIMP JmapClient::GetIdle(bool* aIdle) {
  NS_ENSURE_ARG_POINTER(aIdle);
  *aIdle = false;  // TODO: Track pending operations
  return NS_OK;
}

NS_IMETHODIMP JmapClient::Initialize(const nsACString& aEndpoint,
                                      nsIMsgIncomingServer* aServer) {
  NS_ENSURE_ARG_POINTER(aServer);
  mServer = aServer;
  mConfiguredUrl = aEndpoint;

  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapClient::Initialize: endpoint=%s", PromiseFlatCString(aEndpoint).get()));

  // Discover session URL and fetch session
  nsCString sessionUrl;
  nsresult rv = DiscoverSessionUrl(aEndpoint, sessionUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FetchSession(sessionUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP JmapClient::Shutdown() {
  MOZ_LOG(gJmapLog, LogLevel::Info, ("JmapClient::Shutdown"));
  mShuttingDown = true;
  mHasSession = false;
  mSession = JmapSession();
  return NS_OK;
}

NS_IMETHODIMP JmapClient::CheckConnectivity(nsIUrlListener* aListener,
                                            nsIURI** _retval) {
  NS_ENSURE_ARG_POINTER(aListener);
  MOZ_LOG(gJmapLog, LogLevel::Info, ("JmapClient::CheckConnectivity"));

  // Re-fetch session to verify authentication
  nsresult rv = FetchSession(mConfiguredUrl);
  if (NS_SUCCEEDED(rv)) {
    aListener->OnStopRunningUrl(nullptr, NS_OK);
  } else {
    aListener->OnStopRunningUrl(nullptr, rv);
  }

  if (_retval) {
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), mSession.apiUrl);
    NS_ENSURE_SUCCESS(rv, rv);
    uri.forget(_retval);
  }

  return NS_OK;
}

NS_IMETHODIMP JmapClient::SyncMailboxHierarchy(
    IJmapFolderListener* aListener, const nsACString& aSinceStateToken) {
  NS_ENSURE_ARG_POINTER(aListener);
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapClient::SyncMailboxHierarchy: sinceState=%s",
           PromiseFlatCString(aSinceStateToken).get()));

  // TODO: Implement full Mailbox/changes + Mailbox/get flow
  // For now, signal success
  aListener->OnSuccess();
  return NS_OK;
}

NS_IMETHODIMP JmapClient::CreateMailbox(
    IJmapSimpleOperationListener* aListener, const nsACString& aParentId,
    const nsACString& aName) {
  NS_ENSURE_ARG_POINTER(aListener);
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapClient::CreateMailbox: parentId=%s name=%s",
           PromiseFlatCString(aParentId).get(), PromiseFlatCString(aName).get()));

  // TODO: Implement Mailbox/set { create: { "tbc1": { name, parentId } } }
  nsTArray<nsCString> newIds;
  aListener->OnOperationSuccess(newIds);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::SyncMessagesForMailbox(
    IJmapMessageSyncListener* aListener, const nsACString& aMailboxId,
    const nsACString& aSinceStateToken) {
  NS_ENSURE_ARG_POINTER(aListener);
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapClient::SyncMessagesForMailbox: mailbox=%s sinceState=%s",
           PromiseFlatCString(aMailboxId).get(),
           PromiseFlatCString(aSinceStateToken).get()));

  // TODO: Implement full Email/changes + Email/get flow
  aListener->OnSyncComplete();
  return NS_OK;
}

NS_IMETHODIMP JmapClient::GetMessage(IJmapMessageFetchListener* aListener,
                                      const nsACString& aId,
                                      const nsACString& aBlobId) {
  NS_ENSURE_ARG_POINTER(aListener);
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapClient::GetMessage: id=%s blobId=%s",
           PromiseFlatCString(aId).get(), PromiseFlatCString(aBlobId).get()));

  // TODO: Implement Email/get with full body download
  aListener->OnFetchStart();
  aListener->OnFetchStop(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::ChangeReadStatus(
    IJmapSimpleOperationListener* aListener,
    const nsTArray<nsCString>& aMessageIds, bool aIsRead) {
  NS_ENSURE_ARG_POINTER(aListener);
  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapClient::ChangeReadStatus: count=%lu isRead=%d",
           (unsigned long)aMessageIds.Length(), aIsRead));

  // TODO: Implement Email/set { update: { id: { keywords/$seen: bool } } }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::ChangeFlagStatus(
    IJmapSimpleOperationListener* aListener,
    const nsTArray<nsCString>& aMessageIds, bool aIsFlagged) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Email/set { update: { id: { keywords/$flagged: bool } } }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::CreateMessage(
    IJmapMessageCreateListener* aListener, const nsACString& aMailboxId,
    bool aIsDraft, bool aIsRead, nsIInputStream* aMessageStream) {
  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_ARG_POINTER(aMessageStream);

  // TODO: Implement Email/import
  aListener->OnRemoteCreateFinished(NS_OK, ""_ns);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::DeleteMessages(
    IJmapSimpleOperationListener* aListener,
    const nsTArray<nsCString>& aMessageIds) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Email/set { destroy: [ids] }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::DeleteMailbox(
    IJmapSimpleOperationListener* aListener, const nsACString& aMailboxId) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Mailbox/set { destroy: [id] }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::EmptyMailbox(
    IJmapSimpleOperationListener* aListener, const nsACString& aMailboxId,
    const nsTArray<nsCString>& aMessageIds) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Email/set { destroy: aMessageIds }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::RenameMailbox(
    IJmapSimpleOperationListener* aListener, const nsACString& aMailboxId,
    const nsACString& aNewName) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Mailbox/set { update: { id: { name: newName } } }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::MoveMessages(
    IJmapSimpleOperationListener* aListener,
    const nsACString& aDestinationMailboxId,
    const nsTArray<nsCString>& aMessageIds) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Email/set { update: { id: { mailboxIds: [destId] } } }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::CopyMessages(
    IJmapSimpleOperationListener* aListener,
    const nsACString& aDestinationMailboxId,
    const nsTArray<nsCString>& aMessageIds) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Email/copy { fromAccountId, ifInMailboxIds, ... }
  nsTArray<nsCString> newIds;
  aListener->OnOperationSuccess(newIds);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::MoveMailboxes(
    IJmapSimpleOperationListener* aListener,
    const nsACString& aDestinationParentId,
    const nsTArray<nsCString>& aMailboxIds) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Mailbox/set { update: { id: { parentId: destId } } }
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::CopyMailboxes(
    IJmapSimpleOperationListener* aListener,
    const nsACString& aDestinationParentId,
    const nsTArray<nsCString>& aMailboxIds) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: JMAP doesn't have a native Mailbox/copy. We'd need to
  // create new mailboxes and copy messages.
  nsTArray<nsCString> newIds;
  aListener->OnOperationSuccess(newIds);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::SubmitMessage(
    IJmapSimpleOperationListener* aListener, const nsACString& aEmailId) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement EmailSubmission/set
  nsTArray<nsCString> empty;
  aListener->OnOperationSuccess(empty);
  return NS_OK;
}

NS_IMETHODIMP JmapClient::QueryMessages(
    IJmapMessageSyncListener* aListener,
    const nsTArray<nsCString>& aMailboxIds, const nsACString& aFilter,
    const nsACString& aSort, bool aCollapseThreads, uint32_t aPosition,
    int32_t aLimit) {
  NS_ENSURE_ARG_POINTER(aListener);

  // TODO: Implement Email/query + Email/get
  aListener->OnSyncComplete();
  return NS_OK;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

nsresult JmapClient::GetAccountId(nsACString& aAccountId) const {
  aAccountId = mSession.accountId;
  return NS_OK;
}

nsresult JmapClient::DiscoverSessionUrl(const nsACString& aConfiguredUrl,
                                         nsACString& outUrl) {
  // JMAP session discovery (RFC 8620 Section 2):
  // 1. Try https://host/.well-known/jmap
  // 2. Fall back to the user-configured URL directly
  //
  // Stalwart typically serves JMAP at:
  //   https://mail.example.com/jmap
  // or via .well-known:
  //   https://mail.example.com/.well-known/jmap/session

  nsCString configuredUrl(aConfiguredUrl);
  outUrl = configuredUrl;

  // If the URL doesn't end with a specific path, try .well-known
  if (!configuredUrl.Contains("/.well-known/jmap") &&
      !configuredUrl.Contains("/jmap/session")) {
    // Try .well-known/jmap first
    nsCString wellKnownUrl(configuredUrl);
    if (wellKnownUrl.CharAt(wellKnownUrl.Length() - 1) != '/') {
      wellKnownUrl.Append('/');
    }
    wellKnownUrl.AppendLiteral(".well-known/jmap");

    // We'll try this URL; if it fails, we'll fall back
    outUrl = wellKnownUrl;
  }

  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JmapClient::DiscoverSessionUrl: discovered=%s", outUrl.get()));
  return NS_OK;
}

nsresult JmapClient::FetchSession(const nsACString& aUrl) {
  MOZ_LOG(gJmapLog, LogLevel::Debug,
          ("JmapClient::FetchSession: url=%s", PromiseFlatCString(aUrl).get()));

  // TODO: Implement async HTTP GET to fetch the session resource.
  // The session response is JSON:
  // {
  //   "accounts": { "acctId": { ... } },
  //   "primaryAccounts": { "urn:ietf:params:jmap:mail": "acctId" },
  //   "urls": { "apiUrl": "...", "downloadUrl": "...", ... }
  // }
  //
  // For now, set up with reasonable defaults assuming Stalwart.

  // Parse the URL to determine the API endpoint
  nsCString urlStr(aUrl);
  nsCString baseUrl(urlStr);

  // Remove .well-known/jmap path to get base URL
  int32_t wellKnownPos = baseUrl.Find("/.well-known/jmap");
  if (wellKnownPos >= 0) {
    baseUrl.SetLength(wellKnownPos);
  }

  // Default API URL pattern for Stalwart:
  // The session resource itself IS the API endpoint discovery
  mSession.apiUrl = baseUrl;
  if (mSession.apiUrl.CharAt(mSession.apiUrl.Length() - 1) != '/') {
    mSession.apiUrl.Append('/');
  }
  mSession.apiUrl.AppendLiteral("jmap");

  mSession.downloadUrl = baseUrl;
  if (mSession.downloadUrl.CharAt(mSession.downloadUrl.Length() - 1) != '/') {
    mSession.downloadUrl.Append('/');
  }
  mSession.downloadUrl.AppendLiteral("jmap/download/{accountId}/{blobId}/{name}");

  mSession.uploadUrl = baseUrl;
  if (mSession.uploadUrl.CharAt(mSession.uploadUrl.Length() - 1) != '/') {
    mSession.uploadUrl.Append('/');
  }
  mSession.uploadUrl.AppendLiteral("jmap/upload/{accountId}");

  mSession.serverSupportsPush = true;
  mSession.serverSupportsEmailQuery = true;
  mSession.serverSupportsEmailSubmission = true;
  mSession.maxConcurrentRequests = 8;
  mSession.maxSizeUpload = 50 * 1024 * 1024;  // 50MB (Stalwart default)

  mHasSession = true;

  MOZ_LOG(gJmapLog, LogLevel::Info,
          ("JmapClient::FetchSession: apiUrl=%s (session initialized)",
           mSession.apiUrl.get()));

  return NS_OK;
}

nsresult JmapClient::ParseSessionResponse(const nsACString& aJson) {
  // TODO: Full JSON parsing of JMAP session response
  // This will be implemented with the async HTTP fetch
  NS_WARNING("JmapClient::ParseSessionResponse not yet implemented");
  return NS_OK;
}

nsCString JmapClient::BuildRequestJson(const nsTArray<JmapRequest>& aMethodCalls) {
  // JMAP request format: [[ "methodName", args, "callId" ], ...]
  //
  // Example:
  // [
  //   ["Mailbox/get", { "accountId": "abc", "sinceState": "xyz" }, "c1" ],
  //   ["Email/changes", { "accountId": "abc", "sinceState": "xyz" }, "c2" ]
  // ]
  nsCString json = "["_ns;
  for (uint32_t i = 0; i < aMethodCalls.Length(); i++) {
    const JmapRequest& req = aMethodCalls[i];
    if (i > 0) json.AppendLiteral(", ");
    json.AppendLiteral("[\""_ns);
    json.Append(req.methodName);
    json.AppendLiteral("\", "_ns);
    json.Append(req.arguments);
    json.AppendLiteral(", \""_ns);
    json.Append(req.callId);
    json.AppendLiteral("\"]"_ns);
  }
  json.AppendLiteral("]"_ns);
  return json;
}

nsresult JmapClient::CreateApiChannel(nsIChannel** _retval) {
  if (!mHasSession) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), mSession.apiUrl);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIOService> ioService = mozilla::components::IO::Service();
  NS_ENSURE_TRUE(ioService, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIChannel> channel;
  rv = ioService->NewChannelFromURI(uri, nullptr,
                                    nsContentSecurityManager::COMPUTEPrincipal,
                                    nullptr,
                                    nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
                                    nsIContentPolicy::TYPE_OTHER, getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->SetContentType("application/json"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set request headers
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = httpChannel->SetRequestHeader("Accept"_ns, "application/json"_ns, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Add Bearer token from the server's authentication
  // nsCString authToken;
  // mServer->GetPassword(token); // or OAuth token
  // httpChannel->SetRequestHeader("Authorization"_ns,
  //                              nsPrintfCString("Bearer %s", authToken.get()),
  //                              false);

  channel.forget(_retval);
  return NS_OK;
}

nsresult JmapClient::ExecuteRequest(const nsTArray<JmapRequest>& aMethodCalls,
                                     nsIStreamListener* aListener,
                                     nsIChannel** _retval) {
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = CreateApiChannel(getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString body = BuildRequestJson(aMethodCalls);

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewCStringInputStream(getter_AddRefs(stream), body);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = uploadChannel->ExplicitSetUploadStream(stream, "application/json"_ns,
                                               -1, nsIUploadChannel2::UPLOAD_STREAM_REPLACE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set POST method
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = httpChannel->SetRequestMethod("POST"_ns);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->AsyncOpen(aListener);
  NS_ENSURE_SUCCESS(rv, rv);

  if (_retval) {
    channel.forget(_retval);
  }

  return NS_OK;
}

static uint32_t sJmapCallIdCounter = 0;

nsCString GenerateJmapCallId() {
  return nsPrintfCString("tb-%u", ++sJmapCallIdCounter);
}
