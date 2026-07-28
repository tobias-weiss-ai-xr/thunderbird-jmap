/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! JMAP XPCOM library.
//!
//! This crate provides the JMAP protocol client as an XPCOM component,
//! following the same pattern as ews_xpcom.

pub mod bridge;
pub mod client;
pub mod error;
pub mod types;
