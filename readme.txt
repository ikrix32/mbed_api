- update system_nrf51.c to automatically start low frequency clock
- c files should be compiled with --c99
- rename ble.h to nrf_ble.h

static void init_clock(void);

#if defined ( __CC_ARM )
    uint32_t SystemCoreClock __attribute__((used)) = __SYSTEM_CLOCK;  
#elif defined ( __ICCARM__ )
    __root uint32_t SystemCoreClock = __SYSTEM_CLOCK;
#elif defined   ( __GNUC__ )
    uint32_t SystemCoreClock __attribute__((used)) = __SYSTEM_CLOCK;
#endif

void SystemCoreClockUpdate(void)
{
    SystemCoreClock = __SYSTEM_CLOCK;
}

void SystemInit(void)
{
    /* If desired, switch off the unused RAM to lower consumption by the use of RAMON register.
       It can also be done in the application main() function. */

    /* Prepare the peripherals for use as indicated by the PAN 26 "System: Manual setup is required
       to enable the use of peripherals" found at Product Anomaly document for your device found at
       https://www.nordicsemi.com/. The side effect of executing these instructions in the devices 
       that do not need it is that the new peripherals in the second generation devices (LPCOMP for
       example) will not be available. */
    if (is_manual_peripheral_setup_needed())
    {
        *(uint32_t volatile *)0x40000504 = 0xC007FFDF;
        *(uint32_t volatile *)0x40006C18 = 0x00008000;
    }
    
    /* Disable PROTENSET registers under debug, as indicated by PAN 59 "MPU: Reset value of DISABLEINDEBUG
       register is incorrect" found at Product Anomaly document four your device found at 
       https://www.nordicsemi.com/. There is no side effect of using these instruction if not needed. */
    if (is_disabled_in_debug_needed())
    {
        NRF_MPU->DISABLEINDEBUG = MPU_DISABLEINDEBUG_DISABLEINDEBUG_Disabled << MPU_DISABLEINDEBUG_DISABLEINDEBUG_Pos;
    }

    // Start the external 32khz crystal oscillator.
    init_clock();
}

void init_clock(void)
{
    /* For compatibility purpose, the default behaviour is to first attempt to initialise an
       external clock, and after a timeout, use the internal RC one. To avoid this wait, boards that
       don't have an external oscillator can set TARGET_NRF_LFCLK_RC directly. */
    uint32_t i = 0;
    const uint32_t polling_period = 200;
    const uint32_t timeout = 1000000;

#if defined(TARGET_NRF_LFCLK_RC)
    NRF_CLOCK->LFCLKSRC             = (CLOCK_LFCLKSRC_SRC_RC << CLOCK_LFCLKSRC_SRC_Pos);
#else
    NRF_CLOCK->LFCLKSRC             = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
#endif
    NRF_CLOCK->EVENTS_LFCLKSTARTED  = 0;
    NRF_CLOCK->TASKS_LFCLKSTART     = 1;

    /* Wait for the external oscillator to start up.
       nRF51822 product specification (8.1.5) gives a typical value of 300ms for external clock
       startup duration, and a maximum value of 1s. When using the internal RC source, typical delay
       will be 390µs, so we use a polling period of 200µs.

       We can't use us_ticker at this point, so we have to rely on a less precise method for
       measuring our timeout. Because of this, the actual timeout will be slightly longer than 1
       second, which isn't an issue at all, since this fallback should only be used as a safety net.
       */
    for (i = 0; i < (timeout / polling_period); i++) {
        if (NRF_CLOCK->EVENTS_LFCLKSTARTED != 0)
            return;
        nrf_delay_us(polling_period);
    }

    /* Fallback to internal clock. Belt and braces, since the internal clock is used by default
       whilst no external source is running. This is not only a sanity check, but it also allows
       code down the road (e.g. ble initialisation) to directly know which clock is used. */
    NRF_CLOCK->LFCLKSRC         = (CLOCK_LFCLKSRC_SRC_RC << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {
        // Do nothing.
    }
}