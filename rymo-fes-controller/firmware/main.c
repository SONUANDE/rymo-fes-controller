/*
 * FES controller firmware reference.
 * Transcribed from the supplied project source document; see docs/DEVELOPMENT_NOTES.md.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"
#include "nrf_pwr_mgmt.h"
#include "nrfx_ppi.h"
#include "nrfx_timer.h"
#include "nrfx_gpiote.h"
#include "nrfx_saadc.h"
#include "nrfx_spim.h"
#include "nrf_delay.h"
#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */
#define DEVICE_NAME                     "UNKNOWN DEVICE"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */
#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */
#define APP_ADV_DURATION                18000                                       /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */
#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */
/* Number of bytes reserved for received UART data. */
#define UART_RX_BUF_SIZE 256
/* Number of bytes reserved for queued UART transmissions. */
#define UART_TX_BUF_SIZE 256
//======================= DAC =======================================================
/* SPI instance used to send output values to the MCP DAC devices. */
static const nrfx_spim_t m_spim = NRFX_SPIM_INSTANCE(0);
/* SPI clock pin shared by the DAC devices. */
#define MCP_SCK_PIN     29      
/* SPI data-output pin carrying DAC commands. */
#define MCP_MOSI_PIN    30      
/* Manual chip-select pin for DAC 1. */
#define MCP1_CS_PIN     28
/* Manual chip-select pin for DAC 2. */
#define MCP2_CS_PIN     27       
/* Set by the SPI callback when an asynchronous transfer completes. */
static volatile bool m_transfer_done = false;// transfer flag
/* Two-byte buffer holding the command sent to the DAC. */
static uint8_t m_tx_buffer[2];
//======================= PULSE GENERATION ===================================================
/* Timer 3 produces the pulse timing for channel 1. */
static const nrfx_timer_t m_timer = NRFX_TIMER_INSTANCE(3);
/* Timer 4 produces the pulse timing for channel 2. */
static const nrfx_timer_t m_timer_2ch = NRFX_TIMER_INSTANCE(4);
/* PPI channel that starts the channel 1 pulse. */
static nrf_ppi_channel_t m_ppi_1;
/* PPI channel that ends the channel 1 pulse. */
static nrf_ppi_channel_t m_ppi_2;
/* PPI channel that starts the channel 2 pulse. */
static nrf_ppi_channel_t m_ppi_1_2ch;
/* PPI channel that ends the channel 2 pulse. */
static nrf_ppi_channel_t m_ppi_2_2ch;
/* High-side transistor control pin for channel 1. */
#define HS_BJT_PIN 12
/* Low-side transistor control pin for channel 1. */
#define LS_BJT_PIN 11
/* High-side transistor control pin for channel 2. */
#define HS_BJT_PIN_2CH 25
/* Low-side transistor control pin for channel 2. */
#define LS_BJT_PIN_2CH 26
/*
 * Purpose:
 * Sets the channel 1 timer period and pulse width.
 *
 * Parameters:
 * freq_hz - requested pulse frequency; pulse_width_us - active pulse duration in microseconds.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Disables, clears, configures, and enables timer 3.
 */
static void pulse_set(uint32_t freq_hz, uint32_t pulse_width_us){
    /* Holds the result returned while configuring the peripheral. */
    ret_code_t err;
    /* Pulse repetition period calculated from the requested frequency. */
    uint32_t period_us;
    /* Timer position used to trigger the ADC sample within the pulse. */
    uint32_t adc_ticks;
    APP_ERROR_CHECK_BOOL(freq_hz > 0);
    period_us = 1000000UL / freq_hz;
    
    adc_ticks = pulse_width_us / 2;
    /* Do not allow the two compare events to overlap. */
    if(!((pulse_width_us > 0) &&
                         (pulse_width_us < period_us)))
     {
        return;
     }
    nrfx_timer_disable(&m_timer);
    nrfx_timer_clear(&m_timer);
    nrfx_timer_extended_compare(&m_timer,
                                NRF_TIMER_CC_CHANNEL0,
                                1,
                                0,
                                false);
    nrfx_timer_extended_compare(&m_timer,
                                NRF_TIMER_CC_CHANNEL1,
                                pulse_width_us + 1,
                                0,
                                false);
    nrfx_timer_extended_compare(&m_timer,
                                NRF_TIMER_CC_CHANNEL2,
                                period_us,
                                NRF_TIMER_SHORT_COMPARE2_CLEAR_MASK,
                                false);
    nrfx_timer_extended_compare(&m_timer,
                                NRF_TIMER_CC_CHANNEL3,
                                adc_ticks,                
                                0,
                                false);  
    nrfx_timer_enable(&m_timer);
   
}
/*
 * Purpose:
 * Sets the channel 2 timer period and pulse width.
 *
 * Parameters:
 * freq_hz - requested pulse frequency; pulse_width_us - active pulse duration in microseconds.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Disables, clears, configures, and enables timer 4.
 */
static void pulse_set_2ch(uint32_t freq_hz, uint32_t pulse_width_us){
    /* Holds the result returned while configuring the peripheral. */
    ret_code_t err;
    /* Pulse repetition period calculated from the requested frequency. */
    uint32_t period_us;
    /* Timer position used to trigger the ADC sample within the pulse. */
    uint32_t adc_ticks;
    APP_ERROR_CHECK_BOOL(freq_hz > 0);
    period_us = 1000000UL / freq_hz;
    
    adc_ticks = pulse_width_us / 2;
    /* Do not allow the two compare events to overlap. */
    if(!((pulse_width_us > 0) &&
                         (pulse_width_us < period_us)))
     {
        return;
     }
    nrfx_timer_disable(&m_timer_2ch);
    nrfx_timer_clear(&m_timer_2ch);
    nrfx_timer_extended_compare(&m_timer_2ch,
                                NRF_TIMER_CC_CHANNEL0,
                                1,
                                0,
                                false);
    nrfx_timer_extended_compare(&m_timer_2ch,
                                NRF_TIMER_CC_CHANNEL1,
                                pulse_width_us + 1,
                                0,
                                false);
    nrfx_timer_extended_compare(&m_timer_2ch,
                                NRF_TIMER_CC_CHANNEL2,
                                period_us,
                                NRF_TIMER_SHORT_COMPARE2_CLEAR_MASK,
                                false);
    nrfx_timer_extended_compare(&m_timer_2ch,
                                NRF_TIMER_CC_CHANNEL3,
                                adc_ticks,                
                                0,
                                false);  
    nrfx_timer_enable(&m_timer_2ch);
   
}
//=================================== SAADC ===========================================
#define SAADC_SAMPLES_PER_BUFFER 1
/* Double buffer used by SAADC so one sample block can be read while the next is acquired. */
static nrf_saadc_value_t m_buffer[2][SAADC_SAMPLES_PER_BUFFER] __attribute__((aligned(4)));
/* Indicates that SAADC has completed a sample block for the main loop. */
static volatile bool    m_block_ready;
/* Index of the SAADC buffer that most recently completed. */
static volatile uint8_t m_ready_buffer;
/* Reserved flag for a completed channel 2 sample block. */
static volatile bool    m_block_ready_2ch;
/* Reserved index for the completed channel 2 sample buffer. */
static volatile uint8_t m_ready_buffer_2ch;
/* PPI link from the channel 1 timer to the SAADC sample task. */
static nrf_ppi_channel_t m_ppi_channel;
/* PPI link from the channel 2 timer to the SAADC sample task. */
static nrf_ppi_channel_t m_ppi_channel_2ch;
//===================================================================================================
BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */
/* Current BLE connection handle; invalid when no central is connected. */
static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
/* Largest Nordic UART payload accepted by the connected peer. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
/* Service UUIDs published in the BLE advertising scan response. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};
//========================================================================================================
 // SPI EVENT HANDLER
/*
 * Purpose:
 * Handles completion of an SPI transfer.
 *
 * Parameters:
 * p_event - completed SPI event; p_context - unused application context.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Runs in the SPI event context.
 */
static void spim_handler(
    nrfx_spim_evt_t const * p_event,
    void * p_context)
{
    if (p_event->type == NRFX_SPIM_EVENT_DONE)
    {
        /*
         * Transfer finished.
         *
         * Nothing else required.
         *
         * SPIM has already:
         *
         * CS LOW
         * 16 clocks
         * CS HIGH
         */
    }
}
//write func
/* Two-byte buffer holding the command sent to the DAC. */
/* m_tx_buffer is declared above with the SPI state. */
/*
 * Purpose:
 * Sends one 12-bit value to the selected MCP DAC.
 *
 * Parameters:
 * dac_value - DAC output value; only its lower 12 bits are used.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Updates the shared transmit buffer and starts an SPI transfer.
 */
static void mcp_write(
    uint16_t dac_value)
{
    /* Complete 16-bit command word sent to the DAC. */
    uint16_t command;
    dac_value &= 0x0FFF;
    command =
        0x3000 |
        dac_value;
    m_tx_buffer[0] =
        (uint8_t)(command >> 8);
    m_tx_buffer[1] =
        (uint8_t)(command & 0xFF);
    nrfx_spim_xfer_desc_t xfer =
        NRFX_SPIM_XFER_TX(
            m_tx_buffer,
            2
        );
    APP_ERROR_CHECK(
        nrfx_spim_xfer(
            &m_spim,
            &xfer,
            0
        )
    );
}
// SPI INIT
/*
 * Purpose:
 * Initializes SPI and both DAC chip-select pins.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Configures SPI0 and sets both DACs inactive.
 */
static void spim_init(void)
{
    nrfx_spim_config_t config =
        NRFX_SPIM_DEFAULT_CONFIG;
    config.sck_pin =
        MCP_SCK_PIN;
    config.mosi_pin =
        MCP_MOSI_PIN;
    config.miso_pin =
        NRFX_SPIM_PIN_NOT_USED;
    /*
     * IMPORTANT:
     *
     * Let SPIM hardware control CS.
     */
    //config.ss_pin =
    //   MCP1_CS_PIN;
     config.ss_pin = NRFX_SPIM_PIN_NOT_USED;
    /*
     * MCP4921:
     *
     * SPI MODE 0
     *
     * CPOL = 0
     * CPHA = 0
     */
    config.mode =
        NRF_SPIM_MODE_0;
    /*
     * MCP4921 expects MSB first.
     */
    config.bit_order =
        NRF_SPIM_BIT_ORDER_MSB_FIRST;
    /*
     * 1 MHz SPI clock.
     */
    config.frequency =
        NRF_SPIM_FREQ_1M;
    APP_ERROR_CHECK(
        nrfx_spim_init(
            &m_spim,
            &config,
            NULL,
            NULL
        )
    );
        // Configure both CS pins as GPIO outputs
    nrf_gpio_cfg_output(MCP1_CS_PIN);
    nrf_gpio_cfg_output(MCP2_CS_PIN);
    // Both DACs deselected
    nrf_gpio_pin_set(MCP1_CS_PIN);
    nrf_gpio_pin_set(MCP2_CS_PIN);
}
//====================================== SAADC===================================================================
/*
 * Purpose:
 * Handles a completed SAADC buffer.
 *
 * Parameters:
 * p_event - SAADC event containing the completed buffer.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Updates the ready-buffer state and requeues the completed buffer.
 */
static void saadc_callback(nrfx_saadc_evt_t const *p_event)
{
    if (p_event->type == NRFX_SAADC_EVT_DONE)
    {
        /* Points to the buffer that SAADC has just filled. */
        nrf_saadc_value_t *completed = p_event->data.done.p_buffer;
        /* Identify which buffer has just been filled. */
        m_ready_buffer = (completed == m_buffer[0]) ? 0 : 1;
        /* Tell main() a buffer is ready. */
        m_block_ready = true;
        /* Immediately return this buffer to the driver queue. */
        APP_ERROR_CHECK( nrfx_saadc_buffer_convert(completed, SAADC_SAMPLES_PER_BUFFER));
         //nrf_gpio_pin_toggle(18);
    }
}
/* Tracks whether SAADC has been started and therefore needs a stop task. */
static bool m_saadc_running = false;
/*
 * Purpose:
 * Initializes SAADC for one analog input.
 *
 * Parameters:
 * input - Nordic SAADC input to connect to logical channel 0.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Initializes SAADC and queues both sample buffers.
 */
static void saadc_init_one_channel(nrf_saadc_input_t input)
{
    /* Local SAADC configuration for the selected analog input. */
    nrfx_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
    saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;
    saadc_config.oversample = NRF_SAADC_OVERSAMPLE_DISABLED;
    APP_ERROR_CHECK(nrfx_saadc_init(&saadc_config, saadc_callback));
    /* Channel settings passed to logical SAADC channel 0. */
    nrf_saadc_channel_config_t cfg =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(input);
    /* Always use logical SAADC channel 0. */
    APP_ERROR_CHECK(nrfx_saadc_channel_init(0, &cfg));
    APP_ERROR_CHECK(nrfx_saadc_buffer_convert(m_buffer[0],
                                               SAADC_SAMPLES_PER_BUFFER));
    APP_ERROR_CHECK(nrfx_saadc_buffer_convert(m_buffer[1],
                                               SAADC_SAMPLES_PER_BUFFER));
}
/*
 * Purpose:
 * Starts SAADC sampling support.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Waits for SAADC startup and marks the module as running.
 */
static void saadc_start(void)
{
    NRF_SAADC->EVENTS_STARTED = 0;
    nrf_saadc_task_trigger(NRF_SAADC_TASK_START);
    while (NRF_SAADC->EVENTS_STARTED == 0) { }
    NRF_SAADC->EVENTS_STARTED = 0;
    m_saadc_running = true;
}
/*
 * Purpose:
 * Stops SAADC safely before changing channels.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Disables sampling PPI links, stops SAADC, and uninitializes it.
 */
static void saadc_stop(void)
{
    /* Prevent either timer from issuing another SAMPLE task. */
    APP_ERROR_CHECK(nrfx_ppi_channel_disable(m_ppi_channel));
    APP_ERROR_CHECK(nrfx_ppi_channel_disable(m_ppi_channel_2ch));
    if (m_saadc_running)
    {
        NRF_SAADC->EVENTS_STOPPED = 0;
        nrf_saadc_task_trigger(NRF_SAADC_TASK_STOP);
        while (NRF_SAADC->EVENTS_STOPPED == 0) { }
        NRF_SAADC->EVENTS_STOPPED = 0;
        m_saadc_running = false;
    }
    nrfx_saadc_uninit();
}
/*
 * Purpose:
 * Starts the selected pulse and sampling channel.
 *
 * Parameters:
 * channel - output channel number; frequency - pulse frequency; pulse_width - pulse duration.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Stops the current path, reconfigures SAADC, and enables the selected timer and PPI link.
 */
static void start_channel(uint8_t channel,
                          uint32_t frequency,
                          uint32_t pulse_width)
{
    nrfx_timer_disable(&m_timer);
    nrfx_timer_clear(&m_timer);
    nrfx_timer_disable(&m_timer_2ch);
    nrfx_timer_clear(&m_timer_2ch);
    saadc_stop();
    if (channel == 1)
    {
        /* TIM3 -> AIN0 only */
        saadc_init_one_channel(NRF_SAADC_INPUT_AIN0);
        saadc_start();
        pulse_set(frequency, pulse_width);
        APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_ppi_channel));
    }
    else
    {
        /* TIM4 -> AIN1 only */
        saadc_init_one_channel(NRF_SAADC_INPUT_AIN1);
        saadc_start();
        pulse_set_2ch(frequency, pulse_width);
        APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_ppi_channel_2ch));
    }
}
/*
 * Purpose:
 * Connects timer events to GPIO and SAADC tasks through PPI.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Allocates and enables PPI channels.
 */
static void ppi_init(void)
{
    /* Holds the result returned while configuring the peripheral. */
    ret_code_t err;
//======================================== CH1 =============================================================
    err = nrfx_ppi_channel_alloc(&m_ppi_1); APP_ERROR_CHECK(err);
    err = nrfx_ppi_channel_alloc(&m_ppi_2);  APP_ERROR_CHECK(err);
    // ==========================================================
    // PULSE START: HS HIGH, LS LOW
    // ==========================================================
    nrfx_ppi_channel_assign(
        m_ppi_1,
        nrfx_timer_compare_event_address_get(
            &m_timer,
            NRF_TIMER_CC_CHANNEL0),
        nrfx_gpiote_set_task_addr_get(HS_BJT_PIN));
    nrfx_ppi_channel_fork_assign(
        m_ppi_1,
        nrfx_gpiote_clr_task_addr_get(LS_BJT_PIN));
    // ==========================================================
    // PULSE END: HS LOW, LS HIGH
    // ==========================================================
    nrfx_ppi_channel_assign(
        m_ppi_2,
        nrfx_timer_compare_event_address_get(
            &m_timer,
            NRF_TIMER_CC_CHANNEL1),
        nrfx_gpiote_clr_task_addr_get(HS_BJT_PIN));
    nrfx_ppi_channel_fork_assign(
        m_ppi_2,
        nrfx_gpiote_set_task_addr_get(LS_BJT_PIN));
  //================================= CH1 ADC========================================
   
    
    APP_ERROR_CHECK(nrfx_ppi_channel_alloc(&m_ppi_channel));
    APP_ERROR_CHECK(nrfx_ppi_channel_assign(
        m_ppi_channel,
        nrfx_timer_compare_event_address_get( &m_timer, NRF_TIMER_CC_CHANNEL3),
        nrf_saadc_task_address_get(NRF_SAADC_TASK_SAMPLE)
        )
    );
    nrfx_ppi_channel_enable(m_ppi_1);
    nrfx_ppi_channel_enable(m_ppi_2);
   // APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_ppi_channel));
//============================================= CH2 ================================================================
    err = nrfx_ppi_channel_alloc(&m_ppi_1_2ch); APP_ERROR_CHECK(err);
    err = nrfx_ppi_channel_alloc(&m_ppi_2_2ch); APP_ERROR_CHECK(err);
// ==========================================================
    // PULSE START: HS HIGH, LS LOW
    // ==========================================================
    nrfx_ppi_channel_assign(
        m_ppi_1_2ch,
        nrfx_timer_compare_event_address_get(
            &m_timer_2ch,
            NRF_TIMER_CC_CHANNEL0),
        nrfx_gpiote_set_task_addr_get(HS_BJT_PIN_2CH));
    nrfx_ppi_channel_fork_assign(
        m_ppi_1_2ch,
        nrfx_gpiote_clr_task_addr_get(LS_BJT_PIN_2CH));
    // ==========================================================
    // PULSE END: HS LOW, LS HIGH
    // ==========================================================
    nrfx_ppi_channel_assign(
        m_ppi_2_2ch,
        nrfx_timer_compare_event_address_get(
            &m_timer_2ch,
            NRF_TIMER_CC_CHANNEL1),
        nrfx_gpiote_clr_task_addr_get(HS_BJT_PIN_2CH));
    nrfx_ppi_channel_fork_assign(
        m_ppi_2_2ch,
        nrfx_gpiote_set_task_addr_get(LS_BJT_PIN_2CH));
  //================================CH2 ADC=========================================
   
    
    APP_ERROR_CHECK(nrfx_ppi_channel_alloc(&m_ppi_channel_2ch));
    APP_ERROR_CHECK(nrfx_ppi_channel_assign(
        m_ppi_channel_2ch,
        nrfx_timer_compare_event_address_get( &m_timer_2ch, NRF_TIMER_CC_CHANNEL3),
        nrf_saadc_task_address_get(NRF_SAADC_TASK_SAMPLE)
        )
    );
    nrfx_ppi_channel_enable(m_ppi_1_2ch);
    nrfx_ppi_channel_enable(m_ppi_2_2ch);
  //  APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_ppi_channel_2ch));
    
}
/*
 * Purpose:
 * Prepares pulse-control GPIO pins for task control.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Initializes GPIOTE and enables output tasks for four pins.
 */
static void gpiote_init(void)
{
    /* Stores the result of each GPIOTE or PPI setup call. */
    ret_code_t err; 
    if (!nrfx_gpiote_is_init())
    {
        err = nrfx_gpiote_init();
        APP_ERROR_CHECK(err);
    }
    //=================================== CH1 ================================================
    nrfx_gpiote_out_config_t hs_config = NRFX_GPIOTE_CONFIG_OUT_TASK_TOGGLE(true);
    nrfx_gpiote_out_config_t ls_config = NRFX_GPIOTE_CONFIG_OUT_TASK_TOGGLE(false);
    err = nrfx_gpiote_out_init(HS_BJT_PIN, &hs_config);
    APP_ERROR_CHECK(err);
    err = nrfx_gpiote_out_init(LS_BJT_PIN, &ls_config);
    APP_ERROR_CHECK(err);
    nrfx_gpiote_out_task_enable(HS_BJT_PIN);
    nrfx_gpiote_out_task_enable(LS_BJT_PIN);
    //======================================= CH2 =============================================
    err = nrfx_gpiote_out_init(HS_BJT_PIN_2CH, &hs_config);
    APP_ERROR_CHECK(err);
    err = nrfx_gpiote_out_init(LS_BJT_PIN_2CH, &ls_config);
    APP_ERROR_CHECK(err);
    nrfx_gpiote_out_task_enable(HS_BJT_PIN_2CH);
    nrfx_gpiote_out_task_enable(LS_BJT_PIN_2CH);
}
/*
 * Purpose:
 * Initializes the two hardware timers used for pulse generation.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Configures timers 3 and 4 for 1 MHz operation.
 */
static void timer_init(void)
{
    /* Holds the result returned while configuring the peripheral. */
    ret_code_t err;
    
    /* Shared configuration applied to both hardware timers. */
    nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG;
    timer_cfg.frequency = NRF_TIMER_FREQ_1MHz;
    timer_cfg.mode      = NRF_TIMER_MODE_TIMER;
    timer_cfg.bit_width = NRF_TIMER_BIT_WIDTH_32;
    err = nrfx_timer_init(&m_timer, &timer_cfg, NULL);
    APP_ERROR_CHECK(err);
    err = nrfx_timer_init(&m_timer_2ch, &timer_cfg, NULL);
    APP_ERROR_CHECK(err);
  
}
/*
 * Purpose:
 * Reports a Nordic SDK assertion to the error handler.
 *
 * Parameters:
 * line_num - source line that asserted; p_file_name - source file name.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Calls the application error handler, which may halt execution.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}
/**@brief Function for initializing the timer module.
 */
/*
 * Purpose:
 * Initializes the application timer module.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Starts the SDK timer support used by BLE components.
 */
static void timers_init(void)
{
    /* Status returned while the application timer module is initialized. */
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}
/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
/*
 * Purpose:
 * Sets the device name and preferred BLE connection parameters.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Updates GAP settings in the SoftDevice.
 */
static void gap_params_init(void)
{
    /* Receives the status returned by BLE GAP configuration calls. */
    uint32_t                err_code;
    /* Preferred BLE connection interval and timeout settings. */
    ble_gap_conn_params_t   gap_conn_params;
    /* Security mode used when exposing the device name. */
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}
/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
/*
 * Purpose:
 * Handles errors raised by the Queued Write module.
 *
 * Parameters:
 * nrf_error - SDK error code to report.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Passes the error to the application error handler.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}
/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
/*
 * Purpose:
 * Parses BLE Nordic UART configuration commands.
 *
 * Parameters:
 * p_evt - received Nordic UART Service event.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * May change DAC output, timers, SAADC input, and PPI sampling links.
 */
static void nus_data_handler(ble_nus_evt_t *p_evt) {
  if (p_evt->type == BLE_NUS_EVT_RX_DATA) {
    /* Receives the status returned by the current SDK call. */
    uint32_t err_code;
    
    /* Text buffer used to hold the incoming BLE configuration command. */
    char freq[50];
    size_t command_length = p_evt->params.rx_data.length;
    if (command_length == 0 || command_length >= sizeof(freq))
    {
        NRF_LOG_WARNING("Ignoring empty or oversized BLE command");
        return;
    }

    NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, command_length);

    memcpy(freq, p_evt->params.rx_data.p_data, command_length);
    freq[command_length] = '\0';
    /* Parsed frequency, pulse width, DAC value, and selected output channel. */
    int frequency, pulse_width, dac, channel;
    if (sscanf(freq, "%d:%d:%d:%d", &frequency, &pulse_width, &dac, &channel) == 4) {
      
      if(frequency < 1){frequency = 1;}
      if(frequency > 150){frequency = 150;}
      if(pulse_width < 10){pulse_width = 10;}
      if(pulse_width > 400){pulse_width = 400;}
      if(dac < 0){dac = 0;}
      if(dac > 4095){dac = 4095;}
      if(channel > 2){channel = 2;}
      if(channel < 1){channel = 1;}
      NRF_LOG_INFO("CFG,%d,%d,%d,%d", frequency, pulse_width, dac, channel);
      switch (channel)
      {
          case 1:
              /* Stop both timer sources before touching SAADC. */
              nrfx_timer_disable(&m_timer);
              nrfx_timer_clear(&m_timer);
              nrfx_timer_disable(&m_timer_2ch);
              nrfx_timer_clear(&m_timer_2ch);
              /* Stop PPI sampling and fully release the old SAADC channel. */
              saadc_stop();
              /* Set DAC 1 before starting its pulse timer. */
              nrf_gpio_pin_clear(MCP1_CS_PIN);
              mcp_write(dac);
              nrf_gpio_pin_set(MCP1_CS_PIN);
              /* TIM3 samples AIN0 only. */
              saadc_init_one_channel(NRF_SAADC_INPUT_AIN0);
              saadc_start();
              APP_ERROR_CHECK(nrfx_ppi_channel_disable(m_ppi_channel_2ch));
              APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_ppi_channel));
              /* This enables TIM3. */
              pulse_set(frequency, pulse_width);
              break;
          case 2:
              /* Stop both timer sources before touching SAADC. */
              nrfx_timer_disable(&m_timer);
              nrfx_timer_clear(&m_timer);
              nrfx_timer_disable(&m_timer_2ch);
              nrfx_timer_clear(&m_timer_2ch);
              /* Stop PPI sampling and fully release the old SAADC channel. */
              saadc_stop();
              /* Set DAC 2 before starting its pulse timer. */
              nrf_gpio_pin_clear(MCP2_CS_PIN);
              mcp_write(dac);
              nrf_gpio_pin_set(MCP2_CS_PIN);
              /* TIM4 samples AIN1 only. */
              saadc_init_one_channel(NRF_SAADC_INPUT_AIN1);
              saadc_start();
              APP_ERROR_CHECK(nrfx_ppi_channel_disable(m_ppi_channel));
              APP_ERROR_CHECK(nrfx_ppi_channel_enable(m_ppi_channel_2ch));
              /* This enables TIM4. */
              pulse_set_2ch(frequency, pulse_width);
              break;
      }
     
    
    }
  }
}
/**@snippet [Handling the data received over BLE] */
/**@brief Function for initializing services that will be used by the application.
 */
/*
 * Purpose:
 * Initializes the Queued Write and Nordic UART services.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Registers the BLE data handler and initializes service state.
 */
static void services_init(void)
{
    /* Receives the status returned while the Nordic UART Service is initialized. */
    uint32_t           err_code;
    /* Nordic UART Service initialization settings. */
    ble_nus_init_t     nus_init;
    /* Queued Write module initialization settings. */
    nrf_ble_qwr_init_t qwr_init = {0};
    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;
    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);
    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));
    nus_init.data_handler = nus_data_handler;
    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}
/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
/*
 * Purpose:
 * Handles connection-parameter negotiation events.
 *
 * Parameters:
 * p_evt - event reported by the Connection Parameters module.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Disconnects the current peer if negotiation fails.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    /* Receives the status returned by the current SDK call. */
    uint32_t err_code;
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}
/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
/*
 * Purpose:
 * Handles Connection Parameters module errors.
 *
 * Parameters:
 * nrf_error - SDK error code to report.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Passes the error to the application error handler.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}
/**@brief Function for initializing the Connection Parameters module.
 */
/*
 * Purpose:
 * Configures the BLE Connection Parameters module.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Initializes the module and registers its event and error handlers.
 */
static void conn_params_init(void)
{
    /* Receives the status returned by the current BLE module call. */
    uint32_t               err_code;
    /* Connection Parameters module configuration. */
    ble_conn_params_init_t cp_init;
    memset(&cp_init, 0, sizeof(cp_init));
    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;
    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}
/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
/*
 * Purpose:
 * Puts the device into system-off sleep mode.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Prepares wakeup buttons and does not return after system-off is entered.
 */
static void sleep_mode_enter(void)
{
    /* Status returned while changing the board indication before sleep. */
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);
    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);
    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}
/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
/*
 * Purpose:
 * Responds to BLE advertising state changes.
 *
 * Parameters:
 * ble_adv_evt - advertising event reported by the BLE advertising module.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Changes the board indication or enters sleep mode.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    /* Receives the status returned by the current SDK call. */
    uint32_t err_code;
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}
/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
/*
 * Purpose:
 * Handles BLE events received from the SoftDevice.
 *
 * Parameters:
 * p_ble_evt - BLE event; p_context - unused observer context.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Updates connection state, LEDs, GATT settings, and may disconnect a peer.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    /* Receives the status returned by the current SDK call. */
    uint32_t err_code;
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            nrf_gpio_pin_set(31);
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            nrf_gpio_pin_clear(31);
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;
        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;
        default:
            // No implementation needed.
            break;
    }
}
/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
/*
 * Purpose:
 * Enables and configures the SoftDevice BLE stack.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Sets BLE RAM usage and registers the BLE observer.
 */
static void ble_stack_init(void)
{
    /* Receives the status returned while enabling or configuring BLE. */
    ret_code_t err_code;
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);
    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    /* Application RAM boundary negotiated with the SoftDevice. */
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);
    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);
    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}
/**@brief Function for handling events from the GATT library. */
/*
 * Purpose:
 * Handles GATT library events.
 *
 * Parameters:
 * p_gatt - GATT module instance; p_evt - GATT event.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Updates the allowed Nordic UART payload length after an MTU change.
 */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}
/**@brief Function for initializing the GATT library. */
/*
 * Purpose:
 * Initializes GATT support and the peripheral MTU.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Registers the GATT event handler and requests the configured MTU.
 */
void gatt_init(void)
{
    /* Receives the status returned while enabling or configuring BLE. */
    ret_code_t err_code;
    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}
/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
/*
 * Purpose:
 * Handles board-support button and system events.
 *
 * Parameters:
 * event - BSP event to process.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * May enter sleep, disconnect BLE, or restart advertising.
 */
void bsp_event_handler(bsp_event_t event)
{
    /* Receives the status returned by the current SDK call. */
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;
        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;
        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;
        default:
            break;
    }
}
/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
/*
 * Purpose:
 * Handles received UART bytes and UART errors.
 *
 * Parameters:
 * p_event - UART event supplied by the SDK.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Buffers UART data and may send a completed line over BLE.
 */
void uart_event_handle(app_uart_evt_t * p_event)
{
    /* Accumulates UART bytes until a complete line can be sent over BLE. */
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    /* Current write position in the UART receive buffer. */
    static uint8_t index = 0;
    uint32_t       err_code;
    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;
            if ((data_array[index - 1] == '\n') ||
                (data_array[index - 1] == '\r') ||
                (index >= m_ble_nus_max_data_len))
            {
                if (index > 1)
                {
                    NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                    NRF_LOG_HEXDUMP_DEBUG(data_array, index);
                    do
                    {
                        /* Length of the UART line being forwarded over BLE. */
                        uint16_t length = (uint16_t)index;
                        err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
                            (err_code != NRF_ERROR_RESOURCES) &&
                            (err_code != NRF_ERROR_NOT_FOUND))
                        {
                            APP_ERROR_CHECK(err_code);
                        }
                    } while (err_code == NRF_ERROR_RESOURCES);
                }
                index = 0;
            }
            break;
        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;
        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;
        default:
            break;
    }
}
/**@snippet [Handling the data received over UART] */
/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
/*
 * Purpose:
 * Initializes the UART transport.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Configures pins and creates the UART FIFO buffers.
 */
static void uart_init(void)
{
    uint32_t                     err_code;
    /* UART pin, flow-control, parity, and baud-rate settings. */
    app_uart_comm_params_t const comm_params =
    {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
#if defined (UART_PRESENT)
        .baud_rate    = NRF_UART_BAUDRATE_921600
#else
        .baud_rate    = NRF_UARTE_BAUDRATE_921600
#endif
    };
    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */
/**@brief Function for initializing the Advertising functionality.
 */
/*
 * Purpose:
 * Builds the BLE advertising configuration.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Initializes advertising data, timing, and its event handler.
 */
static void advertising_init(void)
{
    /* Receives the status returned by the current BLE module call. */
    uint32_t               err_code;
    /* Advertising data and timing configuration. */
    ble_advertising_init_t init;
    memset(&init, 0, sizeof(init));
    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;
    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = on_adv_evt;
    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);
    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}
/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
/*
 * Purpose:
 * Initializes board buttons and LEDs.
 *
 * Parameters:
 * p_erase_bonds - receives whether stored bonds should be erased.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Registers the BSP event handler and writes the startup decision through the pointer.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    /* BSP event reported while the buttons and LEDs are initialized. */
    bsp_event_t startup_event;
    uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);
    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}
/**@brief Function for initializing the nrf log module.
 */
/*
 * Purpose:
 * Initializes the logging module and its backends.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Enables configured logging outputs.
 */
static void log_init(void)
{
    /* Status returned when the logging backend is initialized. */
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}
/**@brief Function for initializing power management.
 */
/*
 * Purpose:
 * Initializes SDK power management.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Enables low-power idle handling.
 */
static void power_management_init(void)
{
    /* Receives the status returned while enabling or configuring BLE. */
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}
/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
/*
 * Purpose:
 * Runs one iteration of idle processing.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * May put the CPU into a low-power state when no log work is pending.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}
/**@brief Function for starting advertising.
 */
/*
 * Purpose:
 * Starts fast BLE advertising.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Makes the device discoverable to BLE central devices.
 */
static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}
/*
 * Purpose:
 * Sends one byte through UART.
 *
 * Parameters:
 * byte - value to queue for transmission.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Waits while the UART FIFO is busy.
 */
static void send_byte(uint8_t byte)
{
    while (app_uart_put(byte) == NRF_ERROR_BUSY);
}
/*
 * Purpose:
 * Sends an ADC sample block in the UART frame format.
 *
 * Parameters:
 * samples - pointer to the samples to serialize.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Writes a frame header and sample bytes to UART.
 */
static void send_adc_block(int16_t const *samples)
{
    send_byte(0xAA);  // frame header
    send_byte(0x55);
    for (uint32_t i = 0; i < 256; i++)
    {
        /* Unsigned representation of the current ADC sample for byte-wise transmission. */
        uint16_t value = (uint16_t)samples[i];
        send_byte((uint8_t)(value & 0xFF));       // low byte
        send_byte((uint8_t)(value >> 8));         // high byte
    }
}
/*
 * Purpose:
 * Sends a null-terminated string through UART.
 *
 * Parameters:
 * str - text to transmit.
 *
 * Returns:
 * None.
 *
 * Side effects:
 * Waits while the UART FIFO is busy.
 */
void send_uart( const char *str)
{
    while(*str)
    {
      while(app_uart_put((uint8_t)*str) == NRF_ERROR_BUSY);
      str++;
    }
}
/**@brief Application main function.
 */
/*
 * Purpose:
 * Initializes the application and processes completed ADC samples.
 *
 * Parameters:
 * None.
 *
 * Returns:
 * 0 is not returned because the application runs continuously.
 *
 * Side effects:
 * Initializes all peripherals and continuously sends ADC data over BLE.
 */
int main(void)
{
    /* Set when startup requests removal of stored BLE bonding data. */
    bool erase_bonds;
    // Initialize.
    uart_init();
    log_init();
    timers_init();
    buttons_leds_init(&erase_bonds);
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    NRF_LOG_INFO("================================");
    NRF_LOG_INFO("External nRF52832 STARTED");
    NRF_LOG_INFO("RTT logging is working");
    NRF_LOG_INFO("================================");
   
/////////////////////////////////////////////////// BLE- FREQ
    gpiote_init();
    timer_init();     // 10 Hz, HS ON for 50 us
 //   saadc_init();
    ppi_init();
    spim_init();
///////////////////////////////////////////////////////
    /* Start SAADC and wait until it is ready for PPI SAMPLE events. */
    NRF_SAADC->EVENTS_STARTED = 0;
    nrf_saadc_task_trigger(NRF_SAADC_TASK_START);
    while (NRF_SAADC->EVENTS_STARTED == 0)
    {
        /* Wait only during startup. */
    }
    NRF_SAADC->EVENTS_STARTED = 0;
    nrf_gpio_pin_clear(31);//BEN
    // Start execution.
    NRF_LOG_INFO(
        "BLE pulse generator + SAADC + SPI_DAC started\r\n"
    );
    NRF_LOG_FLUSH();
    advertising_start();
    
    
    // Enter main loop.
    for (;;)
    {
        if (m_block_ready)
        {
            uint32_t i;
            /* Takes a stable copy of the buffer index selected by the interrupt. */
            uint8_t buffer_index = m_ready_buffer;
            for (i = 0; i < SAADC_SAMPLES_PER_BUFFER; i++)
            {
                /* Text payload sent for the current ADC sample. */
                char ble_data[20]; 
                /* Signed ADC reading before it is converted to text. */
                int32_t raw_value = m_buffer[buffer_index][i];
                if (raw_value < 0)
                {
                    raw_value = 0;
                }
                sprintf(ble_data, "%04ld\r\n", (long)raw_value);
                /* Number of bytes in the BLE payload. */
                uint16_t length = strlen(ble_data);
                if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
                {
                    /* Result returned by the Nordic UART data send operation. */
                    ret_code_t err_code;
                    err_code = ble_nus_data_send(
                                    &m_nus,
                                    (uint8_t *)ble_data,
                                    &length,
                                    m_conn_handle);
                    if (err_code != NRF_SUCCESS &&
                        err_code != NRF_ERROR_RESOURCES &&
                        err_code != NRF_ERROR_INVALID_STATE)
                    {
                        APP_ERROR_CHECK(err_code);
                    }
                }
            }
            m_block_ready = false;
        }
          NRF_LOG_PROCESS();
    }
     idle_state_handle();
}
