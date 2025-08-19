#define LOCAL_LOG_LEVEL LOG_NONE

#include <stdint.h>
#include "adc_get.h"
#include "log.h"
#include "periph/gpio.h"
#include "periph_conf.h"
#include "mutex.h"
#include "ztimer.h"

#ifndef ADC_GCLK_SRC
    #define ADC_GCLK_SRC SAM0_GCLK_MAIN
#endif

#ifndef ADC_GAIN_FACTOR_DEFAULT
    #define ADC_GAIN_FACTOR_DEFAULT (0)
#endif

/* Prototypes */
static void _adc_poweroff(Adc* dev);
static void _setup_clock(Adc* dev);
static void _setup_calibration(Adc* dev);

static mutex_t _lock[] = { MUTEX_INIT, MUTEX_INIT };  // for ADC0, ADC1

static inline void _prep(adc_t adc)
{
    mutex_lock(&_lock[adc]);
}

static inline void _done(adc_t adc)
{
    mutex_unlock(&_lock[adc]);
}

static inline void _wait_syncbusy(Adc* dev)
{
#ifdef ADC_STATUS_SYNCBUSY
    while (dev->STATUS.reg & ADC_STATUS_SYNCBUSY) {}
#else
    /* Ignore the ADC SYNCBUSY.SWTRIG status
     * The ADC SYNCBUSY.SWTRIG gets stuck to '1' after wake-up from Standby Sleep mode.
     * SAMD5x/SAME5x errata: DS80000748 (page 10)
     */
    while (dev->SYNCBUSY.reg & ~ADC_SYNCBUSY_SWTRIG) {}
#endif
}

static void _adc_poweroff(Adc* dev)
{
    _wait_syncbusy(dev);

    /* Disable */
    dev->CTRLA.reg &= ~ADC_CTRLA_ENABLE;
    _wait_syncbusy(dev);

    /* Disable bandgap */
#ifdef SYSCTRL_VREF_BGOUTEN
    if (ADC_REF_DEFAULT == ADC_REFCTRL_REFSEL_INT1V) {
        SYSCTRL->VREF.reg &= ~SYSCTRL_VREF_BGOUTEN;
    }
#else
    if (ADC_REF_DEFAULT == ADC_REFCTRL_REFSEL_INTREF) {
        SUPC->VREF.reg &= ~SUPC_VREF_VREFOE;
    }
#endif
}

static void _setup_clock(Adc* dev)
{
    /* Enable gclk in case we are the only user */
    sam0_gclk_enable(ADC_GCLK_SRC);

#ifdef PM_APBCMASK_ADC
    /* Power On */
    PM->APBCMASK.reg |= PM_APBCMASK_ADC;
    /* GCLK Setup */
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN
                      | GCLK_CLKCTRL_GEN(ADC_GCLK_SRC)
                      | GCLK_CLKCTRL_ID(ADC_GCLK_ID);
    /* Configure prescaler */
    dev->CTRLB.reg = ADC_PRESCALER;
#else
    /* Power on */
    #ifdef MCLK_APBCMASK_ADC
        MCLK->APBCMASK.reg |= MCLK_APBCMASK_ADC;
    #else
        #ifdef MCLK_APBDMASK_ADC0
            if (dev == ADC0) {
                MCLK->APBDMASK.reg |= MCLK_APBDMASK_ADC0;
            } else {
                MCLK->APBDMASK.reg |= MCLK_APBDMASK_ADC1;
            }
        #else
            MCLK->APBDMASK.reg |= MCLK_APBDMASK_ADC;
        #endif
    #endif

    #ifdef ADC0_GCLK_ID
        /* GCLK Setup */
        if (dev == ADC0) {
            GCLK->PCHCTRL[ADC0_GCLK_ID].reg = GCLK_PCHCTRL_CHEN
                    | GCLK_PCHCTRL_GEN(ADC_GCLK_SRC);
        }
        else {
            GCLK->PCHCTRL[ADC1_GCLK_ID].reg = GCLK_PCHCTRL_CHEN
                    | GCLK_PCHCTRL_GEN(ADC_GCLK_SRC);
        }
        /* Configure prescaler */
        dev->CTRLA.reg = ADC_PRESCALER;
    #else
        /* GCLK Setup */
        GCLK->PCHCTRL[ADC_GCLK_ID].reg = GCLK_PCHCTRL_CHEN
                | GCLK_PCHCTRL_GEN(ADC_GCLK_SRC);
        /* Configure prescaler */
        dev->CTRLB.reg = ADC_PRESCALER;
    #endif
#endif
}

static void _setup_calibration(Adc* dev)
{
#ifdef ADC_CALIB_BIAS_CAL
    /* Load the fixed device calibration constants */
    dev->CALIB.reg =
        ADC_CALIB_BIAS_CAL((*(uint32_t*)ADC_FUSES_BIASCAL_ADDR >>
                            ADC_FUSES_BIASCAL_Pos)) |
        ADC_CALIB_LINEARITY_CAL((*(uint64_t*)ADC_FUSES_LINEARITY_0_ADDR >>
                                ADC_FUSES_LINEARITY_0_Pos));
#else
    /* Set default calibration from NVM */
    #ifdef ADC0_FUSES_BIASCOMP_ADDR
        if (dev == ADC0) {
            dev->CALIB.reg =
                ADC0_FUSES_BIASCOMP((*(uint32_t*)ADC0_FUSES_BIASCOMP_ADDR)) >>
                ADC_CALIB_BIASCOMP_Pos |
                ADC0_FUSES_BIASREFBUF((*(uint32_t*)ADC0_FUSES_BIASREFBUF_ADDR) >>
                ADC0_FUSES_BIASREFBUF_Pos);
        }
        else {
            dev->CALIB.reg =
                ADC1_FUSES_BIASCOMP((*(uint32_t*)ADC1_FUSES_BIASCOMP_ADDR)) >>
                ADC_CALIB_BIASCOMP_Pos |
                ADC1_FUSES_BIASREFBUF((*(uint32_t*)ADC1_FUSES_BIASREFBUF_ADDR) >>
                ADC1_FUSES_BIASREFBUF_Pos);
        }
    #else
        dev->CALIB.reg =
                ADC_FUSES_BIASCOMP((*(uint32_t*)ADC_FUSES_BIASCOMP_ADDR)) >>
                ADC_CALIB_BIASCOMP_Pos |
                ADC_FUSES_BIASREFBUF((*(uint32_t*)ADC_FUSES_BIASREFBUF_ADDR) >>
                ADC_FUSES_BIASREFBUF_Pos);
    #endif
#endif
}

int adc_configure(Adc* dev, adc_res_t res)
{
    /* Individual comparison necessary because ADC Resolution Bits are not
     * numerically in order and 16Bit (averaging - not currently supported)
     * falls between 12bit and 10bit.  See datasheet for details */
    if (!((res == ADC_RES_8BIT) || (res == ADC_RES_10BIT) ||
          (res == ADC_RES_12BIT))){
        LOG_ERROR("adc: invalid resolution\n");
        return -1;
    }

    _adc_poweroff(dev);

    if (dev->CTRLA.reg & ADC_CTRLA_SWRST ||
        dev->CTRLA.reg & ADC_CTRLA_ENABLE ) {
        LOG_ERROR("adc: not ready\n");
        return -1;
    }

    _setup_clock(dev);
    _setup_calibration(dev);

    /* Set ADC resolution */
#ifdef ADC_CTRLC_RESSEL
    /* Reset resolution bits in CTRLC */
    dev->CTRLC.reg &= ~ADC_CTRLC_RESSEL_Msk;
    dev->CTRLC.reg |= res;
#else
    /* Reset resolution bits in CTRLB */
    dev->CTRLB.reg &= ~ADC_CTRLB_RESSEL_Msk;
    dev->CTRLB.reg |= res;
#endif

    /* Set Voltage Reference */
    dev->REFCTRL.reg = ADC_REF_DEFAULT;
    /* Disable all interrupts */
    dev->INTENCLR.reg = 0xFF;

#ifdef SYSCTRL_VREF_BGOUTEN
    /* Enable bandgap if VREF is internal 1V */
    if (ADC_REF_DEFAULT == ADC_REFCTRL_REFSEL_INT1V) {
        SYSCTRL->VREF.reg |= SYSCTRL_VREF_BGOUTEN;
    }
#else
    /* Enable bandgap if necessary */
    if (ADC_REF_DEFAULT == ADC_REFCTRL_REFSEL_INTREF) {
        SUPC->VREF.reg |= SUPC_VREF_VREFOE;
    }
#endif
#ifdef ADC_REFCTRL_REFSEL_AREFA
    if (ADC_REF_DEFAULT == ADC_REFCTRL_REFSEL_AREFA) {
        gpio_init_mux(ADC_REFSEL_AREFA_PIN, GPIO_MUX_B);
    }
#endif
#ifdef ADC_REFCTRL_REFSEL_AREFB
    if (ADC_REF_DEFAULT == ADC_REFCTRL_REFSEL_AREFB) {
        gpio_init_mux(ADC_REFSEL_AREFB_PIN, GPIO_MUX_B);
    }
#endif
#ifdef ADC_REFCTRL_REFSEL_AREFC
    if (ADC_REF_DEFAULT == ADC_REFCTRL_REFSEL_AREFC) {
        gpio_init_mux(ADC_REFSEL_AREFC_PIN, GPIO_MUX_B);
    }
#endif

#ifdef ADC_SAMPLELEN
    // Settling: Sampling Time Length: 1-63, 1 ADC CLK per
    dev->SAMPCTRL.reg = ADC_SAMPLELEN;
#endif

#ifdef ADC_SAMPLENUM
    // Averaging while measuring ADC_SAMPLENUM samples
    uint8_t adc_avgctrl_adjres;
    switch ( ADC_SAMPLENUM ) {
        case ADC_AVGCTRL_SAMPLENUM_1:
            adc_avgctrl_adjres = ADC_AVGCTRL_ADJRES(0);
            break;
        case ADC_AVGCTRL_SAMPLENUM_2:
            adc_avgctrl_adjres = ADC_AVGCTRL_ADJRES(1);
            break;
        case ADC_AVGCTRL_SAMPLENUM_4:
            adc_avgctrl_adjres = ADC_AVGCTRL_ADJRES(2);
            break;
        case ADC_AVGCTRL_SAMPLENUM_8:
            adc_avgctrl_adjres = ADC_AVGCTRL_ADJRES(3);
            break;
        default:
            adc_avgctrl_adjres = ADC_AVGCTRL_ADJRES(4);
            break;
    }
    dev->AVGCTRL.reg = ADC_SAMPLENUM | adc_avgctrl_adjres;
#endif

#ifdef ADC0
    /* The SAMD5x/SAME5x family has two ADCs: ADC0 and ADC1. */
    const adc_t adc = (dev != ADC0);
#else
    const adc_t adc = 0;
#endif

#ifdef ADC0_IRQ
    if ( adc == 0 )
        NVIC_EnableIRQ(ADC0_IRQ);
#ifdef ADC1_IRQ
    else
        NVIC_EnableIRQ(ADC1_IRQ);
#endif
#endif

    /* Enable ADC Module */
    dev->CTRLA.reg |= ADC_CTRLA_ENABLE;
    _wait_syncbusy(dev);

    // It seems that it needs additional delay even after SYNCBUSY.ENABLE gets cleared.
    // See Table 54-32. Analog Comparator Characteristics
    //   Tstart (Startup time): 4.7 - 7.5 us.
    ztimer_sleep(ZTIMER_USEC, 5);

    return 0;
}

int adc_init(adc_t line)
{
    if (line >= ADC_NUMOF) {
        LOG_ERROR("adc: line arg not applicable\n");
        return -1;
    }

#ifdef ADC0
    const adc_t adc = (adc_channels[line].dev != ADC0);
#else
    const adc_t adc = 0;
#endif

    _prep(adc);

    uint8_t muxpos = (adc_channels[line].inputctrl & ADC_INPUTCTRL_MUXPOS_Msk)
                   >> ADC_INPUTCTRL_MUXPOS_Pos;
    uint8_t muxneg = (adc_channels[line].inputctrl & ADC_INPUTCTRL_MUXNEG_Msk)
                   >> ADC_INPUTCTRL_MUXNEG_Pos;

    /* configure positive input pin */
    if (muxpos < 0x18) {
        assert( muxpos < ARRAY_SIZE(sam0_adc_pins[adc]) );
        gpio_init(sam0_adc_pins[adc][muxpos], GPIO_IN);
        gpio_init_mux(sam0_adc_pins[adc][muxpos], GPIO_MUX_B);
    }

    /* configure negative input pin */
    if (adc_channels[line].inputctrl & ADC_INPUTCTRL_DIFFMODE) {
        assert( muxneg < ARRAY_SIZE(sam0_adc_pins[adc]) );
        gpio_init(sam0_adc_pins[adc][muxneg], GPIO_IN);
        gpio_init_mux(sam0_adc_pins[adc][muxneg], GPIO_MUX_B);
    }

    _done(adc);

    return 0;
}

// The SAMD5x/SAME5x family has two ADCs, ADC0 and ADC1. The two inputs can be sampled
// simultaneously, as each ADC includes sample and hold circuits.
static struct {
    void (*callback)(void*, uint16_t);
    void* arg;
} _isr[2];  // for ADC0, ADC1

int32_t adc_get(adc_t line, void (*callback)(void*, uint16_t), void* arg)
{
    if ( line >= ADC_NUMOF ) {
        LOG_ERROR("adc: line arg not applicable\n");
        return -1;
    }

#ifdef ADC0
    Adc* const dev = adc_channels[line].dev;
    const adc_t adc = (dev != ADC0);
#else
    Adc* const dev = ADC;
    const adc_t adc = 0;
#endif

    int result = 0;  // Would return 0 for interrupt-driven measurement.

    _prep(adc);  // Wait if there is a measurement in progress.

    const uint16_t adc_inputctrl = ADC_GAIN_FACTOR_DEFAULT
                                 | adc_channels[line].inputctrl
                                 | ADC_NEG_INPUT;
    if ( dev->INPUTCTRL.reg != adc_inputctrl )
        dev->INPUTCTRL.reg = adc_inputctrl;

    if ( callback == NULL ) {
        dev->INTFLAG.reg |= ADC_INTFLAG_RESRDY;  // Clear the interrup flag if set
        _wait_syncbusy(dev);
        dev->SWTRIG.bit.START = 1;
        while ( !dev->INTFLAG.bit.RESRDY ) {}
        result = dev->RESULT.reg;
        _done(adc);
    } else {
        // Enable the Result Ready Interrupt if p_isr_cb is provided.
        dev->INTFLAG.reg |= ADC_INTFLAG_RESRDY;
        dev->INTENSET.bit.RESRDY = 1;
        _isr[adc].callback = callback;
        _isr[adc].arg = arg;
        _wait_syncbusy(dev);
        dev->SWTRIG.bit.START = 1;
    }

    return result;
}

#ifdef ADC0_ISR
void ADC0_ISR(void)
{
    if ( ADC0->INTFLAG.bit.RESRDY ) {
        ADC0->INTENCLR.bit.RESRDY = 1;  // Disable interrupt until adc_get() enables again.
        const uint16_t result = ADC0->RESULT.reg;  // Will also clear the interrupt flag.

        if ( _isr[0].callback != NULL )
            _isr[0].callback(_isr[0].arg, result);
    }

    _done(0);
    cortexm_isr_end();
}
#endif

#ifdef ADC1_ISR
void ADC1_ISR(void)
{
    if ( ADC1->INTFLAG.bit.RESRDY ) {
        ADC1->INTENCLR.bit.RESRDY = 1;
        const uint16_t result = ADC1->RESULT.reg;

        if ( _isr[1].callback != NULL )
            _isr[1].callback(_isr[1].arg, result);
    }

    _done(1);
    cortexm_isr_end();
}
#endif
