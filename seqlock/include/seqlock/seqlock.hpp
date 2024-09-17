#pragma once

namespace lib {

class Foo {
   public:
    Foo() = default;
    ~Foo() = default;

    Foo(const Foo &) = default;
    Foo &operator=(const Foo &) = default;

    Foo(Foo &&) = default;
    Foo &operator=(Foo &&) = default;

    void operator()();

   private:
};

}  // namespace lib
