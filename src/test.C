#include "memory_manager.h"
#include "ob.h"
#include "sim.h"
#include "utils.h"
#include <boost/random/bernoulli_distribution.hpp>

#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

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
    o.reset( 0, 0, 100, 5, 1, Side::Offer, false , NOOPNotify::instance()) ;
    CHECK( bids.do_prices_agree( o ) );
    o.reset( 0, 0,  99, 5, 1, Side::Offer , false, NOOPNotify::instance());
    CHECK( bids.do_prices_agree(  o ) );
    o.reset( 0, 0,  101, 5, 1, Side::Offer, false, NOOPNotify::instance() );
    CHECK( not bids.do_prices_agree( o ) );
    Level offers( 100, Side::Offer , mem);
    o.reset( 0, 0, 100, 5, 1, Side::Bid, false, NOOPNotify::instance() );
    CHECK( offers.do_prices_agree( o ) );
    o.reset( 0, 0,  99, 5, 1, Side::Bid, false, NOOPNotify::instance() );
    CHECK( not offers.do_prices_agree( o ) );
    o.reset( 0, 0,  101, 5, 1, Side::Bid, false, NOOPNotify::instance() );
    CHECK( offers.do_prices_agree( o ) );
}

TEST_CASE( "match orderbook full", "[Level]" ) {
    using namespace SDB;
    { //full match 
        MemoryManager<Order> mem; 
        Order::PtrSet set;   
        OrderIDType oid ;
        oid.fill(0);
        Level bids( 100, Side::Bid, mem ) ; 
        Level offers( 101, Side::Offer, mem ) ; 
        for (TimeType t = 0 ; t < 10; ++t) { 
            bids.add_order( get_new_order(mem,oid, t, 0, 100,  t+1, t+1, Side::Bid , false), set );
            increment(oid);
            offers.add_order( get_new_order(mem,oid, t, 0, 101,  t+1, t+1, Side::Offer, false ), set );
            increment(oid);
        }

        Order & new_offer = get_new_order(mem,oid, 100, 0, 100, 55 ,55,  Side::Offer, false );
        increment(oid);
        bids.match(new_offer, set, 0 );
        CHECK( 0 == new_offer.remaining_size_ );
        CHECK( bids.orders_.empty() );

        Order & new_bid = get_new_order(mem,oid, 100, 0, 101, 55 ,55,  Side::Bid, false );
        increment(oid);
        offers.match(new_bid, set, 0);
        CHECK( 0 == new_bid.remaining_size_ );
        CHECK( offers.orders_.empty() );
    }

    { //full match 
        MemoryManager<Order> mem; 
        Order::PtrSet set;   
        OrderIDType oid ;
        oid.fill(0);
        Level bids( 100, Side::Bid,mem ) ; 
        Level offers( 101, Side::Offer,mem ) ; 
        for (TimeType t = 0 ; t < 10; ++t) { 
            bids.add_order( get_new_order(mem,oid, t, 0, 100,  t+1, t+1, Side::Bid, false ), set );
            increment(oid);
            offers.add_order( get_new_order(mem,oid, t, 0, 101,  t+1, t+1, Side::Offer, false ), set );
            increment(oid);
        }

        Order & new_offer = get_new_order(mem,oid, 100, 0, 99, 55 ,55,  Side::Offer, false );
        increment(oid);
        bids.match(new_offer, set, 0);
        CHECK( 0 == new_offer.remaining_size_ );
        CHECK( bids.orders_.empty() );

        Order & new_bid = get_new_order(mem,oid, 100, 0, 101, 55 ,55,  Side::Bid, false );
        increment(oid);
        offers.match(new_bid, set, 0);
        CHECK( 0 == new_bid.remaining_size_ );
        CHECK( offers.orders_.empty() );
    }
    { //no match 
        MemoryManager<Order> mem; 
        Order::PtrSet set;   
        OrderIDType oid ;
        oid.fill(0);
        Level bids( 100, Side::Bid,mem ) ; 
        Level offers( 101, Side::Offer,mem ) ; 
        for (TimeType t = 0 ; t < 10; ++t) { 
            bids.add_order( get_new_order(mem,oid, t, 0, 100,  t+1, t+1, Side::Bid, false ), set );
            increment(oid);
            offers.add_order( get_new_order(mem,oid, t, 0, 101,  t+1, t+1, Side::Offer, false ), set );
            increment(oid);
        }

        Order & new_offer = get_new_order(mem,oid, 100, 0, 101, 55 ,55,  Side::Offer, false );
        increment(oid);
        bids.match(new_offer, set, 0);
        CHECK( 55 == new_offer.remaining_size_ );
        CHECK( 10 == bids.orders_.size() );

        Order & new_bid = get_new_order(mem,oid, 100, 0, 100, 55 ,55,  Side::Bid, false );
        increment(oid);
        offers.match(new_bid, set, 0);
        CHECK( 55 == new_bid.remaining_size_ );
        CHECK( 10 == offers.orders_.size() );
    }

}
TEST_CASE( "match orderbook hidden", "[Level]" ) {
    using namespace SDB;
    MemoryManager<Order> mem; 
    Order::PtrSet set;   
    OrderIDType oid;
    oid.fill(0);
    Level bids( 100, Side::Bid , mem) ; 
    for (TimeType t = 0 ; t < 10; ++t) { 
        bids.add_order( get_new_order(mem,oid, t, 0, 100,  10, 2, Side::Bid, false ), set );
        increment(oid);
    }
    CHECK( 10 == bids.orders_.front().remaining_size_ );
    CHECK( 2 == bids.orders_.front().shown_size_ );
    OrderIDType noid ;
    noid.fill(0);
    noid[0] = std::numeric_limits<OrderIDType::value_type>::max();
    //trade 1
    Order & new_offer = get_new_order(mem, noid, 100, 0, 100, 1 ,1,  Side::Offer, false );
    bids.match(new_offer, set, 0);
    CHECK( 0 == new_offer.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    CHECK( 0 == bids.orders_.front().order_id_[0] );
    CHECK( 9 == bids.orders_.front().remaining_size_ );
    CHECK( 1 == bids.orders_.front().shown_size_ );
    //trade another 1
    Order & new_offer2 = get_new_order(mem,noid, 100, 0, 100, 1 ,1,  Side::Offer, false );
    bids.match(new_offer2, set, 0);
    CHECK( 0 == new_offer2.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    CHECK( 0 == bids.orders_.back().order_id_[0] );
    CHECK( 8 == bids.orders_.back().remaining_size_ );
    CHECK( 2 == bids.orders_.back().shown_size_ );
    Order & new_offer3 = get_new_order(mem,noid, 100, 0, 100, 4 ,2,  Side::Offer, false );
    bids.match(new_offer3, set, 0);
    CHECK( 0 == new_offer3.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    OrderIDType i;
    i.fill(0);
    i[0] = 3; 
    for (const auto & o : bids.orders_ ) {
        CHECK( i == o.order_id_ );
        i[0] = (i[0]+1) % 10;
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
    CHECK_THROWS_AS( eng.cancel_order( eng.next_order_id_ ), std::runtime_error) ;
    REQUIRE( not eng.all_bids_.empty() );
    CHECK( not eng.all_bids_.begin()->orders_.empty() );
    OrderIDType oid;
    oid.fill(0);
    eng.cancel_order( oid );//first order is deleted
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

TEST_CASE( "simulate_a - no trades", "[ClientState]" ) {
    using namespace SDB;
    
    std::vector<OrderBookEventWithClientID> msgs ;
    OrderIDType oid; 
    std::unordered_set<OrderIDType, boost::hash<OrderIDType> > set;
    oid.fill(0);
    msgs.emplace_back( OrderBookEvent( 0, oid, 100, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,0 ) ;
    set.emplace(oid);
    increment(oid);
    msgs.emplace_back( OrderBookEvent( 1, oid, 101, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,1 ) ;
    set.emplace(oid);
    increment(oid);
    msgs.emplace_back( OrderBookEvent( 2, oid, 100, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,2 ) ;
    msgs.emplace_back( OrderBookEvent( 3, oid, 100, 0, 2, 0, NotifyMessageType::Ack, Side::Bid  ) ,3 ) ;
    set.emplace(oid);
    increment(oid);

    std::vector<TimeType> times{0,1,2,3} ;
    std::vector<double> prices{1000,100,100,100} ;


    MatchingEngine eng;
    RecordingSimulationHandler<OrderBookEventWithClientID> recorder( &eng.mem_, true, true, true , nullptr );
    simulate_a( msgs, times, prices, Side::Offer, set, 4, 5, eng, recorder );

    CHECK( msgs.size() <= recorder.msgs_.size() );

    REQUIRE( recorder.trades_.empty() ); //because only 2 traded 

}

TEST_CASE( "simulate_a - one trade", "[ClientState]" ) {
    using namespace SDB;
    
    std::vector<OrderBookEventWithClientID> msgs ;
    OrderIDType oid; 
    std::unordered_set<OrderIDType, boost::hash<OrderIDType> > set;
    oid.fill(0);
    msgs.emplace_back( OrderBookEvent( 0, oid, 100, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,0 ) ;
    set.emplace(oid);
    increment(oid);
    msgs.emplace_back( OrderBookEvent( 1, oid, 101, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,1 ) ;
    set.emplace(oid);
    increment(oid);
    msgs.emplace_back( OrderBookEvent( 2, oid, 100, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,2 ) ;
    msgs.emplace_back( OrderBookEvent( 3, oid, 100, 0, 4, 0, NotifyMessageType::Ack, Side::Bid  ) ,3 ) ;
    set.emplace(oid);
    increment(oid);

    std::vector<TimeType> times{0,1,2,3} ;
    std::vector<double> prices{1000,100,100,100} ;

    MatchingEngine eng;
    RecordingSimulationHandler<OrderBookEventWithClientID> recorder( &eng.mem_, true, true, true, nullptr );
    simulate_a( msgs, times, prices, Side::Offer, set, 4, 5, eng, recorder );

    CHECK( msgs.size() <= recorder.msgs_.size() );

    REQUIRE( not recorder.trades_.empty() ); //because 4 has traded 

    CHECK( std::get<0>(recorder.trades_.front()) == 3   );
    CHECK( std::get<1>(recorder.trades_.front()) == 100 );

}

TEST_CASE( "simulate_a - one trade and refill", "[ClientState]" ) {
    using namespace SDB;
    
    std::vector<OrderBookEventWithClientID> msgs ;
    std::unordered_set<OrderIDType, boost::hash<OrderIDType> > set;
    OrderIDType oid; 
    oid.fill(0);
    msgs.emplace_back( OrderBookEvent( 0, oid, 100, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,0 ) ;
    set.emplace(oid);
    increment(oid);
    msgs.emplace_back( OrderBookEvent( 1, oid, 101, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,1 ) ;
    set.emplace(oid);
    increment(oid);
    msgs.emplace_back( OrderBookEvent( 2, oid, 100, 0, 2, 0, NotifyMessageType::Ack, Side::Offer) ,2 ) ;
    msgs.emplace_back( OrderBookEvent( 3, oid, 100, 0, 4, 0, NotifyMessageType::Ack, Side::Bid  ) ,3 ) ;
    set.emplace(oid);
    increment(oid);

    std::vector<TimeType> times{0,1,2,3} ;
    std::vector<double> prices{1000,100,100,100} ;

    MatchingEngine eng;
    RecordingSimulationHandler<OrderBookEventWithClientID> recorder( &eng.mem_, true, true, true, nullptr );
    simulate_a( msgs, times, prices, Side::Offer, set, 4, 5, eng, recorder );

    CHECK( msgs.size() <= recorder.msgs_.size() );

    REQUIRE( not recorder.trades_.empty() ); //because 4 has traded 

    CHECK( std::get<0>(recorder.trades_.front()) == 3   );
    CHECK( std::get<1>(recorder.trades_.front()) == 100 );
    CHECK( recorder.simulated_order_status_ == OrderStatus::Acked  );

}

TEST_CASE( "record - simulate_a - no market impact.", "[ClientState]" ) {

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
    RecordingSimulationHandler<OrderBookEventWithClientID> recorder( &eng.mem_, true, false , false, nullptr );
    ClientState::NotificationHandler handler(recorder,eng);
    const TimeType TMax = 10 *1e9; 
    //const TimeType Sec = 1_000_000_000 ; 

    simulate( client_types_and_sizes, eng, handler, TMax );

    REQUIRE( not recorder.msgs_.empty() );
    REQUIRE( not recorder.snapshots_.empty() );

    std::unordered_set<OrderIDType, boost::hash<OrderIDType> > set;
    std::vector<TimeType> times;
    times.reserve( recorder.msgs_.size() );
    times.emplace_back(recorder.msgs_.front().event_time_) ;
    set.emplace(recorder.msgs_.front().oid_);
    for ( auto it = recorder.msgs_.begin()+1; it != recorder.msgs_.end(); ++it) {
        set.emplace( it->oid_ ); 
        if (it->event_time_ != times.back())
            times.emplace_back( it->event_time_ );
    }

    std::vector<double> prices;
    prices.reserve( times.size() );
    auto wm_it = recorder.wm_.begin();
    for ( const auto & t : times ) { 
        auto j = std::upper_bound( wm_it, recorder.wm_.end(), t, 
                []( TimeType time, const std::tuple<TimeType,double> & tpl) { return time < std::get<0>(tpl) ; } );
        if (j != recorder.wm_.end()) 
            REQUIRE( std::get<0>(*j) > t );
        --j; 
        REQUIRE( std::get<0>(*j) == t );
        const double price = std::get<1>(*j);
        if (std::isnan(price)) {
            REQUIRE( not prices.empty() );
            prices.emplace_back( prices.back() ); 
        } else 
            prices.emplace_back( price );
        wm_it = j + 1;
    }


    MatchingEngine eng2;
    RecordingSimulationHandler<OrderBookEventWithClientID> recorder2( &eng2.mem_, true, false, true, nullptr );
    simulate_a( recorder.msgs_, times, prices, Side::Bid, set, 0, 1, eng2, recorder2 );

    CHECK( recorder.msgs_.size() <= recorder2.msgs_.size() );
    CHECK( recorder.snapshots_.size() == recorder2.snapshots_.size() );

    int n_orders_checked = 0; 
    for (size_t i = 0; i < std::min(recorder.snapshots_.size(), recorder2.snapshots_.size() ) ; ++i ) { 
        const auto & [t1, levels1] = recorder.snapshots_[i];
        const auto & [t2, levels2] = recorder2.snapshots_[i];
        REQUIRE(t1==t2);
        REQUIRE(levels1.size()==levels2.size());
        for (size_t j = 0; j < levels1.size(); ++j) { 
            const auto & [price1,side1,orders1] = levels1.at(j);
            const auto & [price2,side2,orders2] = levels2.at(j);
            REQUIRE( price1==price2 );
            REQUIRE( side1==side2 );
            REQUIRE(orders1.size()==orders2.size());
            auto it1 = orders1.begin(); 
            auto it2 = orders2.begin(); 
            for ( ; it1 != orders1.end(); ++it1, ++it2 ) { 
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

}

namespace SDB { 
    struct FakeMatchingEngine : public MatchingEngine {
        double wm_ ; 
        double wm() const { return wm_ ; }
        FakeMatchingEngine(double wm0=0) : wm_(wm0) {};
    };
};

TEST_CASE( "simple stats 1", "[StatisticsSimulationHandler]" ) {
    using namespace SDB;

    FakeMatchingEngine eng; 
    StatisticsSimulationHandler<> handler; 

    boost::random::mt19937 mt;
    mt.seed(0);

    boost::random::normal_distribution<double> order_price_distribution;
    boost::random::bernoulli_distribution<double> binary_distribution;

    constexpr TimeType TMax = 100000;
    OrderIDType oid; 
    oid.fill(0);
    Order & shadow_order = get_new_order( eng.mem_ , oid, 0, 0, 0, 1, 1, Side::Bid, true, handler );
    Order & dummy_order = get_new_order( eng.mem_ , oid, 0, 0, 0, 1, 1, Side::Bid, false, handler );
    for (TimeType t = 0; t < TMax ; ++t) { 
        eng.set_time( t );
        eng.wm_ += 1e-1*order_price_distribution( mt );
        eng.set_time(t);
        const PriceType bbid = std::trunc( eng.wm_ ) - (eng.wm_<0 ? 1:0);
        const double r = eng.wm_ - bbid; 
        REQUIRE( r >= 0 );
        REQUIRE( r <= 1 );
        bool passive_trade_shadow = false;
        const bool aggressive = binary_distribution( 
                mt,
                boost::random::bernoulli_distribution<double>::param_type(r) );
        const PriceType price = bbid + (aggressive ? 1 : 0 ) ; 
        if (aggressive) 
            //we will fill the shadow order aggressively
            handler.log(NotifyMessageType::Trade, shadow_order, t, 1, price );
        else {
            passive_trade_shadow = true;
            if (passive_trade_shadow) 
                handler.log(NotifyMessageType::Trade, shadow_order, t, 1, price );
            else 
                handler.log(NotifyMessageType::Trade, dummy_order, t, 1, price );
        }
        handler.log( eng );
        //std::cout << t << ',' << eng.wm_ << ',' ;
        //if (aggressive or passive_trade_shadow) std::cout << price ;
        //std::cout << std::endl ;
    }
    CHECK( not std::isnan( handler.sum_return_by_dT_ ) );
    CHECK( handler.sum_dT_ > 0 );
    CHECK( std::abs( handler.sum_return_by_dT_ / handler.sum_dT_ ) < 2./std::sqrt(TMax)  );
}

TEST_CASE( "simple stats 2", "[StatisticsSimulationHandler]" ) {
    using namespace SDB;

    FakeMatchingEngine eng; 
    StatisticsSimulationHandler<> handler; 

    boost::random::mt19937 mt;
    mt.seed(0);

    boost::random::normal_distribution<double> order_price_distribution;
    boost::random::bernoulli_distribution<double> binary_distribution;

    constexpr TimeType TMax = 100000;
    OrderIDType oid;
    oid.fill(0);
    Order & shadow_order = get_new_order( eng.mem_ , oid, 0, 0, 0, 1, 1, Side::Bid, true, handler );
    Order & dummy_order = get_new_order( eng.mem_ , oid, 0, 0, 0, 1, 1, Side::Bid, false, handler );
    for (TimeType t = 0; t < TMax ; ++t) { 
        eng.set_time( t );
        eng.wm_ += 1e-1*order_price_distribution( mt );
        eng.set_time(t);
        const PriceType bbid = std::trunc( eng.wm_ ) - (eng.wm_<0 ? 1:0);
        const double r = eng.wm_ - bbid; 
        REQUIRE( r >= 0 );
        REQUIRE( r <= 1 );
        bool passive_trade_shadow = false;
        const bool aggressive = binary_distribution( 
                mt,
                boost::random::bernoulli_distribution<double>::param_type(r) );
        const PriceType price = bbid + (aggressive ? 1 : 0 ) ; 
        if (aggressive) 
            //we will fill the shadow order aggressively
            handler.log(NotifyMessageType::Trade, shadow_order, t, 1, price );
        else {
            passive_trade_shadow = false;
            if (passive_trade_shadow) 
                handler.log(NotifyMessageType::Trade, shadow_order, t, 1, price );
            else 
                handler.log(NotifyMessageType::Trade, dummy_order, t, 1, price );
        }
        handler.log( eng );
    }
    CHECK( not std::isnan( handler.sum_return_by_dT_ ) );
    CHECK( handler.sum_dT_ > 0 );
    CHECK( handler.sum_return_by_dT_ / handler.sum_dT_ < 0  );
}

TEST_CASE( "simple stats 3", "[StatisticsSimulationHandler]" ) {
    using namespace SDB;

    FakeMatchingEngine eng; 
    StatisticsSimulationHandler<> handler; 

    boost::random::mt19937 mt;
    mt.seed(0);

    boost::random::normal_distribution<double> order_price_distribution;
    boost::random::bernoulli_distribution<double> binary_distribution;

    constexpr TimeType TMax = 100000;

    OrderIDType oid;
    oid.fill(0);
    Order & shadow_order = get_new_order( eng.mem_ , oid, 0, 0, 0, 1, 1, Side::Bid, true, handler );
    Order & dummy_order = get_new_order( eng.mem_ , oid, 0, 0, 0, 1, 1, Side::Bid, false, handler );
    for (TimeType t = 0; t < TMax ; ++t) { 
        eng.set_time( t );
        eng.wm_ += 1e-1*order_price_distribution( mt );
        eng.set_time(t);
        const PriceType bbid = std::trunc( eng.wm_ ) - (eng.wm_<0 ? 1:0);
        const double r = eng.wm_ - bbid; 
        REQUIRE( r >= 0 );
        REQUIRE( r <= 1 );
        bool passive_trade_shadow = false;
        const bool aggressive = binary_distribution( 
                mt,
                boost::random::bernoulli_distribution<double>::param_type(r) );
        const PriceType price = bbid + (aggressive ? 1 : 0 ) ; 
        if (aggressive) 
            //we will fill the shadow order aggressively
            handler.log(NotifyMessageType::Trade, shadow_order, t, 1, price );
        else {
            passive_trade_shadow = binary_distribution( 
                    mt,
                    boost::random::bernoulli_distribution<double>::param_type(0.1) );
            if (passive_trade_shadow) 
                handler.log(NotifyMessageType::Trade, shadow_order, t, 1, price );
            else 
                handler.log(NotifyMessageType::Trade, dummy_order, t, 1, price );
        }
        handler.log( eng );
        //std::cout << t << ',' << eng.wm_ << ',' ;
        //if (aggressive or passive_trade_shadow) std::cout << price ;
        //std::cout << std::endl ;
    }
    CHECK( not std::isnan( handler.sum_return_by_dT_ ) );
    CHECK( handler.sum_dT_ > 0 );
    CHECK( handler.sum_return_by_dT_ / handler.sum_dT_ < 0  );
}


TEST_CASE( "simulate_a single param, run it over and over.", "[StatisticsSimulationHandler]" ) {
    using namespace SDB;

    //let's generate an engine and generate random market data:
    std::vector<OrderBookEvent> history; 
    std::vector<TimeType> times ; 
    std::vector<double> prices ; 
    std::unordered_set<OrderIDType, boost::hash<OrderIDType> > set;
    {
        int seed = 0; 
        boost::random::mt19937 mt;
        mt.seed(seed);

        ClientType type1( "type1",mt, 1./60., 1./(30*60.), 100. , 5. , 0.5 );  
        ClientType type2( "type2",mt, 1.,     1.,          10.  , 2. , 0.5 );  


        std::vector<std::tuple<ClientType, int>> client_types_and_sizes ; 
        client_types_and_sizes.emplace_back(
                ClientType( "type1",mt, 1./60., 1./(30*60.), 100. , 5. , 0.5 ), 
                100 );

        client_types_and_sizes.emplace_back(
                ClientType( "type2",mt, 1.,     1.,          10.  , 2. , 0.5 ),
                100 );


        MatchingEngine eng;
        RecordingSimulationHandler<OrderBookEvent> recorder( &eng.mem_, true, false , false, nullptr );
        ClientState::NotificationHandler handler(recorder,eng);
        const TimeType TMax = 10 *1e9; 
        //const TimeType Sec = 1_000_000_000 ; 

        simulate( client_types_and_sizes, eng, handler, TMax );

        REQUIRE( not recorder.msgs_.empty() );
        REQUIRE( not recorder.snapshots_.empty() );

        times.reserve( recorder.msgs_.size() );
        times.emplace_back(recorder.msgs_.front().event_time_) ;
        set.emplace(recorder.msgs_.front().oid_);
        for ( auto it = recorder.msgs_.begin()+1; it != recorder.msgs_.end(); ++it) {
            set.emplace(it->oid_ );
            if (it->event_time_ != times.back())
                times.emplace_back( it->event_time_ );
        }

        auto wm_it = recorder.wm_.begin();
        for ( const auto & t : times ) { 
            auto j = std::upper_bound( wm_it, recorder.wm_.end(), t, 
                    []( TimeType time, const std::tuple<TimeType,double> & tpl) { return time < std::get<0>(tpl) ; } );
            if (j != recorder.wm_.end()) 
                REQUIRE( std::get<0>(*j) > t );
            --j; 
            REQUIRE( std::get<0>(*j) == t );
            const double price = std::get<1>(*j);
            if (std::isnan(price)) {
                REQUIRE( not prices.empty() );
                prices.emplace_back( prices.back() ); 
            } else 
                prices.emplace_back( price );
            wm_it = j + 1;
        }
        history.swap( recorder.msgs_ );
    }


    const int half = 20;
    const double max_adj = 2;
    
    for (int i = 0; i < 1+2*half; ++i)  {
        MatchingEngine eng2;
        StatisticsSimulationHandler<> stats; 
        std::vector<double> adjusted_prices(prices);
        const double delta = (i-half)/double(half)*max_adj;
        for ( auto & p : adjusted_prices ) p += delta;
        simulate_a( history, times, adjusted_prices, Side::Bid, set, 0, 1, eng2, stats );
        std::cout << "4 mean cost [ticks]" << delta << " " << stats.sum_return_by_dT_ / stats.sum_dT_ << std::endl;
        std::cout << "4 total time [sec]" << stats.sum_dT_*1e-9 << std::endl;
        CHECK( not std::isnan( stats.sum_return_by_dT_ ) );
        CHECK( stats.sum_dT_ > 0 );
        //CHECK( stats.sum_return_by_dT_ / stats.sum_dT_ < 0  );
    }

}
TEST_CASE( "empty", "[Utils]" ) {
    using namespace SDB;
    std::vector<std::string_view> vec ; 
    const std::string test1("1,");
    split_string( test1, vec );
    REQUIRE( vec.size() == 2 ) ;
}
TEST_CASE( "test string split", "[Utils]" ) {
    using namespace SDB;
    std::vector<std::string_view> vec ; 
    const std::string test1("1,23,456,7890");
    split_string( test1, vec );
    REQUIRE( vec.size() == 4 ) ;
    CHECK( "1" == std::string(vec[0]) );
    CHECK( "23" == std::string(vec[1]) );
    CHECK( "456" == std::string(vec[2]) );
    CHECK( "7890" == std::string(vec[3]) );

    const std::string test2("1,23,,7890");
    split_string( test2, vec );
    REQUIRE( vec.size() == 4 ) ;
    CHECK( "1" == std::string(vec[0]) );
    CHECK( "23" == std::string(vec[1]) );
    CHECK( "" == std::string(vec[2]) );
    CHECK( "7890" == std::string(vec[3]) );

    const std::string test3("1,23,456,");
    split_string( test3, vec );
    REQUIRE( vec.size() == 4 ) ;

    const std::string test4(",,,");
    split_string( test4, vec );
    REQUIRE( vec.size() == 4 ) ;
    for (const auto & view : vec )
        CHECK( view.size() == 0 );
        
}

TEST_CASE( "increment", "[OrderIDType]" ) {
    using namespace SDB;
    OrderIDType oid;
    oid.fill(0);
    increment(oid);
    CHECK(oid[0] == 1);
    for (size_t i = 1; i < oid.size();++i)
        CHECK(oid[i] == 0);

    increment(oid);
    CHECK(oid[0] == 2);
    for (size_t i = 1; i < oid.size();++i)
        CHECK(oid[i] == 0);

    oid[0] = std::numeric_limits<OrderIDType::value_type>::max();
    increment(oid);
    CHECK(oid[0] == 0);
    CHECK(oid[1] == 1);
    for (size_t i = 2; i < oid.size();++i)
        CHECK(oid[i] == 0);

}
TEST_CASE( "parse", "[Utils]" ) {
    using namespace SDB;
    OrderIDType oid; 
    CHECK( parse( "<FF>", oid ) );
    CHECK( int(oid[0]) == 255 );
    for (size_t i = 1; i < oid.size(); ++i)
        CHECK(int(oid[i]) == 0 );

    CHECK( parse( "s<FF>", oid ) );
    CHECK( oid[0] == 's' );
    CHECK( int(oid[1]) == 255 );
    for (size_t i = 2; i < oid.size(); ++i)
        CHECK(int(oid[i]) == 0 );

    CHECK( parse( "<A1>s<FF>", oid ) );
    CHECK( int(oid[0]) == 161 );
    CHECK( oid[1] == 's' );
    CHECK( int(oid[2]) == 255 );
    for (size_t i = 3; i < oid.size(); ++i)
        CHECK(int(oid[i]) == 0 );

}
