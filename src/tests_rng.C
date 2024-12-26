#include "random_walk.h"
#include <boost/random/bernoulli_distribution.hpp>
#include <catch2/catch_all.hpp>

#ifndef SPDLOG_DEBUG_ON
#define SPDLOG_DEBUG_ON
#endif
#ifndef SPDLOG_TRACE_ON
#define SPDLOG_TRACE_ON
#endif
#include <spdlog/spdlog.h>


#include <fstream>
#include <numeric>
#include <algorithm>
#include <iterator>

#include "agents.h"

TEST_CASE( "mr", "[MeanReversion]" ) {
    using namespace SDB;
    std::vector <double> x; 
    boost::random::mt19937 mt(0);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%L] [%s:%#] [f:%!] %v");
    SECTION("regular") {
        REQUIRE( x.empty() );
        MeanReversion mr ; 
        x.emplace_back(1.);
        for (size_t i = 0; i < 100000; ++i) 
            x.emplace_back( mr.update( x.back(), 1, 1., 0.5, 0.001, mt ) );
        //std::ofstream out("mr.txt") ; std::for_each( x.begin(), x.end(), [&out](double xi) { out << xi << '\n' ; } ); out.close();
        const double mean = std::accumulate( x.begin(), x.end(), 0.0) / x.size() ; 
        const double stdev = std::sqrt( std::inner_product( x.begin(), x.end(), x.begin(), 0.0) / x.size() 
                - mean*mean ); 
        SPDLOG_INFO( "mean  {} stdev {}", mean, stdev );
        REQUIRE( stdev < 1 );
        REQUIRE( std::abs(mean-1) < 0.1 );
    }

    SECTION("bifurcation") {
        REQUIRE( x.empty() );
        BifurcatingMeanReversion mr; 
        x.emplace_back(1.);
        for (size_t i = 0; i < 100000; ++i) 
            x.emplace_back( mr.update( x.back(), 1, 1., 0.5, 0.001, mt ) );
        //std::ofstream out("bi.txt") ; std::for_each( x.begin(), x.end(), [&out](double xi) { out << xi << '\n' ; } ); out.close();
        const double mean = std::accumulate( x.begin(), x.end(), 0.0) / x.size() ; 
        const double stdev = std::sqrt( std::inner_product( x.begin(), x.end(), x.begin(), 0.0) / x.size() 
                - mean*mean ); 
        SPDLOG_INFO( "mean  {} stdev {}", mean, stdev );
        REQUIRE( stdev < 1 );
    }
    SECTION("lognormal") {
        REQUIRE( x.empty() );
        MeanReversion mr; 
        x.emplace_back(1.);
        for (size_t i = 0; i < 100000; ++i) 
            x.emplace_back( mr.update( x.back(), 1, 1., 0.5, 0.001, mt ) );
        std::vector<double> ln ; 
        ln.reserve(x.size());
        std::transform(x.begin(), x.end(), std::back_inserter( ln ), [](double x) { return std::exp(x); } ) ; 
        //std::ofstream out("ln.txt") ; std::for_each( ln.begin(), ln.end(), [&out](double xi) { out << xi << '\n' ; } ); out.close();
        const double mean = std::accumulate( ln.begin(), ln.end(), 0.0) / ln.size() ; const double stdev = std::sqrt( std::inner_product( ln.begin(), ln.end(), ln.begin(), 0.0) / ln.size() 
                - mean*mean );
        SPDLOG_INFO( "mean  {} stdev {}", mean, stdev );
        //REQUIRE( stdev < 1 );
    }
}


TEST_CASE( "cancellation frequency", "[Agents]" ) {
    using namespace SDB;
    std::ofstream out("ext.txt");
    boost::random::mt19937 mt(0);
    const double dt = 1;
    const double TMax = 23*60*60 ; 
    const double freq_lower_limit = 1,  freq_center = 1/60., freq_width = 1./(60*60) ; 
    std::vector< RandomWalkWithState<MeanReversion> > lcs(
            100, 
            RandomWalkWithState<MeanReversion> ( 0 ,  0 , .00001 , .01 ) );
    for( double t = 0; t <= TMax; t+= dt) {
        double sum = 0; 
        double sumsq = 0; 
        for ( auto & rws : lcs ) {
            const double T = 1./sln( rws.x_, freq_center, freq_width, freq_lower_limit);
            sum += T ; 
            sumsq += T*T;
        }
        const double m = sum/lcs.size();
        const double s = std::sqrt(sumsq/lcs.size() - m*m );
        out << t/(60*60) << ' ' << m << ' ' <<  s ;
        for ( auto & rws : lcs ) {
            const double f = sln( rws.x_, freq_center, freq_width, freq_lower_limit);
            out << ' ' << 1/f << ' ' << rws.x_ ;
            rws.update( dt, mt );
        }
        out << '\n';
    }

}

TEST_CASE( "order size", "[Agents]" ) {
    using namespace SDB;
    std::ofstream out("ext.txt");
    boost::random::mt19937 mt(0);
    const double dt = 1;
    const double TMax = 23*60*60 ; 
    const double min_order_size = 1,  center_order_size = 10, max_order_size = 50; 
    const double center_shift = find_center_shift_for_range_bound( 
            min_order_size,
            center_order_size,  
            max_order_size);
    REQUIRE( not std::isnan( center_shift ) );
    std::vector< RandomWalkWithState<MeanReversion> > lcs(
            100, 
            RandomWalkWithState<MeanReversion> ( 0 ,  0 , .0001 , .01 ) );
    for( double t = 0; t <= TMax; t+= dt) {
        double sum = 0; 
        double sumsq = 0; 
        for ( auto & rws : lcs ) {
            //const double rb = (1+std::tanh( rws.x_ ));
            //const double S = rb*(max_order_size-min_order_size) + min_order_size; 
            const double S = range_bound( rws.x_, min_order_size,
                    max_order_size, center_shift);
            REQUIRE( not std::isnan( S ) );
            sum += S ; 
            sumsq += S*S;
        }
        const double m = sum/lcs.size();
        const double s = std::sqrt(sumsq/lcs.size() - m*m );
        out << t/(60*60) << ' ' << m << ' ' <<  s ;
        for ( auto & rws : lcs ) {
            //const double rb = (1+std::tanh( rws.x_ ));
            //const double S = rb*(max_order_size-min_order_size) + min_order_size; 
            const double S = range_bound( rws.x_, min_order_size,
                    max_order_size, center_shift);
            out << ' ' << S << ' ' << rws.x_ ;
            rws.update( dt, mt );
        }
        out << '\n';
    }

}

TEST_CASE( "price distribution", "[Agents]" ) {
    using namespace SDB;
    std::ofstream out("ext.txt");
    boost::random::mt19937 mt(0);
    const double dt = 1;
    const double TMax = 23*60*60 ; 
    std::vector< RandomWalkWithState<BifurcatingMeanReversion> > lcs(
            100, 
            RandomWalkWithState<BifurcatingMeanReversion>( 0 ,  1 , .001 , .01 ) );
    for( double t = 0; t <= TMax; t+= dt) {
        double sum = 0; 
        double sumsq = 0; 
        for ( auto & rws : lcs ) {
            //const double rb = (1+std::tanh( rws.x_ ));
            //const double S = rb*(max_order_size-min_order_size) + min_order_size; 
            const double S = rws.x_;
            REQUIRE( not std::isnan( S ) );
            sum += S ; 
            sumsq += S*S;
        }
        const double m = sum/lcs.size();
        const double s = std::sqrt(sumsq/lcs.size() - m*m );
        out << t/(60*60) << ' ' << m << ' ' <<  s ;
        for ( auto & rws : lcs ) {
            out << ' ' << rws.x_ ;
            rws.update( dt, mt );
        }
        out << '\n';
    }

}

TEST_CASE( "experiment", "[Agents]" ) {
    using namespace SDB;
    spdlog::set_level(spdlog::level::trace);
    boost::random::mt19937 mt(0);
    RandomPriceMakerEnsemble ensemble( 100, mt );
    for (int i = 0; i < 100; ++i) {
        std::ofstream torch(fmt::format("torch{:03d}.pt", i), std::ios::out|std::ios::binary);
        std::ofstream params(fmt::format("params{:03d}.txt", i));
        experiment( mt, &torch, &params, ensemble );
    }
}

TEST_CASE( "price", "[Agents]" ) {
    using namespace SDB;
    spdlog::set_level(spdlog::level::trace);
    boost::random::mt19937 mt(0);
    const std::vector<double> price_mean( {
    -1,  -0.5, -0.1, -0.01, 
    0.01, 0.1,  0.5, 1 } );
    for (const auto pm : price_mean) { 
        FixedPriceMakerEnsemble ensemble( 10, mt );
        ensemble.update( 10, pm, 10 );
        //std::ofstream torch(fmt::format("torch{:06.2f}.pt", pm), std::ios::out|std::ios::binary);
        std::ofstream params(fmt::format("params_pm{:07.2f}.txt", pm));
        experiment( mt, nullptr, &params, ensemble );
    }
}

TEST_CASE( "dir", "[Agents]" ) {
    using namespace SDB;
    spdlog::set_level(spdlog::level::trace);
    boost::random::mt19937 mt(0);
    const std::vector<double> price_mean( { -1, 0,  1} );
    const std::vector<double> cancellation_periods( { 1, 10,  50, 100, 1000 } );
    const std::vector<double> order_size( { 1, 10,  50, 100, 1000 } );
    constexpr size_t nsim = 100;
    std::ofstream out("dir.txt");
    for (const auto & pm : price_mean) 
        for (const auto & cp : cancellation_periods) 
            for (const auto & os : order_size) { 
                double sums = 0, sumsq = 0;
                for (size_t j = 0; j < nsim; ++j) {
                    FixedPriceMakerEnsemble ensemble( 10, mt );
                    ensemble.update( 10, 0, 10);
                    ensemble.price_makers_.back().update( cp, pm, os);
                    //std::ofstream torch(fmt::format("torch{:06.2f}.pt", pm), std::ios::out|std::ios::binary);
                    //std::ofstream params(fmt::format("params_cp{:07.2f}.txt", cp));
                    experiment( mt, nullptr, nullptr, ensemble );
                    sums += ensemble.market_.wm_;
                    sumsq += ensemble.market_.wm_*ensemble.market_.wm_;
                }
                const double m = sums/nsim ; 
                const double s = std::sqrt( sumsq/nsim - m*m ); 
                SPDLOG_INFO( "pm {}, cp {}, os {}, mean {}, std {}",pm, cp, os , m, s );
                out << fmt::format( "{} {} {} {} {}",pm, cp, os , m, s ) << std::endl;
            }
}
namespace SDB { 
    struct SwitchEnsemble : public FixedPriceMakerEnsemble { 
        boost::random::mt19937 & mt_;
        boost::random::exponential_distribution<> switch_distribution_;
        boost::random::bernoulli_distribution<> buy_sell_distribution_;
        std::vector<double> switch_times_ ;
        std::vector<double>::const_iterator it_;
        SwitchEnsemble( const size_t n_agents, boost::random::mt19937 & mt, 
                const double switch_lambda, const double t_max) : 
            FixedPriceMakerEnsemble(n_agents, mt ) , 
            mt_(mt),
            switch_distribution_(switch_lambda), 
            buy_sell_distribution_(0.5)
            {
                while (switch_times_.empty() or switch_times_.back() < t_max ) 
                    switch_times_.push_back( 
                            switch_distribution_(mt) + 
                            (switch_times_.empty() ? 0 : switch_times_.back()) );
                it_ = switch_times_.begin();

                for (auto & pm : price_makers_) 
                    pm.pm_.aggressive_.param( 
                            boost::random::bernoulli_distribution<double>::param_type( 0.0 ) 
                            );
            } 
        bool do_update() {
            bool update = false;
            //SPDLOG_INFO( "it : {} , time : {}", *it_ , 1e-9*market_.time_ );
            while (it_ != switch_times_.end() and 1e-9*market_.time_ > *it_ ) {
                //SPDLOG_INFO( "will update it : {} , time : {}", *it_ , 1e-9*market_.time_/60./60. );
                update = true; 
                ++it_ ; 
            }
            return update;
        }
        void adjust_directional_agents() { 
                const double p = buy_sell_distribution_(mt_) ? 3 : -3;
                const auto i = price_makers_.size()-1;
                //SPDLOG_INFO( "update p to {} at time : {}", p , 1e-9*market_.time_/60./60. );
                price_makers_[i].update( 
                        0.5*60*60,
                        p,
                        500000);
                price_makers_[i].pm_.aggressive_.param( 
                        boost::random::bernoulli_distribution<double>::param_type( 0.0 ) 
                        );
        }
        void update( const double ) { 
            //if (do_update()) adjust_directional_agents();
        }
        void update( 
                const double cancellation_period, 
                const double price_mean,  
                const double order_size_mean ) { 
            FixedPriceMakerEnsemble::update( cancellation_period, price_mean, order_size_mean );
        }

    };
}
TEST_CASE( "experiment2", "[Agents]" ) {
    using namespace SDB;
    spdlog::set_level(spdlog::level::trace);
    boost::random::mt19937 mt(0);
    constexpr double t_max = 24*60*60 ; //seconds
    constexpr double switch_lambda = 1./(60*60);
    SwitchEnsemble ensemble( 10, mt, switch_lambda, t_max );
    ensemble.update( 1, 0, 10);
    ensemble.price_makers_.back().update( 1000, 0, 50);
    REQUIRE( not ensemble.switch_times_.empty() );
    int n_expected = 0;
    for (auto & t : ensemble.switch_times_) {
        if (t < t_max) ++n_expected ;  
        //SPDLOG_INFO( "{} {}", t/60/60, n_expected );
    }

    REQUIRE( ensemble.market_.time_ == 0 );
    int n = 100000;
    int n_observed = 0; 
    while (ensemble.market_.time_*1e-9 < t_max and n > 0) {
        if ( ensemble.do_update() ) {
            ++n_observed;
            //SPDLOG_INFO( "upd at {} {}", ensemble.market_.time_*1e-9/60/60, n_observed );
        }
        //else SPDLOG_INFO( "no upd at {}", ensemble.market_.time_*1e-9 );
        ensemble.market_.time_ += 1e9*0.01*60*60; 
        //SPDLOG_INFO( "inc {}", ensemble.market_.time_ * 1e-9 );
        --n;
    }
    
    if (n_expected != n_observed) SPDLOG_INFO( "expected {}, observed {}", n_expected, n_observed );
    CHECK( n_expected == n_observed );
    ensemble.market_.time_ = 0;
    std::ofstream params(fmt::format("params.txt"));
    experiment( mt, nullptr, &params, ensemble );
}

TEST_CASE( "experiment3", "[Agents]" ) {
    using namespace SDB;
    spdlog::set_level(spdlog::level::trace);
    boost::random::mt19937 mt(0);
    constexpr int ndays = 1;
    constexpr double t_max = ndays*2*60*60 ; //seconds
    constexpr double switch_lambda = 1./(60*60);
    SwitchEnsemble ensemble( 10, mt, switch_lambda, t_max );
    ensemble.update( 10, 0, 1);
    ensemble.adjust_directional_agents();
    REQUIRE( not ensemble.switch_times_.empty() );
    REQUIRE( ensemble.market_.time_ == 0 );
    std::ofstream params(fmt::format("params.txt"));
    experiment( 
            mt, nullptr, &params , ensemble , 
            static_cast<TimeType>( t_max * 1e9 )
    );
}

