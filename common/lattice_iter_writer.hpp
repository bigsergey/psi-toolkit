#ifndef LATTICE_ITER_WRITER_HDR
#define LATTICE_ITER_WRITER_HDR

#include <queue>
#include <string>

#include <boost/algorithm/string/trim.hpp>
#include <boost/foreach.hpp>

#include "lattice.hpp"
#include "lattice_writer_output_iterator.hpp"
#include "processor.hpp"

/*!
  Lattice iter writer takes a lattice and writes its content to the lattice output iterator.
*/
class LatticeIterWriter {

public:

    LatticeIterWriter(
        Lattice& lattice,
        LatticeWriterOutputIterator& outputIterator,
        bool linear,
        bool noAlts,
        bool withBlank,
        std::string basicTag,
        std::vector<std::string> handledTags,
        std::vector<std::string> fallbackTags
    ) :
        lattice_(lattice),
        outputIterator_(outputIterator),
        linear_(linear),
        noAlts_(noAlts),
        withBlank_(withBlank),
        basicTagMask_(lattice.getLayerTagManager().getMask(basicTag)),
        handledTags_(handledTags),
        fallbackTags_(fallbackTags)
    {
        outputIterator.setLatticeAnnotationItemManagerPointer(
                      &lattice_.getAnnotationItemManager());
    }

    void run();

private:

    Lattice & lattice_;
    LatticeWriterOutputIterator & outputIterator_;
    bool linear_;
    bool noAlts_;
    bool withBlank_;
    LayerTagMask basicTagMask_;
    std::vector<std::string> handledTags_;
    std::vector<std::string> fallbackTags_;

    bool isHandledTag_(std::string tagName);

};

#endif
