#include "lattice.hpp"

Lattice::Lattice(std::string text) : layerTagManager_() {
    Lattice::VertexDescriptor vertex = boost::add_vertex(graph_);
    vertices_.push_back(vertex);
    for (int i = 0; i < text.length(); ++i) {
        Lattice::VertexDescriptor vertex = boost::add_vertex(graph_);
        boost::add_edge(
            vertices_.back(), 
            vertex, 
            EdgeEntry(
                AnnotationItem(text.substr(i,1)), 
                layerTagManager_.createSingletonTagCollection("raw")
            ), 
            graph_
        );
        vertices_.push_back(vertex);
    }
}

Lattice::~Lattice() {
    //TODO
}

Lattice::VertexDescriptor Lattice::getVertexForRawCharIndex(int ix) {
    return vertices_[ix];
}

Lattice::VertexDescriptor Lattice::getFirstVertex() {
    return vertices_.front();
}

Lattice::VertexDescriptor Lattice::getLastVertex() {
    return vertices_.back();
}

Lattice::EdgeDescriptor Lattice::addEdge(
    VertexDescriptor from,
    VertexDescriptor to,
    const AnnotationItem& annotationItem,
    LayerTagCollection tags,
    std::list<EdgeDescriptor> partition
) {
    std::pair<EdgeDescriptor, bool> result = boost::add_edge(
        from, 
        to, 
        EdgeEntry(annotationItem, tags, partition), 
        graph_
    );
    if (result.second) {
        for (int i = 0; i < indexedTagCollections_.size(); ++i) {
            if (createIntersection(tags, indexedTagCollections_.right.at(i)).isNonempty()) {
                graph_[from].outEdgesIndex[i].push_back(result.first);
                graph_[to].inEdgesIndex[i].push_back(result.first);
            }
        }
        return result.first;
    }
    throw NoEdgeException("No edge added.");
}

std::pair<Lattice::EdgeDescriptorIterator, Lattice::EdgeDescriptorIterator> Lattice::outEdges(Lattice::VertexDescriptor vertex, LayerTagCollection mask) {
    int ix = addTagCollectionIndex_(mask);
    return std::pair<Lattice::EdgeDescriptorIterator, Lattice::EdgeDescriptorIterator>(
        graph_[vertex].outEdgesIndex[ix].begin(),
        graph_[vertex].outEdgesIndex[ix].end()
    );
}

std::pair<Lattice::EdgeDescriptorIterator, Lattice::EdgeDescriptorIterator> Lattice::inEdges(Lattice::VertexDescriptor vertex, LayerTagCollection mask) {
    int ix = addTagCollectionIndex_(mask);
    return std::pair<Lattice::EdgeDescriptorIterator, Lattice::EdgeDescriptorIterator>(
        graph_[vertex].inEdgesIndex[ix].begin(),
        graph_[vertex].inEdgesIndex[ix].end()
    );
}

Lattice::EdgeDescriptor Lattice::firstOutEdge(Lattice::VertexDescriptor vertex, LayerTagCollection mask) {
    //TODO
    std::pair<Lattice::OutEdgeIterator, Lattice::OutEdgeIterator> allOutEdges = boost::out_edges(vertex, graph_);
    for (
        Lattice::OutEdgeIterator oei = allOutEdges.first;
        oei != allOutEdges.second;
        ++oei
    ) {
        if (createIntersection(graph_[*oei].tagList, mask).isNonempty()) {
            return *oei;
        }
    }
    throw NoEdgeException("No out-edges found.");
}

Lattice::EdgeDescriptor Lattice::firstInEdge(Lattice::VertexDescriptor vertex, LayerTagCollection mask) {
    //TODO
    std::pair<Lattice::InEdgeIterator, Lattice::InEdgeIterator> allInEdges = boost::in_edges(vertex, graph_);
    for (
        Lattice::InEdgeIterator iei = allInEdges.first;
        iei != allInEdges.second;
        ++iei
    ) {
        if (createIntersection(graph_[*iei].tagList, mask).isNonempty()) {
            return *iei;
        }
    }
    throw NoEdgeException("No in-edges found.");
}

std::list<Lattice::EdgeDescriptor> Lattice::edgesSorted(LayerTagCollection mask) {
    std::list<Lattice::EdgeDescriptor> result;
    for (
        std::vector<Lattice::VertexDescriptor>::iterator vi = vertices_.begin();
        vi != vertices_.end();
        ++vi
    ) {
        std::pair<Lattice::EdgeDescriptorIterator, Lattice::EdgeDescriptorIterator> outEdgesIters = outEdges(*vi, mask);
        result.insert(result.end(), outEdgesIters.first, outEdgesIters.second);
    }
    return result;
}

LayerTagManager& Lattice::getLayerTagManager() {
    return layerTagManager_;
}

const AnnotationItem& Lattice::getEdgeAnnotationItem(Lattice::EdgeDescriptor edge) {
    return graph_[edge].category;
}

const LayerTagCollection& Lattice::getEdgeLayerTags(Lattice::EdgeDescriptor edge) {
    return graph_[edge].tagList;
}

int Lattice::addTagCollectionIndex_(LayerTagCollection tags) {
    TagCollectionsBimapLeftIterator li = indexedTagCollections_.left.find(tags);
    if (li != indexedTagCollections_.left.end()) {
        return li->second;
    } else {
        int ix = indexedTagCollections_.size();
        for (
            std::vector<VertexDescriptor>::const_iterator vi = vertices_.begin();
            vi != vertices_.end();
            ++vi
        ) {
            graph_[*vi].outEdgesIndex.push_back(std::list<EdgeDescriptor>());
            std::pair<Lattice::OutEdgeIterator, Lattice::OutEdgeIterator> oeir = boost::out_edges(*vi, graph_);
            for (Lattice::OutEdgeIterator oei = oeir.first; oei != oeir.second; ++oei) {
                if (createIntersection(graph_[*oei].tagList, tags).isNonempty()) {
                    graph_[*vi].outEdgesIndex[ix].push_back(*oei);
                }
            }
            graph_[*vi].inEdgesIndex.push_back(std::list<EdgeDescriptor>());
            std::pair<Lattice::InEdgeIterator, Lattice::InEdgeIterator> ieir = boost::in_edges(*vi, graph_);
            for (Lattice::InEdgeIterator iei = ieir.first; iei != ieir.second; ++iei) {
                if (createIntersection(graph_[*iei].tagList, tags).isNonempty()) {
                    graph_[*vi].inEdgesIndex[ix].push_back(*iei);
                }
            }
        }
        indexedTagCollections_.insert(TagCollectionsBimapItem(tags, ix));
        return ix;
    }
}