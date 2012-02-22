#ifndef ME_TAGGER_HDR
#define ME_TAGGER_HDR

#include "config.hpp"
#include "annotator.hpp"
#include "language_dependent_annotator_factory.hpp"
#include "lang_specific_processor_file_fetcher.hpp"
#include <boost/program_options.hpp>
#include "regexp.hpp"

#include <string>
#include <vector>
#include <map>
#include "maxentmodel.hpp"

class MeTagger : public Annotator {
    public:
        class Factory : public LanguageDependentAnnotatorFactory {
            virtual Annotator* doCreateAnnotator(
                    const boost::program_options::variables_map& options);

            virtual void doAddLanguageIndependentOptionsHandled(
                boost::program_options::options_description& optionsDescription);

            virtual std::string doGetName();
            virtual boost::filesystem::path doGetFile();

            virtual std::list<std::list<std::string> > doRequiredLayerTags();
            virtual std::list<std::list<std::string> > doOptionalLayerTags();
            virtual std::list<std::string> doProvidedLayerTags();

            static const std::string DEFAULT_MODEL_FILE;
            static const int DEFAULT_ITERATIONS;
            static const std::string DEFAULT_UNKNOWN_POS_LABEL;
            static const std::string DEFAULT_CARDINAL_NUMBER_POS_LABEL;
            static const std::string DEFAULT_PROPER_NOUN_POS_LABEL;
        };

        MeTagger(bool trainMode_, std::string modelFile_, int iterations_,
                std::string unknownPosLabel_,
                std::string cardinalNumberPosLabel_,
                std::string properNounPosLabel_,
                std::vector<std::string> openClassLabels_);
        void tag(Lattice &lattice);
        void train(Lattice &lattice);
        void saveModel(std::string path);
        void loadModel(std::string path);

        bool training();
        std::string getModelFile();

    private:
        class Worker : public LatticeWorker {
            public:
                Worker(MeTagger& processor, Lattice& lattice);
            private:
                virtual void doRun();
                MeTagger& processor_;
        };
        virtual LatticeWorker* doCreateLatticeWorker(Lattice& lattice);
        virtual std::string doInfo();

        typedef std::string Outcome;
        typedef std::string Feature;
        typedef std::vector<Feature> Context;

        typedef std::map<int, Lattice::EdgeDescriptor> TokenEdgesMap;

        maxent::MaxentModel m;
        bool openForEvents;
        bool posModelLoaded;
        bool trainMode;
        int trainIterations;
        std::string modelFile;
        std::string unknownPosLabel;
        std::string cardinalNumberPosLabel, properNounPosLabel;
        std::vector<std::string> openClassLabels;

        RegExp rxUpperCaseFirst;
        RegExp rxUpperCaseAll;
        RegExp rxContainsNumber;
        RegExp rxIsNumber;
        RegExp rxContainsLetter;
        RegExp rxContainsPunct;

        TokenEdgesMap createTokenEdgesMap(
                Lattice &lattice, Lattice::VertexDescriptor start,
                Lattice::VertexDescriptor end);
        void tagSegment(Lattice &lattice, TokenEdgesMap tokenEdgesMap);
        void addSampleSentences(Lattice &lattice);
        void addSampleSegment(Lattice &lattice, TokenEdgesMap tokenEdgesMap);

        Context createContext(Lattice &lattice,
                TokenEdgesMap tokenEdgesMap,
                int currentIndex, int window);

        std::string getFormLemma(Lattice &lattice,
                Lattice::EdgeDescriptor edge);
        std::string getFormPartOfSpeech(Lattice &lattice,
                Lattice::EdgeDescriptor edge);
        std::string getFormMorphoTag(Lattice &lattice,
                Lattice::EdgeDescriptor edge);
        bool lemmaEdgeExists(Lattice &lattice, Lattice::EdgeDescriptor token,
                std::string lemma);
        bool lexemeEdgeExists(Lattice &lattice, Lattice::EdgeDescriptor token,
                std::string lemma, std::string partOfSpeech);
        void addLemmaEdge(Lattice &lattice, Lattice::EdgeDescriptor token,
                std::string lemma);
        void addLexemeEdge(Lattice &lattice, Lattice::EdgeDescriptor token,
                std::string lemma, std::string partOfSpeech);
        void addFormEdge(Lattice &lattice, Lattice::EdgeDescriptor token,
                std::string lemma, std::string partOfSpeech, std::string tag);
        bool isDiscarded(Lattice &lattice, Lattice::EdgeDescriptor edge);
};

#endif