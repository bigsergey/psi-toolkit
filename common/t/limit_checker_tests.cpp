#include "tests.hpp"

#include <boost/scoped_ptr.hpp>

#include "limit_checker.hpp"

BOOST_AUTO_TEST_SUITE( limit_checker )

BOOST_AUTO_TEST_CASE( default_limit_checker ) {
    boost::scoped_ptr<LimitChecker> checker(new LimitChecker(8));
    checker->addUnlimitedSpec(10);
    checker->addLimitSpec(20, 30);
    checker->addLimitSpec(30, 20);
    checker->addLimitSpec(100, 12);
    BOOST_CHECK(checker->isAllowed(0, 0));
    BOOST_CHECK(checker->isAllowed(0, -999));
    BOOST_CHECK(checker->isAllowed(-999, 9999));
    BOOST_CHECK(checker->isAllowed(0, 9999));
    BOOST_CHECK(checker->isAllowed(10, 9999));
    BOOST_CHECK(!checker->isAllowed(11, 9999));
    BOOST_CHECK(!checker->isAllowed(11, 31));
    BOOST_CHECK(checker->isAllowed(11, 30));
    BOOST_CHECK(checker->isAllowed(20, 30));
    BOOST_CHECK(!checker->isAllowed(21, 30));
    BOOST_CHECK(!checker->isAllowed(21, 21));
    BOOST_CHECK(checker->isAllowed(21, 20));
    BOOST_CHECK(checker->isAllowed(30, 20));
    BOOST_CHECK(!checker->isAllowed(31, 20));
    BOOST_CHECK(!checker->isAllowed(31, 13));
    BOOST_CHECK(checker->isAllowed(31, 12));
    BOOST_CHECK(checker->isAllowed(100, 12));
    BOOST_CHECK(!checker->isAllowed(101, 12));
    BOOST_CHECK(!checker->isAllowed(101, 9));
    BOOST_CHECK(checker->isAllowed(101, 8));
    BOOST_CHECK(checker->isAllowed(9999, 8));
    BOOST_CHECK(checker->isAllowed(9999, 0));
    BOOST_CHECK(checker->isAllowed(9999, -999));
    BOOST_CHECK(!checker->isAllowed(9999, 9999));
}

BOOST_AUTO_TEST_SUITE_END()
