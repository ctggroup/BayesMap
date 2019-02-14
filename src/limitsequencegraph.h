#ifndef LIMITSEQUENCEGRAPH_H
#define LIMITSEQUENCEGRAPH_H

#include "analysisgraph.h"

#include "tbb/flow_graph.h"
#include <functional>
#include <memory>

class BayesRRmz;

using namespace tbb::flow;

class LimitSequenceGraph : public AnalysisGraph
{
public:
    LimitSequenceGraph(BayesRRmz *bayes, size_t maxParallel = 12);

    void exec(unsigned int numKeptInds,
              unsigned int numIncdSnps,
              const std::vector<unsigned int> &markerIndices) override;

private:
    struct Message {
        unsigned int id;
        unsigned int marker;
        unsigned int numKeptInds;
        unsigned char *decompressBuffer = nullptr;
    };

    std::unique_ptr<graph> m_graph;
    std::unique_ptr<function_node<Message, Message>> m_decompressNode;
    std::unique_ptr<limiter_node<Message>> m_limit;
    std::unique_ptr<sequencer_node<Message>> m_ordering;
    std::unique_ptr<sequencer_node<Message>> m_ordering2;
    std::unique_ptr<function_node<Message>> m_samplingNode;
};

#endif // LIMITSEQUENCEGRAPH_H
