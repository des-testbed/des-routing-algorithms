#ifndef LSR_CLI
#define LSR_CLI
#include <dessert.h>

int cli_set_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_hello_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_tc_interval(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_refresh_list(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_refresh_list(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_set_refresh_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_refresh_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_rt(struct cli_def* cli, char* command, char* argv[], int argc);
int cli_show_nt(struct cli_def* cli, char* command, char* argv[], int argc);

#endif
