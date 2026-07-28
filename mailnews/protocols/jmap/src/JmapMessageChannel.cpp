/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapMessageChannel.h"

#include "nsIMsgFolder.h"
#include "nsIMsgHdr.h"
#include "nsIMsgMessageService.h"
#include "nsIInputStreamPump.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamListener.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "mozilla/dom/ParentProcessChannelHandle.h"
#include "mozilla/Components.h"

extern mozilla::LazyLogModule gJmapLog;

NS_IMPL_ISUPPORTS_INHERITED(JmapMessageChannel, nsHashPropertyBag,
                            nsIChannel, nsIRequest)

JmapMessageChannel::JmapMessageChannel(nsIURI* uri, bool convert)
    : mConvert(convert),
      mURI(uri),
      mContentDisposition(nsIChannel::DISPOSITION_INLINE),
      mContentLength(-1),
      mLoadFlags(nsIRequest::LOAD_NORMAL),
      mPending(true),
      mStatus(NS_OK) {
  mContentType.AssignLiteral(MESSAGE_RFC822);
}

JmapMessageChannel::~JmapMessageChannel() = default;

NS_IMETHODIMP JmapMessageChannel::GetName(nsACString& aName) {
  if (mURI) {
    return mURI->GetSpec(aName);
  }
  aName.Truncate();
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::IsPending(bool* aPending) {
  if (mReadRequest) {
    return mReadRequest->IsPending(aPending);
  }
  *aPending = mPending;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetStatus(nsresult* aStatus) {
  if (mReadRequest && NS_SUCCEEDED(mStatus)) {
    return mReadRequest->GetStatus(aStatus);
  }
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::Cancel(nsresult aStatus) {
  if (mReadRequest) {
    return mReadRequest->Cancel(aStatus);
  }
  mStatus = aStatus;
  mPending = false;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::Suspend() {
  if (mReadRequest) return mReadRequest->Suspend();
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP JmapMessageChannel::Resume() {
  if (mReadRequest) return mReadRequest->Resume();
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP JmapMessageChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetTRRMode(nsIRequest::TRRMode* mode) {
  return GetTRRModeImpl(mode);
}

NS_IMETHODIMP JmapMessageChannel::SetTRRMode(nsIRequest::TRRMode mode) {
  return SetTRRModeImpl(mode);
}

NS_IMETHODIMP JmapMessageChannel::CancelWithReason(
    nsresult aStatus, const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

NS_IMETHODIMP JmapMessageChannel::GetCanceledReason(
    nsACString& aCanceledReason) {
  return GetCanceledReasonImpl(aCanceledReason);
}

NS_IMETHODIMP JmapMessageChannel::SetCanceledReason(
    const nsACString& aCanceledReason) {
  return SetCanceledReasonImpl(aCanceledReason);
}

NS_IMETHODIMP JmapMessageChannel::GetOriginalURI(nsIURI** aOriginalURI) {
  NS_IF_ADDREF(*aOriginalURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetOriginalURI(nsIURI* aOriginalURI) {
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetURI(nsIURI** aURI) {
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetOwner(nsISupports** aOwner) {
  NS_IF_ADDREF(*aOwner = mOwner);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetOwner(nsISupports* aOwner) {
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aNotificationCallbacks) {
  NS_IF_ADDREF(*aNotificationCallbacks = mNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  mNotificationCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP JmapMessageChannel::GetContentType(nsACString& aContentType) {
  aContentType.Assign(mContentType);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetContentType(
    const nsACString& aContentType) {
  nsresult rv =
      NS_ParseResponseContentType(aContentType, mContentType, mCharset);
  if (NS_FAILED(rv) || mContentType.IsEmpty())
    mContentType.AssignLiteral(MESSAGE_RFC822);
  if (NS_FAILED(rv) || mCharset.IsEmpty()) mCharset.AssignLiteral("UTF-8");
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetContentCharset(
    nsACString& aContentCharset) {
  aContentCharset.Assign(mCharset);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetContentCharset(
    const nsACString& aContentCharset) {
  mCharset.Assign(aContentCharset);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetContentLength(int64_t* aContentLength) {
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetContentLength(int64_t aContentLength) {
  mContentLength = aContentLength;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::Open(nsIInputStream** _retval) {
  return NS_ImplementChannelOpen(this, _retval);
}

NS_IMETHODIMP JmapMessageChannel::AsyncOpen(nsIStreamListener* aListener) {
  mPending = false;

  // Get the message header + folder from the URI.
  nsCString spec;
  MOZ_TRY(mURI->GetSpec(spec));

  nsCOMPtr<nsIMsgMessageService> msgService =
      do_GetService("@mozilla.org/messenger/messageservice;1?type=jmap");
  NS_ENSURE_TRUE(msgService, NS_ERROR_UNEXPECTED);

  MOZ_TRY(msgService->MessageURIToMsgHdr(spec, getter_AddRefs(mHdr)));
  NS_ENSURE_TRUE(mHdr, NS_ERROR_FAILURE);

  return StartMessageReadFromStore(aListener);
}

NS_IMETHODIMP JmapMessageChannel::GetCanceled(bool* aCanceled) {
  nsCString canceledReason;
  nsresult rv = mReadRequest->GetCanceledReason(canceledReason);
  NS_ENSURE_SUCCESS(rv, rv);
  *aCanceled = canceledReason.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetContentDisposition(
    uint32_t* aContentDisposition) {
  *aContentDisposition = mContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetContentDisposition(
    uint32_t aContentDisposition) {
  mContentDisposition = aContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP JmapMessageChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP JmapMessageChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP JmapMessageChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP JmapMessageChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

// ParentProcessChannelHandle methods removed for simplicity.
// These are only needed for Fission (multi-process) support.
NS_IMETHODIMP
JmapMessageChannel::GetParentProcessChannelHandle(
    mozilla::dom::ParentProcessChannelHandle** aValue) {
  *aValue = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
JmapMessageChannel::SetParentProcessChannelHandle(
    mozilla::dom::ParentProcessChannelHandle* aValue) {
  return NS_OK;
}

nsresult JmapMessageChannel::StartMessageReadFromStore(
    nsIStreamListener* streamListener) {
  // Get the folder for this message.
  nsCOMPtr<nsIMsgFolder> folder;
  MOZ_TRY(mHdr->GetFolder(getter_AddRefs(folder)));

  // Get an input stream from the offline store.
  nsCOMPtr<nsIInputStream> inputStream;
  MOZ_TRY(folder->GetMsgInputStream(mHdr, getter_AddRefs(inputStream)));

  // Create an input stream pump.
  nsCOMPtr<nsIInputStreamPump> pump =
      do_CreateInstance("@mozilla.org/network/input-stream-pump;1");
  NS_ENSURE_TRUE(pump, NS_ERROR_OUT_OF_MEMORY);

  MOZ_TRY(pump->Init(inputStream, 0, 0, false, nullptr));
  mReadRequest = pump;

  MOZ_TRY(pump->SetLoadFlags(mLoadFlags));
  MOZ_TRY(pump->SetLoadGroup(mLoadGroup));

  // Set up a stream converter if needed (e.g., for message display).
  nsCOMPtr<nsIStreamListener> listener = streamListener;
  if (mConvert) {
    nsCOMPtr<nsIStreamConverterService> converterService =
        mozilla::components::StreamConverter::Service();
    if (converterService) {
      nsCOMPtr<nsIStreamListener> converter;
      if (NS_SUCCEEDED(converterService->AsyncConvertData(
              MESSAGE_RFC822, "*/*", streamListener, mHdr,
              getter_AddRefs(converter)))) {
        listener = converter;
      }
    }
  }

  return pump->AsyncRead(listener);
}
