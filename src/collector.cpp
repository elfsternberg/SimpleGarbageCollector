#include <iostream>
#include <type_traits>
#include <variant>


#define MAX_STACK 256
#define MAX_BARRIER 8

void my_assert(int condition, const char* message) {
  if (!condition) {
    std::cout << message << std::endl;
    exit(1);
  }
}

/* An implementation of Bob Nystrom's "Baby's First Garbage Collector"
   http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/,
   only in C++, and with some educational stuff along the way about
   the new Variant (automagical discriminated unions) coming in
   Libstdc++ version 4, part of the C++ 2017 standard.
*/

class Object {
public:
  unsigned char marked;
  Object *next;
  Object(int v): marked(0), value(v) {}
  // Variant<Pair> uses move semantics; this doesn't result in Pair being built twice.
  Object(Object* head, Object* tail): marked(0), value(Pair(head, tail)) {}

  class Pair {
  public:
    Pair(Object* h, Object* t): head(h), tail(t) {};
    Object* head;
    Object* tail;
  };

  /* This is mostly an exploration of a discriminated union, and
     making one work in the context of a primitive but functional
     garbage collector. */
  std::variant<int, Pair> value;
};

class VM {
public:
  /* Imagine my surprise when I learned that clang doesn't bother to
     zero out memory allocated on the threadstack. */
  VM(): stackSize(0), numObjects(0), maxObjects(MAX_BARRIER), root(NULL) {};
  
  Object* pop() {
    my_assert(stackSize > 0, "Stack underflow!");
    return stack[--stackSize];
  }

  /* This is basically the interface for a very primitive reverse
     polish notation calculator of some kind.  A garbage-collected
     Forth interpreter, perhaps. */

  Object* push(int v) {
    return _push(insert(new Object(v)));
  }
  
  Object* push() {
    return _push(insert(new Object(pop(), pop())));
  }

  /* Lambda-style visitors, enabling descent. */
  void mark(Object *o) {
    if (o->marked) {
      return;
    }

    o->marked = 1;
    return std::visit([this](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) { }
        else if constexpr (std::is_same_v<T, Object::Pair>) {
            this->mark(arg.head);
            this->mark(arg.tail);
          }
      }, o->value);
  }

  /* So named because each scope resembles a collection of objects
     leading horizontally from the vertical stack, creating a spine. */
  void markSpine() {
    for(auto i = 0; i < stackSize; i++) {
      mark(stack[i]);
    }
  }

  void collect() {
    int num = numObjects;
    markSpine();
    sweep();
    maxObjects = numObjects * 2;
#ifdef DEBUG
    std::cout << "Collected " << (num - numObjects) << " objects, "
              << numObjects << " remain." << std::endl;
#endif
  }

  /* The saddest fact: I went with using NULL as our end-of-stack
     discriminator rather than something higher-level, like an
     Optional or Either-variant, because to use those I'd have to use
     recursion to sweep the interpreter's stack, which means I'm at
     the mercy of the C stack, complete with the cost of the unwind at
     the end.  Bummer. */

  /* I look at this and ask, WWHSD?  What Would Herb Sutter Do? */
  
  void sweep() {
    Object** o = &root;
    while(*o) {
      if (!(*o)->marked) {
        Object* unreached = *o;
        *o = unreached->next;
        numObjects--;
        delete unreached;
      } else {
        (*o)->marked = 0;
        o = &(*o)->next;
      }
    }
  }
      
  int numObjects;
  
private:

  /* Heh.  Typo, "Stark overflow."  I'll just leave Tony right there anyway... */
  Object* _push(Object *o) {
    my_assert(stackSize < MAX_STACK, "Stark overflow");
    stack[stackSize++] = o;
    return o;
  }
  
  Object* insert(Object *o) {
    if (numObjects >= maxObjects) {
      collect();
    }
    
    o->marked = 0;
    o->next = root;
    root = o;
    numObjects++;
    return o;
  }
    
  Object* stack[MAX_STACK];
  Object* root;
  int stackSize;
  int maxObjects;
};


void test1() {
  std::cout << "Test 1: Objects on stack are preserved." << std::endl;
  VM vm;
  vm.push(1);
  vm.push(2);
  vm.collect();
  my_assert(vm.numObjects == 2, "Should have preserved objects.");
}

void test2() {
  std::cout << "Test 2: Unreached objects are collected." << std::endl;
  VM vm;
  vm.push(1);
  vm.push(2);
  vm.pop();
  vm.pop();
  vm.collect();
  my_assert(vm.numObjects == 0, "Should have collected objects.");

}
    
void test3() {
  std::cout << "Test 3: Reach nested objects." << std::endl;
  VM vm;
  vm.push(1);
  vm.push(2);
  vm.push();
  vm.push(3);
  vm.push(4);
  vm.push();
  vm.push();
  vm.collect();
  my_assert(vm.numObjects == 7, "Should have reached objects.");
}

/* see: "Everything you need to know about std::variant from C++17, by Bartłomiej Filipek
         https://www.bfilipek.com/2018/06/variant.html
   Bartek says:

   "Those two lines look like a bit of magic :) But all they do is they
   create a struct that inherits all given lambdas and uses their
   Ts::operator(). The whole structure can be now passed to std::visit."

   As I understand it, the ellipsis there is the "Parameter Pack"
   operator, used to define variadic types.  

*/         

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

void tail_setter(std::variant<int, Object::Pair> &c, Object *tail) {
  std::visit(
            overload{
                     [&tail](int i) {},
                     [&tail](Object::Pair &p) { p.tail = tail; }
            }, c);
}

void test4() {
  std::cout << "Test 4: Handle cycles." << std::endl;
  VM vm;
  vm.push(1);
  vm.push(2);
  Object* a = vm.push();
  vm.push(3);
  vm.push(4);
  Object* b = vm.push();
  
  
  tail_setter(a->value, b);
  tail_setter(b->value, a);
  vm.collect();
  my_assert(vm.numObjects == 4, "Should have collected objects.");
}

void perfTest() {
  std::cout << "Performance Test." << std::endl;
  VM vm;

  for (int i = 0; i < 1000; i++) {
    for (int j = 0; j < 20; j++) {
      vm.push(i);
    }

    for (int k = 0; k < 20; k++) {
      vm.pop();
    }
  }
}

int main(int argc, const char * argv[]) {
  test1();
  test2();
  test3();
  test4();
  perfTest();

  return 0;
}
