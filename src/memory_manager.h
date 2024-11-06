#ifndef __MEMORY_MANAGER_H__
#define __MEMORY_MANAGER_H__

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set_hook.hpp>

#include <iostream>
#include <vector>

namespace SDB { 

    
    struct MemoryManaged : 
        public boost::intrusive::list_base_hook<
            boost::intrusive::link_mode< boost::intrusive::safe_link >
        > 
    { 
        MemoryManaged( ) {} ;
        MemoryManaged( const MemoryManaged & ) = delete;
        MemoryManaged & operator=(MemoryManaged & ) = delete;
    };

    namespace Detail { 
        using namespace SDB;
        struct TDHash { 
            template <typename T>
                size_t operator()(const T & t) const {
                    return hash_value(t) ; 
                }
        };
    }

    template <typename T> 
        struct MemoryManager { 
            using list_type = boost::intrusive::list< T, boost::intrusive::constant_time_size<true> >;
            using buffer_array = std::array<T,128*1024>;
            //data
            list_type free_;
            std::vector<buffer_array*> mem_ ; 
            int64_t used_ ;
            //methods

            MemoryManager(const int N=2*1024*1024 ) : used_(0) {
                    int p = 0;
                    int n = N;
                    while (n>1) { 
                        n /= 2; 
                        ++p;
                    }
                    const int Np = 1 << p ;
                    if(Np != N) { 
                        std::cerr << "N has to be power of 2 "  << Np << ' ' << p << ' ' << N ; 
                        exit(1);
                    }
                } 

            ~MemoryManager() { 
                free_.clear();
                for (auto * ptr : mem_ ) 
                    delete ptr;
            }

            void increase_mem() { 
                mem_.emplace_back(new buffer_array()) ; 
                for (T & t : *mem_.back() )
                    free_.push_back(t);
            }

            T & get_unused() {
                if (free_.empty()) increase_mem();
                T & t = free_.front();
                t.clear();
                free_.erase(free_.iterator_to(t));
                used_ += 1;
                //std::cout << "using. used: " <<   used_ << std::endl;
                return t;
            }


            void free(T & t) { 
                t.clear();
                free_.push_front(t);
                used_ -= 1;
                //std::cout << "freed. used: " <<   used_ << std::endl;
            }
        };

} 

#endif 
