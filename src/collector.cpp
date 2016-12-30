#include <iostream>

/* Requires the mapbox header-only variant found at
   https://github.com/mapbox/variant

   Compiles with: 
   clang++ -std=c++14 -I./include/ -o collector collector.cpp
   …  or
   g++ -std=c++1y -I./include/ -g -o collector collector.cpp
   … where 'include' has the variant headers.

   clang version 3.9.1-svn288847-1~exp1 (branches/release_39)
   g++ version (Ubuntu 4.8.5-2ubuntu1~14.04.1) 4.8.5

*/
#include <mapbox/variant.hpp>

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
  mapbox::util::variant<int, Pair> value;
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
    auto marker = mapbox::util::make_visitor(
        [this](int) {},
        [this](Object::Pair p) {
          this->mark(p.head);
          this->mark(p.tail);
    });
    
    if (o->marked) {
      return;
    }

    o->marked = 1;
    return mapbox::util::apply_visitor(marker, o->value);
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
     Optional or Either-variant, because to use those I'd have to
     user recursion to sweep the interpreter's stack, which means
     I'm at the mercy of the C stack, complete with the cost of the
     unwind at the end.  Bummer. */

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

void test4() {
  std::cout << "Test 4: Handle cycles." << std::endl;
  VM vm;
  vm.push(1);
  vm.push(2);
  Object* a = vm.push();
  vm.push(3);
  vm.push(4);
  Object* b = vm.push();

  /* Constructor-based variant visitor. */
  struct tail_setter {
    Object* tail;
    tail_setter(Object *t) : tail(t) {}
    inline void operator()(int &i) {}
    inline void operator()(Object::Pair &p) { p.tail = tail; }
  };
  
  /* Set up a cycle, and also make 2 and 4 unreachable and collectible. */
  mapbox::util::apply_visitor(tail_setter(b), a->value);
  mapbox::util::apply_visitor(tail_setter(a), b->value);
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
