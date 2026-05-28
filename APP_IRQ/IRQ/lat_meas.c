#include "lat_meas.h"
#include "TX32F01_periph.h"
#include <string.h>

static uint32_t      s_rvr;
static lat_stats_t   s_st;

void lat_meas_init(uint32_t rvr)
{
    s_rvr = rvr;
    memset(&s_st, 0, sizeof(s_st));
    s_st.min = 0xFFFFFFFFU;
}

void lat_meas_reset(void)
{
    uint32_t rvr = s_rvr;
    __disable_irq();
    memset(&s_st, 0, sizeof(s_st));
    s_st.min = 0xFFFFFFFFU;
    s_rvr = rvr;
    __enable_irq();
}

/* MUST be the first statement of the SysTick handler.
 * Keeping the SYST_CVR read inline so the compiler emits exactly two LDRs
 * (literal pool address, then load) — that's ~4 cycles after the handler
 * prologue, which is the irreducible measurement overhead. */
void lat_meas_record(void)
{
    uint32_t cvr = SysTick->VAL;
    uint32_t lat = s_rvr - cvr;       /* RVR - CVR == cycles since underflow */

    s_st.last  = lat;
    s_st.count++;
    if (lat < s_st.min) s_st.min = lat;
    if (lat > s_st.max) s_st.max = lat;
    s_st.sum += lat;

    uint32_t bin = lat / LAT_BUCKET_WIDTH;
    if (bin >= LAT_BUCKETS) s_st.outliers++;
    else                    s_st.bucket[bin]++;
}

void lat_meas_snapshot(lat_stats_t *out)
{
    __disable_irq();
    *out = s_st;
    __enable_irq();
}
