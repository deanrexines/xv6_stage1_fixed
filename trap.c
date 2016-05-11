#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;
void send_signal(struct trapframe *tf, int signum);

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  
  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpu->id == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
      extern struct {
        struct spinlock lock;
        struct proc proc[NPROC];
      } ptable;

      struct proc *p;

      acquire(&ptable.lock);
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
          if (p->ticks > 0) {
              p->ticks--;
              if (p->ticks == 0) {
                  p->alarm = 1;
              }
          }
      }
      release(&ptable.lock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpu->id, tf->cs, tf->eip);
    lapiceoi();
    break;

    case T_DIVIDE:
    cprintf("Divide by 0 exception\n");
    // Check if the process has a SIGFPE handler
    if (!(proc->sighandlers[SIGFPE] < 0)) {
        send_signal(tf, SIGFPE);
        break;
    }
    cprintf("No signal handler found\n");
    // If not let it fall through
   
  //PAGEBREAK: 13
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpu->id, tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip, 
            rcr2());
    proc->killed = 1;
  }

 if (proc != 0 && proc->state == RUNNING && proc->alarm > 0 && (tf->cs&3) == DPL_USER ) {
    proc->alarm = 0;
    cprintf("sending signal\n");
    send_signal(tf, SIGALRM);
  }
  // Process exit occurs when it has been killed & is in user space.
  // (If it is still in the kernel, keep running 
  // until returns with system call
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  void 
  send_signal(struct trapframe *tf, int signum) 
  {

    siginfo_t info;
    info.signum = signum;
    int decr = 0;

    decr += sizeof(siginfo_t);
    *((siginfo_t *)(tf->esp - decr)) = info;
    
    // Set return eip to trap frame eip
    decr += 4;
    *((uint *)(tf->esp - decr)) = tf->eip;
    tf->esp-=decr;
    tf->eip = (uint) proc->sighandlers[signum];
  }


  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since currently (supposed to be) yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}
