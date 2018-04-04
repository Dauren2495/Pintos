#include <syscall.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);

void
_start (int argc, char *argv[]) 
{
  printf("arguments are not passed\n");
  printf("Arguments passed: argc is %d argv is %s\n",argc, *argv);
  printf("Nothing printed??\n");
  exit (main (argc, argv));
}
