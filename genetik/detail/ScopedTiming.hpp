#pragma once
#include <chrono>
namespace genetik_detail {
// Optional wall-clock accounting only. No clock calls when the destination is null.
class ScopedTiming {
    using Clock=std::chrono::steady_clock;
    double* destination_;double* secondary_;Clock::time_point start_;
public:
    explicit ScopedTiming(double* destination,double* secondary=nullptr):destination_(destination),secondary_(secondary) {if(destination_)start_=Clock::now();}
    ScopedTiming(const ScopedTiming&)=delete;
    ScopedTiming& operator=(const ScopedTiming&)=delete;
    ~ScopedTiming() noexcept {if(destination_) {const auto seconds=std::chrono::duration<double>(Clock::now()-start_).count();*destination_+=seconds;if(secondary_)*secondary_+=seconds;}}
};
}
