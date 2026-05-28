#include "foc_adc.h"
#include "TX32F01_periph.h"
#include "app_softvec.h"

static foc_adc_cb_t s_cb;
static uint16_t     s_ia_offset = FOC_ADC_MIDSCALE;
static uint16_t     s_ib_offset = FOC_ADC_MIDSCALE;

static void adc_eoc_isr(void)
{
    /* Sequence length=3 → DR1=Ia, DR2=Ib, DR3=Vbus. */
    foc_adc_sample_t s;
    s.ia   = (uint16_t)ADC_GetConversionValue(1);
    s.ib   = (uint16_t)ADC_GetConversionValue(2);
    s.vbus = (uint16_t)ADC_GetConversionValue(3);
    ADC_ClearFlag();
    if (s_cb) s_cb(&s);
}

static void cfg_pins(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_ADC,        ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO1,      ENABLE);
    SCU_Lock();

    GPIO_Init(FOC_IA_PORT,   FOC_IA_PIN,   GPIO_MODE_ANALOG);
    GPIO_Init(FOC_IB_PORT,   FOC_IB_PIN,   GPIO_MODE_ANALOG);
    GPIO_Init(FOC_VBUS_PORT, FOC_VBUS_PIN, GPIO_MODE_ANALOG);
}

int foc_adc_init(foc_adc_cb_t cb)
{
    ADC_InitTypeDef a;
    s_cb = cb;

    cfg_pins();
    ADC_DeInit();

    a.ADC_Vref            = ADC_VREF_VDD;            /* full-scale = Vdd */
    a.ADC_DataAlign       = ADC_ALIGN_Right;
    a.ADC_Sequence_Lenth  = ADC_Sequence_Lenth_3;    /* Ia, Ib, Vbus */
    a.ADC_SAMP_CLK        = ADC_SAMP_CLK_13;         /* short — we need to finish < 100 µs */
    a.ADC_CLK_DIV         = ADC_CLK_DIV_4;           /* 24 MHz / 4 = 6 MHz */
    ADC_Init(&a);

    ADC_ChannelConfig(1, FOC_IA_CH);
    ADC_ChannelConfig(2, FOC_IB_CH);
    ADC_ChannelConfig(3, FOC_VBUS_CH);

    /* TIM0 update event triggers the whole sequence. */
    ADC->CFGR = (ADC->CFGR & ~ADC_CFGR_TRGSEL_Msk)
              | (ADC_CFGR_TRGSEL_Timer0 << ADC_CFGR_TRGSEL_Pos);

    ADC_ITConfig(ENABLE);
    ADC_Cmd(ENABLE);

    /* Soft-vector registration so the BL trampoline forwards to us. */
    app_softvec_register_irq(ADC_IRQn, adc_eoc_isr);
    NVIC_SetPriority(ADC_IRQn, 0);
    NVIC_EnableIRQ(ADC_IRQn);

    return 0;
}

void foc_adc_calibrate_offset(void)
{
    /* PWM should be running at 50% center with outputs DISABLED before
     * calling this so the shunt amp sees zero current.  Average a few
     * sequences.
     *
     * Race fix: by the time we get here, foc_adc_init() has already
     * enabled the ADC IRQ AND TIM0 is auto-triggering conversions every
     * 100 µs. If we leave the IRQ on, adc_eoc_isr() clears the EOC flag
     * before our polling loop can see it → infinite spin. So we
     * temporarily mask the IRQ at the NVIC, drain any pending flag,
     * then poll cleanly. Restore IRQ before returning.
     */
    NVIC_DisableIRQ(ADC_IRQn);
    ADC_ClearFlag();

    uint32_t sa = 0, sb = 0;
    const uint32_t N = 64;
    for (uint32_t i = 0; i < N; ++i) {
        ADC_ClearFlag();
        ADC_StartConversion();
        /* Bounded spin: ADC at 6 MHz with 13-cycle sampling + a few
         * cycles of conversion = ~3 µs per conversion. 100 ms guard. */
        uint32_t guard = 24000UL * 100UL;       /* 24 MHz × 100 ms */
        while (!ADC_GetFlagStatus()) {
            if (--guard == 0) goto restore;     /* ADC hardware dead */
        }
        sa += ADC_GetConversionValue(1);
        sb += ADC_GetConversionValue(2);
    }
    s_ia_offset = (uint16_t)(sa / N);
    s_ib_offset = (uint16_t)(sb / N);

restore:
    ADC_ClearFlag();
    NVIC_ClearPendingIRQ(ADC_IRQn);
    NVIC_EnableIRQ(ADC_IRQn);
}

void foc_adc_get_offsets(uint16_t *ia0, uint16_t *ib0)
{
    if (ia0) *ia0 = s_ia_offset;
    if (ib0) *ib0 = s_ib_offset;
}
