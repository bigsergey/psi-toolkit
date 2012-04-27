#ifndef PROCESSOR_PROMISE_HDR
#define PROCESSOR_PROMISE_HDR

#include "annotator_factory.hpp"

class ProcessorPromise {
public:
    virtual ~ProcessorPromise() {
    }

    Processor* createProcessor();

    std::list<std::list<std::string> > requiredLayerTags();

    std::list<std::list<std::string> > optionalLayerTags();

    std::list<std::string> providedLayerTags();

    AnnotatorFactory::LanguagesHandling languagesHandling() const;

    std::list<std::string> languagesHandled() const;

    bool isAnnotator() const;
    bool isReader() const;
    bool isWriter() const;

private:
    virtual Processor* doCreateProcessor() = 0;

    virtual std::list<std::list<std::string> > doRequiredLayerTags() = 0;

    virtual std::list<std::list<std::string> > doOptionalLayerTags() = 0;

    virtual std::list<std::string> doProvidedLayerTags() = 0;

    virtual AnnotatorFactory::LanguagesHandling doLanguagesHandling() const = 0;

    virtual std::list<std::string> doLanguagesHandled() const = 0;

    virtual bool doIsAnnotator() const = 0;
    virtual bool doIsReader() const = 0;
    virtual bool doIsWriter() const = 0;

};

#endif
