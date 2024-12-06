#pragma once
#include "boost/multi_index/ordered_index_fwd.hpp"
#include "ob.h"

#include <boost/random/exponential_distribution.hpp> 
#include <boost/random/poisson_distribution.hpp> 
#include <boost/random/normal_distribution.hpp> 
#include <boost/random/bernoulli_distribution.hpp> 
#include <boost/random/mersenne_twister.hpp> 

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>


#ifndef _MSC_VER
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wunused-parameter" 
#endif // _MSC_VER
#include <boost/heap/fibonacci_heap.hpp>
#ifndef _MSC_VER
#pragma GCC diagnostic pop 
#endif // _MSC_VER

#include <cmath>
#include <limits>
#include <stdexcept>

namespace SDB { 

    constexpr double EPS = 1e-10 ; 

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


    struct MarketState { 
        TimeType time_ ; 
        double wm_ ;
        std::array<PriceType, 2> bid_prices;
        std::array<SizeType, 2>  bid_sizes;
        std::array<PriceType, 2> ask_prices;
        std::array<SizeType, 2>  ask_sizes ;
    };

    struct Transport ;

    struct Agent {
        const MarketState & market_ ; 
        TimeType next_action_time_ ;

        struct OrderData { 
            const int local_id_;
            OrderIDType order_id_ ; 
            const PriceType price_ ; 
            const SizeType total_size_ ; 
            const SizeType show_ ; 
            mutable SizeType remaining_size_ ; 
            const Side side_ ; 
            bool waiting_to_be_cancelled_;
            OrderData ( const int local_id , PriceType price, SizeType total_size, SizeType show , Side side  ) :
                local_id_(local_id), price_(price), total_size_(total_size), show_(show), remaining_size_(total_size), side_(side), 
                waiting_to_be_cancelled_(false)
            { 
                order_id_.fill( std::numeric_limits<OrderIDType::value_type>::max() ); 
            }

            struct HashLocalID { 
                using is_transparent = void;
                size_t operator()( const int local_id ) const { 
                    return boost::hash<int>()(local_id) ; 
                }
                size_t operator()( const OrderData & od ) const { 
                    return this->operator()(od.local_id_) ; 
                }
            };
            struct EqLocalID { 
                using is_transparent = void;
                bool operator()( const int local_id , const OrderData & od) const { 
                    return od.local_id_ == local_id ; 
                }
                bool operator()( const OrderData & od2 , const OrderData & od) const { 
                    return od.local_id_ == od2.local_id_ ; 
                }
                bool operator()(  const OrderData & od, const int local_id ) const { 
                    return od.local_id_ == local_id ; 
                }
            };
            struct HashOID { 
                using is_transparent = void;
                size_t operator()( const OrderIDType & oid ) const { 
                    return boost::hash<OrderIDType>()(oid) ; 
                }
                size_t operator()( const OrderData & od ) const { 
                    return this->operator()(od.order_id_) ; 
                }
            };
            struct EqOID { 
                using is_transparent = void;
                bool operator()( const OrderIDType oid , const OrderData & od) const { 
                    return od.order_id_ == oid ; 
                }
                bool operator()( const OrderData & od2 , const OrderData & od) const { 
                    return od.order_id_ == od2.order_id_ ; 
                }
                bool operator()(  const OrderData & od, const OrderIDType & oid ) const { 
                    return od.order_id_ == oid ; 
                }
            };

            using LocalIDSet = std::unordered_set<OrderData, HashLocalID, EqLocalID> ;
            using OIDSet = std::unordered_set<OrderData, HashOID, EqOID> ;

            using SET = boost::multi_index::multi_index_container<
                OrderData, 
                boost::multi_index::indexed_by< 
                    boost::multi_index::hashed_unique< 
                        boost::multi_index::member< OrderData, const int , &OrderData::local_id_ >  
                    >,
                    boost::multi_index::hashed_non_unique< //non unique only at time of creation when all oids are same.
                        boost::multi_index::member< OrderData, OrderIDType, &OrderData::order_id_ >  , 
                        boost::hash<OrderIDType>
                    >
                >
            >;
        };

        OrderData::OIDSet orders_; 
        OrderData::LocalIDSet unacked_orders_; 

        Agent( const MarketState & market ) : 
            market_(market), 
            next_action_time_(std::numeric_limits<TimeType>::max()) {} ;

        virtual void handle_message( const OrderData & order_data, 
                const NotifyMessageType mtype, 
                const SizeType traded_size, 
                const PriceType traded_price ) = 0 ;

        void handle_own_order_message( 
                const NotifyMessageType mtype, 
                const int local_oid, 
                const OrderIDType & oid , 
                const SizeType traded_size, 
                const PriceType traded_price ) {
            OrderData::LocalIDSet::const_iterator local_oid_iterator ;
            OrderData::OIDSet::const_iterator oid_iterator ;
            switch( mtype ) { 
                case NotifyMessageType::Ack : 
                    local_oid_iterator = unacked_orders_.find(local_oid);
                    if (local_oid_iterator  == unacked_orders_.end())
                        throw std::runtime_error("Cannot find local oid: " + std::to_string(local_oid));
                    else {
                        OrderData od = *local_oid_iterator;
                        od.order_id_ = oid;
                        const auto pr = orders_.emplace( std::move( od ) );
                        if (not pr.second) 
                            throw std::runtime_error("Cannot insert oid: " + std::to_string(oid));
                        else
                            oid_iterator = pr.first;
                    }
                    break;
                case NotifyMessageType::Trade : 
                    oid_iterator = orders_.find(oid);
                    if (oid_iterator  == orders_.end())
                        throw std::runtime_error("Cannot find oid (t): " + std::to_string(oid));
                    else 
                        oid_iterator->remaining_size_ -= traded_size; 
                    break;
                case NotifyMessageType::Cancel : 
                    oid_iterator = orders_.find(oid);
                    if (oid_iterator  == orders_.end())
                        throw std::runtime_error("Cannot find oid (c): " + std::to_string(oid));
                    else if (not oid_iterator->waiting_to_be_cancelled_)
                        throw std::runtime_error("We didn't ask to be cancelled: " + std::to_string(oid));
                    else
                        oid_iterator->remaining_size_ = 0 ;
                    break;
                case NotifyMessageType::End : 
                    oid_iterator = orders_.find(oid);
                    if (oid_iterator  == orders_.end())
                        throw std::runtime_error("Cannot find oid (e): " + std::to_string(oid));
                    else if (0 != oid_iterator->remaining_size_)
                        throw std::runtime_error("We didn't expect this order to end: " + std::to_string(oid));
                    break;
            };
            handle_message(  *oid_iterator, mtype, traded_size, traded_price );
            if (mtype == NotifyMessageType::End)
                orders_.erase( oid_iterator );
        }; 
        virtual void markets_state_changed(Transport & transport) = 0 ; //market state has changed. determine next action time if needed
        //method to make time go faster:
        TimeType next_action_time_and_type() { return next_action_time_ ; }  ; 
        //actions in the market:

    };

    struct Transport {
        virtual void place_order( const Agent::OrderData & od ) = 0 ;
        virtual void cancel( const OrderIDType & oid ) = 0 ;
    };

    struct PriceMakerAroundWM : public Agent { 
        //these agents decide on the price based on normal distribution around wm.
        //cancel time is also coming from a distribution. 

        using CancellationTime = struct { 
            const TimeType t_cancel_; 
            const OrderIDType order_id_;
        } ; 

        using CancellationTimes = boost::multi_index::multi_index_container<
            CancellationTime, 
            boost::multi_index::indexed_by< 
                boost::multi_index::ordered_non_unique< 
                    boost::multi_index::member< CancellationTime, const TimeType , &CancellationTime::t_cancel_ >  
                >,
                boost::multi_index::hashed_unique< 
                    boost::multi_index::member< CancellationTime, const OrderIDType , &CancellationTime::order_id_ >  , 
                    boost::hash<OrderIDType>
                >
            >
        >;

        int local_id_counter_;
        boost::random::mt19937 & mt_;
        const boost::random::exponential_distribution<> placement_, cancellation_;
        mutable boost::random::normal_distribution<double> order_price_;
        const boost::random::poisson_distribution<SizeType, double> order_size_;
        const size_t n_orders_ ;
        TimeType placement_time_ ; 

        CancellationTimes cancellation_times_ ; 

        PriceMakerAroundWM( const MarketState & market,
                boost::random::mt19937 & mt, 
                const double placement_lambda, 
                const double cancellation_lambda, 
                double order_price_mean, //how far from wm the price is 
                double order_price_std,
                double order_size_mean,
                const size_t n_orders
                ) : Agent(market), 
                local_id_counter_(0),
                mt_(mt),
                placement_(placement_lambda), 
                cancellation_(cancellation_lambda) , 
                order_price_(order_price_mean, order_price_std ),
                order_size_(order_size_mean),
                n_orders_(n_orders)
        {
            next_action_time_ = safe_round<TimeType>(1e9*placement_(mt_));
            placement_time_ = next_action_time_; 
        }

        virtual void markets_state_changed(Transport & transport) {
            //market state has changed. determine next action time if needed
            if (market_.time_ >= placement_time_ and orders_.size() + unacked_orders_.size() < n_orders_ and not std::isnan( market_.wm_ ) ) {
                const double d_price = order_price_( mt_ );
                OrderData od( local_id_counter_++, safe_round<PriceType>( d_price + market_.wm_ ) , 1 + order_size_(mt_) , 2, (d_price > 0) ? Side::Offer : Side::Bid ); 
                unacked_orders_.emplace( std::move( od ) );
                transport.place_order( od );
                placement_time_ += safe_round<TimeType>(1e9*placement_(mt_));
            }
            auto & index = cancellation_times_.get<0>() ; 
            while( not index.empty() and market_.time_ >= index.begin()->t_cancel_ ) { 
                transport.cancel( index.begin()->order_id_ );
                index.erase(index.begin());
            }
            if (index.empty())
                next_action_time_ = placement_time_ ;
            else 
                next_action_time_ = std::min(placement_time_, index.begin()->t_cancel_) ;
        }

        virtual void handle_message( const OrderData & order_data, const NotifyMessageType mtype, 
                const SizeType , const PriceType  ) {
            switch (mtype) {
                case NotifyMessageType::Ack : 
                    { 
                        //setup cancellation time
                        TimeType cancellation_time =  market_.time_ + cancellation_( mt_ );
                        cancellation_times_.emplace( cancellation_time, order_data.order_id_ );
                        next_action_time_ = std::min( next_action_time_, cancellation_time );
                        break;
                    } 
                case NotifyMessageType::Cancel : 
                case NotifyMessageType::End : 
                    {
                        cancellation_times_.get<1>().erase( order_data.order_id_ );
                        break;
                    }
                case NotifyMessageType::Trade : 
                    break;
            }
        }
    };


    struct ClientType { 

        public : 
            const std::string tag_ ; 
        private : 
            boost::random::mt19937 & mt_;

            const double placement_lambda_, cancellation_lambda_, order_size_mean_, order_price_std_, side_mean_;

            const boost::random::exponential_distribution<> placement_, cancellation_;
            mutable boost::random::normal_distribution<double> order_price_;
            const boost::random::poisson_distribution<SizeType, double> order_size_;
            const boost::random::bernoulli_distribution<double> side_;

        public : 
            ClientType( 
                    const std::string & tag, 
                    boost::random::mt19937 & mt,
                    const double placement_lambda, const double cancellation_lambda, double order_size_mean,
                    double order_price_std, double side_mean) :
                tag_(tag),
                mt_(mt),
                placement_lambda_(placement_lambda), 
                cancellation_lambda_(cancellation_lambda), 
                order_size_mean_(order_size_mean) , 
                order_price_std_(order_price_std) , 
                side_mean_(side_mean),
                placement_(placement_lambda_), 
                cancellation_(cancellation_lambda_), 
                order_price_( 0, order_price_std_ ),
                order_size_(order_size_mean_) , 
                side_(side_mean_) 
        { }

            SizeType get_size() const { 
                return 1 + order_size_(mt_);
            }

            double get_price() const { 
                return order_price_(mt_);
            }
            TimeType get_placement_dt() const {
                //return TimeType(lround(1e9*placement_(mt_)));
                return safe_round<TimeType>(1e9*placement_(mt_));
            }
            TimeType get_cancellation_dt( ) const {
                //return TimeType(lround(1e9*cancellation_(mt_)));
                return safe_round<TimeType>(1e9*cancellation_(mt_));
            }

            auto get_next_placement_info() const { 
                const TimeType t = get_placement_dt();
                const double p = get_price();
                const SizeType size = get_size();
                const Side side = side_(mt_) ? Side::Bid : Side::Offer;
                return  std::make_tuple(t, p, size, side);
            }

    };

    struct CerrLogger { 
        const char * to_string( 
                const NotifyMessageType msg_type, 
                const Order & , 
                const TimeType ,
                const SizeType , 
                const SizeType 
                ) { 
            static char msg[1024];
                /*
            if (msg_type == NotifyMessageType::Ack) 
                std::snprintf(msg, 1024, "%fs:N:%llu:%d:%d:%d:%d", notif_time*1e-9, order.order_id_, order.price_, order.shown_size_ , traded_size, traded_price);
            else if (msg_type == NotifyMessageType::End) 
                std::snprintf(msg, 1024, "%fs:D:%llu:%d:%d:%d:%d", notif_time*1e-9, order.order_id_, order.price_, order.shown_size_ , traded_size, traded_price);
            else if (msg_type == NotifyMessageType::Trade) 
                std::snprintf(msg, 1024, "%fs:T:%llu:%d:%d:%d:%d", notif_time*1e-9, order.order_id_, order.price_, order.shown_size_ , traded_size, traded_price);
            else */
                throw std::runtime_error("What is this type? " + std::to_string(int(msg_type)) );
            return msg;
        }
        void log( const NotifyMessageType , const Order & , const TimeType , const SizeType , const SizeType ) { }
        void log( const MatchingEngine & ) {}
        /*
        void log( 
                const NotifyMessageType msg_type, 
                const Order & order, 
                const TimeType notif_time ,
                const SizeType traded_size ,
                const SizeType traded_price
        ) { 
            std::cout << to_string(msg_type, order, notif_time, traded_size, traded_price) << std::endl;
        } */
    };
    
    struct ClientState { 
        const ClientType & client_type_;
        const ClientIDType client_id_ ; 
        OrderIDType active_order_id_ ; 
        mutable TimeType next_action_time_;
        PriceType price_ ; 
        SizeType size_, show_ ; 
        Side side_ ; 
        uint8_t action_ ; // 0-> place order; 1->cancel order

        ClientState( const ClientType & type, ClientIDType cid ) : 
            client_type_(type) , 
            client_id_(cid) , 
            active_order_id_( std::numeric_limits<OrderIDType>::max() ), 
            next_action_time_(0), 
            price_(std::numeric_limits<PriceType>::max()),
            size_(std::numeric_limits<SizeType>::max()),
            show_(std::numeric_limits<SizeType>::max()),
            side_(Side::Bid),
            action_(std::numeric_limits<uint8_t>::max())
        {}

        struct TIME{} ;
        struct CID{} ;
        struct OID{} ;


        void setup_new_order_placement_data(
                const double wm, 
                const TimeType now ) { 
            auto [dt, d_price, size, side] = client_type_.get_next_placement_info();
            const double real_price = std::isnan(wm) ? d_price : wm + d_price;
            // price_ = std::lround( real_price );
            price_ = safe_round<PriceType>( real_price );
            show_ = 2;
            side_ = side;
            size_ = size;
            if (not size) throw std::runtime_error("Zero size??");
            action_ = 0; //next action is order_placement; 
            active_order_id_ = std::numeric_limits<OrderIDType>::max() ; 
            next_action_time_ = now + dt;
        }


        struct Ptr {
            ClientState * p_ ; 
            Ptr( ClientState *p) : p_(p) {} 

            TimeType next_action_time() const { return p_->next_action_time_ ; } 
            ClientIDType client_id() const { return p_->client_id_ ; } 
            OrderIDType active_order_id() const { return p_->active_order_id_ ; } 

            struct HashCID { 
                using is_transparent = void ;
                size_t operator()( const Ptr & p) const {
                    return this->operator()(p.client_id());
                }
                size_t operator()( const ClientIDType & cid) const {
                    return std::hash<ClientIDType>()(cid);
                }
            };
            struct EqCID {
                using is_transparent = void ;
                bool operator()( const Ptr & a, const Ptr & b) const {
                    return a.client_id() == b.client_id() ;
                }
                bool operator()( const Ptr & a, const ClientIDType cid) const {
                    return a.client_id() == cid ;
                }
                bool operator()(const ClientIDType cid, const Ptr & a) const {
                    return a.client_id() == cid ;
                }
            };

            struct HashOID { 
                using is_transparent = void ;
                size_t operator()( const Ptr & p) const {
                    return this->operator()(p.active_order_id());
                }
                size_t operator()( const OrderIDType & oid) const {
                    return boost::hash<OrderIDType>()(oid);
                }
            };
            struct EqOID {
                using is_transparent = void ;
                bool operator()( const Ptr & a, const Ptr & b) const {
                    return a.active_order_id() == b.active_order_id() ;
                }
            };

            struct LessTime { 
                bool operator()( const Ptr & a, const Ptr & b) const {
                    if (a.next_action_time() == b.next_action_time()) 
                        return a.client_id() < b.client_id();
                    else 
                        return a.next_action_time() < b.next_action_time() ;
                }
            };

            struct MoreTime { 
                bool operator()( const Ptr & a, const Ptr & b) const {
                    return a.next_action_time() > b.next_action_time() ;
                }
            };
            using ByCID  = std::unordered_set<Ptr, HashCID, EqCID>;
            using ByOID  = std::unordered_set<Ptr, HashOID, EqOID>;
            //using ByTime = std::priority_queue<Ptr, std::vector<Ptr>, MoreTime> ; 

            using ByTime = boost::heap::fibonacci_heap<Ptr, boost::heap::compare<MoreTime> > ; 
            ByTime::handle_type handle_ ;

        };

        template <INotifier Logger = CerrLogger> 
        struct NotificationHandler { 

            Logger & logger_;

            MatchingEngine & eng_;

            Ptr::ByCID by_cid_;
            mutable Ptr::ByOID by_oid_;
            mutable Ptr::ByTime by_time_ ; 

            NotificationHandler( Logger & logger, MatchingEngine & eng ) : logger_(logger), eng_(eng)  { } ;

            void add( ClientState * ptr ) { 
                auto handle = by_time_.emplace( ptr );
                (*handle).handle_ = handle; 
                by_cid_.emplace( *handle );
            }

            Ptr get_by_cid( ClientIDType cid ) const { 
                auto ptr_it = by_cid_.find( cid );
                if (ptr_it == by_cid_.end()) 
                    throw std::runtime_error("received ack for an unknown client: " + std::to_string(cid));
                return *ptr_it;
            }

            void log( const MatchingEngine & eng ) { logger_.log(eng); }

            void log( const NotifyMessageType msg_type , const Order & order, const TimeType notif_time, 
                    const SizeType traded_size , const PriceType traded_price 
            ) { 
                logger_.log( msg_type, order, notif_time, traded_size, traded_price );
                Ptr p = get_by_cid(order.client_id_);
                if (msg_type==NotifyMessageType::Ack) { 
                    if (p.active_order_id() != std::numeric_limits<OrderIDType>::max()) { 
                        //throw std::runtime_error("received ack but there is already an active order: " + std::to_string(p.active_order_id()));
                        order.is_hidden_ = true;
                    } else {
                        //new order
                        p.p_->next_action_time_ = notif_time + p.p_->client_type_.get_cancellation_dt();
                        p.p_->action_ = 1; //next action is cancellation; 
                        p.p_->active_order_id_ = order.order_id_ ; 
                        by_time_.update( p.handle_ );
                        //by_time_.emplace( p );
                        by_oid_.emplace( p );
                    }
                } else if (msg_type==NotifyMessageType::End) {
                    if (p.active_order_id() != order.order_id_) 
                        throw std::runtime_error("received end but this is not our order : " + std::to_string(p.active_order_id()));
                    const auto n_erased = by_oid_.erase( p );
                    if (n_erased != 1)
                        throw std::runtime_error("could not erase only 1 : erased " + std::to_string(n_erased) );
                    
                    p.p_->setup_new_order_placement_data(eng_.wm(), notif_time);
                    by_time_.update( p.handle_ );
                    by_oid_.emplace( p );
                } else if (msg_type==NotifyMessageType::Trade) {
                } else if (msg_type==NotifyMessageType::Cancel) {
                } else {
                    throw std::logic_error("Not implemented " + std::to_string(int(msg_type)) );
                }
            }
            void operator()( const NotifyMessageType msg_type , const Order & order, const TimeType notif_time) {
                this->operator()(msg_type, order, notif_time, 0, 0);
            }
            const Ptr & get_next_action() {
                const Ptr & wrapper = by_time_.top(); 
                return wrapper;
            }
        };
    };

    template <INotifier  N>
        void simulate(
                const std::vector<std::tuple<ClientType, int>> & client_types_and_sizes , 
                MatchingEngine & eng,
                ClientState::NotificationHandler<N> & handler ,
                const TimeType TMax
                ) {

            ClientIDType n_clients = 0;
            for (const auto & tpl : client_types_and_sizes) n_clients += std::get<1>(tpl);

            std::vector<ClientState> client_states; 
            client_states.reserve( n_clients );
            n_clients = 0; 
            for (const auto & tpl : client_types_and_sizes)  
                for (int i = 0; i < std::get<1>(tpl) ; ++i) 
                    client_states.emplace_back( std::get<0>(tpl) , n_clients++ );

            for (auto & cs : client_states) {
                cs.setup_new_order_placement_data(0.0, 0); //set action time
                handler.add(&cs);
            }
            while ( eng.time_ < TMax ) { 
                const ClientState::Ptr & ptr = handler.get_next_action();
                const ClientState & state = *ptr.p_;
                if (state.next_action_time_>=TMax) break;
                eng.set_time( state.next_action_time_ );
                //send the ptr to the back of queue : 
                state.next_action_time_ = std::numeric_limits<TimeType>::max();
                handler.by_time_.update( ptr.handle_ );
                if (state.action_ == 0) {//place new order
                    eng.add_simulation_order( state.client_id_, state.price_, state.size_,
                            state.show_, state.side_, false, handler);
                } else if (state.action_ == 1) { //cancel
                    eng.cancel_order( state.active_order_id_, handler );
                }
                handler.log( eng );
            }

            eng.set_time( TMax );
            eng.shutdown(handler);

            //handler.log( eng );
        }
    struct replay_error : public std::runtime_error {
        explicit replay_error( const std::string & what) : std::runtime_error(what) {}
    };

    struct OrderBookEvent {
        TimeType event_time_; 
        OrderIDType oid_ ; 
        PriceType price_, trade_price_ ; 
        SizeType size_, trade_size_ ; 
        NotifyMessageType mtype_ ;        
        Side side_ ; 
        struct TimeLess { 
            bool operator()( const OrderBookEvent & a, const OrderBookEvent & b ) const {
                return a.event_time_ < b.event_time_ ;
            }
        };
        friend std::ostream & operator<<(std::ostream & out, const OrderBookEvent & obe ) {
            out << "<OBE @ " <<  obe.event_time_ 
                << " " << obe.mtype_
                << " " << obe.side_
                << " oid:" << obe.oid_
                << " p:" << obe.price_
                << " tp:" << obe.trade_price_
                << " s:" << obe.size_
                << " ts:" << obe.trade_size_
                << ">";
            return out;
        }
    };
    struct OrderBookEventWithClientID : public OrderBookEvent {
        ClientIDType cid_ ; 
    };

    template <typename T> 
        concept OBEConcept = requires ( T & obe ) {
            { obe.event_time_ } -> std::same_as<TimeType&>;
            { obe.oid_ } -> std::same_as<OrderIDType&>;
            { obe.price_ } -> std::same_as<PriceType&>;
            { obe.trade_price_ } -> std::same_as<PriceType&>;
            { obe.size_ } -> std::same_as<SizeType&>;
            { obe.trade_size_ } -> std::same_as<SizeType&>;
            { obe.mtype_ } -> std::same_as<NotifyMessageType&>;
            { obe.side_ } -> std::same_as<Side&>;
        };

    inline ClientIDType get_cid( const OrderBookEvent & , ClientIDType cid ) { return cid ; } 
    inline ClientIDType get_cid( const OrderBookEventWithClientID & obe, ClientIDType ) { return obe.cid_  ; } 


    inline void emplace_back( std::vector<OrderBookEvent> & vec, 
            const TimeType event_time, 
            const OrderIDType oid , 
            const PriceType price, 
            const PriceType trade_price , 
            const SizeType size, 
            const SizeType trade_size , 
            const NotifyMessageType mtype ,        
            const Side side , 
            const ClientIDType 
            ) {
        vec.emplace_back( event_time, oid, price, trade_price, size, trade_size, mtype, side );
    }

    inline void emplace_back( std::vector<OrderBookEventWithClientID> & vec, 
            const TimeType event_time, 
            const OrderIDType oid , 
            const PriceType price, 
            const PriceType trade_price , 
            const SizeType size, 
            const SizeType trade_size , 
            const NotifyMessageType mtype ,        
            const Side side , 
            const ClientIDType cid
            ) {
        vec.emplace_back( OrderBookEvent( event_time, oid, price, trade_price, size, trade_size, mtype, side) , cid );
    }

    enum class OrderStatus : uint8_t { Unknown, Acked, End }; 
    inline std::ostream & operator<<(std::ostream & out, const OrderStatus & status ) {
        if (status == OrderStatus::Unknown ) out << "Unknown"; 
        else if (status == OrderStatus::Acked ) out << "Acked"; 
        else if (status == OrderStatus::End ) out << "End"; 
        return out;
    }

    
    template <OBEConcept OBE = OrderBookEvent> 
        struct SimulationHandler  {
            OrderStatus simulated_order_status_ ;
            SimulationHandler() : simulated_order_status_(OrderStatus::Unknown) {}
            void log( const NotifyMessageType  , const Order & , const TimeType , const SizeType  = 0, const PriceType = 0) {
                throw std::runtime_error("Not implemented");
            }
            void log( const MatchingEngine &  ) {
                throw std::runtime_error("Not implemented");
            }
        };

    template <OBEConcept OBE = OrderBookEvent> 
        struct RecordingSimulationHandler {
            //data
            OrderStatus simulated_order_status_ ;
            const bool record_msgs_;
            const bool record_shadow_;
            const bool record_shadow_trades_;
            MemoryManager<Order> * const mem_;
            std::ostream * out_;
            std::vector<std::tuple<TimeType,PriceType>> trades_ ; 
            std::vector<std::tuple<TimeType,double>> wm_ ; 
            std::vector<OBE> msgs_ ;
            std::vector< 
                std::tuple<
                TimeType, 
                std::vector< std::tuple< PriceType, Side, MemoryManager<Order>::list_type > >
                    >
            > snapshots_ ; 

            //ctor dtor
            RecordingSimulationHandler( 
                    MemoryManager<Order> * mem , 
                    bool record_msgs, 
                    bool record_shadow ,
                    bool record_shadow_trades,
                    std::ostream * out ) : 
                simulated_order_status_(OrderStatus::Unknown),
                record_msgs_(record_msgs), record_shadow_(record_shadow), 
                record_shadow_trades_(record_shadow_trades) ,mem_(mem), out_(out) {};
            ~RecordingSimulationHandler() { free_orders(); }

            //INotifier
            void log( const NotifyMessageType mtype , const Order & o, const TimeType t, const SizeType trade_size = 0, const PriceType trade_price = 0) { 
                if (out_ != nullptr)
                    *out_ << t << ' ' << mtype << " " << std::to_string(o) << " ts:" << trade_size << " tp:"  << trade_price << '\n';
                if (record_msgs_) {
                    if (record_shadow_ or not o.is_shadow_)
                        emplace_back( msgs_, t, o.order_id_, o.price_, trade_price, o.shown_size_, trade_size, mtype, o.side_ , o.client_id_ ) ;
                } 
                if (record_shadow_trades_ and mtype == NotifyMessageType::Trade and o.is_shadow_ ) 
                    trades_.emplace_back( t, trade_price );
                if (o.is_shadow_) { 
                    switch( mtype ) {
                        case NotifyMessageType::Ack : 
                            simulated_order_status_ = OrderStatus::Acked; 
                            break;
                        case NotifyMessageType::End :  
                            simulated_order_status_ = OrderStatus::End; 
                            break;
                        default :  ;
                    }
                }
            }
            void log( const MatchingEngine & eng ) {
                if (mem_ != nullptr)
                    snapshots_.emplace_back( eng.snapshot(record_shadow_) );
                if ( 
                        not wm_.empty() and 
                        std::get<0>(wm_.back()) == eng.time_ )  
                    //update last one if time is same
                    std::get<1>(wm_.back()) = eng.wm();
                else
                    //different time, so push back
                    wm_.emplace_back( eng.time_,  eng.wm() );  
            }

            //mem management
            void free_orders( ) { 
                if (mem_==nullptr) return;
                struct Disposer {
                    MemoryManager<Order> & mem_;
                    void operator()( Order * t ) const {//Disposer
                        mem_.free( *t );
                    }
                } ;
                Disposer disposer(*mem_);
                for ( auto & v : snapshots_ ) 
                    for ( auto & tpl : std::get<1>(v) ) 
                        while ( not std::get<2>(tpl).empty() ) 
                            std::get<2>(tpl).pop_front_and_dispose( disposer );
            }
        };

    template <typename T>
        concept ISimulation = requires( T & notifier ) { 
            { notifier.simulated_order_status_  } -> std::same_as<OrderStatus&>;
        };

    template <typename T> concept ISimulationNotifier = ISimulation<T> && INotifier<T>; 

    template <OBEConcept OBE, ISimulationNotifier SN>
        void simulate_a(
                const std::vector<OBE> & msgs, 
                const std::vector<TimeType> & times,  
                const std::vector<double> & algo_prices,  
                const Side side,
                const std::unordered_set<OrderIDType, boost::hash<OrderIDType>> & ids_not_to_be_used_by_simulator, 
                const ClientIDType cid_shadow,
                const ClientIDType default_cid_market,
                MatchingEngine & eng,
                SN & handler
                ) { 

            if (times.size() != algo_prices.size()) throw std::runtime_error("sizes");
            if (times.front() != msgs.front().event_time_) throw std::runtime_error("front");
            if (times.back() != msgs.back().event_time_) throw std::runtime_error("back");
            if (times.size() > msgs.size()) throw std::runtime_error(
                    "times sizes don't work: " + std::to_string(times.size()) + " vs " + std::to_string( msgs.size()) );

            OrderIDType oid; 
            oid.fill(0);
            increment(oid, ids_not_to_be_used_by_simulator);

            //PriceType algo_price;
            double algo_price = std::numeric_limits<double>::quiet_NaN();
            auto msgs_it = msgs.begin();
            for ( size_t time_index = 0;  time_index < times.size(); ++time_index ) { 
                if ( times[time_index] != msgs_it->event_time_ )
                    throw std::runtime_error("wtf: " + std::to_string(time_index));
                eng.set_time( msgs_it->event_time_ );
                const auto same_time_end_it = std::upper_bound( msgs_it, msgs.end(), *msgs_it , OrderBookEvent::TimeLess()) ;
                if (same_time_end_it != msgs.end() and  same_time_end_it->event_time_ <= msgs_it->event_time_ )
                    throw replay_error( std::string("time order has failed: ") +
                            std::to_string(same_time_end_it->event_time_) + " vs " 
                            + std::to_string( msgs_it->event_time_ ) );
                while ( msgs_it < same_time_end_it ) { 
                    switch ( msgs_it->mtype_ ) {
                        case NotifyMessageType::Ack : 
                            {
                                for (auto kt = msgs_it + 1; kt  < same_time_end_it ; ++kt ) {
                                    if( kt->event_time_ != msgs_it->event_time_ ) 
                                        throw replay_error( std::string("time should be same: ") + 
                                                std::to_string(kt->event_time_) + " vs " + 
                                                std::to_string( msgs_it->event_time_ ) );
                                    if (kt->mtype_== NotifyMessageType::Ack and kt->oid_ != msgs_it->oid_ )
                                        eng.add_replay_order(kt->oid_, get_cid(*kt,default_cid_market), kt->price_, kt->size_, kt->side_, false, handler);
                                }
                                eng.add_replay_order(msgs_it->oid_, get_cid(*msgs_it,default_cid_market), msgs_it->price_, msgs_it->size_, msgs_it->side_, false, handler);
                                for (auto kt = msgs_it + 1; kt  < same_time_end_it ; ++kt ) {
                                    if (kt->mtype_== NotifyMessageType::Ack and kt->oid_ == msgs_it->oid_ )
                                        eng.add_replay_order(kt->oid_, get_cid(*kt,default_cid_market), kt->price_, kt->size_, kt->side_, false, handler);
                                }
                                handler.log(eng);
                                msgs_it = same_time_end_it;
                            }
                            break;
                        case NotifyMessageType::Cancel : 
                            eng.cancel_order(msgs_it->oid_, handler);
                            handler.log(eng);
                            ++msgs_it; 
                            break;
                        case NotifyMessageType::End : 
                        case NotifyMessageType::Trade : 
                            ++msgs_it;
                            break;
                    } 
                }
                //PriceType algo_price_t = std::lround( algo_prices[time_index] );

                if ( std::isnan( algo_prices[time_index] ) ) { 
                    //if we have an order, we need to cancel it. 
                    if ( not std::isnan( algo_price ) ) { 
                        algo_price = algo_prices[time_index]; 
                        eng.cancel_order( oid, handler ); 
                    }  
                } else { 
                    PriceType algo_price_t = safe_round<PriceType>( algo_prices[time_index] );
                    if (time_index==0) {
                        eng.add_replay_order( oid , cid_shadow, algo_price_t, 1, side, true, handler );
                    } else if (
                            std::fabs( algo_prices[time_index] - algo_price ) > 1e-7 or 
                            handler.simulated_order_status_ == OrderStatus::End ) {
                        if (handler.simulated_order_status_ != OrderStatus::End) 
                            eng.cancel_order( oid, handler ); //cancel if not already gone.
                        increment(oid, ids_not_to_be_used_by_simulator);
                        eng.add_replay_order( oid , cid_shadow, algo_price_t, 1, side, true, handler );
                    }
                    algo_price = algo_prices[time_index]; 
                }
            }

        }
    template <OBEConcept OBE = OrderBookEvent> 
        struct StatisticsSimulationHandler {
            OrderStatus simulated_order_status_ ;
            TimeType prev_time_;
            double prev_wm_, sum_wm_by_dt_, sum_dt_; //from a trade to trade where wm avg is
            double sum_return_by_dT_, sum_dT_ ; //across trades

            StatisticsSimulationHandler( ) :
                simulated_order_status_(OrderStatus::Unknown),
                prev_time_(std::numeric_limits<TimeType>::min()),
                prev_wm_(std::numeric_limits<double>::quiet_NaN()) , 
                sum_wm_by_dt_(0), 
                sum_dt_(0),
                sum_return_by_dT_(0),
                sum_dT_(0)
                {}

            void log( const NotifyMessageType mtype , const Order & o, const TimeType , const SizeType = 0, const PriceType trade_price = 0) { 
                if (mtype == NotifyMessageType::Trade and o.is_shadow_ ) {
                    if ( sum_dt_ > EPS ) {
                        const int side_multiplier = (o.side_ == Side::Offer) ? 1 : -1 ;
                        //if I am selling , trade price > wm means I sold high, great:
                        const double average_return_since_last_trade = side_multiplier * ( trade_price - sum_wm_by_dt_ / sum_dt_ );
                        sum_return_by_dT_ += average_return_since_last_trade * sum_dt_ ; 
                        sum_dT_ += sum_dt_ ; 
                    }
                    sum_dt_ = 0; 
                    sum_wm_by_dt_ = 0 ;
                }
                if (o.is_shadow_) { 
                    switch( mtype ) {
                        case NotifyMessageType::Ack : 
                            simulated_order_status_ = OrderStatus::Acked; 
                            break;
                        case NotifyMessageType::End :  
                            simulated_order_status_ = OrderStatus::End; 
                            break;
                        default :  ;
                    }
                }
            }
            template <typename ME>
                void log( const ME & eng) { 
                    if ( not std::isnan( prev_wm_ ) ) {
                        double dt = eng.time_ - prev_time_ ; 
                        sum_wm_by_dt_ += prev_wm_ * dt;
                        sum_dt_ += dt;
                    } 
                    prev_wm_ = eng.wm(); 
                    prev_time_ = eng.time_ ; 
                }

        };

}

namespace std { 
    using namespace SDB;
    inline string to_string( const SDB::ClientState & s ) { 
        const std::string next_time = 
            s.next_action_time_ == std::numeric_limits<TimeType>::max() ? 
            "-" : std::to_string(s.next_action_time_*1e-9) + "s" ; 
        const std::string oid = 
            s.active_order_id_ == std::numeric_limits<OrderIDType>::max() ? 
            "-" : std::to_string(s.active_order_id_) ; 
        const std::string cid = 
            s.client_id_ == std::numeric_limits<ClientIDType>::max() ? 
            "-" : std::to_string(s.client_id_) ; 
        const std::string price = 
            s.price_ == std::numeric_limits<PriceType>::max() ? 
            "-" : std::to_string(s.price_) ; 
        const std::string size = 
            s.size_ == std::numeric_limits<SizeType>::max() ? 
            "-" : std::to_string(s.size_) ; 
        const std::string action = 
            s.action_ == 0 ? "NEW" : (
                    s.action_ ==1 ? "CANCEL" : std::to_string(s.action_) );
        return std::string("<CS") + 
            " type:" + s.client_type_.tag_ +
            " cid:" + cid +
            " time:" + next_time +
            " price:" + price +
            " size:" + size +
            " side:" + std::to_string( s.side_ ) +
            " action:" + action +
            " oid:" + oid +
            ">" ;
    }
}



