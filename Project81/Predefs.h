#ifndef _STD
#define _STD ::std::
#endif // !_STD

#ifndef FWD
#define FWD(e) static_cast<decltype(e)&&>(e)
#endif // !FWD

#ifndef SFWD
#define SFWD(T, a) (_STD forward<T>(a))
#endif // !SFWD


#ifndef MOV
#define MOV(e) (_STD move(e))
#endif // !MOV