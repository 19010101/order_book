#pragma once
#include "ob.h"
#include "random_walk.h"

#include <boost/random/exponential_distribution.hpp> 
#include <boost/random/poisson_distribution.hpp> 
#include <boost/random/normal_distribution.hpp> 
#include <boost/random/bernoulli_distribution.hpp> 
#include <boost/random/mersenne_twister.hpp> 


#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>

#include <ATen/ATen.h>
#include <sstream>
#include <torch/serialize.h>
#include <torch/torch.h>

namespace SDB {

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
                const NotifyMessageType message_type,
                const LocalOrderIDType local_oid, 
                const OrderIDType & oid , 
                const SizeType traded_size, 
                const PriceType traded_price ) {
            SPDLOG_TRACE( "client: {:2d} {} local id: {} oid: 0x{:xspn}"  , client_id_ ,
                message_type , local_oid , spdlog::to_hex(oid) );
            OrderData::LocalIDSet::const_iterator local_oid_iterator ;
            OrderData::OIDSet::const_iterator oid_iterator ;
            switch( message_type ) {
                case NotifyMessageType::Ack : 
                    //std::cerr << "received Ack for local id " << local_oid << " with oid " <<  oid << std::endl;
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
                    else {
                        oid_iterator->remaining_size_ -= std::abs(traded_size); 
                    }
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
            static_cast<AgentSpecifics*>(this)->handle_message(  *oid_iterator, message_type, traded_size, traded_price );
            if (message_type == NotifyMessageType::End and 0 == oid_iterator->remaining_size_)
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
        boost::random::exponential_distribution<> placement_, cancellation_;
        boost::random::exponential_distribution<> order_price_;
        boost::random::poisson_distribution<SizeType, double> order_size_;
        private:
        boost::random::bernoulli_distribution<double> side_, aggressive_;
        public:
        const size_t n_orders_ ;
        TimeType placement_time_ ; 

        CancellationTimes cancellation_times_ ; 

        double side_param() const { return side_.p() ;}
        double aggressive_param() const { return aggressive_.p() ;}
        void side_param(const double p) {
            side_.param( boost::random::bernoulli_distribution<double>::param_type (p) ) ;
            SPDLOG_INFO("side_param(): p={}, p={}, cid:{}", p, side_param(), client_id_) ;
        }
        void aggressive_param(const double p) {
            aggressive_.param( boost::random::bernoulli_distribution<double>::param_type (p) ) ;
        }

        PriceMakerAroundWM(
                const ClientIDType client_id, 
                const MarketState & market,
                boost::random::mt19937 & mt, 
                const double placement_lambda, 
                const double cancellation_lambda,
                const double order_price_mean, //how far from wm the price is
                //const double order_price_std,
                const double order_size_mean,
                const double aggressive_probability,
                const size_t n_orders
                ) : Agent(client_id, market), 
                local_id_counter_(0),
                mt_(mt),
                placement_(placement_lambda), 
                cancellation_(cancellation_lambda) , 
                order_price_(1./order_price_mean ),
                order_size_(std::min(static_cast<SizeType>(order_size_mean), std::numeric_limits< SizeType >::max() )),
                side_(0.5),
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
            //check_order_of_cancellation_times();
            if (auto &index = cancellation_times_.get<0>(); index.empty())
                next_action_time_ = placement_time_;
            else
                next_action_time_ = std::min(placement_time_, index.begin()->t_cancel_);
            //std::cerr << market_.time_ << ", next action time: " << next_action_time_ << ((next_action_time_ == placement_time_) ? " P" : " C") << '\n';
        }

        template <TransportConcept Transport>
            void handle_market_state_changed(Transport & transport) {
                //market state has changed. determine next action time if needed
                /*std::cerr << "time: " << market_.time_ << ", placement time:" << placement_time_
                    << " os:" << orders_.size() 
                    << " uos:" << unacked_orders_.size() 
                    << "\n"; */
                if (market_.time_ >= placement_time_ and
                    orders_.size() + unacked_orders_.size() < n_orders_ 
                ) {
                    const double & wm = std::isnan(market_.wm_) ? 0 : market_.wm_; 
                    const auto dp = order_price_(mt_);
                    const Side passive_side = side_(mt_) ? Side::Offer : Side::Bid;
                    const bool aggressive = aggressive_(mt_);
                    const Side side = aggressive ? get_other_side(passive_side) : passive_side;
                    const double continuous_price =  passive_side == Side::Offer ? wm + dp : wm - dp;
                    //new we round up if offer, round down if buying
                    const auto price = safe_round<PriceType>(
                            side==Side::Offer ? std::ceil(continuous_price+EPS) : std::floor(continuous_price-EPS)
                    );
                    const LocalOrderIDType local_order_id = local_id_counter_++;
                    //std::cerr << "will place order with local id " << local_order_id << std::endl;
                    auto order_size = order_size_(mt_);
                    if (order_size < 0 ) {
                        //SPDLOG_ERROR("negative order size from poisson distribution, client id {}", client_id_);
                        order_size = std::numeric_limits<SizeType>::max();
                    }
                    if (order_size > std::numeric_limits<SizeType>::max() ) 
                        throw std::logic_error("orer size from poisson distribution is too big");
                    if (order_size == 0) order_size = 1;
                    if (false)  {
                        SPDLOG_INFO( "t: {}, cid:{}, wm : {}, p:{}({}) , p-wm: {}, bb-p: {}, p-ba: {}, s: {}, side: {}, aggressive:{}", 
                                market_.time_*1e-9,
                                client_id_,
                                market_.wm_ ,  
                                continuous_price,  price,
                                price - market_.wm_,
                                market_.bid_prices_[0] - price, 
                                price - market_.ask_prices_[0] , 
                                order_size , side, aggressive);
                        SPDLOG_INFO( "bid ages: {}", fmt::join( market_.bid_ages_ , " " ) );
                        SPDLOG_INFO( "ask ages: {}", fmt::join( market_.ask_ages_ , " " ) );
                    }
                    auto [fst, snd] = unacked_orders_.emplace(
                        local_order_id, price, order_size , 2, side);
                    if (not snd) throw std::runtime_error(
                        "Problem placing un-acked order in map: " + std::to_string(local_order_id));
                    transport.place_order(client_id_, *fst);
                }
                while (market_.time_ >= placement_time_)
                    placement_time_ += safe_round<TimeType>(1e9*placement_(mt_));

                auto & index = cancellation_times_.get<0>() ; 
                //if (not index.empty()) std::cerr << "time: " << market_.time_ << ", cancellation time:" << index.begin()->t_cancel_ << "\n";
                while( not index.empty() and market_.time_ >= index.begin()->t_cancel_ ) {
                    //SPDLOG_TRACE("cancelling oid: 0x{:xspn} at time: ", spdlog::to_hex(index.begin()->order_id_), index.begin()->t_cancel_);
                    orders_.find( index.begin()->order_id_ )->waiting_to_be_cancelled_ = true;
                    const auto order_id = index.begin()->order_id_;
                    index.erase(index.begin());
                    transport.cancel( client_id_, order_id);
                }

            }

        void handle_message( const OrderData & order_data, const NotifyMessageType mtype, 
                const SizeType , const PriceType   ) {
            switch (mtype) {
                case NotifyMessageType::Ack : 
                    { 
                        //setup cancellation time
                        TimeType cancellation_time =  market_.time_ + safe_round<TimeType>(1e9*cancellation_( mt_ ) );
                        cancellation_times_.emplace( cancellation_time, order_data.order_id_ );
                        //std::cerr << "inserted cancellation time: " << cancellation_time << ' ' << cancellation_times_.size() << std::endl;
                        break;
                    } 
                case NotifyMessageType::Cancel : 
                case NotifyMessageType::End : 
                    {
                        cancellation_times_.get<1>().erase( order_data.order_id_ );
                        break;
                    }
                case NotifyMessageType::Trade : 
                    //SPDLOG_INFO("trade size {} price {}", trade_size, trade_price );
                    break;
            }
        }
    };

    struct EMA {
        const double T_;
        double x_prev_, t_prev_, ema_;
        explicit EMA(const double T) : T_(T),
                              x_prev_(std::numeric_limits<double>::quiet_NaN()),
                              t_prev_(std::numeric_limits<double>::quiet_NaN()),
                              ema_(std::numeric_limits<double>::quiet_NaN()) {
        }
        void update( const double & t, const double & x ) {
            if (std::isnan(ema_))
                ema_ = x;
            else {
                const double w = std::exp(-(t - t_prev_) / T_);
                ema_ = w * ema_ + (1 - w) * x_prev_;
            }
            x_prev_ = x;
            t_prev_ = t;
        }
    };

    struct TrendFollowerAgent : public Agent<TrendFollowerAgent> {
        LocalOrderIDType local_id_counter_;
        EMA ema_ ;
        const double spread_ ;
        bool disabled_ ;
        size_t bid_count_ , ask_count_ ;

        TrendFollowerAgent(
            const ClientIDType client_id,
            const MarketState &market,
            const double T, const double spread) :
                Agent<TrendFollowerAgent>(client_id, market),
                local_id_counter_(0),
                ema_(T), spread_(spread),
                disabled_(false),
                 bid_count_(0), ask_count_(0) { }
        template <TransportConcept Transport>
            void handle_market_state_changed(Transport & transport) {
                if (std::isnan(market_.wm_)) return;
                ema_.update( 1e-9*market_.time_, market_.wm_ );
                PriceType price ;
                Side side;
                if ( market_.wm_ > ema_.ema_ + spread_) {
                    //trending up. buy at the best offer
                    price = market_.ask_prices_[0];
                    side = Side::Bid;
                } else if ( market_.wm_ < ema_.ema_ - spread_) {
                    //trending down. sell at the best bid.
                    price = market_.bid_prices_[0];
                    side = Side::Offer;
                } else
                    return;
                //collect stats and return
                if (side==Side::Bid) ++bid_count_ ;
                else ++ask_count_ ;
                if (disabled_) return;
                //do we have an order in this side and price? If so, don't do anything.
                for (auto & o : unacked_orders_)
                    if (o.price_==price and o.side_==side) return;
                bool found = false;
                for (auto & o : orders_) {
                    if (o.price_==price and o.side_==side) found = true;
                    else {
                        o.waiting_to_be_cancelled_ = true;
                        transport.cancel(client_id_, o.order_id_ );
                    }
                }
                if (found) return;
                const LocalOrderIDType local_order_id = local_id_counter_++;
                auto [fst, snd] = unacked_orders_.emplace(
                    local_order_id, price, 10, 1, side);
                if (not snd)
                    throw std::runtime_error(
                        "Problem placing un-acked order in map: " + std::to_string(local_order_id));
                transport.place_order(client_id_, *fst);
        }
        static void handle_message( const OrderData & , const NotifyMessageType ,
                const SizeType , const PriceType  ) {
        }
    };

    struct SingleInstrumentMarketMaker : public Agent<SingleInstrumentMarketMaker > {
        LocalOrderIDType local_id_counter_;
        SizeType position_ ;

        SingleInstrumentMarketMaker(
            const ClientIDType client_id,
            const MarketState &market ) :
                Agent<SingleInstrumentMarketMaker>(client_id, market),
                local_id_counter_(0), position_(0) {}
        template <TransportConcept Transport>
            void handle_market_state_changed(Transport & transport) {
                if (std::isnan(market_.wm_)) return;
                if (position_ == 0) {
                    //make sure we are in the market at best bid and best offer.
                } else {
                    const Side side_to_place = (position_<0) ? Side::Bid : Side::Offer;
                    const PriceType price = ( side_to_place == Side::Bid ) ? 
                            ( ( market_.wm_ - market_.bid_prices_[0] < 0.8 ) ? market_.bid_prices_[0] : market_.bid_prices_[0] + 1 ) : 
                            ( ( market_.ask_prices_[0] - market_.wm_ < 0.8 ) ? market_.ask_prices_[0] : market_.ask_prices_[0] - 1 ) ;
                    SizeType wanted = std::abs( position_ ); 
                    SPDLOG_INFO("wanted {} {}@{}", side_to_place, wanted, price ); 
                    for (auto & od : orders_) 
                        if ( od.side_ == side_to_place and od.price_ == price ) { 
                            if (wanted >= od.remaining_size_)
                                wanted -= od.remaining_size_ ; 
                            else {
                                SPDLOG_ERROR("implement amends!");
                                SPDLOG_INFO("cancelling 0x{:xspn} od cid {} with remaining size {}",
                                        spdlog::to_hex(od.order_id_), client_id_, od.remaining_size_ ); 
                                if (not od.waiting_to_be_cancelled_) {
                                    od.waiting_to_be_cancelled_ = true;
                                    transport.cancel( client_id_ , od.order_id_ );
                                }
                                wanted = 0; 
                            }
                        } else if (not od.waiting_to_be_cancelled_) {
                            SPDLOG_INFO("cancelling 0x{:xspn} od cid {} with remaining size {}",
                                    spdlog::to_hex(od.order_id_), client_id_, od.remaining_size_ ); 
                            od.waiting_to_be_cancelled_ = true;
                            transport.cancel( client_id_ , od.order_id_ );
                        }
                    for (auto & od : unacked_orders_ )
                        if ( od.side_ == side_to_place and od.price_ == price ) { 
                            if (wanted >= od.remaining_size_)
                                wanted -= od.remaining_size_ ; 
                            else {
                                SPDLOG_ERROR("implement amends!");
                                wanted = 0; 
                            }
                        }
                    if (wanted>0) {
                        SPDLOG_INFO("cid {} placing order {} {}@{}", client_id_, side_to_place, wanted , price ); 
                        auto [fst, snd] = unacked_orders_.emplace(
                                ++local_id_counter_, price, wanted , 2, side_to_place);
                        if (not snd) throw std::runtime_error(
                                "Problem placing un-acked order in map: " + std::to_string(local_id_counter_));
                        transport.place_order(client_id_, *fst);
                    }
                }
        }
        void handle_message( const OrderData & , const NotifyMessageType , const SizeType size, const PriceType) {
            position_ += size;
        }
    };


    template <INotifier Notifier>
        struct PassThroughTransport {
            MatchingEngine & eng_;
            Notifier & notifier_;
            const boost::random::exponential_distribution<> delay_distribution_;
            const bool delay_disabled_;
            TimeType delay_ ;
            std::unordered_map<ClientIDType, PriceMakerAroundWM *> price_makers ;
            std::unordered_map<ClientIDType, TrendFollowerAgent *> trend_followers ;
            std::unordered_map<ClientIDType, SingleInstrumentMarketMaker *> single_instrument_market_makers_ ;
            std::vector<std::tuple<TimeType,ClientIDType, OrderData>> orders_to_place;
            std::vector<std::tuple<TimeType,OrderIDType>> orders_to_cancel;
            std::unordered_map<ClientIDType, std::unordered_map<PriceType, int> > price_counts;
            PassThroughTransport( MatchingEngine & eng, Notifier & notifier, const double delay_lambda ) :
                eng_(eng), notifier_(notifier) , delay_distribution_(std::max(delay_lambda,EPS)),
                delay_disabled_(delay_lambda <= EPS),
                delay_(0){}

            bool add_agent( PriceMakerAroundWM & agent ) {
                if (trend_followers.contains( agent.client_id_ )) return false;
                if (single_instrument_market_makers_.contains( agent.client_id_ )) return false;
                return price_makers.emplace( agent.client_id_, &agent ).second;
            }
            bool add_agent( TrendFollowerAgent & agent ) {
                if (price_makers.contains( agent.client_id_ )) return false;
                if (single_instrument_market_makers_.contains( agent.client_id_ )) return false;
                return trend_followers.emplace( agent.client_id_, &agent ).second;
            }
            bool add_agent( SingleInstrumentMarketMaker & agent ) {
                if (price_makers.contains( agent.client_id_ )) return false;
                if (trend_followers.contains( agent.client_id_ )) return false;
                return single_instrument_market_makers_.emplace( agent.client_id_, &agent ).second;
            }

            void place_order( const ClientIDType cid, const OrderData & od ) {
                price_counts.emplace( cid, std::unordered_map<PriceType, int>() )
                        .first->second.emplace(od.price_, 0)
                        .first->second += 1;
                orders_to_place.emplace_back( eng_.time_, cid, od );
            }
            void cancel( const ClientIDType cid , const OrderIDType & oid ){
                SPDLOG_TRACE("Canceling order 0x{:xspn} of client {}", spdlog::to_hex(oid), cid );
                orders_to_cancel.emplace_back( eng_.time_, oid );
            }
            void update_next_send_time( boost::random::mt19937 & mt ) {
                delay_ = delay_disabled_ ? 0 : safe_round<TimeType>(1e9*delay_distribution_(mt));
            }
            TimeType next_send_time() const {
                const TimeType next_place_time = not orders_to_place.empty() ?
                    std::get<0>(*orders_to_place.begin()) + delay_ : std::numeric_limits<TimeType>::max();
                const TimeType next_cancel_time = not orders_to_cancel.empty() ?
                    std::get<0>(*orders_to_cancel.begin()) + delay_ : std::numeric_limits<TimeType>::max();
                return std::min(next_place_time, next_cancel_time);
            }
            void send(const TimeType now) {
                auto place_it = orders_to_place.begin();
                for ( ; place_it != orders_to_place.end() and std::get<0>(*place_it) + delay_ <= now ; ++place_it) {
                    const auto & [t,cid, od] = *place_it;
                    eng_.add_simulation_order( cid, od.local_id_, od.price_,
                            od.total_size_, od.show_, od.side_,
                            false, *this);
                }
                auto cancel_it = orders_to_cancel.begin();
                for ( ; cancel_it != orders_to_cancel.end() and std::get<0>(*cancel_it) + delay_ <= now ; ++cancel_it) {
                    const auto & [t, oid] = *cancel_it;
                    eng_.cancel_order(oid, *this);
                }
                if (not orders_to_place.empty())
                    orders_to_place.erase(orders_to_place.begin(), place_it);
                if (not orders_to_cancel.empty())
                    orders_to_cancel.erase(orders_to_cancel.begin(), cancel_it);
            }

            template<typename AgentSpecifics>
            static bool find_and_handle_order_message(
                    const std::unordered_map<ClientIDType, AgentSpecifics *> & agents,
                    const ClientIDType cid,
                    const NotifyMessageType mtype,
                    const LocalOrderIDType & lid,
                    const OrderIDType & oid,
                    const SizeType trade_size,
                    const PriceType trade_price
                ) {
                if (const auto it = agents.find( cid ); it == agents.end()) return false;
                else it->second->handle_own_order_message(mtype, lid, oid, trade_size, trade_price );
                return true;
            }

            void log(const NotifyMessageType mtype, const Order &o, const TimeType t, const SizeType trade_size = 0,
                     const PriceType trade_price = 0) {
                notifier_.log(mtype,  o, t,  trade_size, trade_price );
                const bool done =
                        find_and_handle_order_message(
                            price_makers, o.client_id_, mtype,
                            o.local_id_, o.order_id_, trade_size, trade_price
                        ) ||
                        find_and_handle_order_message(
                            trend_followers, o.client_id_, mtype,
                            o.local_id_, o.order_id_, trade_size, trade_price
                        ) ||
                        find_and_handle_order_message(
                            single_instrument_market_makers_, o.client_id_, mtype,
                            o.local_id_, o.order_id_, trade_size, trade_price
                        ) ;
                if (not done)
                    throw std::runtime_error(std::format("Cannot find client id: {}", o.client_id_) );
            }
            void log( const MatchingEngine & eng ) {
                notifier_.log( eng );
            }

            void error(const OrderIDType &oid, const std::string &msg) {
                notifier_.error(oid, msg);
            }
    };

    inline TimeType get_min_time(
        const std::vector<PriceMakerAroundWM> & price_makers,
        const std::vector<TrendFollowerAgent> & trend_followers
        ) {
        TimeType min_time = std::numeric_limits<TimeType>::max();
        for (auto & a : price_makers) min_time = std::min(min_time, a.next_action_time());
        for (auto & a : trend_followers) min_time = std::min(min_time, a.next_action_time());
        return min_time;
    }

    template <INotifier Notifier>
    auto simulate(
        boost::random::mt19937 & mt,
        MarketState & market,
        std::vector<PriceMakerAroundWM> & price_makers,
        std::vector<TrendFollowerAgent> & trend_followers,
        MatchingEngine & eng,
        Notifier & notifier,
        const double delay_lambda,
        const TimeType t_max,
        std::ostream * outptr
        ) {
        PassThroughTransport<Notifier> transport(eng, notifier, delay_lambda);
        for (auto & a : price_makers)
            if (not transport.add_agent(a))
                throw std::runtime_error(std::format("Cannot add agent: {}", a.client_id_) );
        for (auto & a : trend_followers)
            if (not transport.add_agent(a))
                throw std::runtime_error(std::format("Cannot add agent: {}", a.client_id_) );

        while ( market.time_ <= t_max) {
            transport.log( eng );
            for (auto &pm: price_makers)
                pm.update_next_action_time();
            const TimeType t_algo = get_min_time( price_makers, trend_followers ) ;
            transport.update_next_send_time(mt); //refresh delay_
            const TimeType t_transport = transport.next_send_time(); //earliest order placement time + delay
            const TimeType t = std::min(t_algo, t_transport);
            if (t==market.time_)
                throw std::runtime_error(std::format("Market time is stuck: {}", t) );
            market.time_ = t;
            eng.time_ = market.time_;
            for (auto & a : price_makers) a.markets_state_changed(transport);
            for (auto & a : trend_followers) a.markets_state_changed(transport);
            transport.send(market.time_);
            if (transport.next_send_time() <= market.time_)
                throw std::runtime_error(std::format("Transport next send time should have moved : {} - {}",
                    transport.next_send_time(), market.time_) );

            eng.level2(
                market.bid_prices_, market.bid_sizes_,
                market.ask_prices_, market.ask_sizes_
            );
            if (market.bid_sizes_[0] != 0 and market.ask_sizes_[0] != 0)
                market.wm_ = static_cast<double>(market.bid_prices_[0] * market.ask_sizes_[0] +
                                                 market.ask_prices_[0] * market.bid_sizes_[0]) /
                             static_cast<double>(market.bid_sizes_[0] + market.ask_sizes_[0]);

            /*
            char msg[1024];
            std::snprintf(msg, 1024,
                          "%15.9f %4db@%-3d %4db@%-3d %4db@%-3d wm:%4.2f %4da@%-3d %4da@%-3d %4da@%-3d",
                          static_cast<double>(market.time_) * 1e-9,
                          market.bid_sizes_[2], market.bid_prices_[2],
                          market.bid_sizes_[1], market.bid_prices_[1],
                          market.bid_sizes_[0], market.bid_prices_[0],
                          market.wm_,
                          market.ask_sizes_[0], market.ask_prices_[0],
                          market.ask_sizes_[1], market.ask_prices_[1],
                          market.ask_sizes_[2], market.ask_prices_[2]
            );
            SPDLOG_INFO( "market: {} {}" , market.time_ , market.bid_sizes_[0] != 0 and market.ask_sizes_[0] != 0 ? market.wm_ : std::numeric_limits<double>::quiet_NaN() );
            */
            /*
            std::fprintf(out,
                          "%15.9f %4db@%-3d %4db@%-3d %4db@%-3d wm:%4.2f %4da@%-3d %4da@%-3d %4da@%-3d",
                          static_cast<double>(market.time_) * 1e-9,
                          market.bid_sizes_[2], market.bid_prices_[2],
                          market.bid_sizes_[1], market.bid_prices_[1],
                          market.bid_sizes_[0], market.bid_prices_[0],
                          market.wm_,
                          market.ask_sizes_[0], market.ask_prices_[0],
                          market.ask_sizes_[1], market.ask_prices_[1],
                          market.ask_sizes_[2], market.ask_prices_[2]
            );
            */
            if (outptr != nullptr)
                *outptr << market << '\n';
        }
        return transport.price_counts;
    }
    inline double find_center_shift_for_range_bound(const double s0, const double s1, const double sm ) { 
        const double x = 2*(s1-s0)/(sm-s0) - 1; 
        if (x < -1  or x > 1)
            throw std::runtime_error( fmt::format("{} should be between -1 and 1", x) );
        return std::atanh( x );
    }
    inline double range_bound( const double x, const double min_value, const double max_value ,
            const double center_shift) { 
        const double zero_to_one = (1. + std::tanh(x+center_shift) ) / 2.;
        return min_value + zero_to_one * (max_value - min_value);
    }
    inline double sln( const double x, const double fc, const double fm,  const double f0 ) { 
        return 1/( 1/f0 + ( std::exp( x*std::log((1/fm - 1/f0)/(1/fc - 1/f0)) + std::log(1/fc - 1/f0) ) ) ) ; //return frequency always smaller than f0.
    }

    struct PriceMakerWithRandomParams  {
        boost::random::mt19937 & mt_;
        //const double cancellation_time_lower_limit_ , cancellation_time_upper_limit_ ; 
        const double 
            freq_lower_limit_,  freq_center_, freq_width_ ,
            period_min_,  period_center_, period_max_, period_center_shift_ ,
            order_size_min_, order_size_center_, order_size_max_ , order_size_center_shift_; 
        RandomWalkWithState<MeanReversion> cancellation_, order_size_ ;
        RandomWalkWithState<BifurcatingMeanReversion> price_mean_ ;

        PriceMakerAroundWM pm_; 
        
        PriceMakerWithRandomParams( const ClientIDType cid, const MarketState & market, boost::random::mt19937 & mt) :
            mt_(mt),
            freq_lower_limit_(.1) ,  // 0.1/seconds 
            freq_center_(.1/60.) ,  // 0.1/minute
            freq_width_( 1./( 60*60. ) ), // 1/hour
            period_min_( 1 ), 
            period_center_( 60 ), 
            period_max_( 6*60*60 ), 
            period_center_shift_(
                    find_center_shift_for_range_bound(period_min_, period_center_, period_max_)),
            order_size_min_( 1 ), 
            order_size_center_( 10 ), 
            order_size_max_( 50 ), 
            order_size_center_shift_(
                    find_center_shift_for_range_bound(order_size_min_, order_size_center_, order_size_max_)),
            cancellation_( 0.0 ,  0.0 , .00010 , .01 ) , 
            order_size_  ( 0.0 ,  0.0 , .00010 , .01 ) , 
            price_mean_  ( 1.0 ,  1.0 , .00100 , .1  ) ,
            pm_(cid, market , mt_,
                    1.,  
                    //sln(cancellation_.x_, freq_center_, freq_width_, freq_lower_limit_), 
                    1/range_bound(cancellation_.x_, period_min_, period_max_, 
                        period_center_shift_),
                    price_mean_.x_,
                    range_bound(order_size_.x_, order_size_min_, order_size_max_, 
                        order_size_center_shift_),
                    0.05, 10 ) {}
        void update( 
                const double cancellation_period, 
                const double price_mean,  
                const double order_size_mean ) { 
            pm_.cancellation_.param( 1/cancellation_period  ) ;
            pm_.order_price_.param( 1./price_mean);
            pm_.order_size_.param(
                    boost::random::poisson_distribution<SizeType, double>::param_type( 
                        order_size_mean    ) 
                    );
        }

        void update( const double dt )  {
            update( 
                    range_bound(cancellation_.update( dt, mt_ ),
                        period_min_, period_max_, 
                        period_center_shift_)    , 
                    price_mean_.update( dt, mt_ ), 
                    range_bound(order_size_.update(dt, mt_), 
                        order_size_min_, order_size_max_, 
                        order_size_center_shift_)   
                  );
        }
        double get_cancellation_period() const { return 1./pm_.cancellation_.lambda(); }
        double get_order_price_mean() const { return 1./pm_.order_price_.lambda() ; }
        double order_size_mean() const { return pm_.order_size_.mean() ; }
    };


    struct RandomPriceMakerEnsemble { 
        std::vector<PriceMakerWithRandomParams> price_makers_;
        std::vector<SingleInstrumentMarketMaker> single_instrument_market_makers_ ; 
        MarketState market_{0, std::numeric_limits<double>::quiet_NaN(), {0}, {0}, {0}, {0}, {0}, {0}};
        RandomPriceMakerEnsemble(const size_t n_agents, boost::random::mt19937 & mt) { 
            for (size_t i = 0; i < n_agents; ++i ) 
                price_makers_.emplace_back( i, market_, mt);
        }
        void  update(const double dt) { 
            for (auto & pm : price_makers_) pm.update(dt);
        }
    };
    
    struct FixedPriceMakerEnsemble { 
        std::vector<PriceMakerWithRandomParams> price_makers_;
        std::vector<SingleInstrumentMarketMaker> single_instrument_market_makers_ ; 
        MarketState market_{0, std::numeric_limits<double>::quiet_NaN(), {0}, {0}, {0}, {0}, {0}, {0}};
        FixedPriceMakerEnsemble (const size_t n_agents, boost::random::mt19937 & mt) { 
            for (size_t i = 0; i < n_agents; ++i ) 
                price_makers_.emplace_back( i, market_, mt);
            for (size_t i = 0; i < n_agents; ++i ) {
                price_makers_[i].update( 10,  1, 10 );
                price_makers_[i].pm_.side_param( (i % 2) ? 1 : 0);
            }
        }
        void  update(const double ) { }
        void update( 
                const double cancellation_period, 
                const double price_mean,  
                const double order_size_mean ) { 
            for (auto & pm : price_makers_) pm.update(cancellation_period, price_mean, order_size_mean);
        }
    };

    template <typename Ensemble>
    inline void experiment(boost::random::mt19937 & mt, std::ostream * mkt_out_ptr, std::ostream * params_out_ptr, 
            Ensemble & ensemble, 
            const TimeType t_max = static_cast<TimeType>( 1e9*24*60*60 )
    ) {
        MatchingEngine  eng;
        MarketState & market = ensemble.market_;
        PassThroughTransport<LogNotify> transport(eng, LogNotify::instance(), 0.0);
        for( auto & pm : ensemble.price_makers_) transport.add_agent(pm.pm_);
        for( auto & pm : ensemble.single_instrument_market_makers_) transport.add_agent(pm);
        const std::vector<TrendFollowerAgent> trend_followers;
        std::vector<MarketState> market_data; 

        double last_param_update_time = std::numeric_limits<double>::quiet_NaN();

        bool first = true;
        while ( market.time_ <= t_max) {
            {
                std::ostringstream out ;
                out << market;
                SPDLOG_INFO( "{}", out.str() );
                if (first) {
                    for (size_t i = 0; i < ensemble.price_makers_.size(); ++i)
                        SPDLOG_INFO(
                        "pm {}: placement: {}s, cancellation: {}s, price mean: {}, size mean: {}, side:{} , aggressive:{}",
                        i,
                        1./ensemble.price_makers_[i].pm_.placement_.lambda(),
                        1./ensemble.price_makers_[i].pm_.cancellation_.lambda(),
                        1./ensemble.price_makers_[i].pm_.order_price_.lambda(),
                        ensemble.price_makers_[i].pm_.order_size_.mean(),
                        ensemble.price_makers_[i].pm_.side_param(),
                        ensemble.price_makers_[i].pm_.aggressive_param()
                    );
                    first = false;
                }
            }
            for (auto &pm: ensemble.price_makers_)
                pm.pm_.update_next_action_time();
            const TimeType t_algo = std::ranges::min_element( 
                    ensemble.price_makers_.begin(), 
                    ensemble.price_makers_.end(),
                    [](const PriceMakerWithRandomParams & a,  const PriceMakerWithRandomParams & b) 
                    { return a.pm_.next_action_time() < b.pm_.next_action_time(); }
                    )->pm_.next_action_time();
            transport.update_next_send_time(mt); //refresh delay_
            const TimeType t_transport = transport.next_send_time(); //earliest order placement time + delay
            const TimeType t = std::min(t_algo, t_transport);
            if (t==market.time_)
                throw std::runtime_error(std::format("Market time is stuck: {}", t) );
            market.time_ = t;
            eng.time_ = market.time_;
            for (auto & a : ensemble.price_makers_) a.pm_.markets_state_changed(transport);
            for (auto & a : ensemble.single_instrument_market_makers_) a.markets_state_changed(transport);
            transport.send(market.time_);
            if (transport.next_send_time() <= market.time_)
                throw std::runtime_error(std::format("Transport next send time should have moved : {} - {}",
                    transport.next_send_time(), market.time_) );
            eng.level25(
                market.bid_prices_, market.bid_sizes_, market.bid_ages_,
                market.ask_prices_, market.ask_sizes_, market.ask_ages_
            );
            if (market.bid_sizes_[0] != 0 and market.ask_sizes_[0] != 0)
                market.wm_ = static_cast<double>(market.bid_prices_[0] * market.ask_sizes_[0] +
                                                 market.ask_prices_[0] * market.bid_sizes_[0]) /
                             static_cast<double>(market.bid_sizes_[0] + market.ask_sizes_[0]);
            else 
                market.wm_ = std::numeric_limits<double>::quiet_NaN();

            if (mkt_out_ptr != nullptr)
                market_data.emplace_back( market );
            //if (mkt_out_ptr != nullptr) *mkt_out_ptr << market.time_*1e-9/60./60. << ' ' << market.wm_ << '\n'  ;
            
            const double dt = std::isnan( last_param_update_time ) ? 1+EPS : 1e-9*market.time_ - last_param_update_time;
            if (dt >= 1) {
                ensemble.update(dt);
                last_param_update_time = 1e-9*market.time_; 
                if (params_out_ptr != nullptr) {
                    double 
                        sum_cancel  = 0 , sumsq_cancel = 0,
                                    sum_price  = 0 , sumsq_price = 0,
                                    sum_size  = 0 , sumsq_size = 0;
                    for (const auto & pm : ensemble.price_makers_ ) {
                        sum_cancel += pm.get_cancellation_period(); 
                        sumsq_cancel += pm.get_cancellation_period()*pm.get_cancellation_period(); 
                        sum_price += pm.get_order_price_mean();
                        sumsq_price += pm.get_order_price_mean()*pm.get_order_price_mean();
                        sum_size += pm.order_size_mean();
                        sumsq_size += pm.order_size_mean()*pm.order_size_mean();
                    }
                    //const double mean_cancel = sum_cancel/ensemble.price_makers_.size();
                    //const double std_cancel = std::sqrt(sumsq_cancel/ensemble.price_makers_.size() - mean_cancel*mean_cancel);
                    //const double mean_price = sum_price/ensemble.price_makers_.size();
                    //const double std_price = std::sqrt(sumsq_price/ensemble.price_makers_.size() - mean_price*mean_price);
                    //const double mean_size = sum_size/ensemble.price_makers_.size();
                    //const double std_size = std::sqrt(sumsq_size/ensemble.price_makers_.size() - mean_size*mean_size);
                    const size_t last = ensemble.price_makers_.size()-1;
                    const size_t lm1  = last -1 ;
                    *params_out_ptr << last_param_update_time / 60. / 60. << ' ' << market.wm_
                            << ' ' << 1./ensemble.price_makers_[lm1 ].pm_.cancellation_.lambda()
                            << ' ' << 1./ensemble.price_makers_[last].pm_.cancellation_.lambda()
                            << ' ' << market.bid_ages_.front()
                            << ' ' << market.ask_ages_.front()
                            << ' ' << market.bid_prices_.front()
                            << ' ' << market.ask_prices_.front()
                        ;
                    //for (const auto & pm : ensemble.price_makers_ ) *params_out_ptr<< ' '  << pm.pm_.cancellation_.lambda();
                    *params_out_ptr<< '\n'  ;
                }
            }

        }

        if (mkt_out_ptr != nullptr) {
            SPDLOG_INFO("Done : {}", market_data.size() );
            size_t start = 0; 
            while (start < market_data.size() and market_data[start].time_ < 1e9*60*60/2 )
                ++start; 
            size_t end = start; 
            while (end < market_data.size() and market_data[end].time_ < 1e9*60*60*23.5 ) 
                ++end; 
            constexpr size_t nextra = 5;
            const size_t C = nextra  + 1 + 6*market.bid_prices_.size() ;
            torch::Tensor market_torch = torch::empty({int64_t(end-start),  int64_t(C)});
            if( C != size_t(market_torch.size(1)) ) 
                throw std::runtime_error("I don't know these matrices");

            SPDLOG_INFO("Allocated : {}", market_data.size() );

            if ( not market_torch.is_contiguous()  ) {  
                SPDLOG_ERROR("contiguous : {}" , market_torch.is_contiguous() );
                for (int64_t i = 0; i < int64_t(end-start); ++i) { 
                    const MarketState & m = market_data[i+start];
                    int j = 0;
                    market_torch.index_put_( {i,j} , static_cast<float>(m.time_*1e-9) ) ;
                    for ( const auto x : m.bid_prices_ ) 
                        market_torch.index_put_( {i,++j} , static_cast<float>(x) ) ;
                    for ( const auto x : m.ask_prices_ ) 
                        market_torch.index_put_( {i,++j} , static_cast<float>(x) ) ;
                    for ( const auto x : m.bid_sizes_ ) 
                        market_torch.index_put_( {i,++j} , static_cast<float>(x) ) ;
                    for ( const auto x : m.ask_sizes_ ) 
                        market_torch.index_put_( {i,++j} , static_cast<float>(x) ) ;
                    for ( const auto & x : m.bid_ages_ ) 
                        market_torch.index_put_( {i,++j} , x ) ;
                    for ( const auto & x : m.ask_ages_ ) 
                        market_torch.index_put_( {i,++j} , x ) ;

                }
                SPDLOG_INFO("Stored : {}", market_data.size() );
            } else { 
                const auto & temp0 = market_torch.index( {0,0} ) ; 
                const auto & temp1 = market_torch.index( {0,1} ) ; 
                const auto & temp2 = market_torch.index( {1,0} ) ; 
                float * f0 = temp0.data_ptr<float>();
                float * f1 = temp1.data_ptr<float>();
                float * f2 = temp2.data_ptr<float>();
                size_t d01 = std::distance( f0, f1 );
                size_t d02 = std::distance( f0, f2 );
                if ( 1 == d01 and C == d02 ) { 
                    SPDLOG_INFO( "distance: {}, {}", d01, d02 );
                    for (size_t i = 0; i < end-start; ++i) { 
                        const MarketState & m = market_data[i+start];
                        size_t j = 0;
                        f0[i*C+j] = static_cast<float>(m.time_*1e-9) ;
                        for ( const auto x : m.bid_prices_ ) 
                            f0[i*C + ++j] = static_cast<float>(x) ;
                        for ( const auto x : m.ask_prices_ ) 
                            f0[i*C + ++j] = static_cast<float>(x) ;
                        for ( const auto x : m.bid_sizes_ ) 
                            f0[i*C + ++j] = static_cast<float>(x) ;
                        for ( const auto x : m.ask_sizes_ ) 
                            f0[i*C + ++j] = static_cast<float>(x) ;
                        for ( const auto & x : m.bid_ages_ ) 
                            f0[i*C + ++j] = static_cast<float>(x) ;
                        for ( const auto & x : m.ask_ages_ ) 
                            f0[i*C + ++j] = static_cast<float>(x) ;

                        if (std::isnan( m.wm_ )) { 

                        } else {
                            double wm_high_watermark = m.wm_ ;
                            double wm_low_watermark = m.wm_ ;
                            double sum_wmsq_dt = 0;
                            double sum_wm_dt = 0;
                            double sum_dt = 0;
                            double wm_last = m.wm_ ; 
                            double t_last = m.time_*1e-9; 
                            for (size_t ii = start+i+1; ii < market_data.size(); ++ii) { 
                                const MarketState & mi = market_data[ii];
                                //SPDLOG_INFO( "dt {}, , wm {}, sum_wm_dt {}, sum_wmsq_dt {}, sum_dt {}", (mi.time_ - m.time_)*1e-9 , mi.wm_ , sum_wm_dt, sum_wmsq_dt, sum_dt );
                                if (mi.time_ - m.time_ < static_cast<TimeType>( 1e9*1 ) ) {
                                    if (not std::isnan(mi.wm_) ) { 
                                        wm_high_watermark = std::max( mi.wm_, wm_high_watermark );
                                        wm_low_watermark = std::min( mi.wm_, wm_low_watermark );
                                        const double t_next = 1e-9*mi.time_;
                                        const double dt =( t_next - t_last ); 
                                        sum_wm_dt += wm_last * dt;
                                        sum_wmsq_dt += wm_last * wm_last * dt;
                                        sum_dt += dt;
                                        wm_last = mi.wm_ ; 
                                        t_last = t_next;
                                        //SPDLOG_INFO("post dt {}", dt );
                                    }
                                    continue;
                                } else
                                    break;
                            }

                            const double mean = sum_wm_dt / sum_dt;  
                            double var = sum_wmsq_dt / sum_dt - mean*mean ;
                            if (var < -EPS) 
                                throw std::runtime_error(
                                        fmt::format("Negative var? sum mw : {}, sum sq mw : {}, sum dt : {}", 
                                            sum_wm_dt, sum_wmsq_dt, sum_dt ) );
                            if (var < EPS) var = EPS;
                            const double stdev = std::sqrt( var );

                            f0[i*C + ++j] = static_cast<float>(wm_last - m.wm_) ;
                            f0[i*C + ++j] = static_cast<float>(wm_high_watermark - m.wm_) ;
                            f0[i*C + ++j] = static_cast<float>(wm_low_watermark - m.wm_) ;
                            f0[i*C + ++j] = static_cast<float>(mean - m.wm_) ;
                            f0[i*C + ++j] = static_cast<float>(stdev) ;
                            if (j+1 != C) 
                                throw std::runtime_error( fmt::format(
                                            "Not enough space: We allocated {} columns, but used {} columns.", 
                                            C, j+1 ) );
                        }
                    }
                    SPDLOG_INFO("Stored : {}", market_data.size() );

                } else {
                    SPDLOG_ERROR( "distance: {}, {}", d01, d02 );
                }
            }
            const auto pickled = torch::pickle_save(market_torch);
            SPDLOG_INFO("Pickled : {}", pickled.size() );
            mkt_out_ptr->write( pickled.data(), pickled.size() );
            SPDLOG_INFO("Written : {}", pickled.size() );
        } 

    }

}
