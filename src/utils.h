#pragma once

#include "ob.h"
#include "sim.h"

#include <fstream>
#include <string>
#include <vector>
#include <charconv>

namespace SDB {

    inline void split_string( const std::string_view & line, std::vector<std::string_view> & words, const char sep=',' ) { 
        words.clear();
        size_t i = 0 , loc ; 
        while ( i < line.size() ) { 
            loc =line.find(sep, i) ; 
            const size_t size = (loc == std::string::npos) ?
                line.size() - i : 
                loc - i ;
            words.emplace_back( line.data()+i , size );
            i += size + 1 ;  
        }
        if (loc == line.size()-1 )
            words.emplace_back( line.data()+loc , 0 );
    }

    template<typename T> 
        bool parse( std::string_view & str,  T & t ) { 
            auto [ptr, ec] = std::from_chars( str.begin(), str.begin() + str.size() , t ) ;
            return ec != std::errc() or ptr != str.begin()+str.size() ;
        }

    inline void read_csv_file( std::ifstream & in , std::vector<OrderBookEvent> & obes) { 
        obes.clear();
        std::string line;  
        std::vector<std::string_view> words;
        constexpr std::string_view ENTRY("ENTRY");
        constexpr std::string_view CANCEL("CANCEL");
        constexpr std::string_view AMMEND("AMMEND");
        constexpr std::string_view Ask("Ask");
        std::unordered_map<OrderIDType, std::string> active_orders;
        while ( std::getline( in, line ) ) { 
            split_string(line, words );
            if (words.size()!=5)
                throw std::runtime_error("Wrong num of words in line : " + std::to_string(words.size()) + ". line is '" + line + "'" );
            OrderBookEvent obe;
            if (not parse(words[0], obe.event_time_) )
                throw std::runtime_error(std::string("Cannot parse : '") + std::string(words[0]) + "'" );
            if (not parse(words[1], obe.oid_) )
                throw std::runtime_error(std::string("Cannot parse : '") + std::string(words[1]) + "'" );
            auto it = active_orders.find( obe.oid_ );
            if (words[2] == ENTRY) {
                if (it != active_orders.end())
                    throw std::runtime_error("Error parsing line : " + line + "\n. Observed this oid in a prev line : " + it->second );
                else 
                    active_orders.emplace( obe.oid_ , line );
                obe.mtype_ = NotifyMessageType::Ack ;
            } else if (words[2] == CANCEL) { 
                if (it == active_orders.end())
                    throw std::runtime_error("Error parsing line : " + line + "\n. Cancelling oid that we don't know about : " + 
                            std::to_string(obe.oid_) );
                else 
                    active_orders.erase( it );
                obe.mtype_ = NotifyMessageType::Cancel ;
            } else if (words[2] == AMMEND) { 
            }
            if (not parse(words[3], obe.price_) )
                throw std::runtime_error(std::string("Cannot parse : '") + std::string(words[3]) + "'" );
            if (words[4] == Ask) 
                obe.side_ = Side::Offer;
            else
                obe.side_ = Side::Bid;
            if (not parse(words[5], obe.size_) )
                throw std::runtime_error(std::string("Cannot parse : '") + std::string(words[5]) + "'" );
        }
    }

}