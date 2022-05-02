#ifndef MY_LIB_PREDEFINITIONS_HEADER
#define MY_LIB_PREDEFINITIONS_HEADER 1

#ifndef FWD
#define FWD(e) static_cast<decltype(e)&&>(e)
#endif // !FWD

#ifndef SFWD
#define SFWD(T, a) (_STD forward<T>(a))
#endif // !SFWD


#ifndef MOV
#define MOV(e) (_STD move(e))
#endif // !MOV

#endif // !MY_LIB_PREDEFINITIONS_HEADER

