#include "lamerlemma.hpp"
#include "psi_exception.hpp"

LamerLemma::LamerLemma(const boost::program_options::variables_map& options)
 : m_dict(options.count("pos") || options.count("morpho"), options.count("morpho"))
{
    std::string lang = options["lang"].as<std::string>();
    langCode_ = lang;

    LangSpecificProcessorFileFetcher fileFetcher(__FILE__, lang);
    
    if (options.count("plain-text-lexicon")) {
        if (options.count("binary-lexicon")
            && options["binary-lexicon"].as<std::string>() != DEFAULT_LAMERLEMMA_SPEC)
            throw new PsiException(
                "either --plain-text-lexicon or --binary-lexicon expected, not both");

        boost::filesystem::path plainTextLexiconPath =
            fileFetcher.getOneFile(
                options["plain-text-lexicon"].as<std::string>());

        m_dict.read_dictionary(plainTextLexiconPath.string());
    } else if (options.count("binary-lexicon")) {
        boost::filesystem::path binaryLexiconPath =
            fileFetcher.getOneFile(
                options["binary-lexicon"].as<std::string>());

        m_dict.load(binaryLexiconPath.string());
    }

    if (options.count("save-binary-lexicon")) {
        if (m_dict.is_empty())
            throw new PsiException("no data to save");

        boost::filesystem::path binaryLexiconPath(
            options["save-binary-lexicon"].as<std::string>());

        m_dict.save(binaryLexiconPath.string());
    }

}

bool LamerLemma::lemmatize(const std::string& token,
                               AnnotationItemManager& annotationItemManager,
                               LemmatizerOutputIterator& outputIterator) {
    bool foundLemma = false;
    
    DictionaryItem item = m_dict.look_up(token);
    if(item.get_pos() != "unknown")
        foundLemma = true;
    else
        return false;
        
    Interpretations interpretations = item.get_interpretations();
    
    typedef std::map<std::string, std::vector<Interpretation> > StringInterMap;
    StringInterMap lemma_map;
    BOOST_FOREACH(Interpretation i , interpretations)
        lemma_map[i.get_lemma()].push_back(i);
    
    BOOST_FOREACH(StringInterMap::value_type lem_inter, lemma_map)
    {
        std::string lemma = lem_inter.first;
        outputIterator.addLemma(lemma);
        
        if(m_dict.has_pos()) {
            StringInterMap lexeme_map;
            BOOST_FOREACH(Interpretation i, lem_inter.second)
            {
                std::string lemma = i.get_lemma();
                std::string pos = i.get_pos();
                std::string lexeme = lemma + "+" + pos;
                lexeme_map[lexeme].push_back(i);
            }
            
            BOOST_FOREACH(StringInterMap::value_type lex_inter, lexeme_map)
            {
                std::string lexeme = lex_inter.first;
                std::string pos = lex_inter.second[0].get_pos();
                
                AnnotationItem ai_lexeme(pos, StringFrag(lexeme));
                outputIterator.addLexeme(ai_lexeme);
        
                BOOST_FOREACH(Interpretation i, lex_inter.second) {
                    AnnotationItem form(pos, StringFrag(token));
                    if(m_dict.has_morpho()) {
                        typedef std::pair<std::string, std::string> StringPair;
                        std::vector<std::string>& morphologies = i.get_morpho();
                        BOOST_FOREACH(StringPair kv, morpho_to_features(morphologies)) {
                            annotationItemManager.setValue(form, kv.first, kv.second);
                        }
                    }
                    outputIterator.addForm(form);
                }
            }
        }
    }
    return foundLemma;
}

std::vector<std::pair<std::string, std::string> > 
LamerLemma::morpho_to_features(std::vector<std::string> &morphos) {
    std::vector<std::pair<std::string, std::string> > features;
    BOOST_FOREACH(std::string morpho, morphos) {
        int pos;
        if((pos = morpho.find("=")) != std::string::npos) {
            std::string key = morpho.substr(0, pos);
            std::string value = morpho.substr(pos + 1);
            
            features.push_back(std::make_pair(key, value));    
        }
        else
            features.push_back(std::make_pair(morpho, "1"));
    }
    return features;
}

boost::program_options::options_description LamerLemma::optionsHandled() {
    boost::program_options::options_description desc;

    LanguageDependentAnnotatorFactory::addLanguageDependentOptions(desc);

    desc.add_options()
        ("binary-lexicon",boost::program_options::value<std::string>()
         ->default_value(DEFAULT_LAMERLEMMA_SPEC),
         "path to the lexicon in the binary format")
        ("level", boost::program_options::value<int>()->default_value(3),
         "set word processing level 0-3 (0 - do nothing, 1 - return only base forms, "
         "2 - add grammatical class and main attributes, 3 - add detailed attributes)")
        ("plain-text-lexicon",
         boost::program_options::value<std::string>(),
         "path to the lexicon in the plain text format")
        ("pos", "text file contains part-of-speech information")
        ("morpho", "text file contains morphology information (implies --pos)")
        ("save-binary-lexicon",
         boost::program_options::value<std::string>(),
         "as a side effect the lexicon in the binary format is generated");
    
    return desc;
}


std::string LamerLemma::getName() {
    return "lamerlemma";
}

boost::filesystem::path LamerLemma::getFile() {
    return __FILE__;
}

std::string LamerLemma::getLanguage() const {
    return langCode_;
}

AnnotatorFactory::LanguagesHandling LamerLemma::languagesHandling(
    const boost::program_options::variables_map& options) {
    return LanguageDependentAnnotatorFactory::checkLangOption(options);
}

std::list<std::string> LamerLemma::languagesHandled(
    const boost::program_options::variables_map& options) {

    if (LanguageDependentAnnotatorFactory::checkLangOption(options)
        == AnnotatorFactory::JUST_ONE_LANGUAGE)
        return boost::assign::list_of(options["lang"].as<std::string>());

    std::string fileSuffix = ".bin";

    std::vector<std::string> langs;

    boost::filesystem::path dataDirectory = getItsData(getFile());

    boost::filesystem::directory_iterator end_iter;
    for (boost::filesystem::directory_iterator fiter(dataDirectory);
         fiter != end_iter;
         ++fiter) {
            boost::filesystem::path seg(fiter->path().filename());
            std::string lexiconFileName = seg.string();

            if (lexiconFileName.length() > fileSuffix.length()
                && lexiconFileName.substr(
                    lexiconFileName.length() - fileSuffix.length())
                == fileSuffix)
                langs.push_back(lexiconFileName.substr(
                                    0, lexiconFileName.length() - fileSuffix.length()));
    }

    std::sort(langs.begin(), langs.end());

    return std::list<std::string>(langs.begin(), langs.end());
}

bool LamerLemma::checkRequirements(
    const boost::program_options::variables_map& /*options*/,
    std::ostream & /*message*/) {

    return true;
}

std::list<std::string> LamerLemma::getLayerTags() {
    std::list<std::string> layerTags;

    layerTags.push_back("lamerlemma");

    return layerTags;
}

const std::string LamerLemma::DEFAULT_LAMERLEMMA_SPEC = "%ITSDATA%/%LANG%.bin";