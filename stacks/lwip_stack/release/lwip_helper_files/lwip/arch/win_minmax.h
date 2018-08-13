/**
 * lib/minmax.c: windowed min/max tracker by Kathleen Nichols.
 *
 */
#ifndef MINMAX_H
#define MINMAX_H

#include  "types.h"
#include "rtp_branch_prediction.h"

/* A single data point for our parameterized min-max tracker */
struct minmax_sample
{
  u32_t t;                      /* time measurement was taken */
  u32_t v;                      /* value measured */
};

/* State for the parameterized min-max tracker */
struct minmax
{
  struct minmax_sample s[3];
};

static inline u32_t
minmax_get (const struct minmax *m)
{
  return m->s[0].v;
}

static inline u32_t
minmax_reset (struct minmax *m, u32_t t, u32_t meas)
{
  struct minmax_sample val = {.t = t,.v = meas };

  m->s[2] = m->s[1] = m->s[0] = val;
  return m->s[0].v;
}

/* As time advances, update the 1st, 2nd, and 3rd choices. */
static u32_t
minmax_subwin_update (struct minmax *m, u32_t win,
                      const struct minmax_sample *val)
{
  u32_t dt = val->t - m->s[0].t;

  if (unlikely (dt > win))
    {
      /*
       * Passed entire window without a new val so make 2nd
       * choice the new val & 3rd choice the new 2nd choice.
       * we may have to iterate this since our 2nd choice
       * may also be outside the window (we checked on entry
       * that the third choice was in the window).
       */
      m->s[0] = m->s[1];
      m->s[1] = m->s[2];
      m->s[2] = *val;
      if (unlikely (val->t - m->s[0].t > win))
        {
          m->s[0] = m->s[1];
          m->s[1] = m->s[2];
          m->s[2] = *val;
        }
    }
  else if (unlikely (m->s[1].t == m->s[0].t) && dt > win / 4)
    {
      /*
       * We've passed a quarter of the window without a new val
       * so take a 2nd choice from the 2nd quarter of the window.
       */
      m->s[2] = m->s[1] = *val;
    }
  else if (unlikely (m->s[2].t == m->s[1].t) && dt > win / 2)
    {
      /*
       * We've passed half the window without finding a new val
       * so take a 3rd choice from the last half of the window
       */
      m->s[2] = *val;
    }
  return m->s[0].v;
}

/* Check if new measurement updates the 1st, 2nd or 3rd choice max. */
static inline u32_t
minmax_running_max (struct minmax *m, u32_t win, u32_t t, u32_t meas)
{
  struct minmax_sample val = {.t = t,.v = meas };

  if (unlikely (val.v >= m->s[0].v) ||  /* found new max? */
      unlikely (val.t - m->s[2].t > win))       /* nothing left in window? */
    return minmax_reset (m, t, meas);   /* forget earlier samples */

  if (unlikely (val.v >= m->s[1].v))
    m->s[2] = m->s[1] = val;
  else if (unlikely (val.v >= m->s[2].v))
    m->s[2] = val;

  return minmax_subwin_update (m, win, &val);
}

/* Check if new measurement updates the 1st, 2nd or 3rd choice min. */
static inline u32_t
minmax_running_min (struct minmax *m, u32_t win, u32_t t, u32_t meas)
{
  struct minmax_sample val = {.t = t,.v = meas };

  if (unlikely (val.v <= m->s[0].v) ||  /* found new min? */
      unlikely (val.t - m->s[2].t > win))       /* nothing left in window? */
    return minmax_reset (m, t, meas);   /* forget earlier samples */

  if (unlikely (val.v <= m->s[1].v))
    m->s[2] = m->s[1] = val;
  else if (unlikely (val.v <= m->s[2].v))
    m->s[2] = val;

  return minmax_subwin_update (m, win, &val);
}

#endif
