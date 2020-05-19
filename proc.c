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
pinit(void) {
    initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid() {
    return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// re sche duled between reading lapicid and running through the loop.
struct cpu *
mycpu(void) {
    int apicid, i;

    if (readeflags() & FL_IF)
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

// Disable interrupts so that we are not re sche duled
// while reading proc from the cpu structure
struct proc *
myproc(void) {
    struct cpu *c;
    struct proc *p;
    pushcli();
    c = mycpu();
    p = c->proc;
    popcli();
    return p;
}

int
allocpid(void) {
    int old_pid;
    do {
        old_pid = nextpid;
    } while (!cas(&nextpid, old_pid, old_pid + 1));
    return old_pid;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void) {
    struct proc *p;
    char *sp;
    pushcli();
    do {
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state == UNUSED)
                break;
        }
        // reached last proc with no match
        if (p == &ptable.proc[NPROC]) {
            popcli();
            return 0;
        }
    } while (!cas(&p->state, UNUSED, EMBRYO));
    popcli();

    p->pid = allocpid();

    // Allocate kernel stack.
    if ((p->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *p->tf;
    p->tf = (struct trapframe *) sp;

    // Set up new context to start executing at forkret,

    // which returns to trapret.
    sp -= 4;
    *(uint *) sp = (uint) trapret;

    sp -= sizeof *p->context;
    p->context = (struct context *) sp;
    memset(p->context, 0, sizeof *p->context);

    p->context->eip = (uint) forkret;


    // signals
    sp = p->kstack;
    p->user_trapframe_backup = (struct trapframe *) sp;

    for (int i = 0; i < SIGNALS_SIZE; i++) {
        p->signal_handlers[i].sa_handler = SIG_DFL;
        p->signal_handlers[i].sigmask = 0;
    }
    p->pending_signals = 0;
    p->signal_mask = 0;
    p->got_user_signal = 0;
    p->frozen = 0;
    p->sigcont_bit_is_up = 0;
    p->sigkill_bit_is_up = 0;

    return p;
}

//PAGEBREAK: 32
// Set up first user process.
void userinit(void) {
    struct proc *p;
    extern char _binary_initcode_start[], _binary_initcode_size[];
    p = allocproc();
    initproc = p;
    if ((p->pgdir = setupkvm()) == 0)
        panic("userinit: out of memory?");
    inituvm(p->pgdir, _binary_initcode_start, (int) _binary_initcode_size);
    p->sz = PGSIZE;
    memset(p->tf, 0, sizeof(*p->tf));
    p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
    p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
    p->tf->es = p->tf->ds;
    p->tf->ss = p->tf->ds;
    p->tf->eflags = FL_IF;
    p->tf->esp = PGSIZE;
    p->tf->eip = 0; // beginning of initcode.S

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    if (!cas(&p->state, EMBRYO, RUNNABLE)) panic("panica userinit");
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n) {
    uint sz;
    struct proc *curproc = myproc();

    sz = curproc->sz;
    if (n > 0) {
        if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    } else if (n < 0) {
        if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
            return -1;
    }
    curproc->sz = sz;
    switchuvm(curproc);
    return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void) {
    int i, pid;
    struct proc *np;
    struct proc *curproc = myproc();

    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }

    // Copy process state from proc.
    if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
        kfree(np->kstack);
        np->kstack = 0;
        np->state = UNUSED;
        return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;
    *np->tf = *curproc->tf;

    // signals
    for (int i = 0; i < SIGNALS_SIZE; i++) {
        np->signal_handlers[i].sa_handler = curproc->signal_handlers[i].sa_handler;
        np->signal_handlers[i].sigmask = curproc->signal_handlers[i].sigmask;
    }
    np->signal_mask = curproc->signal_mask;
    // signals

    // Clear %eax so that fork returns 0 in the child.
    np->tf->eax = 0;

    for (i = 0; i < NOFILE; i++)
        if (curproc->ofile[i])
            np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    pid = np->pid;


    cas(&np->state, EMBRYO, RUNNABLE);

    return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
    struct proc *curproc = myproc();
    struct proc *p;
    int fd;

    if (curproc == initproc)
        panic("init exiting");

    // Close all open files.
    for (fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd]) {
            fileclose(curproc->ofile[fd]);
            curproc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;

    pushcli();

    if (!cas(&curproc->state, RUNNING, -ZOMBIE))
        panic("cant cas from running to -zombie in exit");
    // Parent might be sleeping in wait().
    // wakeup1(curproc->parent); --> in  sch eduler

    // Pass abandoned children to init.
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->parent == curproc) {
            p->parent = initproc;
            if (p->state == ZOMBIE || p->state == -ZOMBIE) {
                while (p->state == -ZOMBIE) {}
                wakeup1(initproc);
            }
        }
    }

    // Jump into the sche duler, never to return.
    // curproc->state = ZOMBIE;
    // if (!cas(&curproc->state, -ZOMBIE, ZOMBIE))
    //   panic("cant cas from -zombie to zombie in exit"); --> in sche duler
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
    struct proc *p;
    int havekids, pid;
    struct proc *curproc = myproc();

    pushcli();
    for (;;) {
        // Go to sleep.
        if (!cas(&curproc->state, RUNNING, -SLEEPING)) { // protect the below logic of clearing zombies kids
            panic("wait: cant move from running to -sleeping in sleep");
        }
        curproc->chan = (void *) curproc;
        // Scan through table looking for exited children.
        havekids = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent != curproc)
                continue;
            havekids = 1;
            if (p->state == ZOMBIE || p->state == -ZOMBIE) {
                while (p->state == -ZOMBIE) {}
                if (cas(&p->state, ZOMBIE, _UNUSED)) {
                    // Found one.
                    pid = p->pid;
                    kfree(p->kstack);
                    p->kstack = 0;
                    freevm(p->pgdir);
                    p->pid = 0;
                    p->parent = 0;
                    p->name[0] = 0;
                    int killed;
                    do {
                        killed = p->killed;
                    } while (!cas(&p->killed, killed, 0));
                    curproc->chan = 0;
                    if (!cas(&curproc->state, -SLEEPING, RUNNING)) { // no running kids so no need to sleep
                        panic("cant cas from -SLEEPING to RUNNING in wait");
                    }
                    if (!cas(&p->state, _UNUSED, UNUSED)) {
                        panic("cant cas from -unuesed to unused in wait");
                    }
                    popcli();
                    return pid;
                } else panic("cant move from zombie to -unused in wait");
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || curproc->killed) {
            curproc->chan = 0;
            if (!cas(&curproc->state, -SLEEPING, RUNNING)) {
                panic("cant cas from -SLEEPING to RUNNING in wait");
            }
            popcli();
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
         local_sleep(curproc); //DOC: wait-sleep
    }
}

int sigblocked(struct proc *p, int signum) {
    uint bit = 1;
    bit <<= signum;
    return ((p->signal_mask & bit) > 0);
}

int sigignored(struct proc *p, int signum) {
    return (int) p->signal_handlers[signum].sa_handler == SIG_IGN;
}

//PAGEBREAK: 42
// Per-CPU process sche duler.
// Each CPU calls sche duler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the sche duler.
void scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;

    for (;;) {
        // Enable interrupts on this processor.
        sti();

        // Loop over process table looking for process to run.
        pushcli();
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (!p->frozen ||
                    (p->sigkill_bit_is_up || p->killed)  ||
                (p->frozen && p->sigcont_bit_is_up && !sigblocked(p, SIGCONT) && !sigignored(p, SIGCONT))
                    ) {
                if (cas(&p->state, RUNNABLE, RUNNING)) {
                    if (p->sigkill_bit_is_up) {

                        int killed;
                        do {
                            killed = p->killed;
                        } while (!cas(&p->killed, killed, 1));

                        uint sigkillbit;
                        do {
                            sigkillbit = p->sigkill_bit_is_up;
                        } while (!cas(&p->sigkill_bit_is_up, sigkillbit, 0));

                    }
                    // Switch to chosen process.  It is the process's job
                    // to release ptable.lock and then reacquire it
                    // before jumping back to us.
                    c->proc = p;
                    switchuvm(p);
                    swtch(&(c->scheduler), p->context);
                    switchkvm();
                    // Process is done running for now.
                    // It should have changed its p->state before coming back.
                    c->proc = 0;

                    // after call from yield
                    if (!cas(&p->state, -RUNNABLE, RUNNABLE)) {}

                    // after call from sleep or wait
                    if (cas(&p->state, -SLEEPING, SLEEPING)) {}

                    // from exit
                    if (cas(&p->state, -ZOMBIE, ZOMBIE)) {
                        wakeup1(p->parent);
                    }
                }
            }
        }
        popcli();
    }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
    int intena;
    struct proc *p = myproc();

    if (mycpu()->ncli != 1) {
        panic("sched locks");
    }
    if (p->state == RUNNING)
        panic("sched running");
    if (readeflags() & FL_IF)
        panic("sched interruptible");
    intena = mycpu()->intena;
    swtch(&p->context, mycpu()->scheduler);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
    pushcli();
    if (!cas(&myproc()->state, RUNNING, -RUNNABLE))
        panic("cant move from running to -runnable in yield");
    sched();
    popcli();
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
    static int first = 1;
    // Still holding ptable.lock from scheduler.
    popcli();

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
void sleep(void *chan, struct spinlock *lk) {
    struct proc *p = myproc();

    if (p == 0)
        panic("sleep");

    if (lk == 0)
        panic("sleep without lk");

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    pushcli();
    p->chan = chan;
    if (!cas(&p->state, RUNNING, -SLEEPING)) {
        panic("cant move from running to -sleeping in sleep");
    }
    if (lk != &ptable.lock) {
        // Go to sleep.
        release(lk);
    }

    sched();
    // Tidy up.

    // Reacquire original lock.
    if (lk != &ptable.lock) {
        acquire(lk);
    }
    popcli();
}

void local_sleep(void *chan) {

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.

    // p->state = SLEEPING;

    sched();
}


//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan) {
    struct proc *p;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if ((p->state == SLEEPING || p->state == -SLEEPING) && p->chan == chan) {
            while (p->state == -SLEEPING) {}
            if (cas(&p->state, SLEEPING, -RUNNABLE)) {
                p->chan = 0;
                if (!cas(&p->state, -RUNNABLE, RUNNABLE)) {
                    panic("cant move from -runnable to runnable ");
                }
            }
        }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
    pushcli();
    wakeup1(chan);
    popcli();
}

int set_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (signum == SIGKILL || signum == SIGSTOP) return -1;

    struct proc *p = myproc();
    if (oldact != null) {
        oldact->sa_handler = p->signal_handlers[signum].sa_handler;
        oldact->sigmask = p->signal_handlers[signum].sigmask;
    }

    p->signal_handlers[signum].sa_handler = act->sa_handler;
    p->signal_handlers[signum].sigmask = act->sigmask;

    return 0;
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid, int signum) {
    if (signum < 0 || signum > 31) return -1;

    struct proc *p;

    pushcli();
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            update_pending_signals(p, signum);
            popcli();
            return 0;
        }
    }
    popcli();
    return -1;
}

void update_pending_signals(struct proc *p, int signum) {
    uint bit = 1;
    bit = bit << signum;

    uint pending;
    do {
        pending = p->pending_signals;
    } while (!cas(&p->pending_signals, pending, pending | bit));

    uint sigcontbit;
    if (signum == SIGCONT) {

        do {
            sigcontbit = p->sigcont_bit_is_up;
        } while (!cas(&p->sigcont_bit_is_up, sigcontbit, 1));
    }
    uint sigkillbit;
    if (signum == SIGKILL 
        || ((int)p->signal_handlers[signum].sa_handler == SIG_DFL && signum!=SIGCONT && signum!=SIGSTOP)) {

        do {
            sigkillbit = p->sigkill_bit_is_up;
        } while (!cas(&p->sigkill_bit_is_up, sigkillbit, 1));

        if (p->state == SLEEPING || p->state == -SLEEPING) { // wakup for kill
            while (p->state == -SLEEPING) {}
            if (!cas(&p->state, SLEEPING, RUNNABLE)) {} // if not, wakeup was done by someone else
        }
    }
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void) {
    static char *states[] = {
            [UNUSED] "unused",
            [_UNUSED] "_UNUSED",
            [EMBRYO] "embryo",
            [SLEEPING] "sleep ",
            [RUNNABLE] "runble",
            [RUNNING] "run   ",
            [ZOMBIE] "zombie"};
    int i;
    struct proc *p;
    char *state;
    uint pc[10];

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("pid: %d state: %s pname: %s, forzen: %d, sigkillbit: %d, killed: %d     ",
                p->pid, state, p->name, p->frozen, p->sigkill_bit_is_up, p->killed);

        if (p->state == SLEEPING) {
            getcallerpcs((uint *) p->context->ebp + 2, pc);
            for (i = 0; i < 10 && pc[i] != 0; i++)
                cprintf(" %p", pc[i]);
        }
        cprintf("\n");
    }
}

int sigret(void) {
    struct proc *p = myproc();
    *p->tf = *p->user_trapframe_backup;
    p->signal_mask = p->user_mask_backup;
    p->got_user_signal = 0;

    return 0;
}

void handle_pending_signals(struct trapframe *tf) {
    struct proc *p = myproc();
    if (p == null) return;

    if (p->got_user_signal) return;
    if (p->sigkill_bit_is_up == 0 && (tf->cs & 3) != DPL_USER) return;
    if (p->pending_signals == 0) return;
    uint bit = 1;
    for (int signum = 0; signum < SIGNALS_SIZE; signum++) {
        if ((bit & p->pending_signals) > 0) {
            if ((((~p->signal_mask) & bit) > 0) || signum == SIGKILL || signum == SIGSTOP) {
                uint pending;
                do {
                    pending = p->pending_signals;
                } while (!cas(&p->pending_signals, pending, pending & (~bit)));

                if ((int) p->signal_handlers[signum].sa_handler == SIG_DFL) {
                    do_default_action(signum, p);
                } else if ((int) p->signal_handlers[signum].sa_handler == SIG_IGN) {
                    // ignore
                } else {
                    handle_user_signal(signum);
                }
            }
        }
        bit = bit << 1;
    }
}

void do_default_action(int signum, struct proc *p) {
    // kill is done in sch eduler
    if (signum == SIGCONT) {
        p->frozen = 0;
        uint sigcontbit;
        do {
            sigcontbit = p->sigcont_bit_is_up;
        } while (!cas(&p->sigcont_bit_is_up, sigcontbit, 0));

    } else if (signum == SIGSTOP) {
        p->frozen = 1;
        yield();
    }
}

void handle_user_signal(int signum) {
    struct proc *p = myproc();

    *p->user_trapframe_backup = *p->tf;
    p->user_mask_backup = p->signal_mask;
    p->signal_mask = p->signal_handlers[signum].sigmask;

    uint code_inject_size = (uint) sigret_syscall_finish - (uint) sigret_syscall;
    p->tf->esp -= code_inject_size;
    memmove((void *) p->tf->esp, sigret_syscall, code_inject_size);
    // push signum arg for the user sig_handler
    p->tf->esp -= 4;
    *(int *) p->tf->esp = signum;
    // push return_adress for the user sig_handler func
    p->tf->esp -= 4;
    *(int *) p->tf->esp = p->tf->esp + 8; // start of injected code
    // set next instruction to be the sig_handler
    p->tf->eip = (uint) p->signal_handlers[signum].sa_handler;

    p->got_user_signal = 1;
}
