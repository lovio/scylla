/*
 */

/*
 * Copyright (C) 2014-present ScyllaDB
 *
 * Modified by ScyllaDB
 */

/*
 * SPDX-License-Identifier: (AGPL-3.0-or-later and Apache-2.0)
 */

#pragma once

#include "transport/messages_fwd.hh"
#include "cql3/cql_statement.hh"

namespace cql3 {

class query_processor;

namespace statements {

class use_statement : public cql_statement_no_metadata {
private:
    const seastar::sstring _keyspace;

public:
    use_statement(seastar::sstring keyspace);

    virtual uint32_t get_bound_terms() const override;

    virtual bool depends_on_keyspace(const seastar::sstring& ks_name) const override;

    virtual bool depends_on_column_family(const seastar::sstring& cf_name) const override;

    virtual seastar::future<> check_access(query_processor& qp, const service::client_state& state) const override;

    virtual void validate(query_processor&, const service::client_state& state) const override;

    virtual seastar::future<seastar::shared_ptr<cql_transport::messages::result_message>>
    execute(query_processor& qp, service::query_state& state, const query_options& options) const override;
};

}

}
