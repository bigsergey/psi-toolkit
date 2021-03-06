#ifndef HTML_HELP_FORMATTER_HDR
#define HTML_HELP_FORMATTER_HDR

#include "help_formatter.hpp"
#include "file_storage.hpp"
#include "file_recognizer.hpp"

#include <set>

class HtmlHelpFormatter : public HelpFormatter {

public:
    HtmlHelpFormatter();
    HtmlHelpFormatter(bool useJavaScript);
    ~HtmlHelpFormatter();

    void formatPipelineExamplesInJSON(std::ostream& output);
    void formatHelpsWithTypes(std::ostream& output);

    /*
     * If you pass FileStorage, some example inputs and outputs in non-textual format
     * (such as .rtf, .doc, .jpg etc.) will be displayed as files to download.
     */
    void setFileStorage(FileStorage* fileStorage);
    void unsetFileStorage();

    void setUseJavaScript(bool onOff);

private:

    FileStorage* fileStorage_;
    FileRecognizer fileRecognizer_;
    bool useJavaScript_;


    void doFormatOneProcessorHelp(
        std::string processorName,
        std::string description,
        std::string detailedDescription,
        boost::program_options::options_description options,
        std::list<std::string> aliases,
        std::vector<TestBatch> usingExamples,
        std::list<std::string> languagesHandled,
        std::ostream& output);

    void formatDescription_(
        const std::string& description,
        const std::string& details,
        const std::string& processorName,
        std::ostream& output);

    void formatAliases_(std::list<std::string> aliases, std::ostream& output);

    void formatUsingExamples_(std::vector<TestBatch> batches, std::ostream& output);
    void formatExampleInputOutput_(
        const boost::filesystem::path& filePath,
        std::ostream& output,
        const std::string& divClass);

    void formatAllowedOptions_(
        boost::program_options::options_description options,
        std::ostream& output);

    void doFormatOneAlias(
        std::string aliasName,
        std::list<std::string> processorNames,
        std::ostream& output);

    void doFormatDataFile(std::string text, std::ostream& output);

    void formatPipelineExampleInJSON_(TestBatch batch, std::ostream& output);

    std::string escapeHTML_(const std::string& text);
    std::string escapeJSON_(std::string& text);

    static std::set<std::string> extensionsForRandomExamples_;

    void formatLanguagesHandled_(std::list<std::string> langCodes, std::ostream& output);
};

#endif
