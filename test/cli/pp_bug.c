#include <stdio.h>

#ifndef N
#define N 100
#endif

int main() {

int x,y;
double a[N][N];

for (x=0; x<N; x++)
  for (y=0; y<N; y++)
    a[x][y]=x+y*1.0;
 
#pragma scop    
for (x=0; x<N; x++)
  for (y=0; y<N; y++)
    a[x][y]+=1.0;
#pragma endscop

return(1);
}
