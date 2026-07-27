/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JmapUtils.h"
#include "nsPrintfCString.h"

nsCString JmapGenerateCallId() {
  static uint32_t counter = 0;
  return nsPrintfCString("tbj%u", ++counter);
}

nsresult JmapParseStateToken(const nsACString& aJson,
                              const nsACString& aField, nsACString& _retval) {
  // TODO: Full JSON parsing of JMAP response to extract state tokens
  // Will use JSON parser from toolkit/components/json/nsJSON.h
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsCString JmapBuildMailboxFilter(const nsACString& aMailboxId,
                                  const nsACString& aAdditionalFilter) {
  // JMAP Email filter: { "inMailbox": "mailboxId", ... }
  nsCString filter = "{\"inMailbox\":\""_ns;
  filter.Append(aMailboxId);
  filter.AppendLiteral("\"");

  if (!aAdditionalFilter.IsEmpty()) {
    filter.AppendLiteral(",");
    filter.Append(aAdditionalFilter);
  }

  filter.AppendLiteral("}");
  return filter;
}
