
/*
 Copyright (C) 2003 Neil Firth
 Copyright (C) 2002, 2003 Ferdinando Ametrano
 Copyright (C) 2002, 2003 Sad Rejeb
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it under the
 terms of the QuantLib license.  You should have received a copy of the
 license along with this program; if not, please email quantlib-dev@lists.sf.net
 The license is also available online at http://quantlib.org/html/license.html

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file analyticbarrierengine.hpp
    \brief Analytic barrier option engines
*/

#ifndef quantlib_analytic_barrier_engine_hpp
#define quantlib_analytic_barrier_engine_hpp

#include <ql/Instruments/barrieroption.hpp>
#include <ql/Math/normaldistribution.hpp>

namespace QuantLib {

    //! Pricing engine for Barrier options using analytical formulae
    /*! The formulas are taken from "Option pricing formulas",
         E.G. Haug, McGraw-Hill, p.69 and following.
    */
    class AnalyticBarrierEngine : public BarrierEngine {
      public:
        void calculate() const;
      private:
        CumulativeNormalDistribution f_;
        // helper methods
        double underlying() const;
        double strike() const;
        Time residualTime() const;
        double volatility() const;
        double barrier() const;
        double rebate() const;
        double stdDeviation() const;
        Rate riskFreeRate() const;
        DiscountFactor riskFreeDiscount() const;
        Rate dividendYield() const;
        DiscountFactor dividendDiscount() const;
        double mu() const;
        double muSigma() const;
        double A(double phi) const;
        double B(double phi) const;
        double C(double eta, double phi) const;
        double D(double eta, double phi) const;
        double E(double eta) const;
        double F(double eta) const;
    };

}


#endif
