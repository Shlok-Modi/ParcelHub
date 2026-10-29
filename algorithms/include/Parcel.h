#pragma once
#include <cstdio>
#include <string>

enum class ServiceLevel { Express = 1, SameDay = 2, Standard = 3 };  // value == priority key
enum class Status { Pending, Dispatched };

struct Parcel {
    int id = 0;
    int source = 0;
    int destination = 0;
    ServiceLevel level = ServiceLevel::Standard;
    int priority = 3;          // lower = more urgent
    int deadlineMinutes = 0;   // minutes since midnight (display only for now)
    int arrival = 0;           // arrival sequence/time; earlier wins ties
    Status status = Status::Pending;
};

inline Parcel makeParcel(int id, int src, int dst, ServiceLevel level, int deadlineMinutes, int arrival) {
    Parcel p;
    p.id = id; p.source = src; p.destination = dst;
    p.level = level; p.priority = static_cast<int>(level);
    p.deadlineMinutes = deadlineMinutes; p.arrival = arrival;
    return p;
}

inline const char* toString(ServiceLevel l) {
    switch (l) {
        case ServiceLevel::Express:  return "Express";
        case ServiceLevel::SameDay:  return "Same-day";
        default:                     return "Standard";
    }
}

inline std::string formatTime(int minutes) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", minutes / 60, minutes % 60);
    return buf;
}
