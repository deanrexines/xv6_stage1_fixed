#ifndef XV6_SIGNAL
#define XV6_SIGNAL

+#define SIGALRM 0
+#define SIGFPE 1

//function pointer for the handler
typedef void (*sighandler_t) (siginfo_t);

typedef struct  {
	//defines which signal 
	//number is being caught
    int signum;
} siginfo_t;

// You should define anything signal related that needs to be shared between
// kernel and userspace here

// At a minimum you must define the signal constants themselves
// as well as a sighandler_t type.

#endif