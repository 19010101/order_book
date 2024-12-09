#pragma once
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

namespace SDB {

    struct MarketState { 
        TimeType time_ ; 
        double wm_ ;
        std::array<PriceType, 4> bid_prices_;
        std::array<SizeType, 4>  bid_sizes_;
        std::array<PriceType, 4> ask_prices_;
        std::array<SizeType, 4>  ask_sizes_;
    };

    struct OrderData { 
        LocalOrderIDType local_id_;
        OrderIDType order_id_ ; 
        PriceType price_ ; 
        SizeType total_size_ ; 
        SizeType show_ ; 
        mutable SizeType remaining_size_ ; 
        Side side_ ; 
        mutable bool waiting_to_be_cancelled_;
        OrderData ( const LocalOrderIDType local_id , PriceType price, SizeType total_size, SizeType show , Side side  ) :
            local_id_(local_id), price_(price), total_size_(total_size), show_(show), remaining_size_(total_size), side_(side), 
            waiting_to_be_cancelled_(false)
        { 
            order_id_.fill( std::numeric_limits<OrderIDType::value_type>::max() ); 
        }
        /*
        OrderData( const OrderData & o) :
            local_id_(o.local_id_), 
            order_id_(o.order_id_), 
            price_(o.price_),
            total_size_(o.total_size_),
            show_(o.show_),
            remaining_size_(o.remaining_size_), 
            side_(o.side_), 
            waiting_to_be_cancelled_(o.waiting_to_be_cancelled_) {}
            */

        struct HashLocalID { 
            using is_transparent = void;
            size_t operator()( const LocalOrderIDType local_id ) const { 
                return boost::hash<LocalOrderIDType>()(local_id) ; 
            }
            size_t operator()( const OrderData & od ) const { 
                return this->operator()(od.local_id_) ; 
            }
        };
        struct EqLocalID { 
            using is_transparent = void;
            bool operator()( const LocalOrderIDType local_id , const OrderData & od) const { 
                return od.local_id_ == local_id ; 
            }
            bool operator()( const OrderData & od2 , const OrderData & od) const { 
                return od.local_id_ == od2.local_id_ ; 
            }
            bool operator()(  const OrderData & od, const LocalOrderIDType local_id ) const { 
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
                boost::multi_index::member< OrderData, const LocalOrderIDType , &OrderData::local_id_ >  
                >,
            boost::multi_index::hashed_non_unique< //non unique only at time of creation when all oids are same.
                boost::multi_index::member< OrderData, OrderIDType, &OrderData::order_id_ >  , 
            boost::hash<OrderIDType>
                >
                >
                >;
    };

    template <typename T>
        concept TransportConcept = requires( T & transport, const ClientIDType & cid, const OrderData & od, const OrderIDType & oid ) 
        {
            transport.place_order( cid, od );
            transport.cancel( cid, oid );
        };

    template < typename AgentSpecifics> 
    struct Agent {
        const ClientIDType client_id_ ;
        const MarketState & market_ ; 
        TimeType next_action_time_ ;

        OrderData::OIDSet orders_; 
        OrderData::LocalIDSet unacked_orders_; 

        Agent( const ClientIDType client_id,  const MarketState & market ) : 
            client_id_(client_id),
            market_(market), 
            next_action_time_(std::numeric_limits<TimeType>::max()) {} ;

        void handle_own_order_message( 
                const NotifyMessageType mtype, 
                const LocalOrderIDType local_oid, 
                const OrderIDType & oid , 
                const SizeType traded_size, 
                const PriceType traded_price ) {
            typename OrderData::LocalIDSet::const_iterator local_oid_iterator ;
            typename OrderData::OIDSet::const_iterator oid_iterator ;
            switch( mtype ) { 
                case NotifyMessageType::Ack : 
                    std::cerr << "received Ack for local id " << local_oid << " with oid " <<  oid << std::endl;
                    local_oid_iterator = unacked_orders_.find(local_oid);
                    if (local_oid_iterator  == unacked_orders_.end()) {
                        //we recive ack for orders that reappear due to hidden sizes. 
                        oid_iterator = orders_.find( oid );
                        if (oid_iterator == orders_.end()) //we really don't know this order:
                            throw std::runtime_error("Cannot find local oid: " + std::to_string(local_oid));
                    } else {
                        OrderData od = *local_oid_iterator;
                        od.order_id_ = oid;
                        const auto pr = orders_.emplace( std::move( od ) );
                        if (not pr.second) 
                            throw std::runtime_error("Cannot insert oid: " + std::to_string(oid));
                        else
                            oid_iterator = pr.first;
                        unacked_orders_.erase( local_oid_iterator );
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
                    else if (0 == oid_iterator->remaining_size_) {
                        //we receive End when shown size goes to zero, so, ignore when remaining is not zero
                        /*
                        throw std::runtime_error("We didn't expect this order to end: " + std::to_string(oid) 
                                + ", total size:" + std::to_string(oid_iterator->total_size_)
                                + ", remainig size:" + std::to_string(oid_iterator->remaining_size_)
                                );
                                */
                    }
                    break;
            };
            static_cast<AgentSpecifics*>(this)->handle_message(  *oid_iterator, mtype, traded_size, traded_price );
            if (mtype == NotifyMessageType::End and 0 == oid_iterator->remaining_size_)
                //we receive End when shown size goes to zero, so, ignore when remaining is not zero
                orders_.erase( oid_iterator );
        }; 
        template <TransportConcept Transport>
            void markets_state_changed(Transport & transport) { //market state has changed. determine next action time if needed
                static_cast<AgentSpecifics*>(this)->handle_market_state_changed( transport );
            }
        //method to make time go faster:
        TimeType next_action_time() const { return next_action_time_ ; }  ; 
        //actions in the market:

    };

    struct PriceMakerAroundWM : public Agent<PriceMakerAroundWM> { 
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

        LocalOrderIDType local_id_counter_;
        boost::random::mt19937 & mt_;
        const boost::random::exponential_distribution<> placement_, cancellation_;
        mutable boost::random::normal_distribution<double> order_price_;
        const boost::random::poisson_distribution<SizeType, double> order_size_;
        const boost::random::bernoulli_distribution<double> aggressive_;
        const size_t n_orders_ ;
        TimeType placement_time_ ; 

        CancellationTimes cancellation_times_ ; 

        PriceMakerAroundWM(
                const ClientIDType client_id, 
                const MarketState & market,
                boost::random::mt19937 & mt, 
                const double placement_lambda, 
                const double cancellation_lambda,
                const double order_price_mean, //how far from wm the price is
                const double order_price_std,
                const double order_size_mean,
                const double aggressive_probability,
                const size_t n_orders
                ) : Agent(client_id, market), 
                local_id_counter_(0),
                mt_(mt),
                placement_(placement_lambda), 
                cancellation_(cancellation_lambda) , 
                order_price_(order_price_mean, order_price_std ),
                order_size_(order_size_mean),
                aggressive_(aggressive_probability),
                n_orders_(n_orders)
        {
            next_action_time_ = safe_round<TimeType>(1e9*placement_(mt_));
            placement_time_ = next_action_time_; 
        }

        void check_order_of_cancellation_times() const {
            if (cancellation_times_.empty()) return;
            auto &index = cancellation_times_.get<0>();
            const TimeType first = index.begin()->t_cancel_;
            for (const auto & p : cancellation_times_)
                if (p.t_cancel_ < first)
                    throw std::runtime_error("Not ordered: " +
                        std::to_string(first) + " should be before " +
                        std::to_string(p.t_cancel_)
                        );
        }

        void update_next_action_time() {
            check_order_of_cancellation_times();
            if (auto &index = cancellation_times_.get<0>(); index.empty())
                next_action_time_ = placement_time_;
            else
                next_action_time_ = std::min(placement_time_, index.begin()->t_cancel_);
            std::cerr << market_.time_ << ", next action time: "
                    << next_action_time_ << ((next_action_time_ == placement_time_) ? " P" : " C") << '\n';
        }

        template <TransportConcept Transport>
            void handle_market_state_changed(Transport & transport) {
                //market state has changed. determine next action time if needed
                std::cerr << "time: " << market_.time_ << ", placement time:" << placement_time_ 
                    << " os:" << orders_.size() 
                    << " uos:" << unacked_orders_.size() 
                    << "\n";
                if (market_.time_ >= placement_time_ and
                    orders_.size() + unacked_orders_.size() < n_orders_ and
                    not std::isnan(market_.wm_)
                ) {
                    //const auto price = safe_round<PriceType>(order_price_(mt_) + market_.wm_);
                    std::cerr << market_.time_ << ", market_.wm_: " << market_.wm_ << '\n';
                    const double continuous_price = order_price_(mt_) + market_.wm_;
                    std::cerr << market_.time_ << ", continuous price: " << continuous_price << '\n';
                    const auto price = safe_round<PriceType>(continuous_price);
                    std::cerr << market_.time_ << ", price: " << price << '\n';
                    const bool aggressive = aggressive_(mt_);
                    const Side passive_side = (continuous_price >= market_.wm_) ? Side::Offer : Side::Bid;
                    const Side side = aggressive ? get_other_side(passive_side) : passive_side;
                    const LocalOrderIDType local_order_id = local_id_counter_++;
                    std::cerr << "will place order with local id " << local_order_id << std::endl;
                    auto [fst, snd] = unacked_orders_.emplace(
                        local_order_id, price, 1 + order_size_(mt_), 2, side);
                    if (not snd) throw std::runtime_error(
                        "Problem placing un-acked order in map: " + std::to_string(local_order_id));
                    transport.place_order(client_id_, *fst);
                }
                while (market_.time_ >= placement_time_)
                    placement_time_ += safe_round<TimeType>(1e9*placement_(mt_));

                auto & index = cancellation_times_.get<0>() ; 
                if (not index.empty())
                    std::cerr << "time: " << market_.time_ << ", cancellation time:" << index.begin()->t_cancel_ << "\n";
                while( not index.empty() and market_.time_ >= index.begin()->t_cancel_ ) { 
                    std::cerr << "cancelling oid: " << index.begin()->order_id_ << ", cancellation time:" << index.begin()->t_cancel_ << "\n";
                    orders_.find( index.begin()->order_id_ )->waiting_to_be_cancelled_ = true;
                    const auto order_id = index.begin()->order_id_;
                    index.erase(index.begin());
                    transport.cancel( client_id_, order_id);
                }

            }

        void handle_message( const OrderData & order_data, const NotifyMessageType mtype, 
                const SizeType , const PriceType  ) {
            switch (mtype) {
                case NotifyMessageType::Ack : 
                    { 
                        //setup cancellation time
                        TimeType cancellation_time =  market_.time_ + safe_round<TimeType>(1e9*cancellation_( mt_ ) );
                        cancellation_times_.emplace( cancellation_time, order_data.order_id_ );
                        std::cerr << "inserted cancellation time: " << cancellation_time << ' ' << cancellation_times_.size() << std::endl;
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


}
