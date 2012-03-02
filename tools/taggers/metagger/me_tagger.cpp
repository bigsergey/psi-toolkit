#include "me_tagger.hpp"
#include <sstream>

Annotator* MeTagger::Factory::doCreateAnnotator(
        const boost::program_options::variables_map& options) {
    std::string lang = options["lang"].as<std::string>();
    LangSpecificProcessorFileFetcher fileFetcher(__FILE__, lang);
    std::string modelPathString = "";
    if (options.count("model")) {
        std::string modelFilename = options["model"].as<std::string>();
        boost::filesystem::path modelPath =
            fileFetcher.getOneFile(modelFilename);
        modelPathString = modelPath.string();
    }
    MeTagger* tagger = new MeTagger(
            options.count("train"),
            modelPathString != "" ? modelPathString : DEFAULT_MODEL_FILE,
            options.count("iterations") ? options["iterations"].as<int>() : DEFAULT_ITERATIONS,
            options.count("unknown-pos") ? options["unknown-pos"].as<std::string>() : DEFAULT_UNKNOWN_POS_LABEL,
            options.count("cardinal-number-pos") ? options["cardinal-number-pos"].as<std::string>() : DEFAULT_CARDINAL_NUMBER_POS_LABEL,
            options.count("proper-noun-pos") ? options["proper-noun-pos"].as<std::string>() : DEFAULT_PROPER_NOUN_POS_LABEL,
            options.count("open-class-labels") ? options["open-class-labels"].as<std::vector<std::string> >() : std::vector<std::string>()
            );
    if (! options.count("train")) {
        tagger->loadModel(tagger->getModelFile());
    }
    return tagger;
}

void MeTagger::Factory::doAddLanguageIndependentOptionsHandled(
        boost::program_options::options_description& optionsDescription) {

    optionsDescription.add_options()
        ("model", boost::program_options::value<std::string>()
         ->default_value(DEFAULT_MODEL_FILE), "model file")
        ("iterations", boost::program_options::value<int>()
         ->default_value(DEFAULT_ITERATIONS), "number of iterations")
        ("unknown-pos", boost::program_options::value<std::string>()
         ->default_value(DEFAULT_UNKNOWN_POS_LABEL), "unknown part of speech label")
        ("cardinal-number-pos", boost::program_options::value<std::string>()
         ->default_value(DEFAULT_CARDINAL_NUMBER_POS_LABEL), "cardinal number part of speech label")
        ("proper-noun-pos", boost::program_options::value<std::string>()
         ->default_value(DEFAULT_PROPER_NOUN_POS_LABEL), "proper noun part of speech label")
        ("open-class-labels", boost::program_options::value<std::vector<std::string> >()->multitoken(),
            "open class labels")
        ("train", "training mode")
        ;
}

std::string MeTagger::Factory::doGetName() {
    return "metagger";
}

boost::filesystem::path MeTagger::Factory::doGetFile() {
    return __FILE__;
}

std::list<std::list<std::string> > MeTagger::Factory::doRequiredLayerTags() {
    return std::list<std::list<std::string> >();
}

std::list<std::list<std::string> > MeTagger::Factory::doOptionalLayerTags() {
    return std::list<std::list<std::string> >();
}

std::list<std::string> MeTagger::Factory::doProvidedLayerTags() {
    return std::list<std::string>();
}

const std::string MeTagger::Factory::DEFAULT_MODEL_FILE
= "%ITSDATA%/%LANG%/%LANG%.blm";
const int MeTagger::Factory::DEFAULT_ITERATIONS = 50;
const std::string MeTagger::Factory::DEFAULT_UNKNOWN_POS_LABEL = "ign";
const std::string MeTagger::Factory::DEFAULT_CARDINAL_NUMBER_POS_LABEL = "card";
const std::string MeTagger::Factory::DEFAULT_PROPER_NOUN_POS_LABEL = "name";

LatticeWorker* MeTagger::doCreateLatticeWorker(Lattice& lattice) {
    return new Worker(*this, lattice);
}

MeTagger::Worker::Worker(MeTagger& processor, Lattice& lattice):
    LatticeWorker(lattice), processor_(processor) {
    }

void MeTagger::Worker::doRun() {
    if (processor_.training()) {
        processor_.train(lattice_);
        processor_.saveModel(processor_.getModelFile());
    }
    else
        processor_.tag(lattice_);
}

std::string MeTagger::doInfo() {
    return "maximum entropy pos tagger";
}

MeTagger::MeTagger(bool trainMode_, std::string modelFile_,
        int iterations_, std::string unknownPosLabel_,
        std::string cardinalNumberPosLabel_, std::string properNounPosLabel_,
        std::vector<std::string> openClassLabels_) :
    openForEvents(false), posModelLoaded(false),
    trainMode(trainMode_), trainIterations(iterations_), modelFile(modelFile_),
    unknownPosLabel(unknownPosLabel_),
    cardinalNumberPosLabel(cardinalNumberPosLabel_),
    properNounPosLabel(properNounPosLabel_),
    openClassLabels(openClassLabels_),
    rxUpperCaseFirst("^\\p{Lu}"),
    rxUpperCaseAll("^\\p{Lu}+$"),
    rxContainsNumber("\\d"),
    rxIsNumber("^\\d+([.,]\\d+)(%)?$"),
    rxContainsLetter("\\p{L}"),
    rxContainsPunct("[.,;:?!<>%&'\\-\"/()\\[\\]+=_*$#@~|{}\\\\]") {
}

bool MeTagger::training() {
    return trainMode;
}

std::string MeTagger::getModelFile() {
    return modelFile;
}

void MeTagger::saveModel(std::string path) {
    m.save(path);
}

void MeTagger::loadModel(std::string path) {
    m.load(path);
    posModelLoaded = true;
}

void MeTagger::tag(Lattice &lattice) {
    if (!posModelLoaded) {
        std::cerr << "no model loaded. metagger will do nothing" << std::endl;
        return;
    }
    LayerTagMask segmentMask = lattice.getLayerTagManager().getMask("segment");
    Lattice::EdgesSortedBySourceIterator segmentIt =
        lattice.edgesSortedBySource(segmentMask);
    if (!segmentIt.hasNext()) {
        TokenEdgesMap tokenEdgesMap = createTokenEdgesMap(lattice,
                lattice.getFirstVertex(), lattice.getLastVertex()
                    );
        tagSegment(lattice, tokenEdgesMap);
    }
    while (segmentIt.hasNext()) {
        Lattice::EdgeDescriptor segment = segmentIt.next();

        TokenEdgesMap tokenEdgesMap = createTokenEdgesMap(lattice,
                lattice.getEdgeSource(segment), lattice.getEdgeTarget(segment)
                    );
        tagSegment(lattice, tokenEdgesMap);
    }
}

void MeTagger::tagSegment(Lattice &lattice, TokenEdgesMap tokenEdgesMap) {
    LayerTagMask formMask = lattice.getLayerTagManager().getMask("form");
    std::vector<Outcome> tags;
    for (TokenEdgesMap::iterator tokenIt = tokenEdgesMap.begin();
            tokenIt != tokenEdgesMap.end(); ++ tokenIt) {
        int tokenIndex = tokenIt->first;
        Lattice::EdgeDescriptor token = tokenIt->second;
        std::string prevTag = "";
        if (tokenIndex > 0) {
            prevTag = tags[tokenIndex - 1];
        }
        Context context = createContext(lattice, tokenEdgesMap,
                tokenIndex, 2);
        if (prevTag != "") { //@todo czy ten warunek ma obejmowac tylko te linie nizej czy ten kontekst tez?
            context.push_back(Feature("prevtag=" + prevTag));
        }

        Outcome bestTag = "empty";
        double bestProb = 0;

        Lattice::VertexDescriptor currentVertex =
            lattice.getEdgeSource(token);
        Lattice::InOutEdgesIterator formIt =
            lattice.outEdges(currentVertex, formMask);
        while (formIt.hasNext()) {
            Lattice::EdgeDescriptor form = formIt.next();
            if (getFormPartOfSpeech(lattice, form) == unknownPosLabel) {
                std::string currentOrth = lattice.getEdgeText(token);
                std::vector<std::string> openClasses;
                if (RegExp::PartialMatch(currentOrth, rxContainsPunct)
                        || RegExp::PartialMatch(currentOrth, rxContainsNumber)) {
                    openClasses.push_back(cardinalNumberPosLabel);
                    openClasses.push_back(properNounPosLabel);
                } else {
                    openClasses = openClassLabels;
                }
                if (openClasses.size() > 0) {
                    for (std::vector<std::string>::iterator openClassIt =
                            openClasses.begin();
                            openClassIt != openClasses.end();
                            ++ openClassIt) {
                        Outcome openClassTag(*openClassIt);

                        double prob = m.eval(context, openClassTag);
                        if( prob > bestProb ) {
                            bestTag  = openClassTag;
                            bestProb = prob;
                        }
                    }
                }
            } else {
                Outcome tag( getFormMorphoTag(lattice, form) );
                double prob = m.eval(context, tag);
                if(prob > bestProb) {
                    bestTag = tag;
                    bestProb = prob;
                }
            }
        }
        if(bestTag == "empty") {
            bestTag = m.predict(context);
        }

        tags.push_back(bestTag);
    }

    size_t i = 0;
    while (i < tags.size()) {
        Outcome tag = tags[i];
        TokenEdgesMap::iterator tokenIt = tokenEdgesMap.find(i);
        if (tokenIt != tokenEdgesMap.end()) {
            Lattice::EdgeDescriptor token = tokenIt->second;
            Lattice::VertexDescriptor currentVertex =
                lattice.getEdgeSource(token);
            Lattice::InOutEdgesIterator formIt =
                lattice.outEdges(currentVertex, formMask);
            bool allFormsDiscarded = true;
            while (formIt.hasNext()) {
                Lattice::EdgeDescriptor form = formIt.next();
                if (getFormMorphoTag(lattice, form) != tag) {
                    lattice.discard(form);
                } else {
                    allFormsDiscarded = false;
                }
            }
            if (allFormsDiscarded) {
                if (lattice.getEdgeAnnotationItem(token).getCategory() == "T") {
                    std::string lemma = lattice.getEdgeText(token);
                    std::string partOfSpeech = tag;
                    if (tag.find(":") > 0)
                        partOfSpeech = tag.substr(0, tag.find(":"));

                    if (!lemmaEdgeExists(lattice, token, lemma)) {
                        addLemmaEdge(lattice, token, lemma);
                    }
                    if (!lexemeEdgeExists(lattice, token, lemma, partOfSpeech))
                        addLexemeEdge(lattice, token, lemma, partOfSpeech);

                    addFormEdge(lattice, token, lemma, partOfSpeech, tag);
                }
            }
        }
        i ++;
    }
}

void MeTagger::train(Lattice &lattice) {
    addSampleSentences(lattice);
    m.end_add_event();
    openForEvents = false;
    maxent::verbose = 1;
    m.train(trainIterations);
    maxent::verbose = 0;
}

void MeTagger::addSampleSentences(Lattice &lattice) {
    if(! openForEvents) {
        m.begin_add_event();
        openForEvents = true;
    }
    LayerTagMask segmentMask = lattice.getLayerTagManager().getMask("segment");
    Lattice::EdgesSortedBySourceIterator segmentIt =
        lattice.edgesSortedBySource(segmentMask);
    if (!segmentIt.hasNext()) {
        TokenEdgesMap tokenEdgesMap = createTokenEdgesMap(lattice,
                lattice.getFirstVertex(), lattice.getLastVertex()
                    );
        tagSegment(lattice, tokenEdgesMap);
    }
    while (segmentIt.hasNext()) {
        Lattice::EdgeDescriptor segment = segmentIt.next();

        TokenEdgesMap tokenEdgesMap = createTokenEdgesMap(lattice,
                lattice.getEdgeSource(segment), lattice.getEdgeTarget(segment)
                    );
        addSampleSegment(lattice, tokenEdgesMap);
    }
}

void MeTagger::addSampleSegment(Lattice &lattice,
        TokenEdgesMap tokenEdgesMap) {
    LayerTagMask formMask = lattice.getLayerTagManager().getMask("form");
    for (TokenEdgesMap::iterator tokenIt = tokenEdgesMap.begin();
            tokenIt != tokenEdgesMap.end(); ++ tokenIt) {
        int tokenIndex = tokenIt->first;
        Lattice::EdgeDescriptor token = tokenIt->second;
        std::string prevTag = "";
        if (tokenIndex > 0) {
            int prevIndex = tokenIndex - 1;
            TokenEdgesMap::iterator prevIt =
                tokenEdgesMap.find(prevIndex);
            if (prevIt != tokenEdgesMap.end()) {
                Lattice::EdgeDescriptor prevToken = prevIt->second;
                Lattice::VertexDescriptor prevVertex =
                    lattice.getEdgeSource(prevToken);
                Lattice::InOutEdgesIterator prevFormIt =
                    lattice.outEdges(prevVertex, formMask);
                while (prevFormIt.hasNext()) {
                    Lattice::EdgeDescriptor prevForm = prevFormIt.next();
                    if (isDiscarded(lattice, prevForm))
                        continue;
                    prevTag = getFormMorphoTag(lattice, prevForm);
                }
            }
        }
        Context context = createContext(lattice, tokenEdgesMap,
                tokenIndex, 2);
        if (prevTag != "") { //@todo czy ten warunek ma obejmowac tylko te linie nizej czy ten kontekst tez?
            context.push_back(Feature("prevtag=" + prevTag));
        }

        Lattice::VertexDescriptor currentVertex =
            lattice.getEdgeSource(token);
        Lattice::InOutEdgesIterator formIt =
            lattice.outEdges(currentVertex, formMask);
        while (formIt.hasNext()) {
            Lattice::EdgeDescriptor form = formIt.next();
            if (isDiscarded(lattice, form))
                continue;
            Outcome currentTag = getFormMorphoTag(lattice, form);
            m.add_event(context, currentTag);
            break;
        }
    }
}

MeTagger::TokenEdgesMap MeTagger::createTokenEdgesMap(
        Lattice &lattice, Lattice::VertexDescriptor start,
        Lattice::VertexDescriptor end) {
    TokenEdgesMap tokenEdgesMap;
    LayerTagMask tokenMask = lattice.getLayerTagManager().getMask("token");
    int tokenCount = 0;
    Lattice::VertexDescriptor vertex = start;
    while (vertex < end) {
        Lattice::InOutEdgesIterator tokenIt = lattice.outEdges(vertex, tokenMask);
        if (! tokenIt.hasNext()) {
            ++ vertex;
            continue;
        }
        if (tokenIt.hasNext()) {
            Lattice::EdgeDescriptor token = tokenIt.next();

            if (lattice.getAnnotationCategory(token) != "B") {//skip white spaces
                tokenEdgesMap.insert(std::pair<int, Lattice::EdgeDescriptor>(
                            tokenCount,
                            token
                            ));
                tokenCount ++;
            }
            vertex = lattice.getEdgeTarget(token);
        }
    }
    return tokenEdgesMap;
}

MeTagger::Context MeTagger::createContext(Lattice &lattice,
        TokenEdgesMap tokenEdgesMap,
        int currentIndex, int window) {
    Context context;

    LayerTagMask formMask = lattice.getLayerTagManager().getMask("form");

    std::string currentOrth = "";

    TokenEdgesMap::iterator tokenIt =
        tokenEdgesMap.find(currentIndex);
    if (tokenIt == tokenEdgesMap.end())
        return context;

    Lattice::EdgeDescriptor token = tokenIt->second;
    currentOrth = lattice.getEdgeText(token);
    context.push_back(Feature("curr_word=" + currentOrth));
    context.push_back(Feature("curr_word_length=" +
                boost::lexical_cast<std::string>( currentOrth.size() )));

    Lattice::VertexDescriptor currentVertex =
            lattice.getEdgeSource(token);
    Lattice::InOutEdgesIterator formIt =
        lattice.outEdges(currentVertex, formMask);
    while (formIt.hasNext()) {
        Lattice::EdgeDescriptor form = formIt.next();
        std::string tag = getFormMorphoTag(lattice, form);
        std::string lemma = getFormLemma(lattice, form);
        context.push_back(Feature("curr_has_tag=" + tag));
        context.push_back(Feature("curr_has_lemma=" + lemma));
    }

    if (RegExp::PartialMatch(currentOrth, rxUpperCaseFirst))
        context.push_back("UpperCaseFirst");
    if (RegExp::PartialMatch(currentOrth, rxUpperCaseAll))
        context.push_back("UpperCaseAll");
    if (RegExp::PartialMatch(currentOrth, rxContainsNumber))
        context.push_back("ContainsNumber");
    if (RegExp::PartialMatch(currentOrth, rxIsNumber))
        context.push_back("IsNumber");
    if (RegExp::PartialMatch(currentOrth, rxContainsLetter))
        context.push_back("ContainsLetter");
    if (RegExp::PartialMatch(currentOrth, rxContainsPunct) &&
            RegExp::PartialMatch(currentOrth, rxContainsNumber))
        context.push_back("ContainsPunctAndNumber");
    if (!RegExp::PartialMatch(currentOrth, rxContainsLetter) &&
            !RegExp::PartialMatch(currentOrth, rxContainsNumber))
        context.push_back("ContainsNoLetterAndNoNumber");

    if (currentOrth.size() > 6) {
        context.push_back(Feature("curr_prefix=" + currentOrth.substr(0,1)));
        context.push_back(Feature(
                    "curr_suffix=" + currentOrth.substr(currentOrth.size()-1)));

        context.push_back(Feature("curr_prefix=" + currentOrth.substr(0,2)));
        context.push_back(Feature(
                    "curr_suffix=" + currentOrth.substr(currentOrth.size()-2)));

        context.push_back(Feature("curr_prefix=" + currentOrth.substr(0,3)));
        context.push_back(Feature(
                    "curr_suffix=" + currentOrth.substr(currentOrth.size()-3)));

        context.push_back(Feature(
                    "curr_suffix=" + currentOrth.substr(currentOrth.size()-4)));
    }

    for (int i = 1; i < window; i ++) {
        int prevIndex = 0;
        if (currentIndex >= i)
            prevIndex = currentIndex - i;

        TokenEdgesMap::iterator prevIt =
            tokenEdgesMap.find(prevIndex);
        if (prevIt != tokenEdgesMap.end()) {
            Lattice::EdgeDescriptor prevToken = prevIt->second;
            std::stringstream prevWord;
            prevWord << "prev" << i << "_word=" <<
                lattice.getEdgeText(prevToken);
            context.push_back(Feature(prevWord.str()));

            Lattice::VertexDescriptor prevVertex =
                lattice.getEdgeSource(prevToken);
            Lattice::InOutEdgesIterator prevFormIt =
                lattice.outEdges(prevVertex, formMask);
            while (prevFormIt.hasNext()) {
                Lattice::EdgeDescriptor prevForm = prevFormIt.next();
                std::stringstream prevTag, prevLemma;
                prevTag << "prev" << i << "_has_tag=" <<
                    getFormMorphoTag(lattice, prevForm);
                prevLemma << "prev" << i << "_has_lemma=" <<
                    getFormLemma(lattice, prevForm);
                context.push_back(Feature(prevTag.str()));
                context.push_back(Feature(prevLemma.str()));
            }
        }

        int nextIndex = currentIndex + i;
        if ((size_t)nextIndex >= tokenEdgesMap.size())
            nextIndex = tokenEdgesMap.size() - 1;

        TokenEdgesMap::iterator nextIt =
            tokenEdgesMap.find(nextIndex);
        if (nextIt != tokenEdgesMap.end()) {
            Lattice::EdgeDescriptor nextToken = nextIt->second;
            std::stringstream nextWord;
            nextWord << "next" << i << "_word=" <<
                lattice.getEdgeText(nextToken);
            context.push_back(Feature(nextWord.str()));

            Lattice::VertexDescriptor nextVertex =
                lattice.getEdgeSource(nextToken);
            Lattice::InOutEdgesIterator nextFormIt =
                lattice.outEdges(nextVertex, formMask);
            while (nextFormIt.hasNext()) {
                Lattice::EdgeDescriptor nextForm = nextFormIt.next();
                std::stringstream nextTag, nextLemma;
                nextTag << "next" << i << "_has_tag=" <<
                    getFormMorphoTag(lattice, nextForm);
                nextLemma << "next" << i << "_has_lemma=" <<
                    getFormLemma(lattice, nextForm);
                context.push_back(Feature(nextTag.str()));
                context.push_back(Feature(nextLemma.str()));
            }
        }

    }

    std::set<std::string> uniq;
    uniq.insert( context.begin(), context.end() );
    context.clear();
    context.insert( context.begin(), uniq.begin(), uniq.end() );

    return context;
}

std::string MeTagger::getFormLemma(Lattice &lattice,
        Lattice::EdgeDescriptor edge) {
    std::list<Lattice::Partition> partitions = lattice.getEdgePartitions(edge);
    if (! partitions.empty()) {
        Lattice::Partition firstPartition = partitions.front();
        Lattice::EdgeDescriptor parentEdge = firstPartition.firstEdge(lattice);

        LayerTagCollection tags = lattice.getEdgeLayerTags(parentEdge);
        LayerTagMask mask = lattice.getLayerTagManager().getMask(tags);
        if (lattice.getLayerTagManager().match(mask, "lexeme")) {
            std::string lemma_pos = lattice.getEdgeAnnotationItem(parentEdge).getText();
            if (lemma_pos.find(LEMMA_CATEGORY_SEPARATOR) != std::string::npos) {
                return lemma_pos.substr(0, lemma_pos.find(LEMMA_CATEGORY_SEPARATOR));
            } else {
                return lemma_pos;
            }
        }
    }
    return ""; //@todo: co tu ma zwracac?
}

std::string MeTagger::getFormPartOfSpeech(Lattice &lattice,
        Lattice::EdgeDescriptor edge) {
    std::list<Lattice::Partition> partitions = lattice.getEdgePartitions(edge);
    if (! partitions.empty()) {
        Lattice::Partition firstPartition = partitions.front();
        Lattice::EdgeDescriptor parentEdge = firstPartition.firstEdge(lattice);

        LayerTagCollection tags = lattice.getEdgeLayerTags(parentEdge);
        LayerTagMask mask = lattice.getLayerTagManager().getMask(tags);
        if (lattice.getLayerTagManager().match(mask, "lexeme")) {
            std::string lemma_pos = lattice.getEdgeAnnotationItem(parentEdge).getText();
            if (lemma_pos.find(LEMMA_CATEGORY_SEPARATOR) != std::string::npos) {
                return lemma_pos.substr(lemma_pos.find(LEMMA_CATEGORY_SEPARATOR) + 1, std::string::npos);
            } else {
                return unknownPosLabel; //@todo: co tu ma zwaracac?
            }
        }
    }
    return unknownPosLabel; //@todo: co tu ma zwracac?
}

std::string MeTagger::getFormMorphoTag(Lattice &lattice,
        Lattice::EdgeDescriptor edge) {
    std::string morphoTag = getFormPartOfSpeech(lattice, edge);
    AnnotationItem ai = lattice.getEdgeAnnotationItem(edge);
    std::list< std::pair<std::string, std::string> > av
        = lattice.getAnnotationItemManager().getValues(ai);
    for (std::list< std::pair<std::string, std::string> >::iterator avIt =
            av.begin(); avIt != av.end(); ++ avIt) {
        if (avIt->first == "head" || avIt->first == "orth")
            continue;
        if (morphoTag != "")
            morphoTag += ":";
        morphoTag += avIt->second;
    }
    return morphoTag;
}

bool MeTagger::lemmaEdgeExists(Lattice &lattice,
        Lattice::EdgeDescriptor token, std::string lemma) {
    LayerTagMask lemmaMask = lattice.getLayerTagManager().getMask("lemma");
    Lattice::InOutEdgesIterator lemmaIt =
        lattice.outEdges(lattice.getEdgeSource(token), lemmaMask);
    while (lemmaIt.hasNext()) {
        Lattice::EdgeDescriptor lemmaEdge = lemmaIt.next();
        if (lattice.getEdgeAnnotationItem(lemmaEdge).getText() == lemma)
                return true;
            //LayerTagCollection tags = lattice.getEdgeLayerTags(parentEdge);
            //LayerTagMask mask = lattice.getLayerTagManager().getMask(tags);
            //if (lattice.getLayerTagManager().match(mask, "token")) {
            //}
    }
    return false;
}

void MeTagger::addLemmaEdge(Lattice &lattice,
        Lattice::EdgeDescriptor token, std::string lemma) {
    Lattice::EdgeSequence::Builder seqBuilder(lattice);
    seqBuilder.addEdge(token);

    AnnotationItem annotationItem("word", lemma);
    LayerTagCollection lemmaTag
        = lattice.getLayerTagManager().createSingletonTagCollection("lemma");

    lattice.addEdge(
            lattice.getEdgeSource(token),
            lattice.getEdgeTarget(token),
            annotationItem,
            lemmaTag,
            seqBuilder.build()
            );
}

bool MeTagger::lexemeEdgeExists(Lattice &lattice,
        Lattice::EdgeDescriptor token, std::string lemma,
        std::string partOfSpeech) {
    LayerTagMask lexemeMask = lattice.getLayerTagManager().getMask("lexeme");
    Lattice::InOutEdgesIterator lexemeIt =
        lattice.outEdges(lattice.getEdgeSource(token), lexemeMask);
    while (lexemeIt.hasNext()) {
        Lattice::EdgeDescriptor lexeme = lexemeIt.next();
        if (lattice.getEdgeAnnotationItem(lexeme).getCategory() == partOfSpeech) {
            std::list<Lattice::Partition> partitions = lattice.getEdgePartitions(lexeme);
            if (! partitions.empty()) {
                Lattice::Partition firstPartition = partitions.front();
                Lattice::EdgeDescriptor parentEdge = firstPartition.firstEdge(lattice);

                LayerTagCollection tags = lattice.getEdgeLayerTags(parentEdge);
                LayerTagMask mask = lattice.getLayerTagManager().getMask(tags);
                if (lattice.getLayerTagManager().match(mask, "lemma")) {
                    if (lattice.getEdgeAnnotationItem(parentEdge).getText() == lemma)
                        return true;
                }
            }
        }
    }
    return false;
}

void MeTagger::addLexemeEdge(Lattice &lattice,
        Lattice::EdgeDescriptor token, std::string lemma,
        std::string partOfSpeech) {
    LayerTagMask lemmaMask = lattice.getLayerTagManager().getMask("lemma");
    Lattice::InOutEdgesIterator lemmaIt =
        lattice.outEdges(lattice.getEdgeSource(token), lemmaMask);
    while (lemmaIt.hasNext()) {
        Lattice::EdgeDescriptor lemmaEdge = lemmaIt.next();
        if (lattice.getEdgeAnnotationItem(lemmaEdge).getText() == lemma) {
            Lattice::EdgeSequence::Builder seqBuilder(lattice);
            seqBuilder.addEdge(lemmaEdge);

            AnnotationItem annotationItem(partOfSpeech, StringFrag(
                        lemma + LEMMA_CATEGORY_SEPARATOR + partOfSpeech) );
            LayerTagCollection lexemeTag
                = lattice.getLayerTagManager().createSingletonTagCollection("lexeme");

            lattice.addEdge(
                    lattice.getEdgeSource(token),
                    lattice.getEdgeTarget(token),
                    annotationItem,
                    lexemeTag,
                    seqBuilder.build()
                    );
        }
    }
}

void MeTagger::addFormEdge(Lattice &lattice,
        Lattice::EdgeDescriptor token, std::string lemma,
        std::string partOfSpeech, std::string tag) {
    std::string lexemeText(lemma + LEMMA_CATEGORY_SEPARATOR + partOfSpeech);
    LayerTagMask lexemeMask = lattice.getLayerTagManager().getMask("lexeme");
    Lattice::InOutEdgesIterator lexemeIt =
        lattice.outEdges(lattice.getEdgeSource(token), lexemeMask);
    while (lexemeIt.hasNext()) {
        Lattice::EdgeDescriptor lexeme = lexemeIt.next();
        if (lattice.getEdgeAnnotationItem(lexeme).getCategory() == partOfSpeech) {
            if (lattice.getEdgeAnnotationItem(lexeme).getText() ==
                    lexemeText) {
                Lattice::EdgeSequence::Builder seqBuilder(lattice);
                seqBuilder.addEdge(lexeme);

                AnnotationItem annotationItem(partOfSpeech,
                        StringFrag(lattice.getEdgeText(token)) );
                lattice.getAnnotationItemManager().setValue(
                        annotationItem, "morphology", tag); //@todo: nie mam pomyslu jak z tagu zgadywac jaki atrybut morfosyntaktyczny mamy ustawic
                LayerTagCollection formTag
                    = lattice.getLayerTagManager().createSingletonTagCollection("form");

                lattice.addEdge(
                        lattice.getEdgeSource(token),
                        lattice.getEdgeTarget(token),
                        annotationItem,
                        formTag,
                        seqBuilder.build()
                        );
            }
        }
    }
}

bool MeTagger::isDiscarded(Lattice &lattice, Lattice::EdgeDescriptor edge) {
    LayerTagMask mask = lattice.getLayerTagManager().getMask(
            lattice.getEdgeLayerTags(edge));
    if (lattice.getLayerTagManager().match(mask, "discarded"))
        return true;
    else
        return false;
}
