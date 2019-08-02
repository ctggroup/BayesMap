/*
 * BayesW.hpp
 *
 *  Created on: 26 Nov 2018
 *      Author: admin
 */

#ifndef SPARSEBAYESW_HPP_
#define SPARSEBAYESW_HPP_

#include "bayeswbase.h"

class SparseBayesW : public BayesWBase
{
public:
    SparseBayesW(Data &data, Options &opt, const long memPageSize);

    std::unique_ptr<Kernel> kernelForMarker(const Marker *marker) const override;
    MarkerBuilder *markerBuilder() const override;

protected:
    int estimateBeta(const BayesWKernel *kernel, double *xinit, int ninit, double *xl, double *xr, const beta_params params,
                      double *convex, int npoint, int dometrop, double *xprev, double *xsamp,
                      int nsamp, double *qcent, double *xcent,
                      int ncent, int *neval) override;
};


#endif /* SPARSEBAYESW_HPP_ */