//
//  main.cpp
//  gctb
//
//  Created by Jian Zeng on 14/06/2016.
//  Copyright © 2016 Jian Zeng. All rights reserved.
//
// Modified by Daniel Trejo Banos for memory mapped filed implementation

#include <iostream>
#include "gctb.hpp"
#include "BayesRMmapToy.hpp"
#include <mpi.h>
#include <string>
#include "BayesRRm.h"
#include "BayesRRmz.h"
#include "BayesRRhp.h"
#include "BayesRRpp.h"
#include "BayesRRg.h"
#include "BayesRRgz.h"
#include "BayesW.hpp"

using namespace std;


int main(int argc, const char * argv[]) {
	Eigen::initParallel();
  MPI_Init(NULL, NULL);

  MPI_Comm_size(MPI_COMM_WORLD, &myMPI::clusterSize);
  MPI_Comm_rank(MPI_COMM_WORLD, &myMPI::rank);
  MPI_Get_processor_name(myMPI::processorName, &myMPI::processorNameLength);

  if (myMPI::rank==0) {
    cout << "***********************************************\n";
    cout << "* BayesRRcmd                                  *\n";
    cout << "* Complex Trait Genetics group UNIL            *\n";
    cout << "*                                             *\n";
    cout << "* MIT License                                 *\n";
    cout << "***********************************************\n";
    if (myMPI::clusterSize > 1)
      cout << "\nBayesRRcmd is using MPI with " << myMPI::clusterSize << " processors" << endl;
  }


  Gadget::Timer timer;
  timer.setTime();
  if (myMPI::rank==0) cout << "\nAnalysis started: " << timer.getDate();

  if (argc < 2){
    if (myMPI::rank==0) cerr << " \nDid you forget to give the input parameters?\n" << endl;
    exit(1);
  }
  try {

    Options opt;
    opt.inputOptions(argc, argv);

    Data data;

    bool readGenotypes;

    GCTB gctb(opt);


    if (opt.analysisType == "Bayes" && opt.loadRAM==true && (opt.bayesType == "bayesMmap" || (opt.bayesType == "horseshoe") || (opt.bayesType == "gbayes")||(opt.bayesType == "BayesW" ))) {

      clock_t start = clock();

      readGenotypes = false;
      gctb.inputIndInfo(data, opt.bedFile, opt.phenotypeFile, opt.keepIndFile, opt.keepIndMax,
            opt.mphen, opt.covariateFile);
      gctb.inputSnpInfo(data, opt.bedFile, opt.includeSnpFile, opt.excludeSnpFile,
            opt.includeChr, readGenotypes);

      cout << "Start reading " << opt.bedFile+".bed" << endl;
      clock_t start_bed = clock();
      //data.readBedFile(opt.bedFile+".bed");
      data.readBedFile_noMPI(opt.bedFile+".bed");
      clock_t end   = clock();
      printf("Finished reading the bed file in %.3f sec.\n", (float)(end - start_bed) / CLOCKS_PER_SEC);
      cout << endl;

      if (opt.bayesType == "bayesMmap") {
              		BayesRRm mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
              		mmapToy.runGibbs();
              	} else if (opt.bayesType == "horseshoe") {
              		BayesRRhp mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
              		mmapToy.runGibbs();
              	} else if (opt.bayesType == "gbayes") {
              		//data.readGroupFile2(opt.groupFile);
              		//Eigen::VectorXi G=data.G;
              		BayesRRg mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
              		mmapToy.runGibbs();
              	}else if (opt.bayesType == "BayesW") {
              		//data.readGroupFile2(opt.groupFile);
              		//Eigen::VectorXi G=data.G;
              		BayesW mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
              		mmapToy.runGibbs_Preprocessed();
              	}

      end = clock();
      printf("OVERALL read+compute time = %.3f sec.\n", (float)(end - start) / CLOCKS_PER_SEC);

      //gctb.clearGenotypes(data);
    } else if (opt.analysisType == "Bayes" && (opt.bayesType == "bayesMmap" || (opt.bayesType == "horseshoe") || (opt.bayesType == "gbayes")||(opt.bayesType == "BayesW" ))) {
      clock_t start = clock();
      readGenotypes = false;
      gctb.inputIndInfo(data, opt.bedFile, opt.phenotypeFile, opt.keepIndFile, opt.keepIndMax,
            opt.mphen, opt.covariateFile);
      gctb.inputSnpInfo(data, opt.bedFile, opt.includeSnpFile, opt.excludeSnpFile,
            opt.includeChr, readGenotypes);

      //this function works in reading bedfile, imputing missing data and standardizing by substracting the mean and dividing by sd.
      data.readBedFile_noMPI(opt.bedFile+".bed");

      if (opt.bayesType == "bayesMmap") {
    	  BayesRRm mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
          mmapToy.runGibbs();
      } else if (opt.bayesType == "horseshoe") {
    	  BayesRRhp mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
          mmapToy.runGibbs();
      } else if (opt.bayesType == "gbayes") {
    	  //data.readGroupFile2(opt.groupFile);
    	  //Eigen::VectorXi G=data.G;
    	  BayesRRg mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
    	  mmapToy.runGibbs();
      } else if(opt.bayesType == "BayesW"){
          BayesW mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
          mmapToy.runGibbs_notPreprocessed();
      }

      clock_t end   = clock();
      printf("OVERALL read+compute time = %.3f sec.\n", (float)(end - start) / CLOCKS_PER_SEC);

    } else if (opt.analysisType == "Preprocess") {
    	readGenotypes = false;
    	gctb.inputIndInfo(data, opt.bedFile, opt.phenotypeFile, opt.keepIndFile, opt.keepIndMax,
    			opt.mphen, opt.covariateFile);
    	gctb.inputSnpInfo(data, opt.bedFile, opt.includeSnpFile, opt.excludeSnpFile,
    			opt.includeChr, readGenotypes);

    	cout << "Start preprocessing " << opt.bedFile + ".bed" << endl;
    	clock_t start_bed = clock();
    	data.preprocessBedFile(opt.bedFile + ".bed",
    			opt.bedFile + ".ppbed",
				opt.bedFile + ".ppbedindex",
				opt.bedFile + ".sqnorm",
				opt.compress);
    	clock_t end = clock();
    	printf("Finished preprocessing the bed file in %.3f sec.\n", double(end - start_bed) / double(CLOCKS_PER_SEC));
    	cout << endl;
    } else if (opt.analysisType == "PPBayes") {
    	clock_t start = clock();

    	readGenotypes = false;
        gctb.inputIndInfo(data, opt.bedFile, opt.phenotypeFile, opt.keepIndFile, opt.keepIndMax,
                          opt.mphen, opt.covariateFile);
        gctb.inputSnpInfo(data, opt.bedFile, opt.includeSnpFile, opt.excludeSnpFile,
                          opt.includeChr, readGenotypes);
        if (opt.compress) {
        	cout << "Start reading preprocessed bed file: " << opt.bedFile + ".ppbed" << endl;
        	clock_t start_bed = clock();
        	data.mapCompressedPreprocessBedFile(opt.bedFile + ".ppbed",
        			opt.bedFile + ".ppbedindex");
        	clock_t end = clock();
        	printf("Finished reading preprocessed bed file in %.3f sec.\n", double(end - start_bed) / double(CLOCKS_PER_SEC));
        	cout << endl;

        	if (opt.bayesType == "bayesMmap") {
        		BayesRRmz mmapToy(data, opt);
        		mmapToy.runGibbs();
        	}/* else if (opt.bayesType == "horseshoe") {
        		BayesRRhp mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
        		mmapToy.runGibbs();
        	}*/ else if (opt.bayesType == "gbayes") {
        		//data.readGroupFile2(opt.groupFile);
        		//Eigen::VectorXi G=data.G;
        		BayesRRgz mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
        		mmapToy.runGibbs();
        	}/*else if (opt.bayesType == "BayesW") {
        		//data.readGroupFile2(opt.groupFile);
        		//Eigen::VectorXi G=data.G;
        		BayesW mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
        		mmapToy.runGibbs_Preprocessed();
        	}*/
        } else {
        	cout << "Start reading preprocessed bed file: " << opt.bedFile + ".ppbed" << endl;
        	clock_t start_bed = clock();
        	data.mapPreprocessBedFile(opt.bedFile + ".ppbed", opt.bedFile + ".sqnorm");
        	clock_t end = clock();
        	printf("Finished reading preprocessed bed file in %.3f sec.\n", double(end - start_bed) / double(CLOCKS_PER_SEC));
        	cout << endl;

        	// Run analysis using mapped data files

        	if (opt.bayesType == "bayesMmap") {
        		BayesRRm mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
        		mmapToy.runGibbs();
        	} else if (opt.bayesType == "horseshoe") {
        		BayesRRhp mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
        		mmapToy.runGibbs();
        	} else if (opt.bayesType == "gbayes") {
        		//data.readGroupFile2(opt.groupFile);
        		//Eigen::VectorXi G=data.G;
        		BayesRRg mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));
        		mmapToy.runGibbs();
            }else if (opt.bayesType == "BayesW") {
            	BayesW mmapToy(data, opt, sysconf(_SC_PAGE_SIZE));

            	// Left truncated version
            	if(opt.leftTruncFile != ""){
            		mmapToy.runGibbs_Preprocessed_LeftTruncated();
            	}else{ // Regular version
                	mmapToy.runGibbs_Preprocessed();
            	}
            }

        	data.unmapPreprocessedBedFile();
        }
      clock_t end = clock();
        printf("OVERALL read+compute time = %.3f sec.\n", double(end - start) / double(CLOCKS_PER_SEC));
    } else {
      throw(" Error: Wrong analysis type: " + opt.analysisType);
    }
  }
  catch (const string &err_msg) {
    if (myMPI::rank==0) cerr << "\n" << err_msg << endl;
  }
  catch (const char *err_msg) {
    if (myMPI::rank==0) cerr << "\n" << err_msg << endl;
  }

  timer.getTime();

  if (myMPI::rank==0) {
    cout << "\nAnalysis finished: " << timer.getDate();
    cout << "Computational time: "  << timer.format(timer.getElapse()) << endl;
  }

  MPI_Finalize();

  return 0;
}
