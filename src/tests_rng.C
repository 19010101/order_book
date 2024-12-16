#include "random_walk.h"
#include <catch2/catch_all.hpp>

#define SPDLOG_DEBUG_ON
#define SPDLOG_TRACE_ON
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
    boost::random::mt19937 mt(0);
    //std::ofstream mkt("mkt.txt");
    for (int i = 0; i < 100; ++i) {
        std::ofstream params(fmt::format("params{:03d}.txt", i));
        experiment( mt, nullptr, &params );
    }
}


