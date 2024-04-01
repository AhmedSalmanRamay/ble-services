
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "app_timer.h"

#include "bsp_btn_ble.h"
#include "nrf_pwr_mgmt.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_ble_qwr.h"
#include "nrf_ble_gatt.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"


#define APP_BLE_CONN_CFG_TAG    1
#define APP_BLE_OBSERVER_PRIO   3

#define DEVICE_NAME           "nrf-test"
#define MIN_CONN_INTERVAL      MSEC_TO_UNITS(100 , UNIT_1_25_MS)
#define MAX_CONN_INTERVAL      MSEC_TO_UNITS(200 , UNIT_1_25_MS)
#define SLAVE_LATENCY          0
#define CONN_SUP_TIMEOUT       MSEC_TO_UNITS(2000,UNIT_10_MS)

#define APP_ADV_INTERVAL       300
#define APP_ADV_DURATION       0
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)
#define NEXT_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(30000)
#define MAX_CON_PARAMS_UPDATE_COUNT    3

#define SEC_PARAM_BOND                  1                                     
#define SEC_PARAM_MITM                  0                                      
#define SEC_PARAM_LESC                  0                                       
#define SEC_PARAM_KEYPRESS              0                                       
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                    
#define SEC_PARAM_OOB                   0                                       
#define SEC_PARAM_MIN_KEY_SIZE          7                                       
#define SEC_PARAM_MAX_KEY_SIZE          16                                      

#define DEAD_BEEF                       0xDEADBEEF                             


NRF_BLE_GATT_DEF(m_gatt);

NRF_BLE_QWR_DEF(m_qwr);
BLE_ADVERTISING_DEF(m_advertising);

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

static ble_uuid_t m_adv_uuids[] =                                              
{
    {0x1812, BLE_UUID_TYPE_BLE}
};


static void advertising_start(bool erase_bonds);
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    pm_handler_on_pm_evt(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            advertising_start(false);
            break;

        default:
            break;
    }
}


static void nrf_qwr_error_handler(uint32_t nrf_error)
{
APP_ERROR_CHECK(nrf_error);
}


static void services_init(void){
ret_code_t err_code;
nrf_ble_qwr_init_t qwr_init={0};

qwr_init.error_handler = nrf_qwr_error_handler;
err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
APP_ERROR_CHECK(err_code);

}


static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
ret_code_t err_code;
if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED){
err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
APP_ERROR_CHECK(err_code);
}
if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_SUCCEEDED){

}

}

static void conn_params_error_handler(uint32_t nrf_error)
{
APP_ERROR_CHECK(nrf_error);
}


static void conn_params_init(void)
{
ret_code_t err_code;
ble_conn_params_init_t cp_init;

memset(&cp_init,0, sizeof(cp_init));

cp_init.p_conn_params = NULL;
cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY ;
cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
cp_init.max_conn_params_update_count = MAX_CON_PARAMS_UPDATE_COUNT;

cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
cp_init.disconnect_on_fail = false;
cp_init.evt_handler        = on_conn_params_evt;
    cp_init.error_handler  = conn_params_error_handler;

err_code = ble_conn_params_init(&cp_init);
APP_ERROR_CHECK(err_code);

}


//adv_evt_handle
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("Fast advertising.");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_IDLE:
            //sleep_mode_enter();
            break;

        default:
            break;
    }
}


static void advertising_init(void){

ret_code_t err_code;
ble_advertising_init_t init;
////
uint8_t cust_data = 24;
ble_advdata_service_data_t service_data;

/////
memset(&init , 0 , sizeof(init));

init.advdata.name_type= BLE_ADVDATA_FULL_NAME;

init.advdata.include_appearance = true;

service_data.service_uuid = BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE;
service_data.data.size = sizeof(cust_data);
service_data.data.p_data = &cust_data;

init.advdata.p_service_data_array = &service_data;
init.advdata.service_data_count= 1;

init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
init.config.ble_adv_fast_enabled = true;
init.config.ble_adv_fast_interval  = APP_ADV_INTERVAL;
init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

init.evt_handler= on_adv_evt;
err_code = ble_advertising_init(&m_advertising, &init);
APP_ERROR_CHECK(err_code);

ble_advertising_conn_cfg_tag_set(&m_advertising,APP_BLE_CONN_CFG_TAG);
}


//GAP
static void gap_params_init(void)
{

ret_code_t err_code;
ble_gap_conn_params_t                gap_conn_params;
ble_gap_conn_sec_mode_t              sec_mode;

BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *) DEVICE_NAME, strlen(DEVICE_NAME));
APP_ERROR_CHECK(err_code);

memset(&gap_conn_params,0,sizeof(gap_conn_params));
gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
gap_conn_params.slave_latency = SLAVE_LATENCY;
gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;

err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
APP_ERROR_CHECK(err_code);

}

//gatt initial
static void gatt_init(void){
ret_code_t err_code =nrf_ble_gatt_init(&m_gatt, NULL);
APP_ERROR_CHECK(err_code);

}



static void ble_evt_handler(ble_evt_t const * p_ble_evt ,void * p_context)
{
ret_code_t err_code = NRF_SUCCESS;

switch(p_ble_evt -> header.evt_id)
{
case BLE_GAP_EVT_DISCONNECTED:
NRF_LOG_INFO("DEVICE DISCONNECTED.....")
break;

case BLE_GAP_EVT_CONNECTED:
NRF_LOG_INFO("DEVICE CONNECTED!!.....")

err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
APP_ERROR_CHECK(err_code);

m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr,m_conn_handle);

APP_ERROR_CHECK(err_code);


break;

case BLE_GAP_EVT_PHY_UPDATE_REQUEST:

NRF_LOG_DEBUG( "PHY UPDATE REQUEST");

ble_gap_phys_t const phys=
{
.rx_phys = BLE_GAP_PHY_AUTO,
.tx_phys = BLE_GAP_PHY_AUTO,

};

break;

}

}

// blle -stack-soft-devvice
static void ble_stack_init(void)
{
ret_code_t err_code;
err_code = nrf_sdh_enable_request();
APP_ERROR_CHECK(err_code);

uint32_t ram_start= 0;
err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
 APP_ERROR_CHECK(err_code);

 err_code = nrf_sdh_ble_enable(&ram_start);
 APP_ERROR_CHECK(err_code);

NRF_SDH_BLE_OBSERVER(m_ble_observer,APP_BLE_OBSERVER_PRIO,ble_evt_handler,NULL);

}


static void power_managment_init(void)
{
ret_code_t err_code = nrf_pwr_mgmt_init();
APP_ERROR_CHECK(err_code);
}

static void idle_state_handle(void)
{
if(NRF_LOG_PROCESS() == false)
{
nrf_pwr_mgmt_run();
}
}


static void leds_init(void)
{
ret_code_t err_code = bsp_init(BSP_INIT_LEDS, NULL);
APP_ERROR_CHECK(err_code);
}


static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}


static void delete_bonds(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds!");

    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}

// timer_nrf
static void timers_init(void)
{
ret_code_t err_code = app_timer_init();
APP_ERROR_CHECK(err_code);

}
/* debug function erro--loggerr*/
static void log_init(void)
{
 ret_code_t err_code =NRF_LOG_INIT(NULL);
 APP_ERROR_CHECK(err_code);

NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void advertising_start(bool erase_bonds)
{
    if (erase_bonds == true)
    {
        delete_bonds();
       
    }
    else
    {
        ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);

        APP_ERROR_CHECK(err_code);
    }
}
/*
static void set_random_static_address(void)
{
uint32_t err_code;
static ble_gap_addr_t rs_addr;

rs_addr.addr[0]=0x32;
rs_addr.addr[1]=0xBL;
rs_addr.addr[2]=0xE4;
rs_addr.addr[3]=0x04;
rs_addr.addr[4]=0x00;
rs_addr.addr[5]=0xFF;
rs_addr.addr_type= BLE_GAP_ADDR_TYPE_RANDOM_STATIC;

err_code = sd_ble_gap_addr_set(&rs_addr);
if(err_code != NRF_SUCCESS)
{
NRF_LOG_INFO("FAILED RAND ADDR....");
}
}

static void get_random_static_address(void)
{
uint32_t err_code;
static ble_gap_addr_t my_device_addr;

err_code = sd_ble_gap_addr_get(&my_device_addr);


}
*/
int main(void)
{

 bool erase_bonds=false;

    log_init();
    timers_init();
    leds_init();
    power_managment_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    advertising_init();
    services_init();
    conn_params_init();

    NRF_LOG_INFO("BLE STARTED!!!");
  //  set_random_static_address();

    //application_timers_start();
    advertising_start(erase_bonds);
    //get_random_static_address();

    for (;;)
    {
        idle_state_handle();
    }
}


