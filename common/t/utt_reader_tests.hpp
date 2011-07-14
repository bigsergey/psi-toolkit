#include "utt_reader.hpp"


class UTTReaderTests : public CxxTest::TestSuite {

public:

    void test_simple() {
        
        Lattice lattice("");
        LatticeReader * reader = new UTTReader();
        
        reader->readIntoLattice("../test/files/fr_simple_puddle_input.txt", lattice);
        
        LayerTagCollection tokenTag 
            = lattice.getLayerTagManager().createSingletonTagCollection("token");
        std::list<Lattice::EdgeDescriptor> edges = lattice.edgesSorted(tokenTag);
        std::list<Lattice::EdgeDescriptor>::iterator ei = edges.begin();
        
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), "Amelie");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), " ");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), "a");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), " ");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), "un");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), " ");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), "chat");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), ".");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), "Quelle");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), " ");
        ++ei;
        TS_ASSERT_EQUALS(lattice.getEdgeAnnotationItem(*ei).getCategory(), "âge");
        
        delete reader;
        
    }

};