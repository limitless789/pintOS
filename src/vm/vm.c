#include "vm.h"
#include "pagedir.h"
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"

struct list frame_table;
