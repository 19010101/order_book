#include "ob.h"

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
    o.reset( 0, 0, 0, 100, 5, 1, Side::Offer ) ;
    CHECK( bids.do_prices_agree( o ) );
    o.reset( 0, 0, 0,  99, 5, 1, Side::Offer );
    CHECK( bids.do_prices_agree(  o ) );
    o.reset( 0, 0, 0,  101, 5, 1, Side::Offer );
    CHECK( not bids.do_prices_agree( o ) );
    Level offers( 100, Side::Offer , mem);
    o.reset( 0, 0, 0, 100, 5, 1, Side::Bid );
    CHECK( offers.do_prices_agree( o ) );
    o.reset( 0, 0, 0,  99, 5, 1, Side::Bid );
    CHECK( not offers.do_prices_agree( o ) );
    o.reset( 0, 0, 0,  101, 5, 1, Side::Bid );
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
            bids.add_order( get_new_order(mem,oid++, t, 0, 100,  t+1, t+1, Side::Bid ), set );
            offers.add_order( get_new_order(mem,oid++, t, 0, 101,  t+1, t+1, Side::Offer ), set );
        }

        Order & new_offer = get_new_order(mem,oid++, 100, 0, 100, 55 ,55,  Side::Offer );
        bids.match(new_offer, set, 0 );
        CHECK( 0 == new_offer.remaining_size_ );
        CHECK( bids.orders_.empty() );

        Order & new_bid = get_new_order(mem,oid++, 100, 0, 101, 55 ,55,  Side::Bid );
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
            bids.add_order( get_new_order(mem,oid++, t, 0, 100,  t+1, t+1, Side::Bid ), set );
            offers.add_order( get_new_order(mem,oid++, t, 0, 101,  t+1, t+1, Side::Offer ), set );
        }

        Order & new_offer = get_new_order(mem,oid++, 100, 0, 99, 55 ,55,  Side::Offer );
        bids.match(new_offer, set, 0);
        CHECK( 0 == new_offer.remaining_size_ );
        CHECK( bids.orders_.empty() );

        Order & new_bid = get_new_order(mem,oid++, 100, 0, 101, 55 ,55,  Side::Bid );
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
            bids.add_order( get_new_order(mem,oid++, t, 0, 100,  t+1, t+1, Side::Bid ), set );
            offers.add_order( get_new_order(mem,oid++, t, 0, 101,  t+1, t+1, Side::Offer ), set );
        }

        Order & new_offer = get_new_order(mem,oid++, 100, 0, 101, 55 ,55,  Side::Offer );
        bids.match(new_offer, set, 0);
        CHECK( 55 == new_offer.remaining_size_ );
        CHECK( 10 == bids.orders_.size() );

        Order & new_bid = get_new_order(mem,oid++, 100, 0, 100, 55 ,55,  Side::Bid );
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
        bids.add_order( get_new_order(mem,t, t, 0, 100,  10, 2, Side::Bid ), set );
    }
    CHECK( 10 == bids.orders_.front().remaining_size_ );
    CHECK( 2 == bids.orders_.front().shown_size_ );
    //trade 1
    Order & new_offer = get_new_order(mem,100, 100, 0, 100, 1 ,1,  Side::Offer );
    bids.match(new_offer, set, 0);
    CHECK( 0 == new_offer.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    CHECK( 0 == bids.orders_.front().order_id_ );
    CHECK( 9 == bids.orders_.front().remaining_size_ );
    CHECK( 1 == bids.orders_.front().shown_size_ );
    //trade another 1
    Order & new_offer2 = get_new_order(mem,100, 100, 0, 100, 1 ,1,  Side::Offer );
    bids.match(new_offer2, set, 0);
    CHECK( 0 == new_offer2.remaining_size_ );
    REQUIRE( 10 == bids.orders_.size() );
    CHECK( 0 == bids.orders_.back().order_id_ );
    CHECK( 8 == bids.orders_.back().remaining_size_ );
    CHECK( 2 == bids.orders_.back().shown_size_ );
    Order & new_offer3 = get_new_order(mem,100, 100, 0, 100, 4 ,2,  Side::Offer );
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
            eng.add_order( 0, 100,  10, 2, Side::Bid );
            eng.add_order( 0, 101,  10, 3, Side::Offer );
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
            eng.add_order( 0, 100 - dp ,  10 + dp   , 2, Side::Bid );
            eng.add_order( 0, 100 - dp ,  10 + dp*2 , 3, Side::Bid );

            eng.add_order( 0, 101 + dp ,  10 + dp   , 4, Side::Offer );
            eng.add_order( 0, 101 + dp ,  10 + dp*2 , 5, Side::Offer );

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
        eng.add_order( 0, 102  ,  8   , 2, Side::Bid ); //agressive buy
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
            eng.add_order( 0, 100 - dp ,  10 + dp   , 2, Side::Bid );
            eng.add_order( 0, 100 - dp ,  10 + dp*2 , 3, Side::Bid );

            eng.add_order( 0, 101 + dp ,  10 + dp   , 4, Side::Offer );
            eng.add_order( 0, 101 + dp ,  10 + dp*2 , 5, Side::Offer );

        }
        //agressive order. first order will replenish back to 4, second will be left to 1:
        eng.add_order( 0, 100  ,  100   , 7, Side::Offer ); //agressive sell
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
            eng.add_order( 0, 100 - dp ,  10 + dp   , 2, Side::Bid );
            eng.add_order( 0, 100 - dp ,  10 + dp*2 , 3, Side::Bid );

            eng.add_order( 0, 101 + dp ,  10 + dp   , 4, Side::Offer );
            eng.add_order( 0, 101 + dp ,  10 + dp*2 , 5, Side::Offer );

        }
        //agressive order. first order will replenish back to 4, second will be left to 1:
        eng.add_order( 0, 99  ,  100   , 7, Side::Offer ); //agressive sell
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
    eng.add_order( 0, 100,  10, 2, Side::Bid );
    CHECK_THROWS_AS( eng.cancel_order( eng.next_order_id_ ), 
            std::runtime_error) ;
    REQUIRE( not eng.all_bids_.empty() );
    CHECK( not eng.all_bids_.begin()->orders_.empty() );
    eng.cancel_order( 0 );//second order is deleted
    REQUIRE( eng.all_bids_.empty() );
}


