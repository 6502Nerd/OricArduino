#define     PIN_PA0     2
#define     PIN_PA1     3
#define     PIN_PA2     4
#define     PIN_PA3     5
#define     PIN_PA4     6
#define     PIN_PA5     7
#define     PIN_PA6     8
#define     PIN_PA7     9
#define     PIN_STB     21
#define     PIN_ACK     20

#define     BUSY        digitalWrite(PIN_PA5,LOW)
#define     READY       digitalWrite(PIN_PA5,HIGH)

enum dflat_states {
    st_initialise,
    st_wait_for_select,
    st_get_command,
    st_read,
    st_read_filename,
    st_read_byte,
    st_write,
    st_write_filename,
    st_write_byte,
    st_close_file,
    st_delete,
    st_delete_filename,
    st_directory
};

enum dflat_commands {
    cmd_openread,
    cmd_openwrite,
    cmd_close,
    cmd_delete,
    cmd_dir
};