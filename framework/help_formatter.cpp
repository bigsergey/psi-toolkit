#include <sstream>
#include <fstream>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "help_formatter.hpp"
#include "its_data.hpp"
#include "configurator.hpp"
#include "shallow_aliaser.hpp"

void HelpFormatter::formatHelps(std::ostream& output) {
    std::vector<std::string> processors = MainFactoriesKeeper::getInstance().getProcessorNames();

    BOOST_FOREACH(std::string processorName, processors) {
        output << std::endl;
        formatOneProcessorHelp(processorName, output);
    }
}

void HelpFormatter::formatOneProcessorHelp(std::string processorName, std::ostream& output) {
    doFormatOneProcessorHelp(
        processorName,
        getProcessorDescription(processorName),
        getProcessorDetailedDescription(processorName),
        getProcessorOptions(processorName),
        getAliasesForProcessorName(processorName),
        getProcessorUsingExamples(processorName),
        getLanguagesHandledForProcessor(processorName),
        output
    );
}

bool HelpFormatter::formatProcessorHelpsByName(std::string aliasOrProcessorName,
                                               std::ostream& output) {
    std::vector<std::string> procs = MainFactoriesKeeper::getInstance().getProcessorNames();
    bool isProcessor =
        (std::find(procs.begin(), procs.end(), aliasOrProcessorName) != procs.end());

    if (isProcessor) {
        formatOneProcessorHelp(aliasOrProcessorName, output);
        return true;
    }

    std::set<std::string> aliases = MainFactoriesKeeper::getInstance().getAliasNames();
    bool isAlias = aliases.count(aliasOrProcessorName);

    if (isAlias) {
        BOOST_FOREACH(ProcessorFactory* processor, MainFactoriesKeeper::getInstance().
                getProcessorFactoriesForName(aliasOrProcessorName)) {
            formatOneProcessorHelp(processor->getName(), output);
        }
        return true;
    }

    return false;
}

void HelpFormatter::formatAliases(std::ostream& output) {
    std::set<std::string> aliases = MainFactoriesKeeper::getInstance().getAliasNames();

    BOOST_FOREACH(std::string alias, ShallowAliaser::getAllAliases()) {
        aliases.insert(alias);
    }

    std::list<std::string> processorNames;

    BOOST_FOREACH(std::string alias, aliases) {
        if (ShallowAliaser::hasAlias(alias)) {
            processorNames.push_back(ShallowAliaser::getProcessorNameForAlias(alias));
        }
        else {
            processorNames = getProcessorNamesForAlias(alias);
        }

        formatOneAlias(alias, processorNames, output);

        processorNames.clear();
    }
}

void HelpFormatter::formatOneAlias(std::string aliasName,
                                   std::list<std::string> processorNames,
                                   std::ostream& output) {
    doFormatOneAlias(aliasName, processorNames, output);
}

void HelpFormatter::formatDescription(std::ostream& output) {
    boost::filesystem::path path = getPathToFrameworkDataFile_("description.txt");
    doFormatDataFile(getFileContent(path), output);
}

void HelpFormatter::formatTutorial(std::ostream& output) {
    boost::filesystem::path path = getPathToFrameworkDataFile_("tutorial.txt");
    doFormatDataFile(getFileContent(path), output);
}

void HelpFormatter::formatLicence(std::ostream& output) {
    boost::filesystem::path path = getPathToFrameworkDataFile_("licence.txt");
    doFormatDataFile(getFileContent(path), output);
}

void HelpFormatter::formatAboutPsiFormat(std::ostream& output) {
    boost::filesystem::path path = getPathToFrameworkDataFile_("psi-format.txt");
    doFormatDataFile(getFileContent(path), output);
}

void HelpFormatter::formatFAQ(std::ostream& output) {
    boost::filesystem::path path = getPathToFrameworkDataFile_("faq.txt");
    doFormatDataFile(getFileContent(path), output);
}

void HelpFormatter::formatInstallationGuide(std::ostream& output) {
    boost::filesystem::path path = getPathToFrameworkDataFile_("installation_guide.txt");
    doFormatDataFile(getFileContent(path), output);
}

HelpFormatter::~HelpFormatter() { }

boost::filesystem::path HelpFormatter::getPathToFrameworkDataFile_(const std::string& filename) {
    return Configurator::getInstance().isRunAsInstalled()
        ? Configurator::getInstance().getDataDir().string() + "/framework/" + filename
        : "../framework/data/" + filename;
}

std::string HelpFormatter::getProcessorDescription(std::string processorName) {
    return MainFactoriesKeeper::getInstance().getProcessorFactory(processorName).getDescription();
}

std::string HelpFormatter::getProcessorDetailedDescription(std::string processorName) {
    return MainFactoriesKeeper::getInstance().getProcessorFactory(processorName)
        .getDetailedDescription();
}

boost::program_options::options_description HelpFormatter::getProcessorOptions(
    std::string processorName) {
    return MainFactoriesKeeper::getInstance().getProcessorFactory(processorName).optionsHandled();
}

bool HelpFormatter::areOptionsEmpty(boost::program_options::options_description options) {
    std::stringstream optionsAsStream;
    optionsAsStream << options;

    return optionsAsStream.str() == (ProcessorFactory::OPTION_LABEL + ":\n");
}

std::vector<TestBatch> HelpFormatter::getProcessorUsingExamples(std::string processorName) {
    boost::filesystem::path processorFile =
        MainFactoriesKeeper::getInstance().getProcessorFactory(processorName).getFile();

    std::vector<boost::filesystem::path> directories;
    directories.push_back(getItsData(processorFile));

    testExtractor_.clearTestBatches();
    testExtractor_.lookForTestBatches(directories, "help");

    return testExtractor_.getTestBatches();
}

std::string HelpFormatter::getFileContent(const boost::filesystem::path& path) {
    std::stringstream content;
    content << std::ifstream( path.string().c_str() ).rdbuf();

    return content.str();
}

std::list<std::string> HelpFormatter::getProcessorNamesForAlias(std::string alias) {
    std::list<ProcessorFactory*> processors =
        MainFactoriesKeeper::getInstance().getProcessorFactoriesForName(alias);

    std::list<std::string> processorNames;
    BOOST_FOREACH(ProcessorFactory* processor, processors) {
        processorNames.push_front(processor->getName());
    }

    processorNames.sort();
    return processorNames;
}

std::list<std::string> HelpFormatter::getAliasesForProcessorName(std::string processorName) {
    std::set<std::string> aliases =
        MainFactoriesKeeper::getInstance().getAllAliases(processorName);
    return std::list<std::string>(aliases.begin(), aliases.end());
}

const std::string HelpFormatter::EXAMPLES_HEADER = "Examples";
const std::string HelpFormatter::OPTIONS_HEADER = "Options";
const std::string HelpFormatter::ALIASES_HEADER = "Aliases";
const std::string HelpFormatter::LANGUAGES_HEADER = "Languages";

std::list<std::string> HelpFormatter::getLanguagesHandledForProcessor(
    std::string processorName) {

    ProcessorFactory* processorFactory
        = &MainFactoriesKeeper::getInstance().getProcessorFactory(processorName);

    AnnotatorFactory* annotatorFactory = dynamic_cast<AnnotatorFactory*>(processorFactory);

    if (annotatorFactory != NULL) {
        boost::program_options::variables_map fakeOptions;
        return annotatorFactory->languagesHandled(fakeOptions);
    }

    return std::list<std::string>();
}

std::string HelpFormatter::getProcessorNameWithoutOptions(const std::string& name) {
    std::string trimmedName = boost::algorithm::trim_left_copy(name);
    size_t pos = trimmedName.find_first_of(' ');

    if (pos != std::string::npos) {
        return trimmedName.substr(0, pos);
    }

    return trimmedName;
}

std::vector<std::string> HelpFormatter::getProcessorNames() {
    return MainFactoriesKeeper::getInstance().getProcessorNames();
}

std::string HelpFormatter::getProcessorFullName(const std::string& processorName) {
    std::string fullName = "";

    if (MainFactoriesKeeper::getInstance().hasProcessorFactory(processorName)) {
        ProcessorFactory* processorFactory =
            &MainFactoriesKeeper::getInstance().getProcessorFactory(processorName);

        fullName = processorFactory->getType() + " > ";
        if (processorFactory->isSubTyped()) {
            fullName += processorFactory->getSubType() + " > ";
        }
        fullName += processorFactory->getName();
    }

    return fullName;
}
