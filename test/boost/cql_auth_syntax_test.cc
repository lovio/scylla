/*
 * Copyright (C) 2018-present ScyllaDB
 */

/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#define BOOST_TEST_MODULE core

#include <string_view>

#include <boost/test/unit_test.hpp>
#include <seastar/core/shared_ptr.hh>

#include "cql3/CqlParser.hpp"
#include "cql3/role_options.hh"
#include "cql3/statements/raw/parsed_statement.hh"
#include "cql3/util.hh"

//
// Test basic CQL string quoting
//
BOOST_AUTO_TEST_CASE(maybe_quote) {
    std::string s(65536, 'x');
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote(s), s);
    s += " " + std::string(65536, 'y');
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote(s), "\"" + s + "\"");

    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("a"), "a");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("z"), "z");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("b0"), "b0");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("y9"), "y9");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("c_d"), "c_d");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("x8_"), "x8_");

    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote(""), "\"\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("0"), "\"0\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("9"), "\"9\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("_"), "\"_\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("A"), "\"A\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("To"), "\"To\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("zeD"), "\"zeD\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello world"), "\"hello world\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello_world01234"), "hello_world01234");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello world01234"), "\"hello world01234\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello world\"1234"), "\"hello world\"\"1234\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello_world01234hello_world01234"), "hello_world01234hello_world01234");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello world01234hello_world01234"), "\"hello world01234hello_world01234\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello world\"1234hello_world\"1234"), "\"hello world\"\"1234hello_world\"\"1234\"");

    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("\""), "\"\"\"\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("[\"]"), "\"[\"\"]\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("\"\""), "\"\"\"\"\"\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("\"hell0\""), "\"\"\"hell0\"\"\"");
    BOOST_REQUIRE_EQUAL(cql3::util::maybe_quote("hello \"my\" world"), "\"hello \"\"my\"\" world\"");
}

//
// These tests verify that all excepted variations of CQL syntax related to access-control ("auth") functionality are
// accepted by the parser. They do not verify that invalid syntax is rejected, nor do they verify the correctness of
// anything other than syntax (valid and invalid options for a particular authenticator, for example).
//

///
/// Grammar rule producing a value of type `T`.
///
template <typename T>
using producer_rule_ptr = T (cql3_parser::CqlParser::*)();

///
/// Grammar rule modifying a value of type `T`.
///
template <typename T>
using modifier_rule_ptr = void (cql3_parser::CqlParser::*)(T&);

///
/// Assert that the CQL fragment is valid syntax (does not throw an exception) and return the parsed value.
///
template <typename T>
static T test_valid(std::string_view cql_fragment, producer_rule_ptr<T> rule) {
    T v;
    BOOST_REQUIRE_NO_THROW(v = cql3::util::do_with_parser(cql_fragment, std::mem_fn(rule)));
    return v;
}

///
/// Assert that the CQL fragment is valid syntax (does not throw an exception) and modify the value according to the
/// rule.
///
template <typename T>
void test_valid(std::string_view cql_fragment, modifier_rule_ptr<T> rule, T& v) {
    BOOST_REQUIRE_NO_THROW(
            cql3::util::do_with_parser(cql_fragment, [rule, &v](cql3_parser::CqlParser& parser) {
                (parser.*rule)(v);
                 // Any non-`void` value will do.
                 return 0;
    }));
}

template <typename T>
static auto make_tester(producer_rule_ptr<T> rule) {
    return [rule](std::string_view cql_fragment) {
        return test_valid(cql_fragment, rule);
    };
}
template <typename T>
static auto make_tester(modifier_rule_ptr<T> rule) {
    return [rule](std::string_view cql_fragment, T& v) {
        test_valid(cql_fragment, rule, v);
    };
}

///
/// Assert that the semicolon-terminated CQL query is valid syntax, and return the parsed statement.
///
static std::unique_ptr<cql3::statements::raw::parsed_statement>
test_valid(std::string_view cql_query) {
    return test_valid(cql_query, &cql3_parser::CqlParser::query);
}

BOOST_AUTO_TEST_CASE(user_name) {
    const auto test = make_tester(&cql3_parser::CqlParser::username);

    test("'sam'");
    test("lord_sauron_42");

    // Not worth generalizing `test_valid`.
    BOOST_REQUIRE_THROW(
            (cql3::util::do_with_parser("\"Ring-bearer\"", std::mem_fn(&cql3_parser::CqlParser::username))),
            exceptions::syntax_exception);
}

BOOST_AUTO_TEST_CASE(create_user) {
    // (a) IF NOT EXISTS
    // (b) PASSWORD
    // (c) SUPERUSER | NOSUPERUSER

    test_valid("CREATE USER sam;");
    test_valid("CREATE USER IF NOT EXISTS sam;");
    test_valid("CREATE USER sam WITH PASSWORD 'shire';");
    test_valid("CREATE USER IF NOT EXISTS sam WITH PASSWORD 'shire';");
    test_valid("CREATE USER sam SUPERUSER;");
    test_valid("CREATE USER IF NOT EXISTS sam NOSUPERUSER");
    test_valid("CREATE USER sam WITH PASSWORD 'shire' SUPERUSER");
    test_valid("CREATE USER IF NOT EXISTS sam WITH PASSWORD 'shire' NOSUPERUSER");
}

BOOST_AUTO_TEST_CASE(alter_user) {
    // (a) PASSWORD
    // (b) SUPERUSER | NOSUPERUSER

    test_valid("ALTER USER sam;");
    test_valid("ALTER USER sam WITH PASSWORD 'shire';");
    test_valid("ALTER USER sam NOSUPERUSER;");
    test_valid("ALTER USER sam WITH PASSWORD 'shire' SUPERUSER;");
}

BOOST_AUTO_TEST_CASE(drop_user) {
    test_valid("DROP USER sam;");
    test_valid("DROP USER IF EXISTS sam;");
}

BOOST_AUTO_TEST_CASE(list_users) {
    test_valid("LIST USERS;");
}

BOOST_AUTO_TEST_CASE(permission_or_all) {
    const auto test = make_tester(&cql3_parser::CqlParser::permissionOrAll);

    test("CREATE");
    test("DESCRIBE");
    test("ALL PERMISSIONS");
}

BOOST_AUTO_TEST_CASE(resource_) {
    const auto test = make_tester(&cql3_parser::CqlParser::resource);

    test("ALL KEYSPACES");
    test("KEYSPACE ks");
    test("ks.cf");
    test("cf");

    test("ALL ROLES");
    test("ROLE lord");
}

BOOST_AUTO_TEST_CASE(grant) {
    test_valid("GRANT MODIFY ON KEYSPACE ks TO sam;");
}

BOOST_AUTO_TEST_CASE(revoke_) {
    test_valid("REVOKE ALL PERMISSIONS ON ROLE lord FROM sam;");
}

BOOST_AUTO_TEST_CASE(list_permissions) {
    // (a) ON <resource>
    // (b) OF <user_name>
    // (c) NORECURSIVE

    test_valid("LIST ALL PERMISSIONS;");
    test_valid("LIST SELECT ON KEYSPACE ks;");
    test_valid("LIST MODIFY OF sam;");
    test_valid("LIST SELECT ON ks.cf OF sam;");
    test_valid("LIST MODIFY NORECURSIVE;");
    test_valid("LIST ALL PERMISSIONS ON ALL ROLES NORECURSIVE;");
    test_valid("LIST DESCRIBE OF sam NORECURSIVE;");
    test_valid("LIST ALL PERMISSIONS ON ALL KEYSPACES OF sam NORECURSIVE;");
}

BOOST_AUTO_TEST_CASE(user_or_role_name) {
    const auto test = make_tester(&cql3_parser::CqlParser::userOrRoleName);

    BOOST_REQUIRE_EQUAL(static_cast<cql3::role_name&&>(test("SaM")).to_string(), "sam");
    BOOST_REQUIRE_EQUAL(static_cast<cql3::role_name&&>(test("'SaM'")).to_string(), "SaM");
    BOOST_REQUIRE_EQUAL(static_cast<cql3::role_name&&>(test("\"SaM\"")).to_string(), "SaM");
    // Unreserved keyword.
    BOOST_REQUIRE_EQUAL(static_cast<cql3::role_name&&>(test("LisT")).to_string(), "list");
}

BOOST_AUTO_TEST_CASE(role_options) {
    const auto test = make_tester(&cql3_parser::CqlParser::roleOptions);
    cql3::role_options v;

    test("PASSWORD = 'shire'", v);
    test("OPTIONS = { 'foo': 10, 'bar': 20 }", v);
    test("LOGIN = false", v);
    test("SUPERUSER = true", v);
    test("PASSWORD = 'shire' AND OPTIONS = { 'foo': 10, 'bar': 20 }", v);
    test("PASSWORD = 'shire' AND LOGIN = true AND SUPERUSER = false", v);
}

BOOST_AUTO_TEST_CASE(create_role) {
    // (a) IF NOT EXISTS
    // (b) <roleOptions>

    test_valid("CREATE ROLE \"Ring-bearer\";");
    test_valid("CREATE ROLE IF NOT EXISTS soldier;");
    test_valid("CREATE ROLE soldier WITH PASSWORD = 'sword';");
    test_valid("CREATE ROLE IF NOT EXISTS soldier WITH PASSWORD = 'sword' AND LOGIN = true;");
}

BOOST_AUTO_TEST_CASE(alter_role) {
    test_valid("ALTER ROLE soldier;");
    test_valid("ALTER ROLE soldier WITH PASSWORD = 'sword';");
    test_valid("ALTER ROLE soldier WITH PASSWORD = 'sword' AND SUPERUSER = false;");
}

BOOST_AUTO_TEST_CASE(drop_role) {
    test_valid("DROP ROLE soldier;");
    test_valid("DROP ROLE IF EXISTS soldier;");
}

BOOST_AUTO_TEST_CASE(list_roles) {
    // (a) OF <role_name>
    // (b) NORECURSIVE

    test_valid("LIST ROLES;");
    test_valid("LIST ROLES OF sam;");
    test_valid("LIST ROLES NORECURSIVE;");
    test_valid("LIST ROLES OF \"Ring-bearer\" NORECURSIVE;");
}

BOOST_AUTO_TEST_CASE(grant_role) {
    test_valid("GRANT \"Ring-bearer\" TO sam;");
}

BOOST_AUTO_TEST_CASE(revoke_role) {
    test_valid("REVOKE soldier FROM boromir;");
}
