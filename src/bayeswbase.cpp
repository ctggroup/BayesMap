/*
 * BayesW.cpp
 *
 *  Created on: 26 Nov 2018
 *  Author: Sven Erik Ojavee
 *  Last changes: 22 Feb 2019
 */

#include "data.hpp"
#include "distributions_boost.hpp"
//#include "concurrentqueue.h"
#include "options.hpp"
#include "bayeswbase.h"
#include "bayeswkernel.h"
#include "BayesW_arms.h"
#include "markerbuilder.h"
#include "samplewriter.h"

#include <chrono>
#include <numeric>
#include <random>

/* Pre-calculate used constants */
#define PI 3.14159
#define PI2 6.283185
#define sqrtPI 1.77245385090552
#define EuMasc 0.577215664901532

BayesWBase::BayesWBase(Data &data, Options &opt, const long memPageSize)
: seed(opt.seed)
, data(data)
, opt(opt)
, memPageSize(memPageSize)
, max_iterations(opt.chainLength)
, thinning(opt.thin)
, burn_in(opt.burnin)
, outputFile(opt.mcmcSampleFile)
, bedFile(opt.dataFile)
, dist(opt.seed)
{

}

IndexEntry BayesWBase::indexEntry(unsigned int i) const
{
    return data.ppbedIndex[i];
}

bool BayesWBase::compressed() const
{
    return opt.compress;
}

unsigned char *BayesWBase::compressedData() const
{
    return reinterpret_cast<unsigned char*>(data.ppBedMap);
}

std::string BayesWBase::preprocessedFile() const
{
    return ppFileForType(opt.preprocessDataType, opt.dataFile);
}


BayesWBase::~BayesWBase() = default;

namespace  {

/* Function to check if ARS resulted with error*/
inline void errorCheck(int err){
	if(err>0){
		cout << "Error code = " << err << endl;
		exit(1);
	}
}

struct mu_params {
    double alpha = 0;
    double d = 0;
    VectorXd epsilon;
    double sigma_mu = 0;
};

/* Function for the log density of mu */
inline double mu_dens(double x, void *norm_data)
/* We are sampling mu (denoted by x here) */
{
	double y;

	/* In C++ we need to do a static cast for the void data */
    mu_params p = *(static_cast<mu_params *>(norm_data));

	/* cast voided pointer into pointer to struct norm_parm */
    y = - p.alpha * x * p.d - (( p.epsilon.array()  - x) * p.alpha - EuMasc).exp().sum() - x*x/(2*p.sigma_mu);
	return y;
};

struct theta_params {
    double alpha = 0;
    double sum_failure = 0;
    VectorXd epsilon;
    VectorXd X_j;
    double sigma_mu = 0;
};

/* Function for the log density of some "fixed" covariate effect */
inline double theta_dens(double x, void *norm_data)
/* We are sampling beta (denoted by x here) */
{
	double y;
	/* In C++ we need to do a static cast for the void data */
    theta_params p = *(static_cast<theta_params *>(norm_data));

	/* cast voided pointer into pointer to struct norm_parm */
	y = - p.alpha * x * p.sum_failure - (((p.epsilon -  p.X_j * x)* p.alpha).array() - EuMasc).exp().sum() - x*x/(2*p.sigma_mu); // Prior is the same currently for intercepts and fixed effects
	return y;
};

struct alpha_params {
    double alpha_0 = 0;
    double d = 0;
    VectorXd epsilon;
    VectorXd failure_vector;
    double kappa_0 = 0;
};

/* Function for the log density of alpha */
inline double alpha_dens(double x, void *norm_data)
/* We are sampling alpha (denoted by x here) */
{
	double y;

	/* In C++ we need to do a static cast for the void data */
    alpha_params p = *(static_cast<alpha_params *>(norm_data));
	y = (p.alpha_0 + p.d - 1) * log(x) + x * ((p.epsilon.array() * p.failure_vector.array()).sum() - p.kappa_0) -
			((p.epsilon * x).array() - EuMasc).exp().sum() ;
	return y;
};

}


//Calculate the value of the integral using Adaptive Gauss-Hermite quadrature
//Let's assume that mu is always 0 for speed
double BayesWBase::gauss_hermite_adaptive_integral(int k, double sigma, string n, const BayesWKernel *kernel){
    assert(kernel);

    double temp = 0;
    double sqrt_2ck_sigma = sqrt(2*mixture_classes(k)*sigma_b);

    if(n == "3"){
        double x1,x2;
        double w1,w2,w3;

        x1 = 1.2247448713916;
        x2 = -x1;

        w1 = 1.3239311752136;
        w2 = w1;

        w3 = 1.1816359006037;

        x1 = sigma*x1;
        x2 = sigma*x2;

        temp = w1 * kernel->integrand_adaptive(x1,alpha,sqrt_2ck_sigma) +
                w2 * kernel->integrand_adaptive(x2,alpha,sqrt_2ck_sigma) +
                w3;
    }
    // n=5
    else if(n == "5"){
        double x1,x2,x3,x4;//x5;
        double w1,w2,w3,w4,w5; //These are adjusted weights

        x1 = 2.0201828704561;
        x2 = -x1;
        w1 = 1.181488625536;
        w2 = w1;

        x3 = 0.95857246461382;
        x4 = -x3;
        w3 = 0.98658099675143;
        w4 = w3;

        //	x5 = 0.0;
        w5 = 0.94530872048294;

        x1 = sigma*x1;
        x2 = sigma*x2;
        x3 = sigma*x3;
        x4 = sigma*x4;
        //x5 = sigma*x5;

        temp = w1 * kernel->integrand_adaptive(x1,alpha,sqrt_2ck_sigma) +
                w2 * kernel->integrand_adaptive(x2,alpha,sqrt_2ck_sigma) +
                w3 * kernel->integrand_adaptive(x3,alpha,sqrt_2ck_sigma) +
                w4 * kernel->integrand_adaptive(x4,alpha,sqrt_2ck_sigma) +
                w5;
    }else if(n == "7"){
        double x1,x2,x3,x4,x5,x6;
        double w1,w2,w3,w4,w5,w6,w7; //These are adjusted weights

        x1 = 2.6519613568352;
        x2 = -x1;
        w1 = 1.1013307296103;
        w2 = w1;

        x3 = 1.6735516287675;
        x4 = -x3;
        w3 = 0.8971846002252;
        w4 = w3;

        x5 = 0.81628788285897;
        x6 = -x5;
        w5 = 0.8286873032836;
        w6 = w5;

        w7 = 0.81026461755681;

        x1 = sigma*x1;
        x2 = sigma*x2;
        x3 = sigma*x3;
        x4 = sigma*x4;
        x5 = sigma*x5;
        x6 = sigma*x6;

        temp = w1 * kernel->integrand_adaptive(x1,alpha,sqrt_2ck_sigma) +
                w2 * kernel->integrand_adaptive(x2,alpha,sqrt_2ck_sigma) +
                w3 * kernel->integrand_adaptive(x3,alpha,sqrt_2ck_sigma) +
                w4 * kernel->integrand_adaptive(x4,alpha,sqrt_2ck_sigma) +
                w5 * kernel->integrand_adaptive(x5,alpha,sqrt_2ck_sigma) +
                w6 * kernel->integrand_adaptive(x6,alpha,sqrt_2ck_sigma) +
                w7;
    }else if(n == "11"){
        double x1,x2,x3,x4,x5,x6,x7,x8,x9,x10;//,x11;
        double w1,w2,w3,w4,w5,w6,w7,w8,w9,w10,w11; //These are adjusted weights

        x1 = 3.6684708465596;
        x2 = -x1;
        w1 = 1.0065267861724;
        w2 = w1;

        x3 = 2.7832900997817;
        x4 = -x3;
        w3 = 0.802516868851;
        w4 = w3;

        x5 = 2.0259480158258;
        x6 = -x3;
        w5 = 0.721953624728;
        w6 = w5;

        x7 = 1.3265570844949;
        x8 = -x7;
        w7 = 0.6812118810667;
        w8 = w7;

        x9 = 0.6568095668821;
        x10 = -x9;
        w9 = 0.66096041944096;
        w10 = w9;

        //x11 = 0.0;
        w11 = 0.65475928691459;

        x1 = sigma*x1;
        x2 = sigma*x2;
        x3 = sigma*x3;
        x4 = sigma*x4;
        x5 = sigma*x5;
        x6 = sigma*x6;
        x7 = sigma*x7;
        x8 = sigma*x8;
        x9 = sigma*x9;
        x10 = sigma*x10;
        //	x11 = sigma*x11;

        temp = w1 * kernel->integrand_adaptive(x1,alpha,sqrt_2ck_sigma) +
                w2 * kernel->integrand_adaptive(x2,alpha,sqrt_2ck_sigma) +
                w3 * kernel->integrand_adaptive(x3,alpha,sqrt_2ck_sigma) +
                w4 * kernel->integrand_adaptive(x4,alpha,sqrt_2ck_sigma) +
                w5 * kernel->integrand_adaptive(x5,alpha,sqrt_2ck_sigma) +
                w6 * kernel->integrand_adaptive(x6,alpha,sqrt_2ck_sigma) +
                w7 * kernel->integrand_adaptive(x7,alpha,sqrt_2ck_sigma) +
                w8 * kernel->integrand_adaptive(x8,alpha,sqrt_2ck_sigma) +
                w9 * kernel->integrand_adaptive(x9,alpha,sqrt_2ck_sigma) +
                w10 * kernel->integrand_adaptive(x10,alpha,sqrt_2ck_sigma) +
                w11;
    }else{
        cout << "Possible number of quad_points = 3,5,7,11" << endl;
        exit(1);
    }

    return sigma*temp;
}

//Pass the vector post_marginals of marginal likelihoods by reference
void BayesWBase::marginal_likelihood_vec_calc(VectorXd prior_prob, VectorXd &post_marginals, string n, const BayesWKernel *kernel){
    assert(kernel);
    double exp_sum = kernel->exponent_sum();

    for(int i=0; i < mixture_classes.size(); i++){
        //Calculate the sigma for the adaptive G-H
        double sigma = 1.0/sqrt(1 + alpha * alpha * sigma_b * mixture_classes(i) * exp_sum);
        post_marginals(i+1) = prior_prob(i+1) * gauss_hermite_adaptive_integral(i, sigma, n, kernel);
    }
}

void BayesWBase::init(unsigned int markerCount, unsigned int individualCount, unsigned int fixedCount)
{
	// Read the failure indicator vector
	data.readFailureFile(opt.failureFile);

	// Component variables
	pi_L = VectorXd(K);           		 // prior mixture probabilities
	marginal_likelihoods = VectorXd(K);  // likelihood for each mixture component
	v = VectorXd(K);            		 // vector storing the component assignment

	// Linear model variables
	beta = VectorXd(markerCount);           // effect sizes
	theta = VectorXd(fixedCount);

	sum_failure_fix = VectorXd(fixedCount); // Vector to sum fixed vector * failure vector per fixed effect

	//phenotype vector
	y = VectorXd();
	//residual vector
	epsilon = VectorXd();

	// Init the working variables
	const int km1 = K - 1;

	//vector with component class for each marker
	components = VectorXi(markerCount);
	components.setZero();

	//set priors for pi parameters
	//Give all mixtures (except 0 class) equal initial probabilities
	pi_L(0) = 0.99;
	pi_L.segment(1,km1).setConstant((1-pi_L(0))/km1);

	marginal_likelihoods.setOnes();   //Initialize with just ones

	beta.setZero();
	theta.setZero();

	//initialize epsilon vector as the phenotype vector
	y = data.y.cast<double>().array();
	epsilon = y;
	mu = y.mean();       // mean or intercept

	// Initialize the variables in structures
	//Save variance classes
    mixture_classes.resize(km1); //The future solution


	for(int i=0;i<(km1);i++){
        mixture_classes(i) = opt.S.row(0)[i];   //Save the mixture data (C_k)
	}

    //Store the vector of failures
    failure_vector = data.fail.cast<double>();
    // Save the number of events
    d = failure_vector.array().sum();

	double denominator = (6 * ((y.array() - mu).square()).sum()/(y.size()-1));
    alpha = PI/sqrt(denominator);    // The shape parameter initial value


    for(int i=0; i<(y.size()); ++i){
        epsilon[i] = y[i] - mu; // Initially, all the BETA elements are set to 0, XBeta = 0
	}

    sigma_b = PI2/ (6 * pow(alpha,2) * markerCount ) ;

	// Save the sum(X_j*failure) for each j
	//Previous preprocessed version for reading columns
	//for(int marker=0; marker<M; marker++){
	//	sum_failure(marker) = ((data.mappedZ.col(marker).cast<double>()).array() * used_data_alpha.failure_vector.array()).sum();
	//}

	//If there are fixed effects, find the same values for them
	if(fixedCount > 0){
		for(int fix_i=0; fix_i < fixedCount; fix_i++){
            sum_failure_fix(fix_i) = ((data.X.col(fix_i).cast<double>()).array() * failure_vector.array()).sum();
		}
	}
}
// Function for sampling intercept (mu)
void BayesWBase::sampleMu(){
	// ARS parameters
	int err, ninit = 4, npoint = 100, nsamp = 1, ncent = 4 ;
	int neval;
	double xsamp[0], xcent[10], qcent[10] = {5., 30., 70., 95.};
	double convex = 1.0;
	int dometrop = 0;
	double xprev = 0.0;
	double xinit[4] = {0.995*mu, mu,  1.005*mu, 1.01*mu};     // Initial abscissae
	double *p_xinit = xinit;

	double xl = 2;
	double xr = 5;   //xl and xr and the maximum and minimum values between which we sample

    mu_params params;
    params.alpha = alpha;
    params.d = d;
    params.epsilon = epsilon.array() + mu; // we add to epsilon =Y+mu-X*beta
    params.sigma_mu = sigma_mu;

	// Use ARS to sample mu (with density mu_dens, using parameters from used_data)
    err = arms(xinit,ninit,&xl,&xr,mu_dens,&params,&convex,
			npoint,dometrop,&xprev,xsamp,nsamp,qcent,xcent,ncent,&neval);

	errorCheck(err); // If there is error, stop the program
	mu = xsamp[0];   // Save the sampled value
    epsilon = params.epsilon.array() - mu;// we substract again now epsilon =Y-mu-X*beta
}

// Function for sampling fixed effect (theta_i)
void BayesWBase::sampleTheta(int fix_i){
	// ARS parameters
	int err, ninit = 4, npoint = 100, nsamp = 1, ncent = 4 ;
	int neval;
	double xsamp[0], xcent[10], qcent[10] = {5., 30., 70., 95.};
	double convex = 1.0;
	int dometrop = 0;
	double xprev = 0.0;
	double xinit[4] = {theta(fix_i)-0.01, theta(fix_i),  theta(fix_i)+0.005, theta(fix_i)+0.01};     // Initial abscissae
	double *p_xinit = xinit;

	double xl = -2;
	double xr = 2;			  // Initial left and right (pseudo) extremes

    theta_params params;
    params.alpha = alpha;
    params.sum_failure = sum_failure_fix(fix_i);
    params.X_j = data.X.col(fix_i).cast<double>();  //Take from the fixed effects matrix
    params.epsilon = epsilon.array() + (params.X_j * theta(fix_i)).array(); // Adjust residual
    params.sigma_mu = sigma_mu;


	// Sample using ARS
    err = arms(xinit,ninit,&xl,&xr,theta_dens,&params,&convex,
			npoint,dometrop,&xprev,xsamp,nsamp,qcent,xcent,ncent,&neval);
	errorCheck(err);

	theta(fix_i) = xsamp[0];  // Save the new result
    epsilon = params.epsilon - params.X_j * theta(fix_i); // Adjust residual
}

// Function for sampling marker effect (beta_i)
void BayesWBase::sampleBeta(int marker){

    std::unique_ptr<Kernel> kernel;
    {
        std::unique_ptr<MarkerBuilder> builder{markerBuilder()};
        builder->initialise(marker, data.numInds);
        const auto index = indexEntry(marker);
        if (compressed()) {
            builder->decompress(compressedData(), index);
        } else {
            builder->read(preprocessedFile(), index);
        }
        kernel = kernelForMarker(builder->build());
    }
    auto * gaussKernel = dynamic_cast<BayesWKernel*>(kernel.get());
    assert(gaussKernel);

	//Change the residual vector only if the previous beta was non-zero
    if(beta(gaussKernel->marker->i) != 0){
        epsilon += *gaussKernel->calculateEpsilonChange(beta(gaussKernel->marker->i));
        //Also find the transformed residuals
        vi = (alpha*epsilon.array()-EuMasc).exp();
	}

    gaussKernel->setVi(vi);
    gaussKernel->calculateSumFailure(failure_vector);

	/* Calculate the mixture probability */
	double p = dist.unif_rng();  //Generate number from uniform distribution (for sampling from categorical distribution)

    // Calculate the (ratios of) marginal likelihoods
    marginal_likelihood_vec_calc(pi_L, marginal_likelihoods, quad_points, gaussKernel);
	// Calculate the probability that marker is 0
	double acum = marginal_likelihoods(0)/marginal_likelihoods.sum();

	//Loop through the possible mixture classes
	for (int k = 0; k < K; k++) {
		if (p <= acum) {
			//if zeroth component
			if (k == 0) {
				beta(marker) = 0;
				v[k] += 1.0;
				components[marker] = k;
			}
			// If is not 0th component then sample using ARS
			else {
                beta_params params;
                params.alpha = alpha;
                params.sigma_b = sigma_b;
                params.sum_failure = gaussKernel->sum_failure;
                params.used_mixture = mixture_classes(k-1);

                double safe_limit = 2 * sqrt(sigma_b * mixture_classes(k-1));

				// ARS parameters
                int err, ninit = 4, npoint = 100, nsamp = 1, ncent = 4 ;
                int neval;
                double xsamp[0], xcent[10], qcent[10] = {5., 30., 70., 95.};
                double convex = 1.0;
                int dometrop = 0;
                double xprev = 0.0;
                double xinit[4] = {beta(marker) - safe_limit/10 , beta(marker),  beta(marker) + safe_limit/20, beta(marker) + safe_limit/10};     // Initial abscissae
                double *p_xinit = xinit;

                double xl = beta(marker) - safe_limit  ; //Construct the hull around previous beta value
                double xr = beta(marker) + safe_limit;

                // Sample using ARS
                err = estimateBeta(gaussKernel,xinit,ninit,&xl,&xr, params, &convex,
                        npoint,dometrop,&xprev,xsamp,nsamp,qcent,xcent,ncent,&neval);
				errorCheck(err);

				beta(marker) = xsamp[0];  // Save the new result

                //Re-update the residual vector
                epsilon -= *gaussKernel->calculateEpsilonChange(beta(gaussKernel->marker->i));
                vi = (alpha*epsilon.array()-EuMasc).exp();

				v[k] += 1.0;
				components[marker] = k;
			}
			break;
		} else {
			if((k+1) == (K-1)){
				acum = 1; // In the end probability will be 1
			}else{
				acum += marginal_likelihoods(k+1)/marginal_likelihoods.sum();
			}
		}
	}
}

// Function for sampling Weibull shape parameter (alpha)
void BayesWBase::sampleAlpha(){
	// ARS parameters
	int err, ninit = 4, npoint = 100, nsamp = 1, ncent = 4 ;
	int neval;
	double xsamp[0], xcent[10], qcent[10] = {5., 30., 70., 95.};
	double convex = 1.0;
	int dometrop = 0;
	double xprev = 0.0;
    double xinit[4] = {(alpha)*0.5, alpha,  (alpha)*1.5, (alpha)*3};     // Initial abscissae
	double *p_xinit = xinit;

	// Initial left and right (pseudo) extremes
	double xl = 0.0;
	double xr = 400.0;

    alpha_params params;
    params.alpha_0 = alpha_0;
    params.d = d;
    params.epsilon = epsilon;
    params.failure_vector = failure_vector;
    params.kappa_0 = kappa_0;

	//Sample using ARS
    err = arms(xinit,ninit,&xl,&xr,alpha_dens,&params,&convex,
			npoint,dometrop,&xprev,xsamp,nsamp,qcent,xcent,ncent,&neval);
	errorCheck(err);
    alpha = xsamp[0];
}

/* Adaptive Gauss-Hermite version. Currently RAM solution */
int BayesWBase::runGibbs_Gauss()
{
	const unsigned int M(data.numSnps);
	const unsigned int N(data.numInds);
	const unsigned int numFixedEffects(data.numFixedEffects);
	const int km1 = K - 1;

	init(M, N, numFixedEffects);
	int marker; //Marker index

	SampleWriter writer;
	writer.setFileName(outputFile);
	writer.setMarkerCount(M);
	writer.setIndividualCount(N);

	VectorXd sample(2*M+4); // variable containing a sample of all variables in the model: M marker effects, M mixture assignments, shape (alpha), mu, iteration number and sigma_b(sigma_g)

	//If we have fixed effects, then record their number to samplewriter and create a different header
	if(numFixedEffects > 0){
		writer.setFixedCount(numFixedEffects);
		writer.open_bayesW_fixed();
		sample.resize(numFixedEffects+2*M+4); // all the rest + theta (fixed effects)

	}else{
		writer.open_bayesW();
	}

	// Sampler variables
	std::vector<unsigned int> markerI(M);
	std::iota(markerI.begin(), markerI.end(), 0);

	std::cout<< "Running Gibbs sampling" << endl;
	std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

	// This for MUST NOT BE PARALLELIZED, IT IS THE MARKOV CHAIN
	srand(2);
	for (int iteration = 0; iteration < max_iterations; iteration++) {

		/* 1. Intercept (mu) */
		sampleMu();

		/* 1a. Fixed effects (thetas) */
		if(numFixedEffects > 0){
			for(int fix_i = 0; fix_i < numFixedEffects; fix_i++){
				sampleTheta(fix_i);
			}
		}
		// Calculate the vector of exponent of the adjusted residuals
        vi = (alpha*epsilon.array()-EuMasc).exp();

		std::random_shuffle(markerI.begin(), markerI.end());
		// This for should not be parallelized, resulting chain would not be ergodic, still, some times it may converge to the correct solution
		// 2. Sample beta parameters

		// Set counter for each mixture to be 1 ( (1,...,1) prior)
		v.setOnes();

		// First element for the marginal likelihoods is always is pi_0 *sqrt(pi) for
		marginal_likelihoods(0) = pi_L(0) * sqrtPI;
		for (int j = 0; j < M; j++) {
			marker = markerI[j];
			sampleBeta(marker);
		}
		// 3. Sample alpha parameter
		sampleAlpha();

		// 4. Sample sigma_b
        sigma_b = dist.inv_gamma_rng((double) (alpha_sigma + 0.5 * (M - v[0]+1)),
                (double)(beta_sigma + 0.5 * (M - v[0]+1) * beta.squaredNorm()));

		// 5. Sample prior mixture component probability from Dirichlet distribution
		pi_L = dist.dirichilet_rng(v.array());

		// Write the result to file
        if (iteration >= burn_in && iteration % thinning == 0) {
			if(numFixedEffects > 0){
                sample << iteration, alpha, mu, theta, beta,components.cast<double>(), sigma_b ;
			}else{
                sample << iteration, alpha, mu, beta,components.cast<double>(), sigma_b ;
			}
			writer.write(sample);
		}

		//Print results
        cout << iteration << ". " << M - v[0] +1 <<"; "<<v[1]-1 << "; "<<v[2]-1 << "; " << v[3]-1  <<"; " << alpha << "; " << sigma_b << endl;
	}

	std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count();
	std::cout << "duration: "<<duration << "s\n";

	return 0;
}