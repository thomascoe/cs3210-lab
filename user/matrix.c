#include <inc/lib.h>

/*
 * This test program takes the matrix (row by row):
 * 1 2 3
 * 1 1 1
 * 2 2 2
 *
 * and multiplies it by the following matrix:
 * 1 2 3
 * 3 4 3
 * 1 1 2
 *
 * Which should produce the following result:
 * 10 13 15
 * 5 7 8
 * 10 14 16
 *
 * By calling multiply() from the parent with another 3 numbers, you can
 * effectively add rows to the first matrix in the multiplication operation
 */

int ids[3][3]; // proc IDs of all center children

// Ran by the children. Continuously loops, receiving new x y or z values, as
// well as new values from the northern process
void
run_center(int a)
{
  envid_t north;
  envid_t south;

  north = ipc_recv(0, 0, 0);
  south = ipc_recv(0, 0, 0);
  while (1) {
    int west_val = ipc_recv(0,0,0); // Receive value from parent
    int north_val;
    if (north == -1) {
      north_val = 0;
    } else {
      north_val = ipc_recv(0,0,0); // receive value from north process
    }
    int sum = north_val + west_val * a;
    ipc_send(south, sum, 0, 0); // Send the sum southward
  }

}

// Should only be called by the parent
// Sends x y and z to the children, so they they can comput another row of the answer
void
multiply(int x, int y, int z)
{
  int i, j;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      if (i == 0) {
        ipc_send(ids[i][j], x, 0, 0);
      } else if (i == 1) {
        ipc_send(ids[i][j], y, 0, 0);
      } else if (i == 2) {
        ipc_send(ids[i][j], z, 0, 0);
      }
    }
  }

  // Receive the 3 results
  int res0 = 0;
  int res1 = 0;
  int res2 = 0;
  for (i = 0; i < 3; i++) {
    envid_t src;
    int res = ipc_recv(&src, 0, 0);
    if (src == ids[2][0]) {
      res0 = res;
    } else if (src == ids[2][1]) {
      res1 = res;
    } else if (src == ids[2][2]) {
      res2 = res;
    } else {
      cprintf("Unknown: %d: %d\n", src, res);
    }
  }
  cprintf("Answer row: [%d %d %d]\n", res0, res1, res2);
}

void
umain(int argc, char **argv)
{
  cprintf("Matrix Multiplier\n");
  cprintf("Multiplying the following:\n");
  cprintf("1 2 3   1 2 3\n");
  cprintf("1 1 1 x 3 4 3\n");
  cprintf("2 2 2   1 1 2\n\n");
  int i, j;

  int matrix[3][3] = { // "A" Matrix
    {1,2,3},
    {3,4,3},
    {1,1,2}
  };

  // Fork the processes
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      if ((ids[i][j] = fork()) < 0)
        panic("fork: %e", ids[i][j]);
      if (ids[i][j] == 0) { // We are the child
        run_center(matrix[i][j]);
      }
    }
  }

  // Send each process their north and south envids
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      envid_t north;
      envid_t south;
      // Set north
      if (i == 0) {
        north = -1; // If we are at the top, there is no north child
      } else {
        north = ids[i-1][j];
      }
      // set south
      if (i == 2) {
        south = thisenv->env_id; // if we are at the bottom, set parent as south
      } else {
        south = ids[i+1][j];
      }
      // Send north, south envids
      ipc_send(ids[i][j], north, 0, 0);
      ipc_send(ids[i][j], south, 0, 0);
    }
  }

  // Multiply A by the row [1 2 3]
  multiply(1, 2, 3);

  // Multiply A by the row [1 1 1]
  multiply(1, 1, 1);

  // Multiply A by the row [2 2 2]
  multiply(2, 2, 2);

}
