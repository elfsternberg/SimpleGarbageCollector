---
**WHAT:**

This is an example of Bob Nystrom's
"[Baby's First Garbage Collector](http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/),"
which I've been wanting to implement for a while in order to understand
it better.  To make the problem harder (as I always do), I decided to
write it in C++, and to make it even more fun, I've implemented it using
the new Variant container from C++17.

**WHY:**

I've never written a garbage collector before.  Now I know what it is
and how it works.

**DETAILS:**

The collector Nystrom wrote is a simple bi-color mark-and-sweep
collector for a singly threaded process with distinct pauses based upon
a straightforward memory usage heuristic.  That heuristic is simply, "Is
the amount of memory currently in use twice as much as the last time we
garbage collected?"  If the answer is yes, the collector runs and sweeps
up the stack.

Nystrom's code is highly readable, and I hope mine is as well.  Because
I used Variant, my Object class has an internal Pair class, and then the
Variant is just \<int, Pair\>, where "Pair" is a pair of pointers to
other objects.  The entirety of the VM is basically a stack of
singly-linked lists which either represents integers or collections of
integers in a Lisp-like structure.

The allocator creates two kinds of objects, then: pairs, and lists.  A
pair is created by pushing two other objects onto the stack, then
calling `push()`, which pops them off the stack and replaces them with a
Pair object.  The VM class has two methods, both named `push()`, one of
which pushes an integer, the other a pair.  Since a pair is built from
objects on the stack, the Pair version takes no arguments, and since
C++14 and beyond have move semantics that Variant honors,
Variant\<Pair\> only constructs a single pair.  Pretty nice.  I was also
able to use both lambda-style and constructor-style visitors in my
Variant, which was a fun little bonus.

**NOTE:**

I have included the header files for the
[Mapbox version of Variant](https://github.com/mapbox/variant) since the
C++17 committee's standards haven't quite reached the general public and
the Variant implementation is still a subject of some debate.  This
implementation looks straightforward enough and is a header-only
release.  It works with both GCC 4.8.5 and Clang 3.8, and that's good
enough for me.

The Mapbox variant is BSD licensed, and a copy of the license is
included in the Include directory.

**BUILDING:**

From the base directory of the project:

    mkdir build
    cd build
    cmake ..
    make

And you should be able to run the basic tests.  It's just one file.
