#include "memory_manager.h"
#include "ob.h"
#include "sim.h"
#include <sstream>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE( "test ordering", "[Level]" ) {
    using namespace SDB;
    MemoryManager<Order> mem;
    Level::SET orders ; 
    orders.clear();
    orders.emplace( 100, Side::Bid, mem );
    orders.emplace( 102, Side::Bid, mem );
    orders.emplace( 101, Side::Bid, mem );
    auto it = orders.begin();
    REQUIRE(it != orders.end() );
    CHECK( 102 == it->price_ );
    ++it;
    REQUIRE(it != orders.end() );
    CHECK( 101 == it->price_ );
    ++it;
    REQUIRE(it != orders.end() );
    CHECK( 100 == it->price_ );

    orders.clear();
    orders.emplace( 100, Side::Offer, mem );
    orders.emplace( 102, Side::Offer, mem );
    orders.emplace( 101, Side::Offer, mem );
    it = orders.begin();
    REQUIRE(it != orders.end() );
    CHECK( 100 == it->price_ );
    ++it;
    REQUIRE(it != orders.end() );
    CHECK( 101 == it->price_ );
    ++it;
    REQUIRE(it != orders.end() );
    CHECK( 102 == it->price_ );
}

TEST_CASE( "test prices agree", "[Level]" ) {
    using namespace SDB;
    MemoryManager<Order> mem; 
    Level bids( 100, Side::Bid, mem );
    Order & o = mem.get_unused();
    o.reset( 0, 0, 0, 100, 5, 1, Side::Offer, false , NOOPNotify::instance()) ;
    CHECK( bids.do_prices_agree( o ) );
    o.reset( 0, 0, 0,  99, 5, 1, Side::Offer , false, NOOPNotify::instance());
    CHECK( bids.do_prices_agree(  o ) );
    o.reset( 0, 0, 0,  101, 5, 1, Side::Offer, false, NOOPNotify::instance() );
    CHECK( not bids.do_prices_agree( o ) );
    Level offers( 100, Side::Offer , mem);
    o.reset( 0, 0, 0, 100, 5, 1, Side::Bid, false, NOOPNotify::instance() );
    CHECK( offers.do_prices_agree( o ) );
    o.reset( 0, 0, 0,  99, 5, 1, Side::Bid, false, NOOPNotify::instance() );
    CHECK( not offers.do_prices_agree( o ) );
    o.reset( 0, 0, 0,  101, 5, 1, Side::Bid, false, NOOPNotify::instance() );
    CHECK( offers.do_prices_agree( o ) );
}

TEST_CASE( "match orderbook full", "[Level]" ) {
    using namespace SDB;
    { //full match 
        MemoryManager<Order> mem; 
        Order::PtrSet set;   
        OrderIDType oid = 0;
        Level bids( 100, Side::Bid, mem ) ; 
        Level offers( 101, Side::Offer, mem ) ; 
        for (TimeType t = 0 ; t < 10; ++t) { 
            bids.add_order( get_new_order(mem,oid++, t, 0, 100,  t+1, t+1, Side::Bid , false), set );
            offers.add_order( get_new_order(mem,oid++, t, 0, 101,  t+1, t+1, Side::Offer, false ), set );
        }

        Order & new_offer = get_new_order(mem,oid++, 100, 0, 100, 55 ,55,  Side::Offer, false );
        bids.match(new_offer, set, 0 );
        CHECK( 0 == new_offer.remaining_size_ );
        CHECK( bids.orders_.empty() );

        Order & new_bid = get_new_order(mem,oid++, 100, 0, 101, 55 ,55,  Side::Bid, false );
        offers.match(new_bid, set, 0);
        CHECK( 0 == new_bid.remaining_size_ );
        CHECK( offers.orders_.empty() );
    }

    { //full match 
        MemoryManager<Order> mem; 
        Order::PtrSet set;   
        OrderIDType oid = 0;
        Level bids( 100, Side::Bid,mem ) ; 
        Level offers( 101, Side::Offer,mem ) ; 
        for (TimeType t = 0 ; t < 10; ++t) { 
            bids.add_order( get_new_order(mem,oid++, t, 0, 100,  t+1, t+1, Side::Bid, false ), set );
            offers.add_order( get_new_order(mem,oid++, t, 0, 101,  t+1, t+1, Side::Offer, false ), set );
        }

        Order & new_offer = get_new_order(mem,oid++, 100, 0, 99, 55 ,55,  Side::Offer, false );
        bids.match(new_offer, set, 0);
        CHECK( 0 == new_offer.remaining_size_ );
        CHECK( bids.orders_.empty() );

        Order & new_bid = get_new_order(mem,oid++, 100, 0, 101, 55 ,55,  Side::Bid, false );
        offers.match(new_bid, set, 0);
        CHECK( 0 == new_bid.remaining_size_ );
        CHECK( offers.orders_.empty() );
    }
    { //no match 
        MemoryManager<Order> mem; 
        Order::PtrSet set;   
        OrderIDType oid = 0;
        Level bids( 100, Side::Bid,mem ) ; 
        Level offers( 101, Side::Offer,mem ) ; 
        for (TimeType t = 0 ; t < 10; ++t) { 
            bids.add_order( get_new_order(mem,oid++, t, 0, 100,  t+1, t+1, Side::Bid, false ), set );
            offers.add_order( get_new_order(mem,oid++, t, 0, 101,  t+1, t+1, Side::Offer, false ), set );
        }

        Order & new_offer = get_new_order(mem,oid++, 100, 0, 101, 55 ,55,  Side::Offer, false );
        bids.match(new_offer, set, 0);
        CHECK( 55 == new_offer.remaining_size_ );
        CHECK( 10 == bids.orders_.size() );

        Order & new_bid = get_new_order(mem,oid++, 100, 0, 100, 55 ,55,  Side::Bid, false );
        offers.match(new_bid, set, 0);
        CHECK( 55 == new_bid.remaining_size_ );
        CHECK( 10 == offers.orders_.size() );
    }

}
TEST_CASE( "match orderbook hidden", "[Level]" ) {
    using namespace SDB;
    MemoryManager<Order> mem; 
    Order::PtrSet set;   
    Level bids( 100, Side::Bid , mem) ; 
    for (TimeType t = 0 ; t < 10; ++t) { 
        bids.add_order( get_new_order(mem,t, t, 0, 100,  10, 2, Side::Bid, false ), set );
    }
    CHECK( 10 == bids.orders_.front().remaining_size_ );
    CHECK( 2 == bids.orders_.front().shown_size_ );
    //trade 1
    Order & new_offer = get_new_order(mem,100, 100, 0, 100, 1 ,1,  Side::Offer, false );
    bids.match(new_offer, set, 0);
    CHECK( 0 == new_offer.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    CHECK( 0 == bids.orders_.front().order_id_ );
    CHECK( 9 == bids.orders_.front().remaining_size_ );
    CHECK( 1 == bids.orders_.front().shown_size_ );
    //trade another 1
    Order & new_offer2 = get_new_order(mem,100, 100, 0, 100, 1 ,1,  Side::Offer, false );
    bids.match(new_offer2, set, 0);
    CHECK( 0 == new_offer2.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    CHECK( 0 == bids.orders_.back().order_id_ );
    CHECK( 8 == bids.orders_.back().remaining_size_ );
    CHECK( 2 == bids.orders_.back().shown_size_ );
    Order & new_offer3 = get_new_order(mem,100, 100, 0, 100, 4 ,2,  Side::Offer, false );
    bids.match(new_offer3, set, 0);
    CHECK( 0 == new_offer3.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    OrderIDType i = 3; 
    for (const auto & o : bids.orders_ ) {
        CHECK( i == o.order_id_ );
        i = (i+1) % 10;
    }
}

TEST_CASE( "add order", "[MatchingEngine]" ) {
    using namespace SDB;
    std::array<PriceType, 5> bid_prices, ask_prices;
    std::array<SizeType, 5> bid_sizes, ask_sizes;
    {
        MatchingEngine eng;
        for (TimeType t = 0 ; t < 10; ++t) { 
            eng.add_simulation_order( 0, 100,  10, 2, Side::Bid, false , NOOPNotify::instance() );
            eng.add_simulation_order( 0, 101,  10, 3, Side::Offer, false, NOOPNotify::instance() );
            eng.level2( bid_prices, bid_sizes, ask_prices, ask_sizes);
            CHECK( 100 == bid_prices[0] );
            CHECK( (t+1)*2 == bid_sizes[0] );
            CHECK( 101 == ask_prices[0] );
            CHECK( (t+1)*3 == ask_sizes[0] );
            for (size_t i = 1;  i< bid_sizes.size() ; ++i) {
                CHECK( 0 == bid_prices[i] );
                CHECK( 0 == bid_sizes[i] );
            }
            for (size_t i = 1;  i< ask_sizes.size() ; ++i) {
                CHECK( 0 == ask_prices[i] );
                CHECK( 0 == ask_sizes[i] );
            }
        }
    }
    {
        MatchingEngine eng;
        for (int dp  = 0; dp < 5; ++dp ) {
            eng.add_simulation_order( 0, 100 - dp ,  10 + dp   , 2, Side::Bid, false, NOOPNotify::instance() );
            eng.add_simulation_order( 0, 100 - dp ,  10 + dp*2 , 3, Side::Bid, false, NOOPNotify::instance() );

            eng.add_simulation_order( 0, 101 + dp ,  10 + dp   , 4, Side::Offer, false, NOOPNotify::instance() );
            eng.add_simulation_order( 0, 101 + dp ,  10 + dp*2 , 5, Side::Offer, false, NOOPNotify::instance() );

        }
        eng.level2( bid_prices, bid_sizes, ask_prices, ask_sizes);
        CHECK( 100  == bid_prices[0] );
        CHECK( 99   == bid_prices[1] );
        CHECK( 98   == bid_prices[2] );
        CHECK( 97   == bid_prices[3] );
        CHECK( 96   == bid_prices[4] );

        CHECK( 101  == ask_prices[0] );
        CHECK( 102  == ask_prices[1] );
        CHECK( 103  == ask_prices[2] );
        CHECK( 104  == ask_prices[3] );
        CHECK( 105  == ask_prices[4] );
        for ( int i = 0; i < 5; ++i) {
            CHECK( 5  == bid_sizes[i] );
            CHECK( 9  == ask_sizes[i] );
        }
        //agressive order. first order will replenish back to 4, second will be left to 1:
        eng.add_simulation_order( 0, 102  ,  8   , 2, Side::Bid, false, NOOPNotify::instance() ); //agressive buy
        eng.level2( bid_prices, bid_sizes, ask_prices, ask_sizes);
        CHECK( 5  == bid_sizes[0] );
        CHECK( 5  == ask_sizes[0] ); //this one is 4+1
        CHECK( 100  == bid_prices[0] );
        CHECK( 99   == bid_prices[1] );
        CHECK( 98   == bid_prices[2] );
        CHECK( 97   == bid_prices[3] );
        CHECK( 96   == bid_prices[4] );

        CHECK( 101  == ask_prices[0] );
        CHECK( 102  == ask_prices[1] );
        CHECK( 103  == ask_prices[2] );
        CHECK( 104  == ask_prices[3] );
        CHECK( 105  == ask_prices[4] );
        for ( int i = 1; i < 5; ++i) {
            CHECK( 5  == bid_sizes[i] );
            CHECK( 9  == ask_sizes[i] );
        }
    }
}

TEST_CASE( "add order create new level", "[MatchingEngine]" ) {
    using namespace SDB;
    std::array<PriceType, 5> bid_prices, ask_prices;
    std::array<SizeType, 5> bid_sizes, ask_sizes;
    {
        MatchingEngine eng;
        for (int dp  = 0; dp < 5; ++dp ) {
            eng.add_simulation_order( 0, 100 - dp ,  10 + dp   , 2, Side::Bid, false, NOOPNotify::instance() );
            eng.add_simulation_order( 0, 100 - dp ,  10 + dp*2 , 3, Side::Bid, false, NOOPNotify::instance() );

            eng.add_simulation_order( 0, 101 + dp ,  10 + dp   , 4, Side::Offer, false, NOOPNotify::instance() );
            eng.add_simulation_order( 0, 101 + dp ,  10 + dp*2 , 5, Side::Offer, false, NOOPNotify::instance() );

        }
        //agressive order. first order will replenish back to 4, second will be left to 1:
        eng.add_simulation_order( 0, 100  ,  100   , 7, Side::Offer, false, NOOPNotify::instance() ); //agressive sell
        eng.level2( bid_prices, bid_sizes, ask_prices, ask_sizes);
        //top level bid is gone:
        CHECK( 99   == bid_prices[0] ); CHECK( 5  == bid_sizes[0] );
        CHECK( 98   == bid_prices[1] ); CHECK( 5  == bid_sizes[1] );
        CHECK( 97   == bid_prices[2] ); CHECK( 5  == bid_sizes[2] );
        CHECK( 96   == bid_prices[3] ); CHECK( 5  == bid_sizes[3] );
        CHECK( 0    == bid_prices[4] ); CHECK( 0  == bid_sizes[4] );

        //new top level offer is our agressive order:
        CHECK( 100  == ask_prices[0] ); CHECK( 1  == ask_sizes[0] );
        //remainder is same:
        CHECK( 101  == ask_prices[1] ); CHECK( 9  == ask_sizes[1] );
        CHECK( 102  == ask_prices[2] ); CHECK( 9  == ask_sizes[2] );
        CHECK( 103  == ask_prices[3] ); CHECK( 9  == ask_sizes[3] );
        CHECK( 104  == ask_prices[4] ); CHECK( 9  == ask_sizes[4] );
    }
}

TEST_CASE( "sweep stack", "[MatchingEngine]" ) {
    using namespace SDB;
    std::array<PriceType, 5> bid_prices, ask_prices;
    std::array<SizeType, 5> bid_sizes, ask_sizes;
    {
        MatchingEngine eng;
        for (int dp  = 0; dp < 5; ++dp ) {
            eng.add_simulation_order( 0, 100 - dp ,  10 + dp   , 2, Side::Bid, false, NOOPNotify::instance() );
            eng.add_simulation_order( 0, 100 - dp ,  10 + dp*2 , 3, Side::Bid, false, NOOPNotify::instance() );

            eng.add_simulation_order( 0, 101 + dp ,  10 + dp   , 4, Side::Offer, false, NOOPNotify::instance() );
            eng.add_simulation_order( 0, 101 + dp ,  10 + dp*2 , 5, Side::Offer, false, NOOPNotify::instance() );

        }
        //agressive order. first order will replenish back to 4, second will be left to 1:
        eng.add_simulation_order( 0, 99  ,  100   , 7, Side::Offer, false, NOOPNotify::instance() ); //agressive sell
        eng.level2( bid_prices, bid_sizes, ask_prices, ask_sizes);
        //top two levels of bids are gone:
        CHECK( 98   == bid_prices[0] ); CHECK( 5  == bid_sizes[0] );
        CHECK( 97   == bid_prices[1] ); CHECK( 5  == bid_sizes[1] );
        CHECK( 96   == bid_prices[2] ); CHECK( 5  == bid_sizes[2] );
        CHECK( 0    == bid_prices[3] ); CHECK( 0  == bid_sizes[3] );
        CHECK( 0    == bid_prices[4] ); CHECK( 0  == bid_sizes[4] );

        //new top level offer is our agressive order:
        CHECK( 99   == ask_prices[0] ); CHECK( 6  == ask_sizes[0] );
        //remainder is same:
        CHECK( 101  == ask_prices[1] ); CHECK( 9  == ask_sizes[1] );
        CHECK( 102  == ask_prices[2] ); CHECK( 9  == ask_sizes[2] );
        CHECK( 103  == ask_prices[3] ); CHECK( 9  == ask_sizes[3] );
        CHECK( 104  == ask_prices[4] ); CHECK( 9  == ask_sizes[4] );
    }
}
TEST_CASE( "cancel order", "[MatchingEngine]" ) {
    using namespace SDB;
    MatchingEngine eng;
    eng.add_simulation_order( 0, 100,  10, 2, Side::Bid, false, NOOPNotify::instance() );
    CHECK_THROWS_AS( eng.cancel_order( eng.next_order_id_ ), 
            std::runtime_error) ;
    REQUIRE( not eng.all_bids_.empty() );
    CHECK( not eng.all_bids_.begin()->orders_.empty() );
    eng.cancel_order( 0 );//second order is deleted
    REQUIRE( eng.all_bids_.empty() );
}

namespace SDB {
    struct KeepMessagesNotifier { 
        using MSG = std::tuple< NotifyMessageType, ClientIDType, OrderIDType, TimeType, SizeType, PriceType > ; 
        std::vector<MSG> temp;
        void log( const NotifyMessageType mtype , const Order & o, const TimeType t, const SizeType s = 0, const PriceType p = 0) { 
            temp.emplace_back(mtype, o.client_id_, o.order_id_, t, s, p );
        };
        void log( const MatchingEngine & ) {

        }
        std::unordered_map<ClientIDType, std::unordered_map<PriceType, SizeType> > aggregate_fills() const { 
            std::unordered_map<ClientIDType, std::unordered_map<PriceType, SizeType> > ret ; 
            for ( const auto & [mtype,cid,oid,t,s,p] : temp ) 
                if (s>0) ret.emplace( cid, std::unordered_map<PriceType, SizeType>() ).first->second.emplace( p , 0 ).first->second += s ;
            return ret;
        }

    };

}
TEST_CASE( "reduce size", "[Order]" ) {
    using namespace SDB;
    CHECK(     Order::reduce_size( false, false ) );
    CHECK( not Order::reduce_size( false, true  ) );
    CHECK(     Order::reduce_size( true , false ) );
    CHECK(     Order::reduce_size( true , true  ) );
}

TEST_CASE( "shadow order", "[MatchingEngine]" ) {
    using namespace SDB;
    /*
     * shadow order requirements:
     * 1. when an agressive shadow matches a passive regular, it doesn't change the order book at all. 
     * 2. when a passive shadow matches an agressive regular, it goes back of the queue, but it doesn't change the size of agressive regular.
     * 3. when a passive shadow matches an agressive shadow, they trade as if both are regular. They go the back of the queue etc...
     *
     */

    { // 1. when an agressive shadow matches a passive regular, it doesn't change the order book at all. 
        MatchingEngine eng;
        KeepMessagesNotifier notifier ; 
        eng.add_simulation_order( 0, 100,  10, 2, Side::Bid  , false, notifier );
        eng.add_simulation_order( 1, 100,  10, 3, Side::Bid  , false, notifier );
        eng.add_simulation_order( 2, 100,   2, 2, Side::Offer, true, notifier );

        REQUIRE( eng.all_bids_.contains( 100 ) ); 
        auto & level = *eng.all_bids_.find(100);
        //book order is maintained.
        REQUIRE( 2 == level.orders_.size() ) ; 
        CHECK( 0  == level.orders_.front().client_id_ ) ; //order don't change.
        CHECK( 1  == level.orders_.back().client_id_ ) ; 
        CHECK( 10 == level.orders_.front().remaining_size_ ) ; //real order size don't change
        CHECK( 10 == level.orders_.back().remaining_size_ ) ; //real order size don't change
        const auto & fills = notifier.aggregate_fills();
        CHECK( 2 == fills.size() );
        REQUIRE( fills.contains(0) );
        REQUIRE( fills.find(0)->second.contains(100) );
        CHECK(2 == fills.find(0)->second.find(100)->second);

        REQUIRE( fills.contains(2) );
        REQUIRE( fills.find(2)->second.contains(100) );
        CHECK(2 == fills.find(2)->second.find(100)->second);
    }

    { // 2. when a passive shadow matches an agressive regular, it goes back of the queue, but it doesn't change the size of agressive regular.
        MatchingEngine eng;
        KeepMessagesNotifier notifier ; 
        eng.add_simulation_order( 0, 100,  10, 2, Side::Bid  , true , notifier ); //shadow at the front of queue
        eng.add_simulation_order( 1, 100,  10, 3, Side::Bid  , false, notifier );
        eng.add_simulation_order( 2, 100,   2, 2, Side::Offer, false, notifier );

        REQUIRE( eng.all_bids_.contains( 100 ) ); 
        auto & level = *eng.all_bids_.find(100);
        //book order is maintained.
        REQUIRE( 2 == level.orders_.size() ) ; 
        CHECK( 1 == level.orders_.front().client_id_ ) ; 
        CHECK( 0 == level.orders_.back().client_id_ ) ; //shadow is at the back
        CHECK( 8 == level.orders_.front().remaining_size_ ) ; //real order size is reduced
        CHECK( 8 == level.orders_.back().remaining_size_ ) ; //shadow size is reduced
        const auto & fills = notifier.aggregate_fills();
        CHECK( 3 == fills.size() ); //non-shadow will trade with shadow, AND non-shadow.
        REQUIRE( fills.contains(0) );
        REQUIRE( fills.find(0)->second.contains(100) );
        CHECK(2 == fills.find(0)->second.find(100)->second);

        REQUIRE( fills.contains(1) );
        REQUIRE( fills.find(1)->second.contains(100) );
        CHECK(2 == fills.find(1)->second.find(100)->second);

        REQUIRE( fills.contains(2) );
        REQUIRE( fills.find(2)->second.contains(100) );
        CHECK(4 == fills.find(2)->second.find(100)->second); //even though this order is real and its size is 2, it trades twice, once with shadow and once with real, so total fill size is 4.
    }
    { // 3. when a passive shadow matches an agressive shadow, they trade as if both are regular. They go the back of the queue etc...
        MatchingEngine eng;
        KeepMessagesNotifier notifier ; 
        eng.add_simulation_order( 0, 100,  10, 2, Side::Bid  , true , notifier ); //shadow at the front of queue
        eng.add_simulation_order( 1, 100,  10, 3, Side::Bid  , false, notifier );
        eng.add_simulation_order( 2, 100,   2, 2, Side::Offer, true , notifier ); //shadow aggresive

        REQUIRE( eng.all_bids_.contains( 100 ) ); 
        auto & level = *eng.all_bids_.find(100);
        //book order is maintained.
        REQUIRE( 2 == level.orders_.size() ) ; 
        CHECK( 1 == level.orders_.front().client_id_ ) ; 
        CHECK( 0 == level.orders_.back().client_id_ ) ; //shadow is at the back
        CHECK(10 == level.orders_.front().remaining_size_ ) ; //real order size is not reduced
        CHECK( 8 == level.orders_.back().remaining_size_ ) ; //shadow size is reduced
        const auto & fills = notifier.aggregate_fills();
        CHECK( 2 == fills.size() );
        REQUIRE( fills.contains(0) );
        REQUIRE( fills.find(0)->second.contains(100) );
        CHECK(2 == fills.find(0)->second.find(100)->second);

        REQUIRE( fills.contains(2) );
        REQUIRE( fills.find(2)->second.contains(100) );
        CHECK(2 == fills.find(2)->second.find(100)->second);
    }
}

TEST_CASE( "replenish", "[ClientState]" ) {
    using namespace SDB;
    MatchingEngine eng;
    ReplayData handler(eng.mem_);
    eng.add_replay_order( 0, 0, 100, 2, Side::Offer, false, handler );
    eng.add_replay_order( 1, 1, 100, 2, Side::Offer, false, handler );
    //order 0 replenished: 
    eng.add_replay_order( 0, 0, 100, 2, Side::Offer, false, handler );
    REQUIRE( eng.all_offers_.contains(100) );
    REQUIRE( eng.all_offers_.begin()->orders_.size() == 3 );
    CHECK( eng.all_offers_.begin()->orders_.front().order_id_ == 0 );
    CHECK( eng.all_offers_.begin()->orders_.back().order_id_ == 0 );
    REQUIRE_THROWS_AS( eng.cancel_order(0), std::runtime_error );
    REQUIRE( eng.all_offers_.begin()->orders_.size() == 3 );
    //aggresive order 
    eng.add_replay_order( 2, 2, 100, 6, Side::Bid, false, handler );
    REQUIRE( eng.all_offers_.begin()->orders_.empty() );
    std::vector<int> temp ; 
    temp.emplace_back(1);
    temp.emplace_back(1);
    temp.emplace_back(1);
    temp.emplace_back(2);
    auto it = std::upper_bound( temp.begin(), temp.end(), 1 );
    REQUIRE(3== std::distance( temp.begin(), it ) );
    CHECK( 2 == *it );
}
TEST_CASE( "record - replay basics", "[ClientState]" ) {
    using namespace SDB;
    MatchingEngine eng;
    ReplayData handler(eng.mem_);

    eng.set_time(0);
    eng.add_simulation_order( 0 , 100, 10, 1, Side::Offer, false , handler );
    eng.set_time(1);
    eng.add_simulation_order( 1 , 101, 10, 1, Side::Offer, false , handler );
    eng.set_time(2);
    eng.add_simulation_order( 2 , 101,  2, 2, Side::Bid  , false , handler );

    //average trade price should be 100: 
    for (const auto & m : handler.msgs_ ) {
        if (m.mtype_ == NotifyMessageType::Trade and m.cid_ == 2 )
            CHECK( m.trade_price_ == 100 );
    }

    MatchingEngine eng2;
    ReplayData handler2(eng2.mem_);

    //std::cerr << "replay" << std::endl;
    bool fail = false;

    try { 
        replay( handler.msgs_, eng2, handler2 );
    } catch (const std::runtime_error & e) { 
        std::cerr << e.what() << std::endl; 
        fail = true;
    }

    //average trade price should be 100: 
    for (const auto & m : handler2.msgs_ ) {
        if (m.mtype_ == NotifyMessageType::Trade and m.cid_ == 2 )
            CHECK( m.trade_price_ == 100 );
    }
    CHECK( not fail );
}

TEST_CASE( "record - replay", "[ClientState]" ) {
    using namespace SDB;

    int seed = 0; 
    boost::random::mt19937 mt;
    mt.seed(seed);

    ClientType type1( "type1",mt, 1./60., 1./(30*60.), 100. , 5. , 0.5 );  
    ClientType type2( "type2",mt, 1.,     1.,          10.  , 2. , 0.5 );  


    std::vector<std::tuple<ClientType, int>> client_types_and_sizes ; 
    client_types_and_sizes.emplace_back(
            ClientType( "type1",mt, 1./60., 1./(30*60.), 100. , 5. , 0.5 ), 
            1000 );

    client_types_and_sizes.emplace_back(
            ClientType( "type2",mt, 1.,     1.,          10.  , 2. , 0.5 ),
            1000 );


    MatchingEngine eng;
    ClientState::NotificationHandler<ReplayData> handler(eng);
    const TimeType TMax = 10 *1e9; 

    simulate( client_types_and_sizes, eng, handler, TMax );

    REQUIRE( not handler.msgs_.empty() );
    REQUIRE( not handler.snapshots_.empty() );

    MatchingEngine eng2;
    ReplayData handler2(eng2.mem_);

    replay( handler.msgs_, eng2, handler2 );

    /* for (size_t i = 0; i < std::min(handler.msgs_.size(), handler2.msgs_.size() ) ; ++i ) { 
        std::cout << i 
            << "\n\t" << handler.msgs_[i].str() 
            << "\n\t" << handler2.msgs_[i].str()
            << std::endl ;
    }*/

    CHECK( handler.msgs_.size() == handler2.msgs_.size() );
    CHECK( handler.snapshots_.size() == handler2.snapshots_.size() );

    int n_orders_checked = 0; 
    for (size_t i = 0; i < std::min(handler.snapshots_.size(), handler2.snapshots_.size() ) ; ++i ) { 
        const auto & [t1, levels1] = handler.snapshots_[i];
        const auto & [t2, levels2] = handler2.snapshots_[i];
        /*std::cout << i 
            << " t1:" << t1 << " t2:" << t2 
            << " nl1:" << levels1.size() << " nl2:" << levels2.size()
            << std::endl ;*/
        REQUIRE(t1==t2);
        REQUIRE(levels1.size()==levels2.size());
        for (size_t j = 0; j < levels1.size(); ++j) { 
            const auto & [price1,side1,orders1] = levels1.at(j);
            const auto & [price2,side2,orders2] = levels2.at(j);
            /*std::cout << i << ' ' << j 
                << " l1:" << price1 << " " <<  std::to_string(side1) << " " << orders1.size() 
                << " l2:" << price2 << " " <<  std::to_string(side2) << " " << orders2.size() 
                << std::endl ;*/
            REQUIRE( price1==price2 );
            REQUIRE( side1==side2 );
            REQUIRE(orders1.size()==orders2.size());
            auto it1 = orders1.begin(); 
            auto it2 = orders2.begin(); 
            for ( ; it1 != orders1.end(); ++it1, ++it2 ) { 
                /*if ( it1->creation_time_ != it2->creation_time_ ) std::cout << "creation times differ below:\n";
                std::cout << "order1 " <<  it1->order_id_ << " s:" << it1->shown_size_ << " rs:" << it1->remaining_size_ << " ct: " << it1->creation_time_*1e-9 << '\n';
                std::cout << "order2 " <<  it2->order_id_ << " s:" << it2->shown_size_ << " rs:" << it2->remaining_size_ << " ct: " << it2->creation_time_*1e-9 << '\n';*/
                REQUIRE( it1->price_ == price1 );
                REQUIRE( it1->side_ == side1 );
                REQUIRE( it2->price_ == price2 );
                REQUIRE( it2->side_ == side2 );
                REQUIRE( it1->order_id_ == it2->order_id_ );
                REQUIRE( it1->creation_time_ <= it2->creation_time_ ); //replenish orders' creation time is different
                REQUIRE( it1->client_id_ == it2->client_id_ );
                REQUIRE( it1->shown_size_ == it2->shown_size_ );
                REQUIRE( it1->is_shadow_ == it2->is_shadow_ );
                REQUIRE( it1->is_hidden_ == it2->is_hidden_ );
                ++n_orders_checked;
            }
        }

    }
    CHECK(n_orders_checked>0);
    //std::cout << "n_orders_checked " << n_orders_checked << std::endl;

}

