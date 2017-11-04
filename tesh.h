#ifndef RS2017_VANTOUROUT_DEBENATH_TESH_H
#define RS2017_VANTOUROUT_DEBENATH_TESH_H

#define INIT_NB_ARGS        10
#define EXTENDS_NB_ARGS     5

#define CMD_LINK_NONE       0
#define CMD_LINK_PIPE       1
#define CMD_LINK_OR         2
#define CMD_LINK_AND        3
#define CMD_LINK_SEMICOLON  4

typedef struct Program {
    struct Command *root,
            *last;
} Program;

typedef struct Command {
    char    *stdin,
            *stdout,
            *stderr,
            **args,
            link;

    int     background,
            stdout_append,
            args_count,
            args_size;

    struct Command  *next;
} Command;

/*
 * DEBUG
 */
void        tesh_debug_print_cmds(Command *cmds);

/*
 * TESH
 */
int         isquote(int c);
int         iskeychar(int c);
int         isseparator(int c);
int         tesh(int argc, char **argv);
char*       tesh_readline();
char*       tesh_read_token(char *line);
Program*    tesh_build_program(char *line);
void        tesh_add_command(Program *program, Command* cmd);
void        tesh_cmd_redirect(char **r, char *str);
Command*    tesh_create_cmd();
Program*    tesh_create_program();
void        free_program(Program **program);
void        free_args(char **args);
void        free_cmd(Command **cmd);
void        free_cmds(Command **cmds);

#endif //RS2017_VANTOUROUT_DEBENATH_TESH_H