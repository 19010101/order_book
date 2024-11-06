#pragma once
#include <set>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <limits>

#include "memory_manager.h"

//#include <boost/intrusive/list.hpp>

namespace SDB { 
    using OrderIDType = uint64_t ; 
    using TimeType = uint64_t ; 
    using ClientIDType = uint32_t;
    using PriceType = int16_t;
    using SizeType = uint16_t;
    enum class Side : uint8_t { Bid, Offer } ; 

    Side get_other_side( Side s ) {
        switch (s) {
            case Side::Bid : return Side::Offer ; 
            case Side::Offer : return Side::Bid ; 
        }
        throw std::runtime_error("Come on");
    }

    enum class NotifyMessageType : uint8_t { Ack, Trade, End } ; 

    struct Order;

    template <typename T>
        concept INotifier = requires( T & notifier, const NotifyMessageType mtype, const Order & o, const TimeType now, 
                const SizeType traded_size, const PriceType traded_price ) 
        {
            notifier( mtype, o, now , traded_size, traded_price ) ;
        };

    void NOOPNotify( const NotifyMessageType , const Order & , const TimeType, const SizeType = 0, const PriceType = 0) { };

    struct Order : public MemoryManaged{
        OrderIDType order_id_ ; 
        TimeType creation_time_ ; 
        ClientIDType client_id_ ;
        PriceType price_ ; 
        SizeType total_size_ ; 
        SizeType show_ ; 
        mutable SizeType remaining_size_ ; 
        mutable SizeType shown_size_ ; 
        Side side_ ; 

        
        void reset( OrderIDType oid, TimeType t, ClientIDType cid, PriceType p, SizeType s, SizeType show, Side side) 
        {
            order_id_ = oid;
            creation_time_ = t ;
            client_id_ = cid;
            price_ = p;
            total_size_ = s ;
            show_ = show;
            remaining_size_ = s;
            shown_size_ = 0;
            side_ = side;
            replenish();
        }

        Order() 
        {
            clear();
        }

        void replenish() const { 
            if (shown_size_ != 0)
                throw std::runtime_error("Cannot replenish like this!");
            shown_size_ =  std::min(show_,remaining_size_) ;
        }

        template <INotifier N>
            SizeType match( Order & aggressive_order , const TimeType now, N & notify) { 
                if (aggressive_order.side_ == side_) throw std::runtime_error("WTF!!!"); 
                const SizeType traded_size = std::min(aggressive_order.shown_size_ , shown_size_ );
                if (traded_size==0) throw std::runtime_error("WTF");
                aggressive_order._traded( traded_size , price_ , now, notify);
                _traded( traded_size , price_ , now, notify);
                return traded_size;
            }

        
        void clear() { 
            reset(-1,-1, -1, -1,0,0,Side::Offer);
        }

        private: 
        template <INotifier N>
            void _traded(SizeType traded_size, PriceType traded_price, const TimeType now, N & notify) const { 
                remaining_size_ -= traded_size;     
                shown_size_ -= traded_size;     
                notify( NotifyMessageType::Trade , *this, now, traded_size, traded_price );
                const bool finished = remaining_size_==0;
                if (finished) notify( NotifyMessageType::End, *this, now , 0, 0 );
            }
        public : 
        struct Hash { 
            using is_transparent = void;
            size_t operator()( const Order * ptr ) const { 
                return std::hash<OrderIDType>()(ptr->order_id_);
            }
            size_t operator()( const OrderIDType order_id ) const { 
                return std::hash<OrderIDType>()(order_id);
            }
        };
        struct Eq { 
            using is_transparent = void;
            size_t operator()( const Order * a, const Order * b ) const { 
                return a->order_id_ == b->order_id_;
            }
            size_t operator()( const OrderIDType a, const Order * b ) const { 
                return a == b->order_id_;
            }
        };
        using PtrSet = std::unordered_set< Order*, typename Order::Hash, typename  Order::Eq>;

    };

    struct Level {
        //sub class
        struct Compare {
            using is_transparent = void;
            bool operator()( const Level & op, const PriceType & price ) const {
                if (op.side_ == Side::Offer)
                    return op.price_ < price;
                else 
                    return op.price_ > price;
            }
            bool operator()( const PriceType & price, const Level & op  ) const {
                if (op.side_ == Side::Offer)
                    return price < op.price_ ;
                else 
                    return price > op.price_ ;
            }
            bool operator()( const Level & op, const Level & other ) const {
                if (op.side_ != other.side_) throw std::runtime_error("Cannot be");
                return this->operator()( op, other.price_ );
            }
        };
        using SET = std::set<Level, Compare>;

        //data
        const PriceType price_ ; 
        const Side side_ ; 
        MemoryManager<Order> & mem_;
        mutable MemoryManager<Order>::list_type orders_ ; 
        Compare cmp_ ; 

        //methods
        Level( PriceType p, Side s, MemoryManager<Order> & mem) : 
            price_(p), side_(s) , mem_(mem) , orders_() {}

        
        void add_order( Order & o, Order::PtrSet & set ) const { 
            if (o.price_ != price_ or o.side_ != side_ )
                throw std::runtime_error("Can't add this order to this level!");
            orders_.push_back( o );
            auto pr =set.insert( &o );
            if (not pr.second) 
                throw std::runtime_error("Cannot insert oid");
        }

        SizeType total_shown() const {
            SizeType n = 0; 
            for (const auto & o : orders_) 
                n += o.shown_size_;
            return n;
        }


        bool do_prices_agree( const Order & new_order ) const {
            if (side_ == new_order.side_) throw std::runtime_error("WTF");
            return price_ == new_order.price_ or cmp_( *this, new_order.price_ ); 
        }

        template <INotifier N>
            void match( Order & new_order, Order::PtrSet & pointer_set, const TimeType now, N & notify ) const { 
                if (not do_prices_agree(new_order) )
                    return;
                while (not orders_.empty() && new_order.remaining_size_ > 0) {
                    Order & order_in_book = orders_.front();
                    const SizeType traded_size = order_in_book.match( new_order, now, notify ) ;
                    if (traded_size == 0) throw std::runtime_error("SSSS");
                    if (order_in_book.shown_size_==0) {
                        orders_.pop_front();
                        if (order_in_book.remaining_size_!=0) { //hidden
                            order_in_book.replenish();   
                            orders_.push_back( order_in_book ); 
                        } else {
                            size_t n_erased = pointer_set.erase( &order_in_book) ; 
                            if (n_erased != 1) 
                                throw std::runtime_error("Expected to erase 1 but erased " + std::to_string(n_erased) );
                            mem_.free(order_in_book);
                        }
                    }
                    if (new_order.shown_size_==0 and new_order.remaining_size_!=0)  //hidden
                        new_order.replenish();   
                }
            }
        void match( Order & new_order, Order::PtrSet & pointer_set, const TimeType now) const { 
            match( new_order, pointer_set, now, NOOPNotify);
        }
    };

    template <INotifier N> 
        Order & get_new_order(
                MemoryManager<Order> & mem, 
                OrderIDType oid, TimeType t, ClientIDType cid, PriceType p, SizeType s, SizeType show, Side side, 
                N & notify
                ) {
            Order & o = mem.get_unused();
            o.reset(oid, t, cid, p, s, show, side);
            notify( NotifyMessageType::Ack , o, t, 0, 0 );
            return o;
        }

    Order & get_new_order(
            MemoryManager<Order> & mem, 
            OrderIDType oid, TimeType t, ClientIDType cid, PriceType p, SizeType s, SizeType show, Side side
            ) {
        return get_new_order(mem, oid, t, cid, p, s, show, side, NOOPNotify );
    }
    struct MatchingEngine { 
        OrderIDType next_order_id_ ; 
        TimeType time_ ; 
        MemoryManager<Order> mem_ ; 
        Level::SET all_bids_, all_offers_ ; 
        Order::PtrSet set_;

        MatchingEngine() : next_order_id_(0), time_(0) { }

        void set_time( TimeType time) { time_ = time ; }
        Level::SET & get_book( const Side s ) { 
            if (s == Side::Bid) return all_bids_;
            else return all_offers_ ; 
        }
        template <INotifier N> 
            void add_order( const ClientIDType client_id, const PriceType price, const SizeType size, const SizeType show, const Side side, N & notify) { 
                //Is there a trade? 
                //find the order with the same price: 
                OrderIDType oid = next_order_id_++;
                //Order new_order(oid, time_, client_id, price, size, show, side);
                Order & new_order = get_new_order( mem_,oid, time_, client_id, price, size, show, side, notify);
                auto & all_orders_other_side = get_book( get_other_side(side) );
                while (not all_orders_other_side.empty()) { 
                    const auto top_of_other_side_iter = all_orders_other_side.begin(); 
                    if (not top_of_other_side_iter->do_prices_agree( new_order ) )
                        break;
                    //now match:
                    top_of_other_side_iter->match( new_order, set_, time_, notify );
                    if (top_of_other_side_iter->orders_.empty()) 
                        all_orders_other_side.erase( top_of_other_side_iter );
                    if (new_order.remaining_size_==0)
                        break;
                }
                if (new_order.remaining_size_) 
                    get_book( side ).emplace( price, side, mem_ ).first->add_order( new_order, set_ ) ;
                else
                    mem_.free(new_order);
            }
        void add_order( const ClientIDType client_id, const PriceType price, const SizeType size, const SizeType show, const Side side) { 
            add_order(client_id, price, size, show, side, NOOPNotify );
        }

        template <INotifier N> 
            void cancel_order( const OrderIDType oid, N & notify ) { 
                auto set_it = set_.find(oid);
                if (set_it==set_.end()) 
                    throw std::runtime_error("Cannot find oid " + std::to_string(oid));
                Order & order = **set_it;
                Level::SET & levels = get_book( order.side_ );
                auto levels_iterator = levels.find( order.price_ );
                if (levels_iterator == levels.end()) 
                    throw std::runtime_error("Cannot find price level " + std::to_string(order.price_));
                levels_iterator->orders_.erase( levels_iterator->orders_.iterator_to(order) );
                if ( levels_iterator->orders_.empty() )
                    levels.erase( levels_iterator );
                notify( NotifyMessageType::End, order, time_ , 0, 0);
                set_.erase( set_it ) ; 
                mem_.free(order);
            }
        void cancel_order( const OrderIDType oid ) { 
            cancel_order( oid, NOOPNotify) ;
        }
        template <INotifier N> 
            void shutdown(N & notify) { 
                while (not set_.empty()) {
                    OrderIDType oid = (*set_.begin())->order_id_ ; 
                    cancel_order(oid, notify);
                }
            }


        template<size_t N> 
            void level2( 
                    std::array<PriceType, N> & bid_prices, 
                    std::array<SizeType, N> & bid_sizes, 
                    std::array<PriceType, N> & ask_prices, 
                    std::array<SizeType, N> & ask_sizes ) const 
            { 
                bid_prices.fill(0);
                ask_prices.fill(0);
                bid_sizes.fill(0);
                ask_sizes.fill(0);
                size_t i = 0; 
                auto it = all_bids_.begin();
                while( it != all_bids_.end() and i < N ) {
                    bid_prices[i] = it->price_ ; 
                    bid_sizes[i] = it->total_shown() ; 
                    ++i ; ++it ;
                }
                i = 0; 
                it = all_offers_.begin();
                while( it != all_offers_.end() and i < N ) {
                    ask_prices[i] = it->price_ ; 
                    ask_sizes[i] = it->total_shown() ; 
                    ++i ; ++it ;
                }
            }
        double wm() const {
            std::array<PriceType, 1> bid_prices, ask_prices;
            std::array<SizeType, 1> bid_sizes, ask_sizes;
            level2( bid_prices, bid_sizes, ask_prices, ask_sizes ) ;
            const SizeType tot = bid_sizes[0] + ask_sizes[0] ; 
            if (not tot) 
                return std::numeric_limits<double>::quiet_NaN(); 
            else
                return double(bid_prices[0]*ask_sizes[0] + ask_prices[0]*bid_sizes[0])/double(tot);
        }
    };
}
namespace std { 
    using namespace SDB;
    string to_string( const Side & side ) { 
        switch(side) {
            case Side::Bid : return "bid" ;
            case Side::Offer : return "ask" ;
        }
        throw std::runtime_error("WTF");
    }
}
