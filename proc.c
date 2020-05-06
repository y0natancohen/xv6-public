#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}




int allocpid(void)
{
    int pid;
    acquire(&ptable.lock);
    pid = nextpid++;
    release(&ptable.lock);
    return pid;
}

//
//int
//allocpid(void)
//{
//  int old_pid;
//  do {
//      old_pid = nextpid;
//  } while (!cas(&nextpid, old_pid, old_pid + 1));
////    return old_pid + 1 ; // todo +1 ?
//    return old_pid;
//}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
//    cprintf("inside allocproc\n");
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  release(&ptable.lock);

  p->pid = allocpid();

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Leave room for trap frame backup.
    char *sp1;
    sp1 = p->kstack + sizeof *p->user_trapframe_backup;
    sp1 -= sizeof *p->user_trapframe_backup;
    p->user_trapframe_backup = (struct trapframe *)sp1;

//  sp -= sizeof *p->user_trapframe_backup;
//  p->user_trapframe_backup = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  
  // signals
  // sp -= sizeof (struct trapframe);
  // p->user_trapframe_backup = (struct trapframe *)sp;
  // for(int i=0; i<SIGNALS_SIZE; i++){
  //   sp -= sizeof (struct sigaction);
  //   p->signal_handlers[i] = (struct sigaction*)sp;
  //   // memset(p->signal_handlers[i], 0, sizeof(struct sigaction));
  //   p->signal_handlers[i]->sa_handler = SIG_DFL;
  //   p->signal_handlers[i]->sigmask = 0;
  // }
  p->pending_signals = 0;
  p->signal_mask = 0;

    for(int i=0; i<SIGNALS_SIZE; i++){
    p->signal_handlers[i].sa_handler = SIG_DFL;
    p->signal_handlers[i].sigmask = 0;
  }
  return p;
}




//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  // signals
  for(int i=0; i< SIGNALS_SIZE; i++){
    np->signal_handlers[i].sa_handler = curproc->signal_handlers[i].sa_handler;
    np->signal_handlers[i].sigmask = curproc->signal_handlers[i].sigmask;
  }
  np->signal_mask = curproc->signal_mask;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != RUNNABLE && p->state != FROZEN)
            continue;
        handle_pending_signals_kernel(p);
        if(p->state != RUNNABLE)
            continue;
//        cprintf("scheduler pid: %d\n", p->pid);

//      handle_pending_signals(p); // if the process received sigstop, it will not be runnable anymore
      // todo: it is not the case according to 2.3.1 description - ask in forum
//      if(p->state != RUNNABLE)
//        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

void my_func(struct proc *p, struct cpu *c){

}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid, int signum)
{
     cprintf("inside kill\n");
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
        update_pending_signals(p, signum);
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING && signum==SIGKILL)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void handle_pending_signals(struct trapframe *tf){
//    cprintf("inside handle_pending_signals\n");
    struct proc* p = myproc();
    if(p==null) return;
//    cprintf("handle_pending_signals not null\n");
//    if ((tf->cs&3) != DPL_USER) return; // CPU in user mode
//    cprintf("handle_pending_signals not user\n");

//    cprintf("hi\n");
    // 1. backup process mask
    uint process_mask = p->signal_mask;
//    cprintf("hi1\n");
    uint signals = p->pending_signals;
//    cprintf("hi2\n");
//    cprintf("1inside handle_pending_signals, signals: %d, mask: %d\n", signals, process_mask);
    uint bit = 1;
    uint cont_found = p->pending_signals & (bit << SIGCONT);
    if (p->state == FROZEN){
        if (cont_found){
            do_default_action(SIGCONT, p);
        } else {
            yield();
        }
    } else {
//        cprintf("hi1\n");
        for (int i=0;i< SIGNALS_SIZE; i++){
//            cprintf("hi %d\n", i);
            // 2. override process mask with signal's mask
            uint signum = i;
            uint mask = p->signal_handlers[i].sigmask;
            uint active_signals = (~mask) & (signals);
            uint first = active_signals & bit;
//        cprintf("signum: %d, mask: %d, active: %d, first: %d\n", signum, mask, active_signals, first);
            if(first > 0){
                cprintf("signal!!\n");
                p->pending_signals = p->pending_signals & (bit << signum); // shutdown signal flag
                // 3. run handler
             cprintf("signum %d is active\n", signum);
                if((int)p->signal_handlers[signum].sa_handler == SIG_DFL){
                    // here we do all the default actions
                    do_default_action(signum, p);
                } else if((int)p->signal_handlers[signum].sa_handler == SIG_IGN){
                    // ignore
                } else {
                    handle_user_signal(signum, p);

                }
            }
            bit = bit << 1; // shift left by 1
        }
    }

    // 4. restore process mask
    p->signal_mask = process_mask;
}

void handle_pending_signals_kernel(struct proc *p){
//    cprintf("inside handle_pending_signals\n");
    if(p==null) return;
//    cprintf("handle_pending_signals not null\n");
//    if ((tf->cs&3) != DPL_USER) return; // CPU in user mode
//    cprintf("handle_pending_signals not user\n");

//    cprintf("hi\n");
    // 1. backup process mask
    uint process_mask = p->signal_mask;
//    cprintf("hi1\n");
    uint signals = p->pending_signals;
//    cprintf("hi2\n");
//    cprintf("1inside handle_pending_signals, signals: %d, mask: %d\n", signals, process_mask);
    uint bit = 1;
    uint cont_found = p->pending_signals & (bit << SIGCONT);
    if (p->state == FROZEN){
        if (cont_found){
            do_default_action(SIGCONT, p);
        } else {
            yield();
        }
    } else {
//        cprintf("hi1\n");
        for (int i=0;i< SIGNALS_SIZE; i++){
//            cprintf("hi %d\n", i);
            // 2. override process mask with signal's mask
            uint signum = i;
            uint mask = p->signal_handlers[i].sigmask;
            uint active_signals = (~mask) & (signals);
            uint first = active_signals & bit;
//        cprintf("signum: %d, mask: %d, active: %d, first: %d\n", signum, mask, active_signals, first);
            if(first > 0){
                cprintf("signal!!\n");
                p->pending_signals = p->pending_signals & (bit << signum); // shutdown signal flag
                // 3. run handler
                cprintf("signum %d is active\n", signum);
                if((int)p->signal_handlers[signum].sa_handler == SIG_DFL){
                    // here we do all the default actions
                    do_default_action(signum, p);
                } else if((int)p->signal_handlers[signum].sa_handler == SIG_IGN){
                    // ignore
                } else {
//                    handle_user_signal(signum, p);

                }
            }
            bit = bit << 1; // shift left by 1
        }
    }

    // 4. restore process mask
    p->signal_mask = process_mask;
}


void handle_user_signal(int signum, struct proc* p){
    *p->user_trapframe_backup = *p->tf;
    uint code_inject_size = (uint) sigret_syscall_finish - (uint) sigret_syscall;
    p->tf->esp -= code_inject_size; // make room for injected code
    memmove((void*)p->tf->esp, (void*)sigret_syscall, code_inject_size); // actual injection
    // push signum arg for the user sig_handler
    p->tf->esp -=4;
    *(int*)p->tf->esp = signum;
    // push return_adress for the user sig_handler func
    p->tf->esp -=4;
    *(int*)p->tf->esp = p->tf->esp+8; // start of injected code
    // set next instruction to be the sig_handler
    p->tf->eip = (uint) p->signal_handlers[signum].sa_handler;
}

void update_pending_signals(struct proc *p, int signum){
    uint bit = 1;
    for(int i=0; i < signum;i++){
        bit = bit << 1; // shift left by 1
    }
    p->pending_signals = p->pending_signals | bit;
     cprintf("updated p->pending_signals: %d\n", p->pending_signals);
}

void do_default_action(int signum, struct proc *p){
     cprintf("inside do_default_action, signum: %d, pid: %d\n", signum, p->pid);
    if (signum == SIGKILL){
        p->killed = 1;
    } else if(signum == SIGCONT){
        if(p->state == FROZEN)
            p->state = RUNNABLE;
    } else if (signum == SIGSTOP){
        p->state = FROZEN;
    } else {
        p->killed = 1;
    }
}

void sigret(void){
    *myproc()->tf = *myproc()->user_trapframe_backup;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie",
  [FROZEN]    "frozen"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// sets the proc's signal mask 
uint
sigprocmask(uint sigmask){
  acquire(&ptable.lock);
  uint oldmask = myproc()->signal_mask;
  myproc()->signal_mask = sigmask;
  release(&ptable.lock);
  return oldmask;
}

int 
set_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact){
  if(signum==SIGKILL || signum==SIGSTOP) return -1;
  acquire(&ptable.lock);
  // fill current sig action data to user struct pointed by old act
  if(oldact!=0){
    oldact = &myproc()->signal_handlers[signum];
  }
  // set the new sig action to the relevant signal
  myproc()->signal_handlers[signum].sigmask = act->sigmask;
  myproc()->signal_handlers[signum].sa_handler = act->sa_handler;

  release(&ptable.lock);
  return 0;
}

void ignoreHandler(int signum){

}