#include "seqlock/seqlock.hpp"

#include <iostream>

namespace lib {

void Foo::operator()() { std::cout << "called Foo" << std::endl; }

}  // namespace lib
