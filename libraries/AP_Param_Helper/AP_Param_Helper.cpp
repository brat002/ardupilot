#include "AP_Param_Helper.h"


const AP_Param::GroupInfo AP_Param_Helper::var_info[] = {
// only if board defines parameters
#ifdef BOARD_HAL_VARINFO
    BOARD_HAL_VARINFO,
#endif
    AP_GROUPEND
};


extern const AP_HAL::HAL& hal;

AP_Param_Helper::AP_Param_Helper(bool f){
    f=!f;

    hal_param_helper=this;
    AP_Param::setup_object_defaults(this, var_info); // setup all params
}


AP_Param_Helper * hal_param_helper;
