#pragma once
#include <set>
#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <unordered_set>
#include <limits>
#include <sstream>
#include <boost/container_hash/hash.hpp>

#include <iomanip>


#ifdef SPDLOG_ACTIVE_LEVEL 
#undef SPDLOG_ACTIVE_LEVEL 
#endif
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#ifndef SPDLOG_DEBUG_ON
#define SPDLOG_DEBUG_ON
#endif
#ifndef SPDLOG_TRACE_ON
#define SPDLOG_TRACE_ON
#endif
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "memory_manager.h"

namespace SDB { 
    constexpr double EPS = 1e-10 ; 

    enum class Side : std::uint8_t { Bid, Offer } ; 

    enum class NotifyMessageType : std::uint8_t { Ack, Trade, Cancel, End} ;

}

namespace std { 
    using namespace SDB;
    inline string to_string( const NotifyMessageType & message_type ) {
        switch (message_type) {
            case NotifyMessageType::Ack    : return "Ack   " ;
            case NotifyMessageType::Trade  : return "Trade " ;
            case NotifyMessageType::Cancel : return "Cancel" ; 
            case NotifyMessageType::End    : return "End   " ;
        }
        throw std::runtime_error("WTF to string message type");
    }
    inline string to_string( const Side & side ) { 
        switch(side) {
            case Side::Bid : return "bid" ;
            case Side::Offer : return "ask" ;
        }
        throw std::runtime_error("WTF to string side");
    }
}

namespace SDB { 

    inline std::ostream & operator<<(std::ostream & out, const NotifyMessageType & message_type ) {
        out << std::to_string(message_type) ;
        return out;
    }
    inline std::ostream & operator<<(std::ostream & out, const Side & side ) {
        out << std::to_string(side) ; 
        return out;
    }

    using OrderIDType = std::array<std::uint8_t, 12>  ; 
    using TimeType = int64_t ; 
    using ClientIDType = uint32_t;
    using LocalOrderIDType = uint32_t;
    using PriceType = int16_t;
    using SizeType = int16_t;

} 
namespace std {
    template <size_t N>
    std::ostream & operator<<(std::ostream & out, const std::array<std::uint8_t, N> & oid ) {
        /*
        for (const auto i : oid) 
            if (i > 0x20 and i < 0x7f) out << char(i);
            else out <<  '<' << std::format( "{:X}", i ) << '>' ;
            */

        out << "0x" ; 
        for (const auto i : oid) 
            out <<  std::format( "{:02x}", i );

        return out;
    }
    inline std::string to_string( const OrderIDType & oid) { 
        std::ostringstream out ;
        out << oid;
        return out.str();
    }
}
namespace SDB{
    inline std::string format_as( const NotifyMessageType & message) { return std::to_string(message); }
    inline std::string format_as( const Side & side) { return std::to_string(side); }
    inline Side get_other_side( Side s ) {
        switch (s) {
            case Side::Bid : return Side::Offer ;
            case Side::Offer : return Side::Bid ; 
        }
        throw std::runtime_error("Come on");
    }

    template <typename T, size_t N>
        inline void increment( std::array<T, N> & oid ) { 
            auto it = oid.begin();
            while (it != oid.end()) {
                if (*it < std::numeric_limits<T>::max() ) {
                    ++*it;
                    if (it != oid.begin() ) 
                        *(it-1) = 0;
                    return;
                } else
                    ++it;
            }
            throw std::runtime_error("maximum reached in increment: " + std::to_string(oid) );
        }

    template <typename OID, typename SET>
        inline void increment( OID & oid, const SET & set ) {
            increment(oid) ; 
            while (set.contains(oid))
                increment(oid);
        }

    struct Order;
    struct MatchingEngine ;
    template <typename T>
        concept INotifier = requires( T & notifier, const NotifyMessageType mtype, const Order & o, const TimeType now, 
                const SizeType traded_size, const PriceType traded_price , const MatchingEngine & eng,
                const OrderIDType & oid, const std::string & error_message)
        {
            notifier.log( mtype, o, now , traded_size, traded_price ) ;
            notifier.log( eng );
            notifier.error( oid, error_message ) ;
        };

    struct NOOPNotify {
        static void log( const NotifyMessageType , const Order & , const TimeType, const SizeType = 0, const PriceType = 0) {}
        static void log( const MatchingEngine & ) {}
        static void error( const OrderIDType & , const std::string & ) {}
        static NOOPNotify & instance(){ 
            static NOOPNotify n; 
            return n;
        }
    };

    //inline void NOOPNotify( const NotifyMessageType , const Order & , const TimeType, const SizeType = 0, const PriceType = 0) { };

    struct Order : public MemoryManaged{
        OrderIDType order_id_ ; 
        TimeType creation_time_ ; 
        ClientIDType client_id_ ;
        LocalOrderIDType local_id_ ;
        PriceType price_ ; 
        SizeType total_size_ ; 
        SizeType show_ ; 
        mutable SizeType remaining_size_ ; 
        mutable SizeType shown_size_ ; 
        Side side_ ; 
        bool is_shadow_ ;  //for simulation and strategy testing
        mutable bool is_hidden_ ; //this will be set by an observer who doesn't know total size or remaining size.


        template <INotifier N>
            void reset( TimeType t, ClientIDType cid, LocalOrderIDType local_id, PriceType p, SizeType s, SizeType show, Side side, bool is_shadow, N & notify ) 
            {
                order_id_.fill( std::numeric_limits<std::uint8_t>::max() ) ;
                creation_time_ = t ;
                client_id_ = cid;
                local_id_ = local_id;
                price_ = p;
                total_size_ = s ;
                show_ = show;
                remaining_size_ = s;
                shown_size_ = 0;
                side_ = side;
                is_shadow_ = is_shadow;
                is_hidden_ = false;
                replenish(notify, t);
            }
        template <INotifier N>
            void reset( const OrderIDType & oid, TimeType t, ClientIDType cid,LocalOrderIDType local_id, PriceType p, SizeType s, SizeType show, Side side, bool is_shadow, N & notify ) 
            {
                order_id_ = oid;
                creation_time_ = t ;
                client_id_ = cid;
                local_id_ = local_id;
                price_ = p;
                total_size_ = s ;
                show_ = show;
                remaining_size_ = s;
                shown_size_ = 0;
                side_ = side;
                is_shadow_ = is_shadow;
                is_hidden_ = false;
                replenish(notify, t);
            }

        void clone( const Order & o ) { 
            order_id_  =        o.order_id_ ; 
            creation_time_  =   o.creation_time_ ; 
            client_id_  =       o.client_id_ ;
            price_  =           o.price_ ; 
            total_size_  =      o.total_size_ ; 
            show_  =            o.show_ ; 
            remaining_size_  =  o.remaining_size_ ; 
            shown_size_  =      o.shown_size_ ; 
            side_  =            o.side_ ; 
            is_shadow_  =       o.is_shadow_ ;  
            is_hidden_  =       o.is_hidden_ ;     
        }
        Order() 
        {
            clear();
        }

        template <INotifier N>
            void replenish( N & notify, const TimeType t) const {
                if (remaining_size_ < 0)
                    throw std::logic_error(fmt::format("negative remaining size: {}", remaining_size_));
                if (show_ < 0)
                    throw std::logic_error(fmt::format("negative show: {}", show_));
                if (shown_size_ != 0)
                    throw std::runtime_error("Cannot replenish like this!");
                shown_size_ =  std::min(show_,remaining_size_) ;
                if (shown_size_ != 0 ) 
                    notify.log( NotifyMessageType::Ack , *this, t, 0, 0 ); //external world will see a new order popping at the back.
            }

        template <INotifier N>
            SizeType match( Order & aggressive_order , const TimeType now, N & notify) { 
                if (aggressive_order.side_ == side_) throw std::runtime_error("WTF!!!"); 
                const SizeType traded_size = std::min(aggressive_order.shown_size_ , shown_size_ );
                if (traded_size==0) throw std::runtime_error("WTF");
                aggressive_order._traded( traded_size , price_ , now, is_shadow_, notify);
                _traded( traded_size , price_ , now, aggressive_order.is_shadow_, notify);
                return traded_size;
            }

        
        void clear() { 
            reset(-1, -1, -1,0,0,0,Side::Offer, false, NOOPNotify::instance());
        }

        static bool reduce_size( const bool shadow, const bool other_side_shadow ) { 
            /*
             * f f -> t # usual case, nothing is shadow
             * f t -> f # if self is not shadow, don't reduce it's size
             * t f -> t # if self is shadow, reduce irrespective of if other side is shadow or not.
             * t t -> t  
             *
             */
            if (
                    (not shadow and not other_side_shadow ) or 
                    (    shadow ) 
               ) return true;
            else
                return false;
        }
        private: 
        template <INotifier N>
            void _traded(SizeType traded_size, PriceType traded_price, const TimeType now, const bool other_side_shadow, N & notify) const { 
                if ( reduce_size( is_shadow_ , other_side_shadow ) ) { 
                    remaining_size_ -= traded_size;     
                    shown_size_ -= traded_size;     
                }
                //traded_size here is a positive number. 
                //If our side is offer, we send -1xtraded_size to notif so that they reduce their positions:
                notify.log( NotifyMessageType::Trade , *this, now,
                        side_ == Side::Offer ? -traded_size : traded_size, 
                        traded_price );
                const bool finished = shown_size_==0;
                if (finished) notify.log( NotifyMessageType::End, *this, now , 0, 0 );
            }
        public : 
        struct Hash { 
            using is_transparent = void;
            size_t operator()( const Order * ptr ) const { 
                return boost::hash<OrderIDType>()(ptr->order_id_);
            }
            size_t operator()( const OrderIDType order_id ) const { 
                return boost::hash<OrderIDType>()(order_id);
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
            size_t operator()(  const Order * b, const OrderIDType a ) const { 
                return a == b->order_id_;
            }
        };
        using PtrSet = std::unordered_multiset< Order*, Order::Hash, Order::Eq>;

    };
    struct MarketState { 
        TimeType time_ ; 
        double wm_ ;
        std::array<PriceType, 4> bid_prices_;
        std::array<SizeType, 4>  bid_sizes_;
        std::array<float, 4> bid_ages_;
        std::array<PriceType, 4> ask_prices_;
        std::array<SizeType, 4>  ask_sizes_;
        std::array<float, 4> ask_ages_;
    };

    inline std::ostream & operator<<(std::ostream & out, const MarketState & market ) {
        out << std::right << std::fixed << std::setw(15)<<  std::setprecision(9) << static_cast<double>(market.time_) * 1e-9
                << ' '
                << std::right  << std::setw(3)<<  market.bid_sizes_[2]
                << "b@"
                << std::left << std::setw(3)<<  market.bid_prices_[2]
                << "age" 
                << std::left << std::setw(3)<<  market.bid_ages_[2]
                << ' '
                << std::right  << std::setw(3)<<  market.bid_sizes_[1]
                << "b@"
                << std::left << std::setw(3)<<  market.bid_prices_[1]
                << "age" 
                << std::left << std::setw(3)<<  market.bid_ages_[1]
                << ' '
                << std::right  << std::setw(3)<<  market.bid_sizes_[0]
                << "b@"
                << std::left << std::setw(3)<<  market.bid_prices_[0]
                << "age" 
                << std::left << std::setw(3)<<  market.bid_ages_[0]
                << "  wm:"
                << std::fixed << std::setw(5)<<  std::setprecision(2) << market.wm_
                << ' '
                << std::right  << std::setw(3)<<  market.ask_sizes_[0]
                << "a@"
                << std::left << std::setw(3)<<  market.ask_prices_[0]
                << "age" 
                << std::left << std::setw(3)<<  market.ask_ages_[0]
                << ' '
                << std::right  << std::setw(3)<<  market.ask_sizes_[1]
                << "a@"
                << std::left << std::setw(3)<<  market.ask_prices_[1]
                << "age" 
                << std::left << std::setw(3)<<  market.ask_ages_[1]
                << ' '
                << std::right  << std::setw(3)<<  market.ask_sizes_[2]
                << "a@"
                << std::left << std::setw(3)<<  market.ask_prices_[2] 
                << "age" 
                << std::left << std::setw(3)<<  market.ask_ages_[2]
                ;
        return out;
    }


}

namespace std { 
    using namespace SDB;
    inline string to_string( const Order & o ) { 
        std::ostringstream out; 
        out << "<O: c: " << o.creation_time_*1e-9
            << " " << o.side_  
            << " oid: " << o.order_id_  
            << " p: " << o.price_  
            //<< " ts: " << o.total_size_  
            << " show: " << o.show_  
            //<< " rs: " << o.remaining_size_  
            << " ss: " << o.shown_size_  
            << " shad: " << std::to_string(o.is_shadow_)  
            << " h: " << std::to_string(o.is_hidden_)  
            ;
        return out.str();
    }
}

namespace SDB { 

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

        friend std::ostream & operator<<(std::ostream & out, const Level & l ) {
            out << "<L: " << std::to_string( l.side_ ) 
                << " p: " << l.price_ ;
            for (const auto & o : l.orders_ )  {
                out << "(id:" << o.order_id_ << ",s:" << o.shown_size_ << ",rs:" << o.remaining_size_ << ')' ;
            }
            return out;
        }

        auto snapshot(const bool record_shadow_orders) const {
            std::tuple< PriceType, Side, MemoryManager<Order>::list_type > ret;
            std::get<0>(ret) = price_;
            std::get<1>(ret) = side_;
            for (const Order & o : orders_ ) { 
                if (o.is_shadow_ and not record_shadow_orders) continue;
                Order & clone = mem_.get_unused() ; 
                clone.clone(o); 
                std::get<2>(ret).push_back( clone );
            }
            return ret;
        }
        
        void add_order( Order & o, Order::PtrSet & ptr_set ) const { 
            if (o.price_ != price_ or o.side_ != side_ )
                throw std::runtime_error("Can't add this order to this level!");
            orders_.push_back( o );
            ptr_set.insert( &o );
        }

        SizeType total_shown() const {
            SizeType n = 0; 
            for (const auto & o : orders_) 
                n += o.shown_size_;
            return n;
        }

         float average_age(const TimeType now) const {
            double age_sum = 0; 
            int n = 0; 
            for (const auto & o : orders_)
                if (o.shown_size_>0) {
                    age_sum += (now - o.creation_time_)*1e-9;
                    ++n;
                }
            return age_sum/n;
        }
         float max_age(const TimeType now) const {
            double max_age = std::numeric_limits<double>::lowest();
            for (const auto & o : orders_)
                if (o.shown_size_>0)
                    max_age = std::max( max_age, (now - o.creation_time_)*1e-9 );
            return max_age;
        }
         float num_orders(const TimeType ) const {
            return orders_.size();
        }

        bool do_prices_agree( const Order & new_order ) const {
            if (side_ == new_order.side_) throw std::runtime_error("WTF");
            return price_ == new_order.price_ or cmp_( *this, new_order.price_ ); 
        }

        template <INotifier N>
            void match( Order & new_order, Order::PtrSet & ptr_set, const TimeType now, N & notify ) const { 
                if (not do_prices_agree(new_order) )
                    return;
                while (not orders_.empty() && new_order.remaining_size_ > 0) {
                    Order & order_in_book = orders_.front();
                    const SizeType traded_size = order_in_book.match( new_order, now, notify ) ;
                    if (traded_size == 0) throw std::runtime_error("SSSS");
                    if (order_in_book.shown_size_==0) {
                        orders_.pop_front();
                        if (order_in_book.remaining_size_!=0) { //hidden
                            order_in_book.replenish(notify, now);   
                            orders_.push_back( order_in_book ); 
                        } else {
                            size_t n_erased = 0; 
                            auto equal_range = ptr_set.equal_range(&order_in_book);
                            const size_t total_orders_with_same_oid = std::distance( equal_range.first, equal_range.second );
                            for ( auto it = equal_range.first; it != equal_range.second; ++it )
                                if ( *it == &order_in_book ) { 
                                    ptr_set.erase( it );
                                    n_erased += 1;
                                    break;
                                }
                            if (n_erased != 1) 
                                throw std::runtime_error("Expected to erase 1 but erased " + std::to_string(n_erased) );
                            if ( total_orders_with_same_oid == n_erased )
                                mem_.free(order_in_book);
                        }
                    }
                    if (new_order.shown_size_==0 and new_order.remaining_size_!=0)  //hidden
                        new_order.replenish(notify, now);   
                }
            }
        void match( Order & new_order, Order::PtrSet & ptr_set, const TimeType now) const { 
            match( new_order, ptr_set, now, NOOPNotify::instance() );
        }
    };

    template <INotifier N> 
        Order & get_new_order(
                MemoryManager<Order> & mem, 
                OrderIDType oid, TimeType t, ClientIDType cid, LocalOrderIDType lid, PriceType p, SizeType s, SizeType show, Side side, bool is_shadow,
                N & notify
                ) {
            if (s<0)
                throw std::logic_error(fmt::format("negative order size in a new order: {}", s));
            Order & o = mem.get_unused();
            o.reset(oid, t, cid, lid, p, s, show, side, is_shadow, notify);
            if (o.remaining_size_<0)
                throw std::logic_error(fmt::format("negative remaining size in a new order: {}", o.remaining_size_));
            //notify.log( NotifyMessageType::Ack , o, t, 0, 0 );
            return o;
        }

    inline Order & get_new_order(
            MemoryManager<Order> & mem, 
            OrderIDType oid, TimeType t, ClientIDType cid, LocalOrderIDType lid, PriceType p, SizeType s, SizeType show, Side side, bool is_shadow
            ) {
        return get_new_order(mem, oid, t, cid, lid, p, s, show, side, is_shadow, NOOPNotify::instance() );
    }
    struct MatchingEngine { 
        OrderIDType next_order_id_ ; 
        TimeType time_ ; 
        MemoryManager<Order> mem_ ; 
        Level::SET all_bids_, all_offers_ ; 
        Order::PtrSet ptr_set_;

        MatchingEngine() : time_(0) { next_order_id_.fill( std::numeric_limits<OrderIDType::value_type>::min() ); }

        friend std::ostream & operator<<(std::ostream & out, const MatchingEngine & l ) {
            out << "time: " << l.time_*1e-9 << '\n';
            for (auto it = l.all_offers_.rbegin(); it != l.all_offers_.rend(); ++it)
                out << *it << '\n' ; 
            for (auto it = l.all_bids_.begin(); it != l.all_bids_.end(); ++it)
                out << *it << '\n' ; 
            return out;
        }

        auto snapshot(const bool record_shadow_orders = true) const { 
            std::vector< std::tuple< PriceType, Side, MemoryManager<Order>::list_type > > ret;
            ret.reserve( all_offers_.size() + all_bids_.size() );
            for (auto it = all_offers_.rbegin(); it != all_offers_.rend(); ++it) {
                auto tpl = it->snapshot(record_shadow_orders);
                if ( not std::get<2>(tpl).empty() )
                    ret.emplace_back( std::move(tpl) );
            }
            for (auto it = all_bids_.begin(); it != all_bids_.end(); ++it) {
                auto tpl = it->snapshot(record_shadow_orders);
                if ( not std::get<2>(tpl).empty() )
                    ret.emplace_back( std::move(tpl) );
            }
            return std::make_tuple( time_, std::move(ret) );
        }

        //TODO every time order book is changed, call snapshot.
        void set_time( TimeType time) { 
            time_ = time ; 
        }
        Level::SET & get_book( const Side s ) { 
            if (s == Side::Bid) return all_bids_;
            else return all_offers_ ; 
        }
        private: 
        template <INotifier N> 
            void add_order(const OrderIDType oid, const ClientIDType client_id, const LocalOrderIDType lid, 
                    const PriceType price, const SizeType size, const SizeType show, const Side side, const bool is_shadow, N & notify) { 
                Order & new_order = get_new_order( mem_,oid, time_, client_id, lid, price, size, show, side, is_shadow, notify);
                auto & all_orders_other_side = get_book( get_other_side(side) );
                while (not all_orders_other_side.empty()) { 
                    const auto top_of_other_side_iter = all_orders_other_side.begin(); 
                    if (not top_of_other_side_iter->do_prices_agree( new_order ) )
                        break;
                    //now match:
                    top_of_other_side_iter->match( new_order, ptr_set_, time_, notify );
                    if (top_of_other_side_iter->orders_.empty()) 
                        all_orders_other_side.erase( top_of_other_side_iter );
                    if (new_order.remaining_size_==0)
                        break;
                }
                if (new_order.remaining_size_) 
                    get_book( side ).emplace( price, side, mem_ ).first->add_order( new_order, ptr_set_ ) ;
                else
                    mem_.free(new_order);
                //notify.log(*this);
            }
        public:

        template <INotifier N> 
            void add_simulation_order( const ClientIDType client_id, const LocalOrderIDType lid, const PriceType price, const SizeType size, const SizeType show, const Side side, const bool is_shadow, N & notify) { 
                //this method is for simulation. 
                add_order( next_order_id_, client_id, lid, price, size, show, side, is_shadow, notify);
                increment( next_order_id_);
            }
        template <INotifier N> 
            void add_replay_order( const OrderIDType oid, const ClientIDType client_id, const LocalOrderIDType lid, const PriceType price, const SizeType size, const Side side, const bool is_shadow, N & notify) { 
                //this method is for simulation. 
                add_order( oid, client_id, lid, price, size, size, side, is_shadow, notify);
            }
        template <INotifier N> 
            void cancel_order( const OrderIDType oid, N & notify ) { 
                auto eq_range = ptr_set_.equal_range(oid);
                for (auto it = eq_range.first; it != eq_range.second; ++it)
                    if ((*it)->order_id_ != oid) 
                        std::cerr << "OID mismatch " << oid << " " << *it << " " << std::to_string(**it) << std::endl;  
                if ( 1 != std::distance( eq_range.first, eq_range.second ) ) { 
                    //for (auto it = eq_range.first; it != eq_range.second; ++it) std::cerr << "OOOPS " << oid << " " << *it << " " << std::to_string(**it) << std::endl;  
                    notify.error( oid, std::to_string(time_) +
                            ": cancelling more than one order with oid " +
                            std::to_string(oid)  + ". Num orders is " +
                            std::to_string(std::distance( eq_range.first, eq_range.second ) ) + "." );
                    return;
                }
                Order & order = **eq_range.first;
                Level::SET & levels = get_book( order.side_ );
                auto levels_iterator = levels.find( order.price_ );
                if (levels_iterator == levels.end()) 
                    throw std::runtime_error("Cannot find price level " + std::to_string(order.price_));
                levels_iterator->orders_.erase( levels_iterator->orders_.iterator_to(order) );
                if ( levels_iterator->orders_.empty() )
                    levels.erase( levels_iterator );
                notify.log( NotifyMessageType::Cancel, order, time_ , 0, 0);
                notify.log( NotifyMessageType::End, order, time_ , 0, 0);
                ptr_set_.erase( eq_range.first ) ; 
                mem_.free(order);
                //notify.log(*this);
            }
        void cancel_order( const OrderIDType oid ) { 
            cancel_order( oid, NOOPNotify::instance() ) ;
        }
        template <INotifier N> 
            void shutdown(N & notify) { 
                while (not ptr_set_.empty()) {
                    OrderIDType oid = (*ptr_set_.begin())->order_id_ ; 
                    cancel_order(oid, notify);
                    notify.log( *this );
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
        template<size_t N> 
            void level25( 
                    std::array<PriceType, N> & bid_prices, 
                    std::array<SizeType, N> & bid_sizes, 
                    std::array<float, N> & bid_ages, 
                    std::array<PriceType, N> & ask_prices, 
                    std::array<SizeType, N> & ask_sizes, 
                    std::array<float, N> & ask_ages 
                    ) const 
            { 
                bid_prices.fill(0);
                ask_prices.fill(0);
                bid_sizes.fill(0);
                ask_sizes.fill(0);
                bid_ages.fill(0);
                ask_ages.fill(0);
                size_t i = 0; 
                auto it = all_bids_.begin();
                while( it != all_bids_.end() and i < N ) {
                    bid_prices[i] = it->price_ ; 
                    bid_sizes[i] = it->total_shown() ; 
                    bid_ages[i] = it->average_age(time_);
                    ++i ; ++it ;
                }
                i = 0; 
                it = all_offers_.begin();
                while( it != all_offers_.end() and i < N ) {
                    ask_prices[i] = it->price_ ; 
                    ask_sizes[i] = it->total_shown() ; 
                    ask_ages[i] = it->average_age(time_);
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
    struct LogNotify {
        MarketState market_;
        static void log( const NotifyMessageType mtype , const Order & o, const TimeType t, const SizeType trade_size,
                const PriceType trade_price) {
            //if (false) 
            SPDLOG_INFO( "t: {:12.9f} {}, cid:{}, age:{:12.9f}, side:{}, price:{:03d}, rs:{:05d}, ts:{}, tp:{}, oid:0x{:xspn}", 
                    t*1e-9, mtype, o.client_id_ , 
                    1e-9*(t-o.creation_time_), o.side_, 
                    o.price_, 
                    o.remaining_size_, trade_size, trade_price, 
                    spdlog::to_hex( o.order_id_ )
                    );
        }
        void log( const MatchingEngine & eng ) {
            market_.time_ = eng.time_ ;
            eng.level25(
                market_.bid_prices_, market_.bid_sizes_, market_.bid_ages_,
                market_.ask_prices_, market_.ask_sizes_, market_.ask_ages_
            );
            std::ostringstream ss; 
            ss << market_ ; 
            SPDLOG_INFO ("{}", ss.str() );
        }
        static void error( const OrderIDType & oid , const std::string & msg) {
            SPDLOG_ERROR( "0x{:xspn}: {}" , spdlog::to_hex(oid), msg);
        }
        static LogNotify & instance(){ 
            static LogNotify n; 
            return n;
        }
    };
    template<typename T> 
        bool safe_round( const double & d , T & out ) { 
            const auto ll = std::llround( d );
            out = T( ll );
            return 
                d  <= std::numeric_limits<decltype(ll)>::max() and 
                d  >= std::numeric_limits<decltype(ll)>::min() and 
                ll <= std::numeric_limits<T>::max() and 
                ll >= std::numeric_limits<T>::min() ;
        }

    template<typename T> 
        T safe_round( const double & d ) { 
            T t;
            if ( std::isnan( d ) ) 
                throw std::runtime_error(std::string("Cannot convert NaN to ") + typeid(T).name() );
            if ( not safe_round(d,t) ) throw std::runtime_error(
                    "cannot convert " + std::to_string(d) + " to " + typeid(T).name() );
            return t;
        }


}
