#ifndef PLUGIN_MANAGER_HDR
#define PLUGIN_MANAGER_HDR

#include <map>
#include <boost/shared_ptr.hpp>

#include "abstract_plugin.hpp"
#include "plugin.hpp"
#include "plugin_adapter.hpp"
#include "psi_exception.hpp"

class PluginManager {
public:
    // singleton pattern used here
    static PluginManager& getInstance();

    PluginAdapter * createPluginAdapter(const std::string & pluginName);
    void destroyPluginAdapter(const std::string & pluginName, PluginAdapter * adapterPointer);

    bool checkPluginRequirements(const std::string & pluginName,
                                 const boost::program_options::variables_map& options,
                                 std::ostream & message);

    ~PluginManager();

    class UnknownPluginException : public PsiException {
    public:
        UnknownPluginException(const std::string& pluginName);

        virtual ~UnknownPluginException() throw() {}
    };

private:
    PluginManager();

    PluginManager(const PluginManager &);
    PluginManager& operator=(const PluginManager&);

    void registerPlugin_(AbstractPlugin* pluginPointer);
    bool checkIsPluginActiveWithMessage_(const std::string & pluginName);
    bool checkIsPluginActive_(const std::string & pluginName, bool withMessage = false);
    AbstractPlugin& getPlugin_(const std::string & pluginName);

    std::map<std::string, boost::shared_ptr<AbstractPlugin> > nameToPluginMap_;
};

#endif
