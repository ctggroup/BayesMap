#include <iostream>
#include <string>
#include "BayesRRm.h"
#include "data.hpp"
#include "options.hpp"
#ifndef USE_MPI
#include "BayesRRmz.hpp"
#include "tbb/task_scheduler_init.h"
#endif

using namespace std;

int main(int argc, const char * argv[]) {


    cout << "***********************************************\n";
    cout << "* BayesRRcmd                                  *\n";
    cout << "* Complex Trait Genetics group UNIL           *\n";
    cout << "*                                             *\n";
    cout << "* MIT License                                 *\n";
    cout << "***********************************************\n";

    Gadget::Timer timer;
    timer.setTime();
    cout << "\nAnalysis started: " << timer.getDate();

    if (argc < 2){
        cerr << " \nDid you forget to give the input parameters?\n" << endl;
        exit(1);
    }

    try {
        Options opt;
        opt.inputOptions(argc, argv);

        Data data;

        // Read in the data for every possible option
        data.readFamFile(opt.bedFile + ".fam");
        data.readBimFile(opt.bedFile + ".bim");


#ifdef USE_MPI
        
        cout << "DETECTED USE_MPI FLAGS with opt.bayesType = " << opt.bayesType << " and opt.analysisType = " << opt.analysisType << endl;

        if (opt.bayesType == "bayesMPI" && opt.analysisType == "RAM") {
            cout << "Will launch MPI (RAM) GIBBS" << endl;

            // Read phenotype file and bed file for the option specified
            data.readPhenotypeFile(opt.phenotypeFile);
            data.readBedFile_noMPI(opt.bedFile+".bed");
            
            BayesRRm analysis(data, opt, sysconf(_SC_PAGE_SIZE));
            analysis.runMpiGibbs();

        } else {
            throw(" Error: Wrong analysis type: " + opt.analysisType);
        }

#else
        // RAM solution (analysisType = RAMBayes)
        if (opt.analysisType == "RAMBayes" && ( opt.bayesType == "bayes" || opt.bayesType == "bayesMmap" || opt.bayesType == "horseshoe")) {

            clock_t start = clock();

            // Read phenotype file and bed file for the option specified
            data.readPhenotypeFile(opt.phenotypeFile);
            data.readBedFile_noMPI(opt.bedFile+".bed");

            // Option bayesType="bayesMmap" is going to be deprecated
            if (opt.bayesType == "bayesMmap" || opt.bayesType == "bayes"){
                BayesRRm analysis(data, opt, sysconf(_SC_PAGE_SIZE));
                analysis.runGibbs();
            } else if (opt.bayesType == "horseshoe") {
                //TODO Finish horseshoe
            } else if (opt.bayesType == "bayesW") {
                //TODO Add BayesW
            } else if (opt.bayesType == "bayesG") {
                //TODO add Bayes groups
            }

            clock_t end   = clock();
            printf("OVERALL read+compute time = %.3f sec.\n", (float)(end - start) / CLOCKS_PER_SEC);
        }

        // Pre-processing the data (centering and scaling)
        else if (opt.analysisType == "Preprocess") {
            cout << "Start preprocessing " << opt.bedFile + ".bed" << endl;

            clock_t start_bed = clock();
            data.preprocessBedFile(opt.bedFile + ".bed",
                    opt.bedFile + ".ppbed",
                    opt.bedFile + ".ppbedindex",
                    opt.compress);

            clock_t end = clock();
            printf("Finished preprocessing the bed file in %.3f sec.\n", double(end - start_bed) / double(CLOCKS_PER_SEC));
            cout << endl;
        }else if (opt.analysisType == "PPBayes" || opt.analysisType == "PPAsyncBayes") {
            clock_t start = clock();
            data.readPhenotypeFile(opt.phenotypeFile);
            // Run analysis using mapped data files
            if (opt.compress) {
                cout << "Start reading preprocessed bed file: " << opt.bedFile + ".ppbed" << endl;
                clock_t start_bed = clock();
                data.mapCompressedPreprocessBedFile(opt.bedFile + ".ppbed",
                        opt.bedFile + ".ppbedindex");
                clock_t end = clock();
                printf("Finished reading preprocessed bed file in %.3f sec.\n", double(end - start_bed) / double(CLOCKS_PER_SEC));
                cout << endl;

                std::unique_ptr<tbb::task_scheduler_init> taskScheduler { nullptr };
                if (opt.numThreadSpawned > 0)
                    taskScheduler = std::make_unique<tbb::task_scheduler_init>(opt.numThreadSpawned);

                BayesRRmz analysis(data, opt);
                analysis.runGibbs();
                data.unmapCompressedPreprocessedBedFile();
            } else {
                cout << "Start reading preprocessed bed file: " << opt.bedFile + ".ppbed" << endl;
                clock_t start_bed = clock();
                data.mapPreprocessBedFile(opt.bedFile + ".ppbed");
                clock_t end = clock();
                printf("Finished reading preprocessed bed file in %.3f sec.\n", double(end - start_bed) / double(CLOCKS_PER_SEC));
                cout << endl;

                BayesRRm analysis(data, opt, sysconf(_SC_PAGE_SIZE));
                analysis.runGibbs();

                data.unmapPreprocessedBedFile();
                end = clock();
                printf("OVERALL read+compute time = %.3f sec.\n", double(end - start) / double(CLOCKS_PER_SEC));
            }
        }
        else {
            throw(" Error: Wrong analysis type: " + opt.analysisType);
        }

#endif

    }
        
    catch (const string &err_msg) {
        cerr << "\n" << err_msg << endl;
    }
    catch (const char *err_msg) {
        cerr << "\n" << err_msg << endl;
    }

    timer.getTime();

    cout << "\nAnalysis finished: " << timer.getDate();
    cout << "Computational time: "  << timer.format(timer.getElapse()) << endl;


    return 0;
}
