#pragma once
#include <boost/random/exponential_distribution.hpp> 
#include <boost/random/poisson_distribution.hpp> 
#include <boost/random/normal_distribution.hpp> 
#include <boost/random/bernoulli_distribution.hpp> 
#include <boost/random/mersenne_twister.hpp> 
namespace SDB { 

    struct MeanReversion { 
        boost::random::normal_distribution<double> W_ ;
        MeanReversion() : W_(0,1) {} 
        template <typename RNG>
            double update(
                    const double x,
                    const double x0, 
                    const double k, 
                    const double s, 
                    const double dt, 
                    RNG & rng
                    ) {
                const double W = W_(rng) ; 
                return x + dt*k*(x0-x) + std::sqrt(dt)*s*W ; 
            }
    };
    struct BifurcatingMeanReversion: MeanReversion { 
        boost::random::normal_distribution<double> W_ ;
        BifurcatingMeanReversion() : MeanReversion() {} 
        template <typename RNG>
            double update(
                    const double x,
                    const double x0, 
                    const double k, 
                    const double s, 
                    const double dt, 
                    RNG & rng
                    ) {
                if (x>0) return MeanReversion::update( x, x0, k, s, dt, rng) ; 
                else return MeanReversion::update( x, -x0, k, s, dt, rng) ; 
            }
    };

    template <typename RandomWalk>
    struct RandomWalkWithState {
        RandomWalk mr_ ; 
        double x_ , x0_ , k_, s_ ; 
        RandomWalkWithState(
                double x, 
                double x0, 
                double k, 
                double s) : x_(x), x0_(x0), k_(k), s_(s) {};

        template <typename RNG>
            double update( const double dt, RNG & rng) {
                return x_ = mr_.update( x_, x0_,  k_, s_, dt, rng );
            }
    };
}
