#ifndef LATTICE_HDR
#define LATTICE_HDR


#include <string>
#include <list>
#include <map>
#include <vector>

#include <boost/dynamic_bitset.hpp>

#include <boost/graph/adjacency_list.hpp>

#include "utf8.h"

#include "annotation_item.hpp"
#include "exceptions.hpp"
#include "hash_wrapper.hpp"
#include "layer_tag_manager.hpp"

#include "cutter.hpp"

/*!
  Lattice is used to keep all the information extracted by annotators
  (processors). Language units (tokens, words, phrases etc.)
  recognised by annotators are represented by edges (whereas vertices
  represent points between characters). Each edge is assigned an annotation
  item which is composed of a category and a attribute-value matrix (AVM).
  Each edge has tags which can be used to distinguish various levels, annotators etc.

  Lattice lifecycle:
  - construction, edge is created for each character (optimised!) [tag: raw]
  - formatter, extra edges for mark-ups and entity-likes (one edge "<"
    spanning "&lt;"), [text, mark-ups]
  - splitter - working on text level, Translatica splitter with
    function, get_text (....) [sentence]
  - tokeniser - working on text level (inside sentence?), get_edge_text [token]
  - lemmatiser - working on token level [lemma]
  - corrector - working on text level (if token without lemma),
  - parser - working ok token/lemma level, taking into account blanks
  - chooser - working on parser level
*/


class Lattice {

public:

    struct VertexEntry;
    struct EdgeEntry;

    typedef boost::adjacency_list<
        boost::vecS,
        boost::vecS,
        boost::bidirectionalS,
        VertexEntry,
        EdgeEntry
    > Graph;

    struct EdgeDescriptor;


    struct EdgeDescriptorWrapperToFoolBoost146OrGnu461;

    struct VertexEntry {
        int index;
        std::vector< std::list<EdgeDescriptorWrapperToFoolBoost146OrGnu461> > outEdgesIndex;
        std::vector< std::list<EdgeDescriptorWrapperToFoolBoost146OrGnu461> > inEdgesIndex;

        VertexEntry() : index(-1) { }
        VertexEntry(int ix) : index(ix) { }
    };

    typedef double Score;

    struct Partition;

    struct EdgeEntry {
        AnnotationItem category;
        LayerTagCollection tagList;
        Score score;
        std::list<Partition> partitions;

        EdgeEntry(
            AnnotationItem aCategory,
            LayerTagCollection aTagList,
            Score aScore,
            Partition& aPartition
        ): category(aCategory), tagList(aTagList), score(aScore) { 
            partitions.push_back(aPartition);
        }
    };

    struct Partition {
        std::vector<EdgeDescriptor> links;
    };

    struct EdgeDescriptor {
        Graph::edge_descriptor descriptor;
        int implicitIndex;

        EdgeDescriptor() : descriptor(), implicitIndex(-1) { }
        EdgeDescriptor(int implicitIx) : descriptor(), implicitIndex(implicitIx) { }
        EdgeDescriptor(const Graph::edge_descriptor& ed) : descriptor(ed), implicitIndex(-1) { }
    };

    struct EdgeDescriptorWrapperToFoolBoost146OrGnu461 : public Graph::edge_descriptor {
    public:
        EdgeDescriptorWrapperToFoolBoost146OrGnu461() {}
        EdgeDescriptorWrapperToFoolBoost146OrGnu461(const Graph::edge_descriptor& ed)
            :Graph::edge_descriptor(ed) {}
    };

    typedef int VertexDescriptor;
    typedef Graph::edge_iterator EdgeIterator;
    typedef Graph::out_edge_iterator OutEdgeIterator;
    typedef Graph::in_edge_iterator InEdgeIterator;

    class VertexIterator {
    public:
        VertexIterator(Lattice * lattice) : lattice_(lattice), vd_(0) { }
        bool hasNext();
        VertexDescriptor next();
    private:
        Lattice * lattice_;
        VertexDescriptor vd_;
    };

    class InOutEdgesIterator {
    public:
        InOutEdgesIterator(
            std::list<EdgeDescriptorWrapperToFoolBoost146OrGnu461>::const_iterator begin,
            std::list<EdgeDescriptorWrapperToFoolBoost146OrGnu461>::const_iterator end,
            int implicitIndex = -1
        ) :
            type_(EDGE_DESCRIPTOR_ITER),
            edi_(begin),
            ediEnd_(end),
            implicitIndex_(implicitIndex)
        { }

        InOutEdgesIterator(
            std::pair<OutEdgeIterator, OutEdgeIterator> ir,
            int implicitIndex = -1
        ) :
            type_(OUT_EDGE_ITER),
            oei_(ir.first),
            oeiEnd_(ir.second),
            implicitIndex_(implicitIndex)
        { }

        InOutEdgesIterator(
            std::pair<InEdgeIterator, InEdgeIterator> ir,
            int implicitIndex = -1
        ) :
            type_(IN_EDGE_ITER),
            iei_(ir.first),
            ieiEnd_(ir.second),
            implicitIndex_(implicitIndex)
        { }

        bool hasNext();
        EdgeDescriptor next();
    private:
        enum {EDGE_DESCRIPTOR_ITER, OUT_EDGE_ITER, IN_EDGE_ITER} type_;
        std::list<EdgeDescriptorWrapperToFoolBoost146OrGnu461>::const_iterator edi_;
        std::list<EdgeDescriptorWrapperToFoolBoost146OrGnu461>::const_iterator ediEnd_;
        OutEdgeIterator oei_;
        OutEdgeIterator oeiEnd_;
        InEdgeIterator iei_;
        InEdgeIterator ieiEnd_;
        int implicitIndex_;
    };

    class SortedEdgesIterator {
    public:
        SortedEdgesIterator(Lattice * lattice, LayerTagMask mask);
        bool hasNext();
        EdgeDescriptor next();
    private:
        Lattice * lattice_;
        LayerTagMask mask_;
        VertexIterator vi_;
        InOutEdgesIterator ei_;
    };

    Lattice();

    /**
     * Creates a lattice from `text`. Initially each character of text will be
     * represented as an edge labeled with layer tag `raw` and category `c`
     * (where c is the given character)
     */
    Lattice(std::string text);

    ~Lattice();

    void appendString(std::string text);

    void addSymbols(VertexDescriptor startVertex, VertexDescriptor endVertex);

    void appendStringWithSymbols(std::string text);

    /**
     * Gets the vertex for ix-th character of text
     */
    VertexDescriptor getVertexForRawCharIndex(int ix);

    /**
     * Gets the first vertex (the same as getVertexForRawCharIndex(0))
     */
    VertexDescriptor getFirstVertex();

     /**
     * Gets the last vertex
     */
    VertexDescriptor getLastVertex();

    /**
     * Adds an edge from vertex `from` to vertex `to` with `annonation_item`
     * and `tags` as layer tags. The partition of the edge into subedges
     * is given with `partition` argument.
     *
     * A new will not be added if there is already an edge from vertex `from`
     * to vertex `to` of annotation `annotation_item` (tags and partitions will
     * be updated in such a case).
     */
    EdgeDescriptor addEdge(VertexDescriptor from,
                           VertexDescriptor to,
                           const AnnotationItem& annotationItem,
                           LayerTagCollection tags,
                           Score score,
                           Partition partition = Partition());

    // return outgoing edges which has at least one layer tag from `mask`
    InOutEdgesIterator outEdges(
        VertexDescriptor vertex,
        LayerTagMask mask
    );

    InOutEdgesIterator inEdges(
        VertexDescriptor vertex,
        LayerTagMask mask
    );

    InOutEdgesIterator allOutEdges(VertexDescriptor vertex);
    InOutEdgesIterator allInEdges(VertexDescriptor vertex);

    EdgeDescriptor firstOutEdge(VertexDescriptor vertex, LayerTagMask mask);
    EdgeDescriptor firstInEdge(VertexDescriptor vertex, LayerTagMask mask);


    // returns the list of edges which have at least one layer tag from `mask` sorted
    SortedEdgesIterator edgesSorted(LayerTagMask mask);

    SortedEdgesIterator allEdgesSorted();

    LayerTagManager& getLayerTagManager();

    const AnnotationItem getEdgeAnnotationItem(EdgeDescriptor edge);
    const LayerTagCollection getEdgeLayerTags(EdgeDescriptor edge);

    const std::string& getAllText() const;

    void runCutter(Cutter& cutter, LayerTagMask mask);

private:

    Graph graph_;

    LayerTagManager layerTagManager_;

    std::string allText_;

    boost::dynamic_bitset<> implicitOutEdges_;

    std::map<int, Graph::vertex_descriptor> vertices_;

    typedef boost::bimap<LayerTagCollection,int> TagCollectionsBimap;
    typedef TagCollectionsBimap::value_type TagCollectionsBimapItem;
    typedef TagCollectionsBimap::left_map::const_iterator TagCollectionsBimapLeftIterator;
    TagCollectionsBimap indexedTagCollections_;

    void init_();

    int addTagCollectionIndex_(LayerTagCollection tags);

    VertexDescriptor priorVertex_(VertexDescriptor vertex);


    struct HashFun {
        HASH_WRAPPER_EXTRA_STUFF

        unsigned int operator()(
            const std::pair<
                std::pair<VertexDescriptor, VertexDescriptor>,
                std::pair<AnnotationItem, LayerTagCollection>
            >& k
        ) const {
#ifdef __VS__
            return HASH_WRAPPER_FULL_HASH_TRAITS<int>().operator()(int(k.first.first))
                ^ HASH_WRAPPER_FULL_HASH_TRAITS<int>().operator()(int(k.first.second))
                ^ HASH_WRAPPER_FULL_HASH_TRAITS<long>().operator()(
                    k.second.first.getHash())
                ^ HASH_WRAPPER_FULL_HASH_TRAITS<unsigned long>().operator()(
                    k.second.second.getHash());
#else
            return (int(k.first.first) << 8)
                ^ int(k.first.second)
                ^ (int(k.second.first.getHash()) << 16)
                ^ (int(k.second.second.getHash()) << 24);
#endif
        }

#ifdef __VS__
        bool operator()(
            const std::pair<
                std::pair<VertexDescriptor, VertexDescriptor>,
                std::pair<AnnotationItem, LayerTagCollection>
            >& a,
            const std::pair<
                std::pair<VertexDescriptor, VertexDescriptor>,
                std::pair<AnnotationItem, LayerTagCollection>
            >& b
        ) const {
            return a != b;
        }
#endif
    };


    struct VertexPairHashFun {
        HASH_WRAPPER_EXTRA_STUFF

        unsigned int operator()(
            const std::pair<VertexDescriptor, VertexDescriptor>& k
        ) const {
#ifdef __VS__
            return HASH_WRAPPER_FULL_HASH_TRAITS<int>().operator()(int(k.first))
                ^ HASH_WRAPPER_FULL_HASH_TRAITS<int>().operator()(int(k.second));
#else
            return (int(k.first) << 8) ^ int(k.second);
#endif
        }

#ifdef __VS__
        bool operator()(
            const std::pair<VertexDescriptor, VertexDescriptor>& a,
            const std::pair<VertexDescriptor, VertexDescriptor>& b
        ) const {
            return a != b;
        }
#endif
    };


    typedef HashWrapper3<
        std::pair<
            std::pair<VertexDescriptor, VertexDescriptor>,
            std::pair<AnnotationItem, LayerTagCollection>
        >,
        EdgeDescriptor,
        HashFun
    >::type VVCHash;

    typedef HashWrapper3<
        std::pair<VertexDescriptor, VertexDescriptor>,
        int,
        VertexPairHashFun
    >::type EdgeCounterHash;

    VVCHash vvcHash_;
    EdgeCounterHash edgeCounterHash_;

};


#endif
