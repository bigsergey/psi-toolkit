
#include "rule.hpp"

#include <iostream>

#include "group_action.hpp"
#include <exception>

namespace poleng
{

namespace bonsai
{
    namespace puddle
    {

#if HAVE_RE2
        Rule::Rule( std::string aName, std::string aCompiled, int aLeftCount,
                int aMatchCount, int aRightCount, ActionsPtr aActions,
                RuleTokenPatterns aRuleTokenPatterns,
                RuleTokenModifiers aRuleTokenModifiers,
                RuleTokenRequirements aRuleTokenRequirements,
                RulePatternIndices aRulePatternIndices,
                bool aRepeat, std::string aLeft, std::string aMatch, std::string aRight,
                NegativePatternStrings aNegativePatterns ) {
#else
       Rule::Rule( std::string aName, std::string aCompiled, int aLeftCount,
               int aMatchCount, int aRightCount, ActionsPtr aActions,
               RuleTokenPatterns aRuleTokenPatterns,
               RuleTokenModifiers aRuleTokenModifiers,
               RuleTokenRequirements aRuleTokenRequirements,
               RulePatternIndices aRulePatternIndices,
               bool aRepeat, std::string aLeft, std::string aMatch, std::string aRight) {
#endif

           ruleTokenPatterns = aRuleTokenPatterns;
           ruleTokenModifiers = aRuleTokenModifiers;
           ruleTokenRequirements = aRuleTokenRequirements;
           rulePatternIndices = aRulePatternIndices;

           pattern = PatternPtr( new Pattern("") );
           actions = aActions;

           name = aName;
           setPattern(aCompiled);
           leftCount = aLeftCount;
           matchCount = aMatchCount;
           rightCount = aRightCount;

           repeat = aRepeat;

           left_ = aLeft;
           match_ = aMatch;
           right_ = aRight;

#if HAVE_RE2
           for (NegativePatternStrings::iterator negPatIt =
                   aNegativePatterns.begin(); negPatIt != aNegativePatterns.end();
                   ++ negPatIt) {
               PatternPtr negativePattern = PatternPtr(new Pattern( negPatIt->second ));
               negativePatterns.insert(std::pair<std::string, PatternPtr>(
                           negPatIt->first, negativePattern) );
           }
#endif
       }

bool Rule::apply(std::string &, Lattice &lattice, int matchedStartIndex,
        RuleTokenSizes &ruleTokenSizes,
        std::list<Lattice::EdgeSequence> &rulePartitions) {
    bool ret = false;
    for (Actions::iterator actionIt = actions->begin();
            actionIt != actions->end(); ++ actionIt) {
        if ( (*actionIt)->apply(lattice, matchedStartIndex, ruleTokenSizes,
                    rulePartitions) ) {
            ret = true;
        }
    }
    return ret;
}

bool Rule::test(std::string &, Lattice &lattice, int matchedStartIndex,
        std::vector<StringPiece> &match, RuleTokenSizes &ruleTokenSizes,
        std::list<Lattice::EdgeSequence> &rulePartitions) {

    ruleTokenSizes.clear();
    ruleTokenSizes.assign(rulePatternIndices.size(), 0);

    if (! requiredTokensMatched(match, ruleTokenSizes) )
        return false;

    for (Actions::iterator actionIt = actions->begin();
            actionIt != actions->end(); ++ actionIt) {
        if ( (*actionIt)->test(lattice, matchedStartIndex, ruleTokenSizes,
                    rulePartitions)
                == false) {
            int limit;
            int lastIndex = leftCount + matchCount - 1;
            if (ruleTokenModifiers[lastIndex] == "+" ||
                    ruleTokenModifiers[lastIndex] == "") {
                limit = 1;
            } else {
                limit = 0;
            }
            if (ruleTokenSizes[lastIndex] > limit) {
                ruleTokenSizes[lastIndex] --;
                continue;
            }
            return false;
        }
    }

    int leftBound;
    int rightBound;
    int matchWidth = leftCount + matchCount - 1;
    if (! util::getRuleBoundaries(ruleTokenSizes, leftCount, matchWidth,
                leftBound, rightBound))
        return false;
    Lattice::VertexDescriptor startVertex = lattice::getVertex(
            lattice, leftBound, matchedStartIndex);
    Lattice::VertexDescriptor endVertex = lattice::getVertex(
            lattice, rightBound, matchedStartIndex);
    rulePartitions = lattice::getEdgesRange(lattice,
            startVertex, endVertex);

    return true;
}

int Rule::matchPattern(std::string &sentenceString,
        int &afterIndex, std::vector<StringPiece> &match) {

    int num_groups = pattern->NumberOfCapturingGroups();
#if HAVE_RE2
    std::map<std::string, int> namedGroups = pattern->NamedCapturingGroups();
#endif
    StringPiece* matchedS = new StringPiece[num_groups];
    Arg** matched = new Arg*[num_groups];
    for (int argIt = 0; argIt < num_groups; argIt ++) {
        matched[argIt] = new Arg;
        *matched[argIt] = &matchedS[argIt];
    }
    StringPiece sentence_str(sentenceString);
    StringPiece orig_str(sentenceString);

    try {
        std::string before = "";
#if HAVE_RE2
        while ( RegExp::FindAndConsumeN( &sentence_str, *pattern, matched,
                    num_groups ) ) {
#else
        if ( RegExp::FindAndConsumeN( &sentence_str, *pattern, matched,
                    num_groups ) ) {
#endif
            int prefix_len = matchedS[0].data() - orig_str.data();
            int suffix_start = matchedS[0].data() +
                matchedS[0].size() - orig_str.data();
            std::string prefix = sentenceString.substr(0, prefix_len);
            std::string suffix =
                sentenceString.substr(suffix_start, std::string::npos);
            before += prefix;
            std::string matching = "";
            matching = matchedS[0].as_string();

            for (int i = 0; i < num_groups; i ++) {
                match.push_back(matchedS[i]);
            }

#if HAVE_RE2
            if (! namedGroups.empty()) {
                bool negPatternMatched = false;
                for (std::map<std::string, int>::iterator namedGroupIt =
                        namedGroups.begin(); namedGroupIt != namedGroups.end();
                        ++ namedGroupIt) {
                    std::string groupName = namedGroupIt->first;
                    int groupIndex = namedGroupIt->second - 1;
                    NegativePatterns::iterator negPatternIt =
                        negativePatterns.find(groupName);
                    if (negPatternIt != negativePatterns.end()) {
                        std::string submatch = matchedS[groupIndex].as_string();
                        submatch = util::unescapeSpecialChars(submatch);
                        if (RegExp::FullMatch(submatch, *(negPatternIt->second) )) {
                            negPatternMatched = true;
                            break;
                        }
                    }
                }
                if (negPatternMatched) {
                    before += matchedS[0].as_string();
                    sentence_str.set(suffix.c_str());
                    continue;
                }
            }
#endif

            if (matching == "") {
                delete[] matchedS;
                for (int argIt = 0; argIt < num_groups; argIt ++)
                    delete matched[argIt];
                delete[] matched;
                return -1;
            }
            int r = getPatternEnd(before);
            if (before == "")
                r = getPatternStart(matching);
            afterIndex = getPatternStart(suffix);
            delete[] matchedS;
            for (int argIt = 0; argIt < num_groups; argIt ++)
                delete matched[argIt];
            delete[] matched;
            return r;
        }
        }
        catch (std::exception &e) {
            std::cerr << "Exception in puddle: " << e.what() << std::endl;
            delete[] matchedS;
            for (int argIt = 0; argIt < num_groups; argIt ++)
                delete matched[argIt];
            delete[] matched;
            return -1;
        }
        delete[] matchedS;
        for (int argIt = 0; argIt < num_groups; argIt ++)
            delete matched[argIt];
        delete[] matched;

        return -1;
    }


int Rule::countTokensMatched(std::string matched) {
    int p = 0;

    StringPiece tmpMatched = matched;
    while ( RegExp::FindAndConsumeN( &tmpMatched, "^<<[tsg]<\\d+<\\d+[^>]+>", 0, 0) ) {
        p ++;
    }
    return p;
}

std::string Rule::getName() const {
    return name;
}

int Rule::getLeftCount() const {
    return leftCount;
}

int Rule::getMatchCount() const {
    return matchCount;
}

int Rule::getRightCount() const {
    return rightCount;
}

void Rule::setPattern(std::string aCompiled) {
    pattern = PatternPtr(new Pattern(aCompiled));
    compiled = aCompiled;
}

void Rule::setLeftCount(int aCount) {
    leftCount = aCount;
}

void Rule::setMatchCount(int aCount) {
    matchCount = aCount;
}

void Rule::setRightCount(int aCount) {
    rightCount = aCount;
}

void Rule::setRepeat(bool aRepeat) {
    repeat = aRepeat;
}

bool Rule::getRepeat() const {
    return repeat;
}

void Rule::setMatch(std::string aMatch) {
    match_ = aMatch;
}

void Rule::setLeft(std::string aLeft) {
    left_ = aLeft;
}

void Rule::setRight(std::string aRight) {
    right_ = aRight;
}

void Rule::setRuleTokenPatterns(RuleTokenPatterns aRuleTokenPatterns) {
    ruleTokenPatterns = aRuleTokenPatterns;
}

void Rule::setRuleTokenModifiers(RuleTokenModifiers aRuleTokenModifiers) {
    ruleTokenModifiers = aRuleTokenModifiers;
}

void Rule::setRuleTokenRequirements(RuleTokenRequirements aRuleTokenRequirements) {
    ruleTokenRequirements = aRuleTokenRequirements;
}

void Rule::setRulePatternIndices(RulePatternIndices aRulePatternIndices) {
    rulePatternIndices = aRulePatternIndices;
}

std::string Rule::getMatch() const {
    return match_;
}

std::string Rule::getLeft() const {
    return left_;
}

std::string Rule::getRight() const {
    return right_;
}

void Rule::addAction(ActionPtr action) {
    actions->push_back(action);
}

void Rule::deleteAction(size_t index) {
    if (index < actions->size())
        actions->erase(actions->begin() + index);
}

        int Rule::getPatternStart(std::string &pattern) {
            int r = -1;

            Pattern regPatternInfo("^<<[tsg]<(\\d+)<\\d+[^>]+>");
            int start;
            if (RegExp::PartialMatch(pattern, regPatternInfo, &start)) {
                r = start;
            }

            return r;
        }

        int Rule::getPatternEnd(std::string &pattern) {
            int r = -1;

            Pattern regPatternInfo("<<[tsg]<\\d+<(\\d+)[^>]+>$");
            int end;
            if (RegExp::PartialMatch(pattern, regPatternInfo, &end)) {
                r = end;
            }

            return r;
        }

        bool Rule::requiredTokensMatched(std::vector<StringPiece> &match,
                RuleTokenSizes &ruleTokenSizes) {
            int index = 0;
            int tokensMatched = 0;
            for (RulePatternIndices::iterator indexIt = rulePatternIndices.begin();
                    indexIt != rulePatternIndices.end(); ++ indexIt) {
                std::string part = match[*indexIt].as_string();
                if ((part == "") && (ruleTokenRequirements[index])) {
                    return false;
                }
                ruleTokenSizes.at(index) = countTokensMatched(part);
                if (ruleTokenRequirements[index] && ruleTokenSizes[index] == 0) {
                    return false;
                }
                tokensMatched += ruleTokenSizes.at(index);
                index ++;
            }
            if (tokensMatched == 0) {
                return false;
            }
            return true;
        }
}

}

}

