#ifndef GRAPHVIZ_ADAPTER_IMPL_HDR
#define GRAPHVIZ_ADAPTER_IMPL_HDR

#include "graphviz_adapter_interface.hpp"

#include <map>

#include "config.hpp"

// Fixes warnings "... not defined" from GraphViz include files

#ifndef _BLD_cdt
#define _BLD_cdt 0
#endif

#ifndef _DLL_BLD
#define _DLL_BLD 0
#endif

#ifndef _dll_import
#define _dll_import 0
#endif

#ifndef _PACKAGE_ast
#define _PACKAGE_ast 0
#endif

#if GRAPHVIZ_CGRAPH
#define WITH_CGRAPH
#endif

#include <gvc.h>


class GraphvizAdapterImpl : public GraphvizAdapterInterface {
public:
    GraphvizAdapterImpl();

    virtual ~GraphvizAdapterImpl();

    virtual void init();
    virtual void init(std::string arg0, std::string arg1, std::string arg2);

    virtual void finalize();

    virtual void setRankDir(std::string dir);
    virtual void setOrdering(std::string ordering);

    virtual int addNode(std::string id);
    virtual void setNodeLabel(int node, std::string label);
    virtual void setNodeColor(int node, std::string color);
    virtual void setNodeStyle(int node, std::string style);

    virtual int addEdge(int source, int target, std::string name = std::string(""));
    virtual void setEdgeLabel(int node, std::string label);
    virtual void setEdgeColor(int node, std::string color);
    virtual void setEdgeStyle(int node, std::string style);

private:
    GVC_t * gvc_;
    Agraph_t * g_;

    int nCount_;
    int eCount_;

    std::map<int, Agnode_t*> nodes_;
    std::map<int, Agedge_t*> edges_;

    void init_context_();
    void init_graph_();

    void finalize_context_();
    void finalize_graph_();

};


#endif
