#ifndef XV6_SIGNAL
#define XV6_SIGNAL

typedef struct  {
	//defines which signal 
	//number is being caught
        int signum;
} siginfo_t;

typedef void (*sighandler_t) (siginfo_t);

#define SIGALRM 0
#define SIGFPE 1
 
#endif
