#ifndef options_hpp
#define options_hpp

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <string>
#include <limits.h>
#include <boost/format.hpp>
#include "gadgets.hpp"
#include <Eigen/Eigen>
#include "common.h"

using namespace std;
using namespace boost;

const unsigned Megabase = 1e6;

class Options {
public:
    unsigned chainLength;
    unsigned burnin;
    unsigned seed;
    unsigned numThread;
    int numThreadSpawned = 0; // Default to 0, let TBB do its thing
    unsigned preprocessChunks = 1;
    unsigned thin;  // save every this th sampled value in MCMC
    vector<float> S;    //variance components

    unsigned int numGroups;
    Eigen::MatrixXd mS;
    string groupFile;

    string title;
    string analysisType;
    string bayesType;
    string phenotypeFile;
    string bedFile;
    string mcmcSampleFile;
    string optionFile;
    bool compress = false;
    DataType dataType = DataType::Dense;
    bool preload = false;

    Options(){
        chainLength             = 10000;
        burnin                  = 5000;
        seed                    = static_cast<unsigned int>(std::time(0));
        numThread               = 1;
        numThreadSpawned        = 0;
        preprocessChunks        = 1;
        thin                    = 5;

        S.resize(3);
        S[0]                    = 0.01;
        S[1]                    = 0.001;
        S[2]                    = 0.0001;

        title                   = "brr";
        analysisType            = "Bayes";
        bayesType               = "C";
        phenotypeFile           = "";
        bedFile                 = "";
        mcmcSampleFile          = "bayesOutput.csv";
        optionFile				= "";
        numGroups				=2;
        dataType                = DataType::Dense;
        preload                 = false;
    }

    void inputOptions(const int argc, const char* argv[]);

private:
    void readFile(const string &file);
    void makeTitle(void);
    void seedEngine(void);
};

#endif /* options_hpp */
