#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(int argc, char *argv[])
{

  if(argc > 1){
    printf("Usage: %s do not need parameter\n", argv[0]);
    exit(1);
  }

  top();

  exit(0);
}
