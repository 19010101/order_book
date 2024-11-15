#pragma once
#include "ob.h"

#include <boost/random/exponential_distribution.hpp> 
#include <boost/random/poisson_distribution.hpp> 
#include <boost/random/normal_distribution.hpp> 
#include <boost/random/bernoulli_distribution.hpp> 
#include <boost/random/mersenne_twister.hpp> 

#include <boost/heap/fibonacci_heap.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace SDB { 
    //this code is not tested at all!
    struct ClientType { 

        const std::string tag_ ; 
        boost::random::mt19937 & mt_;

        const double placement_lambda_, cancellation_lambda_, order_size_mean_, order_price_std_, side_mean_;

        const boost::random::exponential_distribution<> placement_, cancellation_;
        mutable boost::random::normal_distribution<double> order_price_;
        const boost::random::poisson_distribution<SizeType, double> order_size_;
        const boost::random::bernoulli_distribution<double> side_;

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
            return TimeType(lround(1e9*placement_(mt_)));
        }
        TimeType get_cancellation_dt( ) const {
            return TimeType(lround(1e9*cancellation_(mt_)));
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
                const Order & order, 
                const TimeType notif_time ,
                const SizeType traded_size, 
                const SizeType traded_price
                ) { 
            static char msg[1024];
            if (msg_type == NotifyMessageType::Ack) 
                std::snprintf(msg, 1024, "%fs:N:%lu:%d:%d:%d:%d", notif_time*1e-9, order.order_id_, order.price_, order.shown_size_ , traded_size, traded_price);
            else if (msg_type == NotifyMessageType::End) 
                std::snprintf(msg, 1024, "%fs:D:%lu:%d:%d:%d:%d", notif_time*1e-9, order.order_id_, order.price_, order.shown_size_ , traded_size, traded_price);
            else if (msg_type == NotifyMessageType::Trade) 
                std::snprintf(msg, 1024, "%fs:T:%lu:%d:%d:%d:%d", notif_time*1e-9, order.order_id_, order.price_, order.shown_size_ , traded_size, traded_price);
            else
                throw std::runtime_error("What is this type? " + std::to_string(int(msg_type)) );
            return msg;
        }
        CerrLogger( const MemoryManager<Order> & ) {} 
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
            price_ = std::lround( real_price );
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
                    return std::hash<OrderIDType>()(oid);
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
        struct NotificationHandler : public Logger { 
            MatchingEngine & eng_;

            Ptr::ByCID by_cid_;
            mutable Ptr::ByOID by_oid_;
            mutable Ptr::ByTime by_time_ ; 

            NotificationHandler( MatchingEngine & eng ) : Logger(eng.mem_), eng_(eng)  { } ;

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

            void log( const MatchingEngine & eng ) { Logger::log(eng); }

            void log( const NotifyMessageType msg_type , const Order & order, const TimeType notif_time, 
                    const SizeType traded_size , const PriceType traded_price 
            ) { 
                Logger::log( msg_type, order, notif_time, traded_size, traded_price );
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

    struct ReplayData { 
        struct MSG { 
            const TimeType time_ ; 
            const ClientIDType cid_ ;
            const OrderIDType oid_;
            const PriceType order_price_, trade_price_ ;
            const SizeType shown_size_, trade_size_ ; 
            const NotifyMessageType mtype_ ;
            const Side side_;
            const bool is_hidden_ ; 

            std::string str() const {
                std::ostringstream out ;
                out << "<M: time:" << time_*1e-9 
                    << ' ' << std::to_string(mtype_)
                    << " oid:" << oid_ 
                    << " po:" << order_price_ 
                    << " pt:" << trade_price_ 
                    << " ss:" << shown_size_ 
                    << " ts:" << trade_size_ 
                    << " side:" << std::to_string( side_ ) 
                    << " h:" << std::to_string( is_hidden_ ) ;
                return out.str();
            }

            bool check_equivalent( const MSG & other ) const { 
                return 
                    time_ == other.time_ and
                    oid_ == other.oid_ and
                    order_price_ == other.order_price_ and
                    trade_price_ == other.trade_price_ and
                    shown_size_ == other.shown_size_ and
                    trade_size_ == other.trade_size_ and
                    mtype_ == other.mtype_ and 
                    side_ == other.side_ and
                    is_hidden_ == other.is_hidden_ ;

            }
            struct TimeLess { 
                bool operator()( const MSG & a, const MSG & b ) const {
                    return a.time_ < b.time_ ;
                }
            };
        };
        std::vector<MSG> msgs_;
        std::vector< 
            std::tuple<
                TimeType, 
                std::vector< std::tuple< PriceType, Side, MemoryManager<Order>::list_type > >
                >
            > snapshots_ ; 
        MemoryManager<Order> & mem_;

        ReplayData( MemoryManager<Order> & mem ) : mem_(mem) {} ;
        ReplayData( const ReplayData & ) = delete ; 
        ReplayData & operator=( const ReplayData & ) = delete;
        ~ReplayData() {
            free_orders();
        }
        void log( const NotifyMessageType mtype , const Order & o, const TimeType t, const SizeType trade_size = 0, const PriceType trade_price = 0) { 
            //std::cerr << "log notif " << msgs_.size() << ' ' << t*1e-9<< ' ' << std::to_string(mtype) << ' ' << std::to_string(o) << '\n'; 
            msgs_.emplace_back( MSG( t, o.client_id_, o.order_id_, o.price_, trade_price, o.shown_size_, trade_size, mtype, o.side_, o.is_hidden_ ) );
        };
        void log( const MatchingEngine & eng ) {
            //std::cerr << "log " << eng << std::endl;
            snapshots_.emplace_back( eng.snapshot() );
        }
        void free_orders( ) { 
            struct Disposer {
                MemoryManager<Order> & mem_;
                void operator()( Order * t ) const {//Disposer
                    mem_.free( *t );
                }
            } ;
            Disposer disposer(mem_);
            for ( auto & v : snapshots_ ) 
                for ( auto & tpl : std::get<1>(v) ) 
                    while ( not std::get<2>(tpl).empty() ) 
                        std::get<2>(tpl).pop_front_and_dispose( disposer );
        }
    };

    struct replay_error : public std::runtime_error {
        explicit replay_error( const std::string & what) : std::runtime_error(what) {}
    };

    template <INotifier N> 
        void replay( const std::vector<ReplayData::MSG> & msgs, MatchingEngine & eng, N & notifier ) { 
            auto it = msgs.begin(); 
            while ( it != msgs.end() ) { 
                eng.set_time( it->time_ );
                switch ( it->mtype_ ) {
                    case NotifyMessageType::Ack : 
                        {
                            const auto jt = std::upper_bound( it, msgs.end(), *it , ReplayData::MSG::TimeLess()) ;
                            if (jt != msgs.end() )
                                if ( jt->time_ <= it->time_ )
                                    throw replay_error( std::string("time order has failed: ") + std::to_string(jt->time_) + " vs " + std::to_string( it->time_ ) );
                            for (auto kt = it + 1; kt  < jt ; ++kt ) {
                                if( kt->time_ != it->time_ ) 
                                    throw replay_error( std::string("time should be same: ") + std::to_string(kt->time_) + " vs " + std::to_string( it->time_ ) );
                                if (kt->mtype_== NotifyMessageType::Ack and kt->oid_ != it->oid_ )
                                    eng.add_replay_order(kt->oid_, kt->cid_, kt->order_price_, kt->shown_size_, kt->side_, false, notifier);
                            }
                            eng.add_replay_order(it->oid_, it->cid_, it->order_price_, it->shown_size_, it->side_, false, notifier);
                            for (auto kt = it + 1; kt  < jt ; ++kt ) {
                                if (kt->mtype_== NotifyMessageType::Ack and kt->oid_ == it->oid_ )
                                    eng.add_replay_order(kt->oid_, kt->cid_, kt->order_price_, kt->shown_size_, kt->side_, false, notifier);
                            }
                            notifier.log(eng);
                            it = jt;
                        }
                        break;
                    case NotifyMessageType::Cancel : 
                        eng.cancel_order(it->oid_, notifier);
                        notifier.log(eng);
                        ++it; 
                        break;
                    case NotifyMessageType::End : 
                    case NotifyMessageType::Trade : 
                        ++it;
                        break;
                } 
            }

        }


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



