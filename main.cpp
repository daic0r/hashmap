#include <iostream>
#include "hashmap.h"
#include <string>

int main() {
    hashmap<int,  std::string> map;

    map.emplace(10, "hello");
}