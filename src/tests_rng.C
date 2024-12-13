#include <catch2/catch_all.hpp>
#include "random_walk.h"

#define SPDLOG_DEBUG_ON
#define SPDLOG_TRACE_ON
#include <spdlog/spdlog.h>

#include <fstream>
#include <numeric>
#include <algorithm>
#include <iterator>

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
