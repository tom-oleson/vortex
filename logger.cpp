
#include "logger.h"

cm_log::multiplex_logger mx_log;
cm_log::rolling_file_logger app_log("./log/", "vortex", ".log", 
                            ((24 * 60) * 60) /*seconds*/, 6 /* retain # */);

void vortex::init_logs() {

    cm_log::console.set_log_level(cm_log::level::info);
    cm_log::console.set_date_time_format("%m/%d/%Y %H:%M:%S");
    cm_log::console.set_message_format("${date_time} [${lvl}]: ${msg}");

    app_log.set_log_level(cm_log::level::info);
    app_log.set_date_time_format("%m/%d/%Y %H:%M:%S");
    app_log.set_message_format("${date_time}${millis} [${lvl}] <${thread}>: ${msg}"); 

    mx_log.add(cm_log::console);
    mx_log.add(app_log);

    set_default_logger(&mx_log);
}
