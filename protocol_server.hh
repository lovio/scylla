/*
 * Copyright (C) 2021-present ScyllaDB
 */

/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "seastarx.hh"
#include <seastar/core/future.hh>
#include <vector>

// Abstraction for a server serving some kind of user-facing protocol.
class protocol_server {
public:
    virtual ~protocol_server() = default;
    /// Name of the server, can be different or the same as than the protocol it serves.
    virtual sstring name() const = 0;
    /// Name of the protocol served.
    virtual sstring protocol() const = 0;
    /// Version of the protocol served (if any -- not all protocols are versioned).
    virtual sstring protocol_version() const = 0;
    /// Addresses the server is listening on, shall be empty when server is not running.
    virtual std::vector<socket_address> listen_addresses() const = 0;
    /// Start the server.
    /// Can be called multiple times, in any state of the server.
    virtual future<> start_server() = 0;
    /// Stop the server.
    /// Can be called multiple times, in any state of the server.
    /// This variant is used on shutdown and therefore it must succeed.
    virtual future<> stop_server() = 0;
    /// Stop the server. Weaker variant of \ref stop_server().
    /// Can be called multiple times, in any state of the server.
    /// This variant is used by the REST API so failure is acceptable.
    virtual future<> request_stop_server() = 0;
};
