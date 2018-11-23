/*
 * BayesRRhp.h
 *
 *  Created on: 21 Sep 2018
 *      Author: admin
 */

#ifndef BAYESRRHP_H_
#define BAYESRRHP_H_
#include"data.hpp"
#include"options.hpp"
#include"distributions_boost.hpp"
class BayesRRhp {
	  Data          &data;//data matrices
	  Options       &opt;
	  const string  bedFile;//bed file
	  const long    memPageSize;//size of memory
	  const string  outputFile;
	  const int     seed;
	  const int 	max_iterations;
	  const int		burn_in;
	  const int 	thinning;
	  double	A;
	  const double	v0E=0.0001;
	  const double  s02E=0.0001;
	  const double  vL=1.0;
	  const double  vT=1.0;
	  const double vC=7.0;
	  const double sC=10.0;
	  double c2=1.0;
	  Distributions_boost dist;
public:
	BayesRRhp(Data &data, Options &opt, const long memPageSize);
	virtual ~BayesRRhp();
	void runGibbs();
};

#endif /* BAYESRRHP_H_ */
