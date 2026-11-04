#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** The serial CLI's "mem" command (message memories); see cli.c. */
int cli_mem_cmd(int argc, char **argv);

/** The serial CLI's "send" command: key arbitrary text once; see cli.c. */
int cli_send_cmd(int argc, char **argv);

#ifdef __cplusplus
}
#endif
