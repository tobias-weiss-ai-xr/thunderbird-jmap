/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! JMAP XPCOM library.
//!
//! This crate provides the core JMAP client implementation used by the
//! C++ XPCOM bridge in mailnews/protocols/jmap/src/JmapClient.cpp.

pub mod client;
pub mod error;
pub mod types;
