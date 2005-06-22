/* $Id: complex_vector.c,v 1.1 2005-06-22 07:11:19 acs Exp $
   Written by Adam Siepel, Summer 2005
   Copyright 2005, Adam Siepel, University of California
*/

/** \file complex_vector.c
    Vectors of complex numbers
    \ingroup base
*/

#include <misc.h>
#include <complex_vector.h> 
#include <assert.h>



Zvector *zvec_new(int size) {
  Zvector *v = smalloc(sizeof(Zvector));
  v->data = smalloc(size * sizeof(Complex));
  v->size = size;
  return v;
}

void zvec_free(Zvector *v) {
  free(v->data);
  free(v);
}

Complex zvec_get(Zvector *v, int i) { /* check */
  return v->data[i];
}

void zvec_set(Zvector *v, int i, Complex val) {
  v->data[i] = val;
}

void zvec_set_all(Zvector *v, Complex val) {
  int i;
  for (i = 0; i < v->size; i++)
    v->data[i] = val;
}

void zvec_copy(Zvector *dest, Zvector *src) {
  int i;
  assert(dest->size == src->size);
  for (i = 0; i < src->size; i++) 
    dest->data[i] = src->data[i];
}

Zvector* zvec_create_copy(Zvector *src) {
  Zvector *copy = zvec_new(src->size);
  zvec_copy(copy, src);
  return copy;
}

void zvec_print(Zvector *v, FILE *F) {
  int i;
  for (i = 0; i < v->size; i++)
    fprintf(F, "%f %f ", v->data[i].x, v->data[i].y);
  fprintf(F, "\n");
}

void zvec_read(Zvector *v, FILE *F) {
  int i;
  for (i = 0; i < v->size; i++)
    fscanf(F, "%lf %lf ", &(v->data[i].x), &(v->data[i].y));
}

Zvector *zvec_new_from_file(FILE *F, int size) {
  Zvector *v = zvec_new(size);
  zvec_read(v, F);
  return v;
}

void zvec_zero(Zvector *v) {
  int i;
  for (i = 0; i < v->size; i++) 
    v->data[i] = z_set(0, 0);
}

void zvec_plus_eq(Zvector *thisv, Zvector *addv) {
  int i;
  assert(thisv->size == addv->size);
  for (i = 0; i < thisv->size; i++) 
    thisv->data[i] = z_add(thisv->data[i], addv->data[i]);  
}

void zvec_minus_eq(Zvector *thisv, Zvector *subv) {
  int i;
  assert(thisv->size == subv->size);
  for (i = 0; i < thisv->size; i++) 
    thisv->data[i] = z_sub(thisv->data[i], subv->data[i]);  
}

void zvec_scale(Zvector *v, double scale_factor) {
  int i;
  for (i = 0; i < v->size; i++) 
    v->data[i] = z_mul_real(v->data[i], scale_factor);
}

