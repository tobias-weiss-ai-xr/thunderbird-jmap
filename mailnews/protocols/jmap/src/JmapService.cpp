/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapService.h"

#include "nsIChannel.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgFolder.h"
#include "nsIMsgHdr.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMsgPluggableStore.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamListener.h"
#include "nsIURIMutator.h"
#include "nsIWebNavigation.h"
#include "nsContentUtils.h"
#include "nsDocShellLoadState.h"
#include "nsMsgUtils.h"
#include "nsNetUtil.h"
#include "SaveAsListener.h"
#include "mozilla/Components.h"

extern mozilla::LazyLogModule gJmapLog;

NS_IMPL_ISUPPORTS(JmapService, nsIMsgMessageService,
                  nsIMsgMessageFetchPartService)

JmapService::JmapService() = default;

JmapService::~JmapService() = default;

// ---------------------------------------------------------------------------
// nsIMsgMessageService
// ---------------------------------------------------------------------------

NS_IMETHODIMP
JmapService::CopyMessage(const nsACString& aSrcURI,
                         nsIStreamListener* aCopyListener, bool aMoveMessage,
                         nsIUrlListener* aUrlListener,
                         nsIMsgWindow* aMsgWindow) {
  NS_ENSURE_ARG_POINTER(aCopyListener);

  nsCOMPtr<nsIURI> channelURI;
  MOZ_TRY(GetUrlForUri(aSrcURI, aMsgWindow, getter_AddRefs(channelURI)));

  return FetchMessage(channelURI, aCopyListener);
}

NS_IMETHODIMP
JmapService::CopyMessages(const nsTArray<nsMsgKey>& aKeys,
                          nsIMsgFolder* srcFolder,
                          nsIStreamListener* aCopyListener, bool aMoveMessage,
                          nsIUrlListener* aUrlListener,
                          nsIMsgWindow* aMsgWindow, nsIURI** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
JmapService::LoadMessage(const nsACString& aMessageURI,
                         nsIDocShell* aDisplayConsumer,
                         nsIMsgWindow* aMsgWindow, nsIUrlListener* aUrlListener,
                         bool aAutodetectCharset) {
  NS_ENSURE_ARG_POINTER(aDisplayConsumer);

  nsCOMPtr<nsIURI> channelURI;
  MOZ_TRY(GetUrlForUri(aMessageURI, aMsgWindow, getter_AddRefs(channelURI)));

  // Load the message through the provided docshell.
  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(channelURI);
  loadState->SetLoadFlags(nsIWebNavigation::LOAD_FLAGS_NONE);
  loadState->SetFirstParty(false);
  loadState->SetTriggeringPrincipal(nsContentUtils::GetSystemPrincipal());
  return aDisplayConsumer->LoadURI(loadState, false);
}

NS_IMETHODIMP
JmapService::SaveMessageToDisk(const nsACString& aMessageURI, nsIFile* aFile,
                               bool aGenerateDummyEnvelope,
                               nsIUrlListener* aUrlListener,
                               bool canonicalLineEnding,
                               nsIMsgWindow* aMsgWindow) {
  nsCOMPtr<nsIURI> channelURI;
  MOZ_TRY(GetUrlForUri(aMessageURI, aMsgWindow, getter_AddRefs(channelURI)));

  RefPtr<SaveAsListener> listener = new SaveAsListener(
      aFile, aGenerateDummyEnvelope, canonicalLineEnding, aUrlListener,
      channelURI);

  return FetchMessage(channelURI, listener);
}

NS_IMETHODIMP
JmapService::GetUrlForUri(const nsACString& aMessageURI,
                          nsIMsgWindow* aMsgWindow, nsIURI** _retval) {
  nsCOMPtr<nsIURI> messageURI;
  MOZ_TRY(NS_NewURI(getter_AddRefs(messageURI), aMessageURI));

  nsAutoCString scheme;
  MOZ_TRY(messageURI->GetScheme(scheme));

  // If the URI is already an x-moz-jmap URI, forward it as-is.
  if (scheme.Equals("x-moz-jmap")) {
    messageURI.forget(_retval);
    return NS_OK;
  }

  // JMAP message URI format: jmap-message://user@server/Path#MessageKey
  // Convert to: x-moz-jmap://user@server/Path/MessageKey
  //
  // The fragment ref is moved into the path to prevent docshell from skipping
  // channel creation when the user switches between messages in the same folder.

  nsAutoCString ref;
  nsresult rv = messageURI->GetRef(ref);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString path;
  rv = messageURI->GetFilePath(path);
  NS_ENSURE_SUCCESS(rv, rv);
  path.Append("/");
  path.Append(ref);

  nsCString query;
  rv = messageURI->GetQuery(query);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!scheme.EqualsLiteral("jmap-message")) {
    MOZ_LOG(gJmapLog, mozilla::LogLevel::Error,
            ("Unknown JMAP message URI scheme: %s", scheme.get()));
    return NS_ERROR_UNEXPECTED;
  }

  return NS_MutateURI(messageURI)
      .SetScheme("x-moz-jmap"_ns)
      .SetPathQueryRef(path)
      .SetQuery(query)
      .Finalize(_retval);
}

NS_IMETHODIMP
JmapService::Search(nsIMsgSearchSession* aSearchSession,
                    nsIMsgWindow* aMsgWindow, nsIMsgFolder* aMsgFolder,
                    const nsACString& aSearchUri) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
JmapService::FetchMimePart(nsIURI* aURI, const nsACString& aMessageURI,
                           nsIStreamListener* aStreamListener,
                           nsIMsgWindow* aMsgWindow,
                           nsIUrlListener* aUrlListener, nsIURI** aURL) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aStreamListener);

  NS_IF_ADDREF(*aURL = aURI);
  return FetchMessage(aURI, aStreamListener);
}

NS_IMETHODIMP
JmapService::StreamMessage(const nsACString& aMessageURI,
                           nsIStreamListener* aStreamListener,
                           nsIMsgWindow* aMsgWindow,
                           nsIUrlListener* aUrlListener, bool aConvertData,
                           const nsACString& aAdditionalHeader, bool aLocalOnly,
                           nsIURI** _retval) {
  NS_ENSURE_ARG_POINTER(aStreamListener);

  nsCOMPtr<nsIURI> channelURI;
  MOZ_TRY(GetUrlForUri(aMessageURI, aMsgWindow, getter_AddRefs(channelURI)));

  // Apply any URI query modifications.
  if (aConvertData || !aAdditionalHeader.IsEmpty()) {
    nsCString query;
    MOZ_TRY(channelURI->GetQuery(query));

    if (!aAdditionalHeader.IsEmpty()) {
      if (!query.IsEmpty()) query.AppendLiteral("&");
      query.AppendLiteral("header=");
      query.Append(aAdditionalHeader);
    }

    if (aConvertData) {
      if (!query.IsEmpty()) query.AppendLiteral("&");
      query.AppendLiteral("convert=true");
    }

    MOZ_TRY(NS_MutateURI(channelURI)
                .SetQuery(query)
                .Finalize(getter_AddRefs(channelURI)));
  }

  NS_IF_ADDREF(*_retval = channelURI);
  return FetchMessage(channelURI, aStreamListener);
}

NS_IMETHODIMP
JmapService::StreamHeaders(const nsACString& aMessageURI,
                           nsIStreamListener* aConsumer,
                           nsIUrlListener* aUrlListener, bool aLocalOnly,
                           nsIURI** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
JmapService::IsMsgInMemCache(nsIURI* aUrl, nsIMsgFolder* aFolder,
                             bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
JmapService::MessageURIToMsgHdr(const nsACString& uri,
                                nsIMsgDBHdr** _retval) {
  RefPtr<nsIURI> uriObj;
  nsresult rv = NS_NewURI(getter_AddRefs(uriObj), uri);
  NS_ENSURE_SUCCESS(rv, rv);

  return MsgHdrFromUri(uriObj, _retval);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

nsresult
JmapService::MsgKeyStringFromMessageURI(nsIURI* uri, nsACString& msgKey) {
  // Expected form: jmap-message://user@server/Path/To/Folder#MessageKey
  nsresult rv = uri->GetRef(msgKey);
  NS_ENSURE_SUCCESS(rv, rv);

  if (msgKey.IsEmpty()) {
    NS_ERROR("JMAP message URI has no message key ref");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult
JmapService::MsgKeyStringFromChannelURI(nsIURI* uri, nsACString& msgKey,
                                        nsACString& folderURIPath) {
  nsresult rv = uri->GetFilePath(folderURIPath);
  NS_ENSURE_SUCCESS(rv, rv);

  folderURIPath.Trim("/", false, true);

  // The last slash-separated word is our message key.
  for (const auto& word : folderURIPath.Split('/')) {
    msgKey.Assign(word);
  }

  // Remove the message key from the path.
  auto keyStartIndex = folderURIPath.Length() - msgKey.Length() - 1;
  folderURIPath.Cut(keyStartIndex, msgKey.Length() + 1);

  return NS_OK;
}

nsresult
JmapService::MsgHdrFromUri(nsIURI* uri, nsIMsgDBHdr** _retval) {
  nsCString keyStr;
  nsCString folderURIPath;

  nsCString scheme;
  nsresult rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (scheme.EqualsLiteral("jmap-message")) {
    rv = MsgKeyStringFromMessageURI(uri, keyStr);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = uri->GetFilePath(folderURIPath);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (scheme.EqualsLiteral("x-moz-jmap")) {
    rv = MsgKeyStringFromChannelURI(uri, keyStr, folderURIPath);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    MOZ_LOG(gJmapLog, mozilla::LogLevel::Error,
            ("Unrecognized JMAP URI scheme: %s", scheme.get()));
    return NS_ERROR_UNEXPECTED;
  }

  nsMsgKey key = msgKeyFromInt(ParseUint64Str(PromiseFlatCString(keyStr).get()));

  // Build a folder URI with the jmap scheme.
  RefPtr<nsIURI> folderUri;
  rv = NS_MutateURI(uri)
           .SetScheme("jmap"_ns)
           .SetFilePath(folderURIPath)
           .SetQuery(""_ns)
           .SetRef(""_ns)
           .Finalize(getter_AddRefs(folderUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString folderSpec;
  rv = folderUri->GetSpec(folderSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsIMsgFolder> folder;
  rv = GetExistingFolder(folderSpec, getter_AddRefs(folder));
  if (NS_FAILED(rv) || !folder) {
    MOZ_LOG(gJmapLog, mozilla::LogLevel::Error,
            ("JMAP: folder not found: %s", folderSpec.get()));
    return NS_ERROR_NOT_AVAILABLE;
  }

  return folder->GetMessageHeader(key, _retval);
}

nsresult
JmapService::FetchMessage(nsIURI* aURI, nsIStreamListener* aStreamListener) {
  nsCOMPtr<nsIIOService> netService = mozilla::components::IO::Service();
  NS_ENSURE_TRUE(netService, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIChannel> messageChannel;
  MOZ_TRY(netService->NewChannelFromURI(
      aURI, nullptr, nsContentUtils::GetSystemPrincipal(), nullptr,
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_OTHER, getter_AddRefs(messageChannel)));

  return messageChannel->AsyncOpen(aStreamListener);
}
