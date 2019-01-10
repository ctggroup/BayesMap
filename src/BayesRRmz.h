/*
 * BayesRRm.h
 *
 *  Created on: 5 Sep 2018
 *      Author: admin
 */

#ifndef SRC_BAYESRRMZ_H_
#define SRC_BAYESRRMZ_H_

#include "data.hpp"
#include "options.hpp"
#include "distributions_boost.hpp"

#include <Eigen/Eigen>
#include <memory>

class LimitSequenceGraph;

class BayesRRmz
{
    friend class LimitSequenceGraph;
    std::unique_ptr<LimitSequenceGraph> flowGraph;
    Data            &data; // data matrices
    Options         &opt;
    const string    bedFile; // bed file
    const string    outputFile;
    const unsigned int seed;
    const unsigned int max_iterations;
    const unsigned int burn_in;
    const unsigned int thinning;
    const double	sigma0  = 0.0001;
    const double	v0E     = 0.0001;
    const double    s02E    = 0.0001;
    const double    v0G     = 0.0001;
    const double    s02G    = 0.0001;
    Eigen::VectorXd cva;
    Distributions_boost dist;
    bool usePreprocessedData;
    bool showDebug;

    // Component variables
    VectorXd priorPi;   // prior probabilities for each component
    VectorXd pi;        // mixture probabilities
    VectorXd cVa;       // component-specific variance
    VectorXd logL;      // log likelihood of component
    VectorXd muk;       // mean of k-th component marker effect size
    VectorXd denom;     // temporal variable for computing the inflation of the effect variance for a given non-zero componnet
    int m0;             // total num ber of markes in model
    VectorXd v;         // variable storing the component assignment
    VectorXd cVaI;      // inverse of the component variances

    // Mean and residual variables
    double mu;          // mean or intercept
    double sigmaG;      // genetic variance
    double sigmaE;      // residuals variance

    // Linear model variables
    VectorXd beta;       // effect sizes
    VectorXd y_tilde;    // variable containing the adjusted residuals to exclude the effects of a given marker
    VectorXd epsilon;    // variable containing the residuals
    double betasqn=0;

    VectorXd y;
    VectorXd components;



public:
    BayesRRmz(Data &data, Options &opt);
    virtual ~BayesRRmz();
    int runGibbs(); // where we run Gibbs sampling over the parametrised model
    void processColumn(unsigned int marker, const Map<VectorXd> &Cx);

    void setDebugEnabled(bool enabled) { showDebug = enabled; }
    bool isDebugEnabled() const { return showDebug; }

private:
    void init(int K, unsigned int markerCount, unsigned int individualCount);
    void printDebugInfo() const;
};

#endif /* SRC_BAYESRRM_H_ */
