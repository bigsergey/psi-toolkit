#ifndef PSI_ASPELL_HDR
#define PSI_ASPELL_HDR

#include "annotator.hpp"
#include "language_dependent_annotator_factory.hpp"

class PSIAspell : public Annotator {

public:
    class Factory : public LanguageDependentAnnotatorFactory {
    private:
        virtual Annotator* doCreateAnnotator(
            const boost::program_options::variables_map& options);

        virtual void doAddLanguageIndependentOptionsHandled(
            boost::program_options::options_description& optionsDescription);

        virtual std::string doGetName();
        virtual boost::filesystem::path doGetFile();

        virtual std::list<std::list<std::string> > doRequiredLayerTags();

        virtual std::list<std::list<std::string> > doOptionalLayerTags();

        virtual std::list<std::string> doProvidedLayerTags();

        virtual std::string doGetContinuation(
            const boost::program_options::variables_map& options) const;
    };

    PSIAspell();

private:

    class Worker : public LatticeWorker {
    public:
        Worker(Processor& processor, Lattice& lattice);
    private:
        virtual void doRun();
        Processor& processor_;
    };

    virtual LatticeWorker* doCreateLatticeWorker(Lattice& lattice);

    virtual std::string doInfo();
};

#endif

