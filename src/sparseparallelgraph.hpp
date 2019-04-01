#ifndef SPARSEPARALLELGRAPH_H
#define SPARSEPARALLELGRAPH_H

#include "analysisgraph.hpp"

#include "tbb/flow_graph.h"
#include <functional>
#include <memory>

class SparseBayesRRG;

using namespace tbb::flow;

class SparseParallelGraph : public AnalysisGraph
{
public:
    SparseParallelGraph(SparseBayesRRG *bayes, size_t maxParallel = 6);


    void exec(unsigned int numInds,
              unsigned int numSnps,
              const std::vector<unsigned int> &markerIndices) override;

private:
    struct Message {
        Message(unsigned int id = 0, unsigned int marker = 0, unsigned int numInds = 0)
            : id(id)
            , marker(marker)
            , numInds(numInds)
        {

        }

        unsigned int id = 0;
        unsigned int marker = 0;
        unsigned int numInds = 0;

        double old_beta = 0.0;
        double beta = 0.0;
    };

    SparseBayesRRG *m_bayes = nullptr;
    std::unique_ptr<graph> m_graph;
    std::unique_ptr<function_node<Message, Message>> m_asyncComputeNode;
    std::unique_ptr<limiter_node<Message>> m_limit;
    std::unique_ptr<sequencer_node<Message>> m_ordering;

    using decision_node = multifunction_node<Message, tbb::flow::tuple<continue_msg, Message> >;
    std::unique_ptr<decision_node> m_decisionNode;
    std::unique_ptr<function_node<Message>> m_globalComputeNode;
};

#endif // SPARSEPARALLELGRAPH_H
