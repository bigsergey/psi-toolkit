#ifndef APERTIUM_FORMAT_RULES_HDR
#define APERTIUM_FORMAT_RULES_HDR

#include <string>
#include <map>


class FormatOptions {
public:

    FormatOptions(int largeblocksSize,
        const std::string& inputEncoding, const std::string& outputEncoding,
        const std::string& escapeChars, const std::string& spaceChars,
        bool caseSensitive);

    bool isCaseSensitive();

private:

    int largeblocksSize_;
    std::string escapeChars_;
    std::string spaceChars_;
    bool caseSensitive_;

};


class FormatRule {
public:

    FormatRule(const std::string& type, bool eos, int priority,
        const std::string& beginEnd);
    FormatRule(const std::string& type, bool eos, int priority,
        const std::string& begin, const std::string& end);

    std::string getType();
    bool isEos();
    int getPriority();

private:

    std::string type_;
    bool eos_;
    int priority_;

    std::string beginEnd_;
    std::string begin_;
    std::string end_;

};


class ReplacementRule {
public:

    ReplacementRule(const std::string& regexp);

    void addReplacement(std::string source, std::string target);

    std::string getRegexp();
    unsigned int replacementsCount();

private:

    std::string regexp_;
    std::map<std::string, std::string> sourceToTargetMap_;

};

#endif
