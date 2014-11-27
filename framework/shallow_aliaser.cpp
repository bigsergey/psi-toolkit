#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "shallow_aliaser.hpp"

#include <iostream>

#include "split_pipeline.hpp"


typedef std::map<std::string, std::string> string_map;

std::map<std::string, std::string> ShallowAliaser::ALIASES =
    boost::assign::map_list_of
        ("docx-reader", "apertium-reader --format docx")
        ("read-docx",   "apertium-reader --format docx")
        ("xlsx-reader", "apertium-reader --format xlsx")
        ("read-xlsx",   "apertium-reader --format xlsx")
        ("pptx-reader", "apertium-reader --format pptx")
        ("read-pptx",   "apertium-reader --format pptx")
        ("html-reader", "apertium-reader --format html")
        ("read-html",   "apertium-reader --format html")

        ("write-tokens",    "simple-writer --tags token")
        ("get-tokens",      "simple-writer --tags token")
        ("write-segments",  "simple-writer --tags segment")
        ("get-segments",    "simple-writer --tags segment")

        ("xml-writer", "bracketing-writer --skip-symbol-edges --opening-bracket \n"
            "<edge tags=\"%T\" category=\"%c\" attrs=\"%A\"> --closing-bracket </edge>\n"
            " --av-separator : --av-pairs-separator ;")
        ("write-xml", "bracketing-writer --skip-symbol-edges --opening-bracket \n"
            "<edge tags=\"%T\" category=\"%c\" attrs=\"%A\"> --closing-bracket </edge>\n"
            " --av-separator : --av-pairs-separator ;")
        ("get-xml", "bracketing-writer --skip-symbol-edges --opening-bracket \n"
            "<edge tags=\"%T\" category=\"%c\" attrs=\"%A\"> --closing-bracket </edge>\n"
            " --av-separator : --av-pairs-separator ;")

        ("write-tree",  "gv-writer --tree")
        ("get-tree",    "gv-writer --tree")
        ("draw-tree",   "gv-writer --tree")

        ("write-parse-tree",    "gv-writer --tree --tags parse")
        ("get-parse-tree",      "gv-writer --tree --tags parse")
        ("draw-parse-tree",     "gv-writer --tree --tags parse")

        ("translate-plen",  "gobio --lang pl ! bilexicon --lang pl --trg-lang en ! transferer"
            " --lang pl --trg-lang en")
        ("translate-ples",  "gobio --lang pl ! bilexicon --lang pl --trg-lang es ! transferer"
            " --lang pl --trg-lang es")
        ("translate-plru",  "gobio --lang pl ! bilexicon --lang pl --trg-lang ru ! transferer"
            " --lang pl --trg-lang ru")
        ;


ShallowAliaser::ShallowAliaser() { }

std::string ShallowAliaser::replace(const std::string& pipeline) {
    std::string result = std::string(" " + pipeline + " ");

    // FIXME: optimize using regexps
    BOOST_FOREACH(const string_map::value_type& pair, ALIASES) {
        boost::replace_all(result, " " + pair.first + " ", " " + pair.second + " ");
    }

    boost::algorithm::trim(result);
    return result;
}

std::vector<std::string> ShallowAliaser::replace(std::vector<std::string> pipeline) {
    for (size_t i = 0; i < pipeline.size(); ++i) {
        boost::replace_all(pipeline[i], "\\", "\\\\\\\\");
        boost::replace_all(pipeline[i], "\\\\\\\\n", "\\\\n");
        boost::replace_all(pipeline[i], "\\\\\\\\\\\\n", "\\\\\\\\n");
        boost::replace_all(pipeline[i], "\"", "\\\"");
        if (pipeline[i].find(" ") != std::string::npos) {
            pipeline[i] = "\"" + pipeline[i] + "\"";
        }
    }

    std::string pipelineAsString = boost::algorithm::join(pipeline, " ");
    std::string resultAsString = replace(pipelineAsString);

    return splitPipeline(resultAsString);
}

std::set<std::string> ShallowAliaser::getAllAliases() {
    std::set<std::string> aliases;

    BOOST_FOREACH(const string_map::value_type& pair, ALIASES) {
        aliases.insert(boost::algorithm::trim_copy(pair.first));
    }

    return aliases;
}

bool ShallowAliaser::hasAlias(const std::string& alias) {
    return ALIASES.count(alias);
}

std::string ShallowAliaser::getProcessorNameForAlias(const std::string& alias) {
    return ALIASES[alias];
}

/* adding new aliases dynamically */

void ShallowAliaser::addReadReaderAlias(std::string alias, std::string expand) {
    addAlias_("read-" + alias, expand);
    addAlias_(alias + "-reader", expand);
}

void ShallowAliaser::addWriteGetAlias(std::string alias, std::string expand) {
    addAlias_("write-" + alias, expand);
    addAlias_("get-" + alias, expand);
}

void ShallowAliaser::addWriteWriterGetAlias(std::string alias, std::string expand) {
    addWriteGetAlias(alias, expand);
    addAlias_(alias + "-writer", expand);
}

void ShallowAliaser::addWriteGetDrawAlias(std::string alias, std::string expand) {
    addWriteGetAlias(alias, expand);
    addAlias_("draw-" + alias, expand);
}

void ShallowAliaser::addAlias_(std::string key, std::string value) {
    ALIASES.insert(std::pair<std::string, std::string>(key, value));
}
