#ifndef PSI_LATTICE_READER_HDR
#define PSI_LATTICE_READER_HDR

// for unknown reasons property_tree has to be included
// before Spirit/Lambda (at least for GCC 4.6.1 &
// Boost 1.47.0 at Arch Linux)
#include <boost/property_tree/ptree.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/qi.hpp>

#include "lattice_reader.hpp"
#include "quoter.hpp"
#include "psi_quoter.hpp"


namespace qi = boost::spirit::qi;


struct PsiLRItem {
    int ordinal;
    int beginning;
    int length;
    std::string text;
    std::string tags;
    std::string annotationItem;

    void unescape(Quoter & quoter) {
        text = quoter.unescape(text);
        tags = quoter.unescape(tags);
        annotationItem = quoter.unescape(annotationItem);
    }
};


BOOST_FUSION_ADAPT_STRUCT(
    PsiLRItem,
    (int, ordinal)
    (int, beginning)
    (int, length)
    (std::string, text)
    (std::string, tags)
    (std::string, annotationItem)
)


struct PsiLRGrammar : public qi::grammar<std::string::const_iterator, PsiLRItem()> {

    PsiLRGrammar() : PsiLRGrammar::base_type(start) {

        start
            %= qi::int_
            >> whitespaces
            >> qi::int_
            >> whitespaces
            >> qi::int_
            >> whitespaces
            >> +(qi::char_ - ' ')
            >> whitespaces
            >> +(qi::char_ - ' ')
            >> whitespaces
            >> +(qi::char_ - ' ')
            ;

        whitespaces
            %= +(qi::lit(' ') | qi::lit('\t') | qi::lit('\v') | qi::lit('\f') | qi::lit('\r'))
            ;

    }

    qi::rule<std::string::const_iterator, PsiLRItem()> start;
    qi::rule<std::string::const_iterator, qi::unused_type()> whitespaces;

};


class PsiLatticeReader : public LatticeReader {

public:

    /**
     * Gets format name (here: "Psi").
     */
    std::string getFormatName();

private:
    virtual std::string doInfo();

    class Worker : public ReaderWorker {
    public:
        Worker(PsiLatticeReader& processor,
               std::istream& inputStream,
               Lattice& lattice);

        virtual void doRun();

    private:
        PsiLatticeReader& processor_;
    };

    virtual ReaderWorker* doCreateReaderWorker(std::istream& inputStream, Lattice& lattice) {
        return new Worker(*this, inputStream, lattice);
    }

};


#endif
