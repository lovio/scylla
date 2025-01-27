/*
 * Copyright 2015-present ScyllaDB
 */

/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "frozen_schema.hh"
#include "db/schema_tables.hh"
#include "canonical_mutation.hh"
#include "schema_mutations.hh"
#include "idl/uuid.dist.hh"
#include "idl/keys.dist.hh"
#include "idl/mutation.dist.hh"
#include "idl/frozen_schema.dist.hh"
#include "serializer_impl.hh"
#include "serialization_visitors.hh"
#include "idl/uuid.dist.impl.hh"
#include "idl/keys.dist.impl.hh"
#include "idl/mutation.dist.impl.hh"
#include "idl/frozen_schema.dist.impl.hh"

frozen_schema::frozen_schema(const schema_ptr& s)
    : _data([&s] {
        schema_mutations sm = db::schema_tables::make_schema_mutations(s, api::new_timestamp(), true);
        bytes_ostream out;
        ser::writer_of_schema<bytes_ostream> wr(out);
        std::move(wr).write_version(s->version())
                     .write_mutations(sm)
                     .end_schema();
        return to_bytes(out.linearize());
    }())
{ }

schema_ptr frozen_schema::unfreeze(const db::schema_ctxt& ctxt) const {
    auto in = ser::as_input_stream(_data);
    auto sv = ser::deserialize(in, boost::type<ser::schema_view>());
    return sv.mutations().is_view()
         ? db::schema_tables::create_view_from_mutations(ctxt, sv.mutations(), sv.version())
         : db::schema_tables::create_table_from_mutations(ctxt, sv.mutations(), sv.version());
}

frozen_schema::frozen_schema(bytes b)
    : _data(std::move(b))
{ }

bytes_view frozen_schema::representation() const
{
    return _data;
}
