#include "ob.h"
#include "sim.h"

#include <chrono>
#include <limits>




int main(const int argc,const char ** argv) { 
    using namespace SDB;

    if (argc!=4) return -1;
    

    boost::random::mt19937 mt;
    mt.seed(0);

    ClientType type1( "type1", mt, 1./60., 1./(30*60.), 100. , 5. , 0.5 );  
    ClientType type2( "type2",mt,  1.,     1.,  10. , 2. , 0.5 );  

    const ClientIDType N1 = std::atoi( argv[1] ); 
    const ClientIDType N2 = std::atoi( argv[2] ) ;
    //const TimeType TMax = 24*60*60 * 1e9; //nanos from start 
    const TimeType TMax = std::atof( argv[3] ) * 1e9; //nanos from start 

    std::vector<ClientState> client_states; 
    client_states.reserve( N1 + N2);

    for (ClientIDType cid = 0; cid < N1; ++cid)  
        client_states.emplace_back( type1 , cid );
    for (ClientIDType cid = N1; cid < N1+N2; ++cid)  
        client_states.emplace_back( type2 , cid );
    

    MatchingEngine eng;
    CerrLogger logger;
    ClientState::NotificationHandler handler(logger,eng);

    for (auto & cs : client_states) {
        cs.setup_new_order_placement_data(0.0, 0); //set action time
        handler.add(&cs);
    }

    if (false) {
        int n = 0 ; 
        for (auto it = handler.by_time_.ordered_begin(); it != handler.by_time_.ordered_end(); ++it ) {
            n += 1;
            const auto & state_ptr = *it;
            //std::chrono::duration<double, std::ratio<1>> dt( state_ptr.p_->next_action_time_*1e-9 );
            std::cout << "start " << n << " " << std::to_string( *state_ptr.p_ ) << std::endl;
        }
    }

    long lastsec = -1;
    while ( eng.time_ < TMax ) { 
        const ClientState::Ptr & ptr = handler.get_next_action();
        const ClientState & state = *ptr.p_;
        if (false)
            std::cout << "will work on " << std::to_string(state) << "\n";
        if (state.next_action_time_>=TMax) break;
        eng.set_time( state.next_action_time_ );
        //send the ptr to the back of queue : 
        state.next_action_time_ = std::numeric_limits<TimeType>::max();
        handler.by_time_.update( ptr.handle_ );

        long secs = lround(eng.time_*1e-9);
        if (( secs%(60*60)==0 and secs != lastsec) ) {
            int n_orders = 0, n_orders2 = 0; 
            for (const auto & x : eng.all_bids_) n_orders += x.orders_.size();
            for (const auto & x : eng.all_offers_) n_orders += x.orders_.size();
            for (auto & cs : client_states)
                if (cs.active_order_id_ != std::numeric_limits<OrderIDType>::max())
                    n_orders2 += 1;
            
            std::cout << std::chrono::duration<double, std::ratio<1>>( eng.time_*1e-9 )<< 
                " norders:" << eng.mem_.used_ << ":" << n_orders << ":" << n_orders2 << 
                " bidlevels:" << eng.all_bids_.size() << 
                " asklevels:" << eng.all_offers_.size()  << 
                " order_ptr_set:" << eng.ptr_set_.size()  <<  //not a hash set 
                " by_cid:" << handler.by_cid_.size()  <<  //hashset
                " by_oid:" << handler.by_oid_.size()  <<  //hashset
                " by_time:" << handler.by_time_.size()  << //priority queue 
                std::endl;
            lastsec = secs;
        }
        if (state.action_ == 0) {//place new order
            eng.add_simulation_order( state.client_id_,0,  state.price_, state.size_,
                    state.show_, state.side_, false, handler);
        } else if (state.action_ == 1) { //cancel
            eng.cancel_order( state.active_order_id_, handler );
        }
        if (false) {
            int n = 0 ; 
            for (auto it = handler.by_time_.ordered_begin(); it != handler.by_time_.ordered_end(); ++it ) {
                n += 1;
                const auto & state_ptr = *it;
                //std::chrono::duration<double, std::ratio<1>> dt( state_ptr.p_->next_action_time_*1e-9 );
                std::cout << "state " << n << " " << std::to_string( *state_ptr.p_ ) << std::endl;
            }
        }
    }

    //we are done: let's look at orders:
    if (false) {
        int n = 0 ; 
        for (auto it = handler.by_time_.ordered_begin(); it != handler.by_time_.ordered_end(); ++it ) {
            n += 1;
            const auto & state_ptr = *it;
            //std::chrono::duration<double, std::ratio<1>> dt( state_ptr.p_->next_action_time_*1e-9 );
            std::cout << "final " << n << " " << std::to_string( *state_ptr.p_ ) << std::endl;
        }
    }

    eng.set_time( TMax );
    eng.shutdown(handler);

    std::cout << "used: " <<   eng.mem_.used_ << std::endl;
    std::cout << "t: " << std::chrono::duration<double, std::ratio<1>>( TMax*1e-9 ) << std::endl;

    return 0;
}

