#ifndef CHART_HPP_HDR
#define CHART_HPP_HDR


#include <boost/optional.hpp>

#include "av_ai_converter.hpp"
#include "lattice.hpp"
#include "limit_checker.hpp"


template<typename E, typename C>
struct empty_marked_edges_index
{
public:
    void mark_out_edge(const E&, const C&) {}
    void mark_in_edge (const E&, const C&) {}
};

template<typename E, typename C>
struct simple_marked_edges_index
{
private:
    std::vector<E> marked_out_edges_;
    std::vector<E> marked_in_edges_;

public:
    void mark_out_edge(const E& e, const C&) { marked_out_edges_.push_back(e); }
    void mark_in_edge (const E& e, const C&) { marked_in_edges_.push_back(e);  }

    typedef typename std::vector<E>::iterator marked_out_edge_iterator;
    typedef typename std::vector<E>::iterator marked_in_edge_iterator;

    std::pair<marked_out_edge_iterator, marked_out_edge_iterator>
    marked_out_edges()
    {
        return std::pair<marked_out_edge_iterator, marked_out_edge_iterator>(
        marked_out_edges_.begin(), marked_out_edges_.end());
    }

    std::pair<marked_in_edge_iterator, marked_in_edge_iterator>
    marked_in_edges()
    {
        return std::pair<marked_in_edge_iterator, marked_in_edge_iterator>(
        marked_in_edges_.begin(), marked_in_edges_.end());
    }
};

// I - indeks przy kazdym wierzcholku
template<typename C, typename S, typename V, typename R,
    template<typename, typename> class I = simple_marked_edges_index>
class chart
{
public:
    typedef typename Lattice::VertexDescriptor vertex_descriptor;
    typedef typename Lattice::EdgeDescriptor edge_descriptor;

    typedef typename Lattice::VertexIterator vertex_iterator;

    typedef typename Lattice::InOutEdgesIterator out_edge_iterator;
    typedef typename Lattice::InOutEdgesIterator in_edge_iterator;

    typedef typename std::list<Lattice::Partition>::iterator partition_iterator;
    // typedef typename std::string::iterator variant_iterator;

    typedef C category_type;
    typedef V variant_category_type;
    typedef S score_type;
    typedef R rule_type;
    typedef typename std::string variant_type;
    typedef typename std::string variant;

    typedef I<edge_descriptor, category_type> marked_edges_index_type;

    chart(
        Lattice & lattice,
        AV_AI_Converter & av_ai_converter,
        const std::string& terminalTag,
        LimitChecker & limitChecker);

    vertex_descriptor add_vertex();

    std::pair<edge_descriptor, bool>   add_edge(
    vertex_descriptor u,
    vertex_descriptor v,
    const category_type& category,
    score_type score,
    rule_type rule);

    std::pair<edge_descriptor, bool>   add_edge(
    vertex_descriptor u,
    vertex_descriptor v,
    const category_type& category,
    score_type score,
    rule_type rule,
    edge_descriptor link);

    std::pair<edge_descriptor, bool>   add_edge(
    vertex_descriptor u,
    vertex_descriptor v,
    const category_type& category,
    score_type score,
    rule_type rule,
    edge_descriptor  left_link,
    edge_descriptor right_link);

    /**
     * Uzywac ostroznie lub wcale - moze powodowac segfault w accomodaterze!
     */
    void remove_edge(
    edge_descriptor e);

    int nb_edges(
    vertex_descriptor u,
    vertex_descriptor v);

    edge_descriptor   add_partition(
    edge_descriptor edge,
    score_type score,
    rule_type rule);

    edge_descriptor   add_partition(
    edge_descriptor edge,
    score_type score,
    rule_type rule,
    edge_descriptor link);

    edge_descriptor   add_partition(
    edge_descriptor edge,
    score_type score,
    rule_type rule,
    edge_descriptor left_link,
    edge_descriptor right_link);

    edge_descriptor   mark_edge(
    edge_descriptor edge);

    vertex_iterator vertices();
    size_t nb_vertices() const;

    out_edge_iterator out_edges(vertex_descriptor vertex);
    in_edge_iterator in_edges(vertex_descriptor vertex);

    marked_edges_index_type& marked_edges_index(vertex_descriptor vertex);

    vertex_descriptor edge_source(edge_descriptor edge, bool skip_blank = false);
    vertex_descriptor edge_target(edge_descriptor edge, bool skip_blank = false);
    category_type edge_category(edge_descriptor edge);
    score_type edge_score(edge_descriptor edge) const;
    // bool edge_accommodated(edge_descriptor edge) const;
    // void mark_edge_as_accommodated(edge_descriptor edge);

    /**
     * Eeturn terminal edge that is an ancestor of the given edge.
     */
    boost::optional<edge_descriptor> edge_terminal_origin(edge_descriptor edge);

    bool could_be_final(edge_descriptor edge) const;

    std::pair<partition_iterator, partition_iterator> edge_partitions(edge_descriptor edge);
    // std::pair<variant_iterator, variant_iterator> edge_variants(edge_descriptor edge);
    // void remove_duplicate_variants(edge_descriptor edge);
    // void sort_variants(edge_descriptor edge, size_t limit);

    // void add_variant(
    // edge_descriptor edge,
    // partition_iterator piter,
    // const V& new_variant,
    // score_type score);

    // void add_variant(
    // edge_descriptor edge,
    // partition_iterator piter,
    // const V& new_variant,
    // score_type score,
        // variant_iterator    link_vit);

    // void add_variant(
    // edge_descriptor edge,
    // partition_iterator piter,
    // const V& new_variant,
    // score_type score,
        // variant_iterator    left_link_vit,
        // variant_iterator    right_link_vit);


    // rule_type partition_rule(partition_iterator piter);
    int partition_rule_id(partition_iterator piter);
    int partition_tree_choice(partition_iterator piter);
    // std::vector<edge_descriptor>& partition_links(partition_iterator piter);
    edge_descriptor partition_link(partition_iterator piter, size_t index);
    size_t partition_links_size(partition_iterator piter);
    Lattice::Partition::Iterator partition_links_iterator(partition_iterator piter);
    // partition_iterator variant_partition(variant_iterator vit);
    // variant_category_type edge_variant_category(variant_iterator vit);
    // score_type variant_score(variant_iterator vit);
    // std::vector<variant_iterator>& variant_links(variant_iterator vit);

    void sort_topologically();
    size_t topological_index(vertex_descriptor vertex) const;
    vertex_descriptor vertex_by_index(size_t i) const;

    // zwraca rozmiar wektora wierzcholkow
    size_t topological_count() const;

    vertex_descriptor getFirstVertex() const;
    vertex_descriptor getLastVertex() const;

    typedef Lattice::EdgeEntry edge_entry;


private:

    typedef Lattice::VertexEntry vertex_entry;
    typedef Lattice::Partition partition;

    // typename Lattice::Graph graph_;

    Lattice & lattice_;
    AV_AI_Converter av_ai_converter_;

    LayerTagCollection outputTags_;
    LayerTagMask inputTagMask_;

    LayerTagCollection formTag_;
    LayerTagCollection lexemeTag_;
    LayerTagCollection lemmaTag_;
    LayerTagCollection gobioTag_;
    LayerTagMask toBePutInApostrophesTagMask_;

    const LayerTagCollection& getChartOutputTags_() const;
    const LayerTagMask& getChartInputTagMask_() const;

    typedef Lattice::HashFun hash_fun;
    typedef Lattice::VertexPairHashFun vertex_pair_hash_fun;

    LimitChecker & limitChecker_;

    std::string terminalTag_;

public:
    struct vertex_hash_fun
    {
    HASH_WRAPPER_EXTRA_STUFF

    unsigned int operator()(const vertex_descriptor& k) const
        {
        return (unsigned int)k;
        }

#ifdef __VS__
    bool operator()(const vertex_descriptor& a,
            const vertex_descriptor& b) const
        {
        return a != b;
        }
#endif

    };

    /*
    struct edge_hash_fun
    {
    HASH_WRAPPER_EXTRA_STUFF

    chart* chart_;

    edge_hash_fun(chart* chart):chart_(chart) { }

    unsigned int operator()(const edge_descriptor& k) const
        {
#ifdef __VS__
        return
          HASH_WRAPPER_FULL_HASH_TRAITS<unsigned int>().operator()(
              unsigned int(chart_->edge_source(k)))
          ^ HASH_WRAPPER_FULL_HASH_TRAITS<unsigned int>().operator()(
              unsigned int(chart_->edge_target(k)));
#else
        return (unsigned int)(chart_->edge_source(k))
            ^ (unsigned int)(chart_->edge_target(k)) ;
#endif
        }

#ifdef __VS__
    bool operator()(const edge_descriptor& a,
            const edge_descriptor& b) const
        {
        return a != b;
        }
#endif

    };
    */

private:
    // Lattice::VVCHash vvc_hash_;

    // typedef Lattice::VVCHash::iterator vvc_hash_iterator_type_;


    // Lattice::EdgeCounterHash edge_counter_hash_;


    typename HashWrapper3<vertex_descriptor, size_t, vertex_hash_fun>::type topological_order_;
    std::vector<vertex_descriptor> sorted_vertices_;

    typename HashWrapper3<
        vertex_descriptor,
        marked_edges_index_type,
        vertex_hash_fun
    >::type markedEdgesIndexes_;

};

#endif
