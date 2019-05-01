/* C lib and other "standard" includes */
#include <apps/xdk_sensors/xdk_sensors.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Wish & Mist includes */
#include "wish_app.h" 
#include "mist_app.h"
#include "mist_model.h"
#include "mist_model.h"
#include "mist_handler.h"
#include "wish_debug.h"
#include "bson.h"

#include "wish_port_config.h"

/* Mist app includes */
#include "mist_api.h"

#include "bson.h"
#include "bson_visit.h"

#include "BCDS_Orientation.h"

/* Sensor Handler for the Orientation Sensor */
Orientation_HandlePtr_T xdkOrientationSensor_Handle;

static enum mist_error sensor_read(mist_ep* ep, wish_protocol_peer_t* peer, int request_id);


static mist_ep mist_super_ep = {.id = "mist", .label = "", .type = MIST_TYPE_STRING, .read = sensor_read };
static mist_ep mist_name_ep = {.id = "name", .label = "Name", .type = MIST_TYPE_STRING, .read = sensor_read };

static mist_ep accelerometer_ep = {.id = "accelerometer", .label = "Accelerometer" , .type = MIST_TYPE_STRING, .read = sensor_read };
static mist_ep accelerometer_x_ep = {.id = "x", .label = "Accelerometer x" , .unit = "m/s²", .type = MIST_TYPE_FLOAT, .read = sensor_read };
static mist_ep accelerometer_y_ep = {.id = "y", .label = "Accelerometer y" , .unit = "m/s²", .type = MIST_TYPE_FLOAT, .read = sensor_read };
static mist_ep accelerometer_z_ep = {.id = "z", .label = "Accelerometer z" , .unit = "m/s²", .type = MIST_TYPE_FLOAT, .read = sensor_read };

static mist_ep orientation_ep = {.id = "orientation", .label = "Orientation" , .type = MIST_TYPE_STRING, .read = sensor_read };
static mist_ep orientation_heading_ep = {.id = "heading", .label = "Heading" , .unit = "°", .type = MIST_TYPE_FLOAT, .read = sensor_read };
static mist_ep orientation_pitch_ep = {.id = "pitch", .label = "Pitch" , .unit = "°", .type = MIST_TYPE_FLOAT, .read = sensor_read };
static mist_ep orientation_roll_ep = {.id = "roll", .label = "Roll" , .unit = "°", .type = MIST_TYPE_FLOAT, .read = sensor_read };
static mist_ep orientation_yaw_ep = {.id = "yaw", .label = "Yaw" , .unit = "°", .type = MIST_TYPE_FLOAT, .read = sensor_read };

static mist_ep environment_ep = {.id = "environment", .label = "Environment" , .type = MIST_TYPE_STRING, .read = sensor_read };
static mist_ep environment_rh_ep = {.id = "rh", .label = "Relative humidity" , .unit = "%",.type = MIST_TYPE_INT, .read = sensor_read };
static mist_ep environment_abs_hum_ep = {.id = "abs_hum", .label = "Absolute humidity" , .unit = "g/kg",.type = MIST_TYPE_FLOAT, .read = sensor_read };
static mist_ep environment_temp_ep = {.id = "t", .label = "Temperature" , .unit = "°C", .type = MIST_TYPE_FLOAT, .read = sensor_read };
static mist_ep environment_pres_ep = {.id = "p", .label = "Ambient pressure" , .unit ="kPa", .type = MIST_TYPE_INT, .read = sensor_read };

static enum mist_error sensor_read(mist_ep* ep, wish_protocol_peer_t* peer, int request_id) {
    size_t result_max_len = 256;
    uint8_t result[result_max_len];

    char full_epid[MIST_EPID_LEN];
    mist_ep_full_epid(ep, full_epid);

    bson bs;
    bson_init_buffer(&bs, result, result_max_len);

    WISHDEBUG(LOG_CRITICAL, "in sensor_read");
    /* FIXME: should use full_epid in name comparisons */
    if (ep == &orientation_heading_ep) {

    	Orientation_EulerData_T eulerValueInDegree = { 0 };
#if 0
    	Retcode_T returnEulerValue = Orientation_readEulerDegreeVal(&eulerValueInDegree);
    	if (RETCODE_SUCCESS == returnEulerValue) {

    	}
#endif
    	bson_append_start_object(&bs, "data");
        bson_append_double(&bs, "heading", eulerValueInDegree.heading);
        bson_append_finish_object(&bs);
    }
    else if (ep == &orientation_pitch_ep) {
        bson_append_string(&bs, "data", "1.0.1");
    }
    else if (ep == &orientation_roll_ep) {
        bson_append_string(&bs, "data", "PORT_VERSION");
    }
    else if (ep == &orientation_yaw_ep) {
        int32_t reltime = 0; //user_get_uptime_minutes();
        bson_append_int(&bs, "data", reltime);
    }
    else if (ep == &mist_super_ep) {
        bson_append_string(&bs, "data", "");
    }
    else if (ep == &mist_name_ep) {
        bson_append_string(&bs, "data", "Sensors");
    }
    bson_finish(&bs);
  
    mist_read_response(ep->model->mist_app, full_epid, request_id, &bs);
    
    WISHDEBUG(LOG_CRITICAL, "returning from wifi_read");
    return MIST_NO_ERROR;
    
}

static wish_app_t *app;
static mist_app_t* mist_app;


static void init_app(wish_app_t* app, bool ready) {
    PORT_PRINTF("We are here in init_app!");
    
    if (ready) {
        PORT_PRINTF("API ready!");
    
    } else {
        PORT_PRINTF("API not ready!");
    }
}




void xdk_sensors_app_init(void) {
    WISHDEBUG(LOG_CRITICAL, "xdk_sensors_app_init");
    char *name = "Sensors";
    mist_app = start_mist_app();
    if (mist_app == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Failed creating wish app");
        return;
    }

    app = wish_app_create(name);

    if (app == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Failed creating wish app");
        return; 
    }
    WISHDEBUG(LOG_CRITICAL, "here3");
    wish_app_add_protocol(app, &(mist_app->protocol));
    mist_app->app = app;

    WISHDEBUG(LOG_CRITICAL, "Adding EPs");
    mist_ep_add(&(mist_app->model), NULL, &mist_super_ep);
    mist_ep_add(&(mist_app->model), mist_super_ep.id, &mist_name_ep);

    mist_ep_add(&(mist_app->model), NULL, &accelerometer_ep);
    mist_ep_add(&(mist_app->model), accelerometer_ep.id, &accelerometer_x_ep);
    mist_ep_add(&(mist_app->model), accelerometer_ep.id, &accelerometer_y_ep);
    mist_ep_add(&(mist_app->model), accelerometer_ep.id, &accelerometer_z_ep);

    mist_ep_add(&(mist_app->model), NULL, &orientation_ep);
    mist_ep_add(&(mist_app->model), orientation_ep.id, &orientation_heading_ep);
    mist_ep_add(&(mist_app->model), orientation_ep.id, &orientation_roll_ep);
    mist_ep_add(&(mist_app->model), orientation_ep.id, &orientation_pitch_ep);
    mist_ep_add(&(mist_app->model), orientation_ep.id, &orientation_yaw_ep);

    mist_ep_add(&(mist_app->model), NULL, &environment_ep);
	mist_ep_add(&(mist_app->model), environment_ep.id, &environment_rh_ep);
	mist_ep_add(&(mist_app->model), environment_ep.id, &environment_temp_ep);
	mist_ep_add(&(mist_app->model), environment_ep.id, &environment_pres_ep);
	mist_ep_add(&(mist_app->model), environment_ep.id, &environment_abs_hum_ep);


    app->ready = init_app;
     
    PORT_PRINTF("Commencing mist api init3\n");
    wish_app_connected(app, true);

#if 0
    Retcode_T returnValue = RETCODE_FAILURE;
    returnValue = Orientation_init(xdkOrientationSensor_Handle);
    if ( RETCODE_SUCCESS != returnValue) {
    	PORT_PRINTF("Could not init orientation virtual sensor\n");
    }
#endif

    PORT_PRINTF("exiting xdk_sensors_app_init");
}
