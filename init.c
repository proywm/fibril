#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>
#include <signal.h>
#include <unistd.h>

#include "tls.h"
#include "page.h"
#include "safe.h"
#include "util.h"
#include "debug.h"
#include "deque.h"
#include "joint.h"
#include "sched.h"
#include "stack.h"
#include "shmap.h"
#include "fibrili.h"

char __data_start, _end;
char _fibril_tls_start, _fibril_tls_end;
tls_t __fibril_local__ _tls;

int     _nprocs;
int  *  _pids;

joint_t _joint;

static int * _tls_files;

static void globe_init(int nprocs)
{
  shmap_init(nprocs);

  void * dat_start = PAGE_ALIGN_DOWN(&__data_start);
  void * dat_end   = PAGE_ALIGN_UP(&_end);

  DEBUG_DUMP(2, "data", (dat_start, "%p"), (dat_end, "%p"));
  DEBUG_ASSERT(PAGE_ALIGNED(dat_start) && PAGE_ALIGNED(dat_end));

  shmap_copy(dat_start, dat_end - dat_start, "data");
}

static void tls_init(int id)
{
  char path[FILENAME_LIMIT];
  sprintf(path, "tls_%d", id);

  void * addr = &_fibril_tls_start;
  size_t size = (void *) &_fibril_tls_end - addr;

  DEBUG_ASSERT(PAGE_ALIGNED(addr));
  DEBUG_ASSERT(PAGE_DIVISIBLE(size));

  _tls_files[id] = shmap_copy(addr, size, path);

  barrier(_nprocs);

  DEQ.deqs = malloc(sizeof(tls_t *) * _nprocs);

  int i;
  for (i = 0; i < _nprocs; ++i) {
    if (i != id) {
      DEQ.deqs[i] = shmap_mmap(NULL, size, _tls_files[i]);
    }
  }

  if (id == 0) {
    _joint.stack.top = STACK_BOTTOM;
    _joint.stack.btm = STACK_BOTTOM;
    _joint.stptr = &_joint.stack;

    DEQ.jtptr = &_joint;
  }
}

static int child_main(void * id_)
{
  int id = (int) (size_t) id_;

  _tid = id;
  _pid = getpid();

  tls_init(id);
  stack_init_child(id);

  barrier(_nprocs);
  sched_work(id, _nprocs);
  return 0;
}

int fibril_init(int nprocs)
{
  _tid = 0;
  _pid = getpid();
  _nprocs = nprocs;
  _pids = malloc(sizeof(int) * nprocs);

  globe_init(nprocs);
  stack_init(nprocs);

  _tls_files = malloc(sizeof(int) * nprocs);

  /** Create workers. */
  int i;
  for (i = 1; i < nprocs; ++i) {
    SAFE_NNCALL(
        _pids[i] = clone(child_main, STACK_ADDRS[i],
          CLONE_FS | CLONE_FILES | CLONE_IO | SIGCHLD, (void *) (size_t) i)
    );
  }

  tls_init(0);
  barrier(nprocs);
  free(_tls_files);
  return 0;
}

