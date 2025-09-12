#include <stdio.h>
#include <stdint.h>

#define F (1 << 14)
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

int int_to_fp(int n){
  return n * F;
}
int fp_to_int(int x){
  return x / F;
}
int fp_to_int_round(int x){
  if (x >= 0) return (x + F / 2) / F;
  else return (x - F / 2) / F;
}
int fp_add_fp(int x, int y){
  return x + y;
}
int fp_sub_fp(int x, int y){
  return x - y;
}
int int_add_fp(int x, int n){
  return x + n * F;
}
int int_sub_fp(int x, int n){
  return x - n * F;
}
int fp_mul_fp(int x, int y){
  return ((int64_t) x) * y / F;
}
int int_mul_fp(int x, int n){
  return x * n;
}
int fp_div_fp(int x, int y){
  return ((int64_t) x) * F / y;
}
int int_div_fp(int x, int n){
  return x / n;
}