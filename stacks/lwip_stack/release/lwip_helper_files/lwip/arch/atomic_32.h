/*
 * atomic_32.h
 *
 */

#ifndef ATOMIC_32_H_
#define ATOMIC_32_H_

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    volatile int counter;
  } atomic_t;

#define ATOMIC_INIT(i) {(i)}
#define atomic_read(v) (*(volatile int *)&(v)->counter)
#define atomic_set(v, i) (((v)->counter) = (i))

  static inline void atomic_add (int i, atomic_t * v)
  {
    __asm__ __volatile__ (
                           /*LOCK_PREFIX "addl %1,%0" */
                           "addl %1,%0":"+m" (v->counter):"ir" (i));
  }

  static inline void atomic_sub (int i, atomic_t * v)
  {
    __asm__ __volatile__ (
                           /*LOCK_PREFIX "subl %1,%0" */
                           "subl %1,%0":"+m" (v->counter):"ir" (i));
  }

  static __inline__ void atomic_inc (atomic_t * v)
  {
    __asm__ __volatile__ (
                           /*LOCK_PREFIX "incl %0" */
                           "incl %0":"+m" (v->counter));
  }

  static __inline__ void atomic_dec (atomic_t * v)
  {
    __asm__ __volatile__ (
                           /*LOCK_PREFIX "decl %0" */
                           "decl %0":"+m" (v->counter));
  }
#ifdef __cplusplus
}
#endif
#endif /* ATOMIC_32_H_ */
