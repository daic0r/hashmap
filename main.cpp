#include <iostream>
#include "hashmap.h"
#include <string>

struct S {
    int i_;

    S() = default;
    explicit S(int i) : i_{i} {}
    S(S const& rhs) : i_{ rhs.i_ } {
        std::cout << "Copy ctor\n";
    }
    S& operator=(S const& rhs) {
        i_ = rhs.i_;
        std::cout << "Copy assignment\n";
        return *this;
    }
    S(S&& rhs) noexcept : i_{ std::exchange(rhs.i_, 0) } { 
        std::cout << "Move ctor\n";
    }
    S& operator=(S&& rhs) noexcept {
        i_ = std::exchange(rhs.i_, 0);
        std::cout << "Move assignment\n";
        return *this;
    }
    ~S() {
        std::cout << "Dtor\n";
    }
};

int main() {
    hashmap<int, S> map;

    for (int i = 100; i < 200; ++i)
        map.emplace(i, S{i});

    auto val = map.find(150).value().get();
    std::cout << "Value is " << val.i_ << std::endl;
}