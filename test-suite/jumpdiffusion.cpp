
/*
 Copyright (C) 2004 Ferdinando Ametrano

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

#include "jumpdiffusion.hpp"
#include <ql/DayCounters/actual360.hpp>
#include <ql/Instruments/vanillaoption.hpp>
#include <ql/PricingEngines/Vanilla/analyticeuropeanengine.hpp>
#include <ql/PricingEngines/Vanilla/jumpdiffusionengine.hpp>
#include <ql/TermStructures/flatforward.hpp>
#include <ql/Volatilities/blackconstantvol.hpp>

#include <cppunit/TestSuite.h>
#include <cppunit/TestCaller.h>
#include <map>

// This makes it easier to use array literals (alas, no std::vector literals)
#define LENGTH(a) (sizeof(a)/sizeof(a[0]))

using namespace QuantLib;

namespace {

    Handle<TermStructure> makeFlatCurve(const Handle<Quote>& forward,
        DayCounter dc) {
        Date today = Date::todaysDate();
        return Handle<TermStructure>(new
            FlatForward(today, today, RelinkableHandle<Quote>(forward), dc));
    }

    Handle<BlackVolTermStructure> makeFlatVolatility(
        const Handle<Quote>& volatility, DayCounter dc) {
        Date today = Date::todaysDate();
        return Handle<BlackVolTermStructure>(new
            BlackConstantVol(today, RelinkableHandle<Quote>(volatility), dc));
    }

    double relativeError(double x1, double x2, double reference) {
        if (reference != 0.0)
            return QL_FABS(x1-x2)/reference;
        else
            return 1.0e+10;
    }

    std::string payoffTypeToString(const Handle<Payoff>& payoff) {

        // PlainVanillaPayoff?
        Handle<PlainVanillaPayoff> pv;
        #if defined(HAVE_BOOST)
        pv = boost::dynamic_pointer_cast<PlainVanillaPayoff>(payoff);
        #else
        try {
            pv = payoff;
        } catch (...) {}
        #endif
        if (!IsNull(pv)) {
            // ok, the payoff is PlainVanillaPayoff
            return "PlainVanillaPayoff";
        }

        // CashOrNothingPayoff?
        Handle<CashOrNothingPayoff> coo;
        #if defined(HAVE_BOOST)
        coo = boost::dynamic_pointer_cast<CashOrNothingPayoff>(payoff);
        #else
        try {
            coo = payoff;
        } catch (...) {}
        #endif
        if (!IsNull(coo)) {
            // ok, the payoff is CashOrNothingPayoff
            return "Cash ("
                + DoubleFormatter::toString(coo->cashPayoff())
                + ") or Nothing Payoff";
        }

        // AssetOrNothingPayoff?
        Handle<AssetOrNothingPayoff> aoo;
        #if defined(HAVE_BOOST)
        aoo = boost::dynamic_pointer_cast<AssetOrNothingPayoff>(payoff);
        #else
        try {
            aoo = payoff;
        } catch (...) {}
        #endif
        if (!IsNull(aoo)) {
            // ok, the payoff is AssetOrNothingPayoff
            return "AssetOrNothingPayoff";
        }

        // SuperSharePayoff?
        Handle<SuperSharePayoff> ss;
        #if defined(HAVE_BOOST)
        ss = boost::dynamic_pointer_cast<SuperSharePayoff>(payoff);
        #else
        try {
            ss = payoff;
        } catch (...) {}
        #endif
        if (!IsNull(ss)) {
            // ok, the payoff is SuperSharePayoff
            return "SuperSharePayoff";
        }

        throw Error("payoffTypeToString : unknown payoff type");
    }


    std::string exerciseTypeToString(const Handle<Exercise>& exercise) {

        // EuropeanExercise?
        Handle<EuropeanExercise> european;
        #if defined(HAVE_BOOST)
        european = boost::dynamic_pointer_cast<EuropeanExercise>(exercise);
        #else
        try {
            european = exercise;
        } catch (...) {}
        #endif
        if (!IsNull(european)) {
            return "European";
        }

        // AmericanExercise?
        Handle<AmericanExercise> american;
        #if defined(HAVE_BOOST)
        american = boost::dynamic_pointer_cast<AmericanExercise>(exercise);
        #else
        try {
            american = exercise;
        } catch (...) {}
        #endif
        if (!IsNull(american)) {
            return "American";
        }

        // BermudanExercise?
        Handle<BermudanExercise> bermudan;
        #if defined(HAVE_BOOST)
        bermudan = boost::dynamic_pointer_cast<BermudanExercise>(exercise);
        #else
        try {
            bermudan = exercise;
        } catch (...) {}
        #endif
        if (!IsNull(bermudan)) {
            return "Bermudan";
        }

        throw Error("exerciseTypeToString : unknown exercise type");
    }

    void jumpOptionTestFailed(std::string greekName,
                              const Handle<StrikedTypePayoff>& payoff,
                              const Handle<Exercise>& exercise,
                              double s,
                              double q,
                              double r,
                              Date today,
                              DayCounter dc,
                              double v,
                              double intensity,
                              double meanLogJump,
                              double jumpVol,
                              double expected,
                              double calculated,
                              double error,
                              double tolerance) {

        Time t = dc.yearFraction(today, exercise->lastDate());

        CPPUNIT_FAIL(exerciseTypeToString(exercise) + " "
            + OptionTypeFormatter::toString(payoff->optionType()) +
            " option with "
            + payoffTypeToString(payoff) + ":\n"
            "    underlying value: "
            + DoubleFormatter::toString(s) + "\n"
            "    strike:           "
            + DoubleFormatter::toString(payoff->strike()) +"\n"
            "    dividend yield:   "
            + DoubleFormatter::toString(q) + "\n"
            "    risk-free rate:   "
            + DoubleFormatter::toString(r) + "\n"
            "    reference date:   "
            + DateFormatter::toString(today) + "\n"
            "    maturity:         "
            + DateFormatter::toString(exercise->lastDate()) + "\n"
            "    time to expiry:   "
            + DoubleFormatter::toString(t) + "\n"
            "    volatility:       "
            + DoubleFormatter::toString(v) + "\n\n"
            "    intensity:        "
            + DoubleFormatter::toString(intensity) + "\n"
            "    mean log-jump:    "
            + DoubleFormatter::toString(meanLogJump) + "\n"
            "    jump volatility:  "
            + DoubleFormatter::toString(jumpVol) + "\n\n"
            "    expected   " + greekName + ": "
            + DoubleFormatter::toString(expected) + "\n"
            "    calculated " + greekName + ": "
            + DoubleFormatter::toString(calculated) + "\n"
            "    error:            "
            + DoubleFormatter::toString(error) + "\n"
            + (tolerance==Null<double>() ? std::string("") :
            "    tolerance:        " + DoubleFormatter::toString(tolerance)));
    }

    void jump2OptionTestFailed(std::string greekName,
                               const Handle<StrikedTypePayoff>& payoff,
                               const Handle<Exercise>& exercise,
                               double s,
                               double q,
                               double r,
                               Date today,
                               DayCounter dc,
                               double v,
                               double intensity,
                               double gamma,
                               double expected,
                               double calculated,
                               double error,
                               double tolerance) {

        Time t = dc.yearFraction(today, exercise->lastDate());

        CPPUNIT_FAIL(exerciseTypeToString(exercise) + " "
            + OptionTypeFormatter::toString(payoff->optionType()) +
            " option with "
            + payoffTypeToString(payoff) + ":\n"
            "    underlying value: "
            + DoubleFormatter::toString(s) + "\n"
            "    strike:           "
            + DoubleFormatter::toString(payoff->strike()) +"\n"
            "    dividend yield:   "
            + DoubleFormatter::toString(q) + "\n"
            "    risk-free rate:   "
            + DoubleFormatter::toString(r) + "\n"
            "    reference date:   "
            + DateFormatter::toString(today) + "\n"
            "    maturity:         "
            + DateFormatter::toString(exercise->lastDate()) + "\n"
            "    time to expiry:   "
            + DoubleFormatter::toString(t) + "\n"
            "    volatility:       "
            + DoubleFormatter::toString(v) + "\n"
            "    intensity:        "
            + DoubleFormatter::toString(intensity) + "\n"
            "    gamma:            "
            + DoubleFormatter::toString(gamma) + "\n\n"
            "    expected   " + greekName + ": "
            + DoubleFormatter::toString(expected) + "\n"
            "    calculated " + greekName + ": "
            + DoubleFormatter::toString(calculated) + "\n"
            "    error:            "
            + DoubleFormatter::toString(error) + "\n"
            + (tolerance==Null<double>() ? std::string("") :
            "    tolerance:        " + DoubleFormatter::toString(tolerance)));
    }

    struct HaugMertonData {
        Option::Type type;
        double strike;
        double s;      // spot
        double q;      // dividend
        double r;      // risk-free rate
        Time t;        // time to maturity
        double v;      // volatility
        double jumpIntensity;
        double gamma;
        double result; // result
        double tol;    // tolerance
    };

}



// tests


void JumpDiffusionTest::testMerton76() {

    /* The data below are from 
       "Option pricing formulas", E.G. Haug, McGraw-Hill 1998, pag 9

       Haug use the arbitrary truncation criterium of 11 terms in the sum, which
       doesn't guarantee convergence up to 1e-2.
       Using Haug's criterium Haug's values have been correctly reproduced. Anyway the
       following values have the right 1e-2 accuracy: any value different from Haug has
       been noted
    */
    HaugMertonData values[] = {
        //        type, strike,   spot,    q,    r,    t,  vol, int, gamma, value, tol
        // gamma = 0.25, strike = 80
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.25, 20.67, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.25, 21.74, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.25, 23.63, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.25, 20.65, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.25, 21.70, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.25, 23.61, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.25, 20.64, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.25, 21.70, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.25, 23.61, 1e-2 }, // Haug 23.28
        // gamma = 0.25, strike = 90
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.25, 11.00, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.25, 12.74, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.25, 15.40, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.25, 10.98, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.25, 12.75, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.25, 15.42, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.25, 10.98, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.25, 12.75, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.25, 15.42, 1e-2 }, // Haug 15.20
        // gamma = 0.25, strike = 100
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.25,  3.42, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.25,  5.88, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.25,  8.95, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.25,  3.51, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.25,  5.96, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.25,  9.02, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.25,  3.53, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.25,  5.97, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.25,  9.03, 1e-2 }, // Haug 8.89
        // gamma = 0.25, strike = 110
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.25,  0.55, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.25,  2.11, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.25,  4.67, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.25,  0.56, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.25,  2.16, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.25,  4.73, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.25,  0.56, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.25,  2.17, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.25,  4.74, 1e-2 }, // Haug 4.66
        // gamma = 0.25, strike = 120
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.25,  0.10, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.25,  0.64, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.25,  2.23, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.25,  0.06, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.25,  0.63, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.25,  2.25, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.25,  0.05, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.25,  0.62, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.25,  2.25, 1e-2 }, // Haug 2.21

        // gamma = 0.50, strike = 80
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.50, 20.72, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.50, 21.83, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.50, 23.71, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.50, 20.66, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.50, 21.73, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.50, 23.63, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.50, 20.65, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.50, 21.71, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.50, 23.61, 1e-2 }, // Haug 23.28
        // gamma = 0.50, strike = 90
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.50, 11.04, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.50, 12.72, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.50, 15.34, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.50, 11.02, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.50, 12.76, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.50, 15.41, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.50, 11.00, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.50, 12.75, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.50, 15.41, 1e-2 }, // Haug 15.18
        // gamma = 0.50, strike = 100
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.50,  3.14, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.50,  5.58, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.50,  8.71, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.50,  3.39, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.50,  5.87, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.50,  8.96, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.50,  3.46, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.50,  5.93, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.50,  9.00, 1e-2 }, // Haug 8.85
        // gamma = 0.50, strike = 110
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.50,  0.53, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.50,  1.93, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.50,  4.42, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.50,  0.58, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.50,  2.11, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.50,  4.67, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.50,  0.57, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.50,  2.14, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.50,  4.71, 1e-2 }, // Haug 4.62
        // gamma = 0.50, strike = 120
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.50,  0.19, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.50,  0.71, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.50,  2.15, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.50,  0.10, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.50,  0.66, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.50,  2.23, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.50,  0.07, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.50,  0.64, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.50,  2.24, 1e-2 }, // Haug 2.19

        // gamma = 0.75, strike = 80
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.75, 20.79, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.75, 21.96, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.75, 23.86, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.75, 20.68, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.75, 21.78, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.75, 23.67, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.75, 20.66, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.75, 21.74, 1e-2 },
        { Option::Call,  80.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.75, 23.64, 1e-2 }, // Haug 23.30
        // gamma = 0.75, strike = 90
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.75, 11.11, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.75, 12.75, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.75, 15.30, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.75, 11.09, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.75, 12.78, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.75, 15.39, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.75, 11.04, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.75, 12.76, 1e-2 },
        { Option::Call,  90.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.75, 15.40, 1e-2 }, // Haug 15.17
        // gamma = 0.75, strike = 100
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.75,  2.70, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.75,  5.08, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.75,  8.24, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.75,  3.16, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.75,  5.71, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.75,  8.85, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.75,  3.33, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.75,  5.85, 1e-2 },
        { Option::Call, 100.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.75,  8.95, 1e-2 }, // Haug 8.79
        // gamma = 0.75, strike = 110
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.75,  0.54, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.75,  1.69, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.75,  3.99, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.75,  0.62, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.75,  2.05, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.75,  4.57, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.75,  0.60, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.75,  2.11, 1e-2 },
        { Option::Call, 110.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.75,  4.66, 1e-2 }, // Haug 4.56
        // gamma = 0.75, strike = 120
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25, 1.0,  0.75,  0.29, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25, 1.0,  0.75,  0.84, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25, 1.0,  0.75,  2.09, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25, 5.0,  0.75,  0.15, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25, 5.0,  0.75,  0.71, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25, 5.0,  0.75,  2.21, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.10, 0.25,10.0,  0.75,  0.11, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.25, 0.25,10.0,  0.75,  0.67, 1e-2 },
        { Option::Call, 120.00, 100.00, 0.00, 0.08, 0.50, 0.25,10.0,  0.75,  2.23, 1e-2 }  // Haug 2.17
};



    DayCounter dc = Actual360();
    Handle<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<SimpleQuote> qRate(new SimpleQuote(0.0));
    Handle<TermStructure> qTS = makeFlatCurve(qRate, dc);
    Handle<SimpleQuote> rRate(new SimpleQuote(0.0));
    Handle<TermStructure> rTS = makeFlatCurve(rRate, dc);
    Handle<SimpleQuote> vol(new SimpleQuote(0.0));
    Handle<BlackVolTermStructure> volTS = makeFlatVolatility(vol, dc);

    Handle<SimpleQuote> jumpIntensity(new SimpleQuote(0.0));
    Handle<SimpleQuote> meanLogJump(new SimpleQuote(0.0));
    Handle<SimpleQuote> jumpVol(new SimpleQuote(0.0));
    
    Handle<BlackScholesStochasticProcess> stochProcess(new
        Merton76StochasticProcess(
            RelinkableHandle<Quote>(spot),
            RelinkableHandle<TermStructure>(qTS),
            RelinkableHandle<TermStructure>(rTS),
            RelinkableHandle<BlackVolTermStructure>(volTS),
            RelinkableHandle<Quote>(jumpIntensity),
            RelinkableHandle<Quote>(meanLogJump),
            RelinkableHandle<Quote>(jumpVol)));

    Handle<VanillaEngine> baseEngine(new AnalyticEuropeanEngine);
    Handle<PricingEngine> engine(new JumpDiffusionEngine(baseEngine));

    Date today = Date::todaysDate();


    for (Size i=0; i<LENGTH(values); i++) {

        Handle<StrikedTypePayoff> payoff(new
            PlainVanillaPayoff(values[i].type, values[i].strike));

        Date exDate = today.plusDays(int(values[i].t*360+0.5));
        Handle<Exercise> exercise(new EuropeanExercise(exDate));

        spot ->setValue(values[i].s);
        qRate->setValue(values[i].q);
        rRate->setValue(values[i].r);


        jumpIntensity->setValue(values[i].jumpIntensity);

        // delta in Haug's notation
        double jVol = values[i].v *
            QL_SQRT(values[i].gamma / values[i].jumpIntensity);
        jumpVol->setValue(jVol);

        // z in Haug's notation
        double diffusionVol = values[i].v * QL_SQRT(1.0 - values[i].gamma);
        vol  ->setValue(diffusionVol);

        // Haug is assuming zero meanJump
        double meanJump = 0.0;
        meanLogJump->setValue(QL_LOG(1.0+meanJump)-0.5*jVol*jVol);

        double totalVol = QL_SQRT(values[i].jumpIntensity*jVol*jVol+diffusionVol*diffusionVol);
        double volError = QL_FABS(totalVol-values[i].v);
        QL_REQUIRE(volError<1e-13,
            "" + DoubleFormatter::toString(volError) +
            " mismatch");

        VanillaOption option(stochProcess,
                             payoff,
                             exercise,
                             engine);

        double calculated = option.NPV();
        double error = QL_FABS(calculated-values[i].result);
        if (error>values[i].tol) {
            jump2OptionTestFailed("value",
                                  payoff, exercise,
                                  values[i].s, values[i].q, values[i].r,
                                  today, dc,
                                  values[i].v,
                                  values[i].jumpIntensity, values[i].gamma,
                                  values[i].result, calculated,
                                  error, values[i].tol);
        }
    }

}

void JumpDiffusionTest::testGreeks() {

    std::map<std::string,double> calculated, expected, tolerance;
    tolerance["delta"]  = 1.0e-4;
    tolerance["gamma"]  = 1.0e-4;
    tolerance["theta"]  = 1.0e-4;
    tolerance["rho"]    = 1.0e-4;
    tolerance["divRho"] = 1.0e-4;
    tolerance["vega"]   = 1.0e-4;

    Option::Type types[] = { Option::Call, Option::Put, Option::Straddle };
    double strikes[] = { 50.0, 100.0, 150.0 };
    double underlyings[] = { 100.0 };
    Rate qRates[] = { -0.05, 0.0, 0.05 };
    Rate rRates[] = { 0.0, 0.01, 0.2 };
    Time residualTimes[] = { 1.0 };
    double vols[] = { 0.11 };
    double jInt[] = { 1.0, 5.0 };
    double mLJ[] = { -0.20, 0.0, 0.20 };
    double jV[] = { 0.01, 0.25 };

    DayCounter dc = Actual360();
    Handle<SimpleQuote> spot(new SimpleQuote(0.0));
    Handle<SimpleQuote> qRate(new SimpleQuote(0.0));
    Handle<TermStructure> qTS = makeFlatCurve(qRate, dc);
    Handle<SimpleQuote> rRate(new SimpleQuote(0.0));
    Handle<TermStructure> rTS = makeFlatCurve(rRate, dc);
    Handle<SimpleQuote> vol(new SimpleQuote(0.0));
    Handle<BlackVolTermStructure> volTS = makeFlatVolatility(vol, dc);

    Handle<SimpleQuote> jumpIntensity(new SimpleQuote(0.0));
    Handle<SimpleQuote> meanLogJump(new SimpleQuote(0.0));
    Handle<SimpleQuote> jumpVol(new SimpleQuote(0.0));
    
    Handle<BlackScholesStochasticProcess> stochProcess(new
        Merton76StochasticProcess(
            RelinkableHandle<Quote>(spot),
            RelinkableHandle<TermStructure>(qTS),
            RelinkableHandle<TermStructure>(rTS),
            RelinkableHandle<BlackVolTermStructure>(volTS),
            RelinkableHandle<Quote>(jumpIntensity),
            RelinkableHandle<Quote>(meanLogJump),
            RelinkableHandle<Quote>(jumpVol)));

    Handle<StrikedTypePayoff> payoff;

    Date today = Date::todaysDate();

    Handle<VanillaEngine> baseEngine(new AnalyticEuropeanEngine);
    Handle<PricingEngine> engine(new JumpDiffusionEngine(baseEngine));

    for (Size i=0; i<LENGTH(types); i++) {
      for (Size j=0; j<LENGTH(strikes); j++) {
      for (Size jj1=0; jj1<LENGTH(jInt); jj1++) {
        jumpIntensity->setValue(jInt[jj1]);
      for (Size jj2=0; jj2<LENGTH(mLJ); jj2++) {
        meanLogJump->setValue(mLJ[jj2]);
      for (Size jj3=0; jj3<LENGTH(jV); jj3++) {
        jumpVol->setValue(jV[jj3]);
        for (Size k=0; k<LENGTH(residualTimes); k++) {
          Date exDate = today.plusDays(int(residualTimes[k]*360+0.5));
          Handle<Exercise> exercise(new EuropeanExercise(exDate));
          Date exDateP = exDate.plusDays(1),
               exDateM = exDate.plusDays(-1);
          Time dT = (exDateP-exDateM)/360.0;
          for (Size kk=0; kk<1; kk++) {
              // option to check
              if (kk==0) {
                  payoff = Handle<StrikedTypePayoff>(new
                    PlainVanillaPayoff(types[i], strikes[j]));
              } else if (kk==1) {
                  payoff = Handle<StrikedTypePayoff>(new
                    CashOrNothingPayoff(types[i], strikes[j],
                    100.0));
              } else if (kk==2) {
                  payoff = Handle<StrikedTypePayoff>(new
                    AssetOrNothingPayoff(types[i], strikes[j]));
              } else if (kk==3) {
                  payoff = Handle<StrikedTypePayoff>(new
                    GapPayoff(types[i], strikes[j], 100.0));
              }
              Handle<VanillaOption> option(new
                  VanillaOption(stochProcess, payoff, exercise, engine));
              // time-shifted exercise dates and options
              Handle<Exercise> exerciseP(new EuropeanExercise(exDateP));
              Handle<VanillaOption> optionP(new
                  VanillaOption(stochProcess, payoff, exerciseP, engine));
              Handle<Exercise> exerciseM(new EuropeanExercise(exDateM));
              Handle<VanillaOption> optionM(new
                  VanillaOption(stochProcess, payoff, exerciseM, engine));

              for (Size l=0; l<LENGTH(underlyings); l++) {
                double u = underlyings[l];
                for (Size m=0; m<LENGTH(qRates); m++) {
                  double q = qRates[m];
                  for (Size n=0; n<LENGTH(rRates); n++) {
                    double r = rRates[n];
                    for (Size p=0; p<LENGTH(vols); p++) {
                      double v = vols[p];
                      spot->setValue(u);
                      qRate->setValue(q);
                      rRate->setValue(r);
                      vol->setValue(v);

                      double value         = option->NPV();
                      calculated["delta"]  = option->delta();
                      calculated["gamma"]  = option->gamma();
//                      calculated["theta"]  = option->theta();
                      calculated["rho"]    = option->rho();
                      calculated["divRho"] = option->dividendRho();
//                      calculated["vega"]   = option->vega();

                      if (value > spot->value()*1.0e-5) {
                          // perturb spot and get delta and gamma
                          double du = u*1.0e-5;
                          spot->setValue(u+du);
                          double value_p = option->NPV(),
                                 delta_p = option->delta();
                          spot->setValue(u-du);
                          double value_m = option->NPV(),
                                 delta_m = option->delta();
                          spot->setValue(u);
                          expected["delta"] = (value_p - value_m)/(2*du);
                          expected["gamma"] = (delta_p - delta_m)/(2*du);

                          // perturb rates and get rho and dividend rho
                          double dr = r*1.0e-5;
                          rRate->setValue(r+dr);
                          value_p = option->NPV();
                          rRate->setValue(r-dr);
                          value_m = option->NPV();
                          rRate->setValue(r);
                          expected["rho"] = (value_p - value_m)/(2*dr);

                          double dq = q*1.0e-4;
                          qRate->setValue(q+dq);
                          value_p = option->NPV();
                          qRate->setValue(q-dq);
                          value_m = option->NPV();
                          qRate->setValue(q);
                          expected["divRho"] = (value_p - value_m)/(2*dq);

                          // perturb volatility and get vega
                          double dv = v*1.0e-4;
                          vol->setValue(v+dv);
                          value_p = option->NPV();
                          vol->setValue(v-dv);
                          value_m = option->NPV();
                          vol->setValue(v);
                          // expected["vega"] = (value_p - value_m)/(2*dv);

                          // get theta from time-shifted options
                          // expected["theta"] = (optionM->NPV() - optionP->NPV())/dT;

                          // compare
                          std::map<std::string,double>::iterator it;
                          for (it = expected.begin(); 
                               it != expected.end(); ++it) {
                              std::string greek = it->first;
                              double expct = expected  [greek],
                                     calcl = calculated[greek],
                                     tol   = tolerance [greek];
                              double error = QL_FABS(expct-calcl);
                              if (error>tol) {
                                  jumpOptionTestFailed(greek, payoff, exercise,
                                                       u, q, r,
                                                       today, dc,
                                                       v,
                                                       jInt[jj1],
                                                       mLJ[jj2], jV[jj3],
                                                       expct, calcl,
                                                       error, tol);
                              }
                          }
                      }
                    }
                  }
                }
              }
            }
        }
      } // strike loop
      }
      }
      }
    } // type loop
}

CppUnit::Test* JumpDiffusionTest::suite() {
    CppUnit::TestSuite* tests =
        new CppUnit::TestSuite("American option tests");

    tests->addTest(new CppUnit::TestCaller<JumpDiffusionTest>
        ("Testing Merton 76 jump diffusion model for European options",
        &JumpDiffusionTest::testMerton76));

    tests->addTest(new CppUnit::TestCaller<JumpDiffusionTest>
        ("Testing jump diffusion option greeks",
        &JumpDiffusionTest::testGreeks));

    return tests;
}

