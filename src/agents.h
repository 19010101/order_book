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
#include <boost/multi_index/member.hpp>

#include <iomanip>
#include <fstream>

namespace SDB {

    struct MarketState { 
        TimeType time_ ; 
        double wm_ ;
        std::array<PriceType, 4> bid_prices_;
        std::array<SizeType, 4>  bid_sizes_;
        std::array<PriceType, 4> ask_prices_;
        std::array<SizeType, 4>  ask_sizes_;
    };

    std::ostream & operator<<(std::ostream & out, const MarketState & market ) {
        out << std::right << std::fixed << std::setw(15)<<  std::setprecision(9) << static_cast<double>(market.time_) * 1e-9
                << ' '
                << std::right  << std::setw(3)<<  market.bid_sizes_[2]
                << "b@"
                << std::left << std::setw(3)<<  market.bid_prices_[2]
                << ' '
                << std::right  << std::setw(3)<<  market.bid_sizes_[1]
                << "b@"
                << std::left << std::setw(3)<<  market.bid_prices_[1]
                << ' '
                << std::right  << std::setw(3)<<  market.bid_sizes_[0]
                << "b@"
                << std::left << std::setw(3)<<  market.bid_prices_[0]
                << "  wm:"
                << std::fixed << std::setw(5)<<  std::setprecision(2) << market.wm_
                << ' '
                << std::right  << std::setw(3)<<  market.ask_sizes_[0]
                << "a@"
                << std::left << std::setw(3)<<  market.ask_prices_[0]
                << ' '
                << std::right  << std::setw(3)<<  market.ask_sizes_[1]
                << "a@"
                << std::left << std::setw(3)<<  market.ask_prices_[1]
                << ' '
                << std::right  << std::setw(3)<<  market.ask_sizes_[2]
                << "a@"
                << std::left << std::setw(3)<<  market.ask_prices_[2] ;
        return out;
    }

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
                    orders_.size() + unacked_orders_.size() < n_orders_ and
                    not std::isnan(market_.wm_)
                ) {
                    //const auto price = safe_round<PriceType>(order_price_(mt_) + market_.wm_);
                    //std::cerr << market_.time_ << ", market_.wm_: " << market_.wm_ << '\n';
                    const double continuous_price = order_price_(mt_) + market_.wm_;
                    //std::cerr << market_.time_ << ", continuous price: " << continuous_price << '\n';
                    const auto price = safe_round<PriceType>(continuous_price);
                    //std::cerr << market_.time_ << ", price: " << price << '\n';
                    const bool aggressive = aggressive_(mt_);
                    const Side passive_side = (continuous_price >= market_.wm_) ? Side::Offer : Side::Bid;
                    const Side side = aggressive ? get_other_side(passive_side) : passive_side;
                    const LocalOrderIDType local_order_id = local_id_counter_++;
                    //std::cerr << "will place order with local id " << local_order_id << std::endl;
                    auto [fst, snd] = unacked_orders_.emplace(
                        local_order_id, price, 1 + order_size_(mt_), 2, side);
                    if (not snd) throw std::runtime_error(
                        "Problem placing un-acked order in map: " + std::to_string(local_order_id));
                    transport.place_order(client_id_, *fst);
                }
                while (market_.time_ >= placement_time_)
                    placement_time_ += safe_round<TimeType>(1e9*placement_(mt_));

                auto & index = cancellation_times_.get<0>() ; 
                //if (not index.empty()) std::cerr << "time: " << market_.time_ << ", cancellation time:" << index.begin()->t_cancel_ << "\n";
                while( not index.empty() and market_.time_ >= index.begin()->t_cancel_ ) {
                    SPDLOG_TRACE("cancelling oid: 0x{:xspn} at time: ", spdlog::to_hex(index.begin()->order_id_),
                        index.begin()->t_cancel_);
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
            void handle_market_state_changed(Transport & ) {
                if (std::isnan(market_.wm_)) return;
                if (position_ == 0) {
                    //make sure we are in the market at best bid and best offer.
                } else {
                    //cancel all passive orders;
                    //send and aggresive order to get out of the position
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
            std::vector<std::tuple<TimeType,ClientIDType, OrderData>> orders_to_place;
            std::vector<std::tuple<TimeType,OrderIDType>> orders_to_cancel;
            std::unordered_map<ClientIDType, std::unordered_map<PriceType, int> > price_counts;
            PassThroughTransport( MatchingEngine & eng, Notifier & notifier, const double delay_lambda ) :
                eng_(eng), notifier_(notifier) , delay_distribution_(std::max(delay_lambda,EPS)),
                delay_disabled_(delay_lambda <= EPS),
                delay_(0){}

            bool add_agent( PriceMakerAroundWM & agent ) {
                if (trend_followers.contains( agent.client_id_ )) return false;
                return price_makers.emplace( agent.client_id_, &agent ).second;
            }
            bool add_agent( TrendFollowerAgent & agent ) {
                if (price_makers.contains( agent.client_id_ )) return false;
                return trend_followers.emplace( agent.client_id_, &agent ).second;
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


    void experiment() {

        boost::random::mt19937 mt(0);
        std::vector<PriceMakerAroundWM> price_makers;
        constexpr size_t n_agents = 10;
        MarketState market{0, 0.0, {}, {}, {}, {}};

        for (size_t i = 0; i < n_agents; ++i ) {
            const bool large_orders = i%2 == 1;
            if (large_orders)
                price_makers.emplace_back(
                    i, market, mt,
                    1., 1. ,
                    -.5, 1., 10., 0.01,
                    10);
        }

    }
}
