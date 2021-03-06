#include "tests.hpp"

#include <fstream>
#include <set>
#include <string>

#include <boost/scoped_ptr.hpp>

#include "annotation_item_manager.hpp"
#include "lattice.hpp"
#include "lattice_preparators.hpp"
#include "dot_lattice_writer.hpp"


BOOST_AUTO_TEST_SUITE( dot_lattice_writer )


BOOST_AUTO_TEST_CASE( dot_lattice_writer_simple ) {

    AnnotationItemManager aim;
    Lattice lattice(aim);
    lattice_preparators::prepareSimpleLattice(lattice);

    std::set<std::string> filter;

    boost::scoped_ptr<LatticeWriter<std::ostream> > writer(new DotLatticeWriter(
        true, // show tags
        false, // color
        filter, // filter
        false, // tree
        false, // align
        true // show symbol edges
    ));

    BOOST_CHECK_EQUAL(writer->getFormatName(), "DOT");
    BOOST_CHECK_EQUAL(writer->info(), "DOT writer");

    std::ostringstream osstr;
    writer->writeLattice(lattice, osstr);

    std::string line;
    std::string contents;
    std::ifstream s(ROOT_DIR "formats/dot/t/files/simple.dot");
    while (getline(s, line)) {
        contents += line;
        contents += "\n";
    }

    BOOST_CHECK_EQUAL(osstr.str(), contents);

}


BOOST_AUTO_TEST_CASE( dot_lattice_writer_advanced ) {

    AnnotationItemManager aim;
    Lattice lattice(aim);
    lattice_preparators::prepareAdvancedLattice(lattice);

    std::set<std::string> filter;

    boost::scoped_ptr<LatticeWriter<std::ostream> > writer(new DotLatticeWriter(
        true, // show tags
        false, // color
        filter, // filter
        false, // tree
        false, // align
        true // show symbol edges
    ));

    std::ostringstream osstr;
    writer->writeLattice(lattice, osstr);

    std::string line;
    std::string contents;
    std::ifstream s(ROOT_DIR "formats/dot/t/files/advanced.dot");
    while (getline(s, line)) {
        contents += line;
        contents += "\n";
    }

    BOOST_CHECK_EQUAL(osstr.str(), contents);

}


BOOST_AUTO_TEST_CASE( dot_lattice_writer_tree ) {

    AnnotationItemManager aim;
    Lattice lattice(aim);
    lattice_preparators::prepareRegularLattice(lattice);

    std::set<std::string> filter;

    boost::scoped_ptr<LatticeWriter<std::ostream> > writer(new DotLatticeWriter(
        false, // show tags
        false, // color
        filter, // filter
        true, // tree
        false, // align
        true // show symbol edges
    ));

    std::ostringstream osstr;
    writer->writeLattice(lattice, osstr);

    std::string line;
    std::string contents;
    std::ifstream s(ROOT_DIR "formats/dot/t/files/tree.dot");
    while (getline(s, line)) {
        contents += line;
        contents += "\n";
    }

    BOOST_CHECK_EQUAL(osstr.str(), contents);

}


BOOST_AUTO_TEST_SUITE_END()
