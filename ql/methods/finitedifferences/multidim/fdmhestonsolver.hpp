/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Andreas Gaida
 Copyright (C) 2008 Ralph Schreyer
 Copyright (C) 2008 Klaus Spanderen

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file fdmhestonsolver.hpp
*/

#ifndef quantlib_fdm_heston_solver_hpp
#define quantlib_fdm_heston_solver_hpp

#include <ql/handle.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/math/interpolations/bicubicsplineinterpolation.hpp>
#include <ql/methods/finitedifferences/multidim/fdmmesher.hpp>
#include <ql/methods/finitedifferences/multidim/fdmhestonop.hpp>
#include <ql/methods/finitedifferences/multidim/fdmdirichletboundary.hpp>
#include <ql/methods/finitedifferences/multidim/fdmstepconditioncomposite.hpp>
#include <ql/methods/finitedifferences/multidim/fdmsnapshotcondition.hpp>

namespace QuantLib {

    class FdmHestonSolver : LazyObject {
      public:
        typedef std::vector<boost::shared_ptr<FdmDirichletBoundary> > 
            BoundaryConditionSet;
        enum FdmSchemeType { 
            HundsdorferScheme, DouglasScheme, CraigSneydScheme }; 

        FdmHestonSolver(
            const Handle<HestonProcess>& process,
            const boost::shared_ptr<FdmMesher>& mesher,
            const BoundaryConditionSet & bcSet,
            const boost::shared_ptr<FdmStepConditionComposite> & condition,
            const boost::shared_ptr<Payoff>& payoff,
            Time maturity,
            Size timeSteps,
            FdmSchemeType type = HundsdorferScheme, 
            Real theta = 0.3, Real mu = 0.5);

        Real valueAt(Real s, Real v) const;
        Real deltaAt(Real s, Real v, Real eps) const;
        Real gammaAt(Real s, Real v, Real eps) const;
        Real thetaAt(Real s, Real v) const;

      protected:
        void performCalculations() const;
 
      private:
        Handle<HestonProcess> process_;
        const boost::shared_ptr<FdmMesher> mesher_;
        const BoundaryConditionSet bcSet_;
        const boost::shared_ptr<FdmSnapshotCondition> thetaCondition_;
        const boost::shared_ptr<FdmStepConditionComposite> condition_;
        const Time maturity_;
        const Size timeSteps_;

        const FdmSchemeType schemeType_;
        const Real theta_, mu_;

        std::vector<Real> x_, v_, initialValues_;
        mutable Matrix resultValues_;
        mutable boost::shared_ptr<BicubicSpline> interpolation_;
    };
}

#endif