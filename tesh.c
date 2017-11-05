#include <readline/readline.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "tesh.h"

/*
 * DEBUG
 */
void tesh_debug_print_cmds(Command *cmds) {
    int         i;
    Command    *current_cmd;

    current_cmd = cmds;

    while(current_cmd) {
        printf("[cmd] ");

        if(current_cmd->args) {
            for (i = 0; current_cmd->args[i]; i++) {
                printf("(%d) %s ", i, current_cmd->args[i]);
            }

            printf("\n");
        }

        if(current_cmd->link) {
            printf("%d\n", current_cmd->link);
        }

        if(current_cmd->background) {
            printf("Background\n");
        }

        if(current_cmd->stdin) {
            printf("[stdin] %s\n", current_cmd->stdin);
        }

        if(current_cmd->stdout) {
            printf("[stdout] %s", current_cmd->stdout);

            if(current_cmd->stdout_append) {
                printf(" (append)");
            }

            printf("\n");
        }

        if(current_cmd->stderr) {
            printf("[stderr] %s\n", current_cmd->stderr);
        }

        current_cmd = current_cmd->next;
    }
}

/*
 * TESH
 */

int isquote(int c) {
    return (
            c == '\'' ||
            c == '"'
    );
}

int iskeychar(int c) {
    return (
            c == '<' ||
            c == '>' ||
            c == '|' ||
            c == '#' || // TODO : commentaires
            c == ';' ||
            c == '&'
    );
}

int isseparator(int c) {
    return (
            iskeychar(c) ||
            isspace(c)
    );
}

int tesh(int argc, char **argv) {
    char       *line;

    Program    *program;
    int         quit;

    quit = 0;

    while(!quit) {
        line = tesh_readline();

        // EOF
        if(!line) {
            break;
        }

        program = tesh_build_program(line);

        if(program) {
            if(program->root) {
#ifdef DEBUG
                tesh_debug_print_cmds(program->root);
#endif
            }

            if (program->last && program->last->args && strcmp(program->last->args[0], "quit") == 0) {
                quit = 1;
            }
            else
                execCmds(program);

            free_program(&program);
        }

        free(line);
    }

    return EXIT_SUCCESS;
}

char* tesh_readline() {
    char   *line;
    size_t  line_length;

    line = readline("~ ");

    // EOF
    if(line == NULL) {
        return NULL;
    }

    line_length = strlen(line);

    if(line_length > 0) {
        while (line[line_length - 1] == '\\') {
            char *line_append,
                    *line_tmp;

            line[line_length - 1] = 0;

            line_append = readline("> ");

            line_tmp = (char *) malloc(1 + line_length * sizeof(char));
            strcpy(line_tmp, line);
            free(line);

            line = (char *) malloc(1 + (line_length + strlen(line_append)) * sizeof(char));
            strcpy(line, line_tmp);
            free(line_tmp);

            strcat(line, line_append);
            free(line_append);

            line_length = strlen(line);
        }
    }

    return line;
}

void tesh_add_command(Program *program, Command* cmd) {
    if(!program->root) {
        program->root = cmd;
    }
    else {
        program->last->next = cmd;
    }

    program->last = cmd;
}

/**
 * Ajoute un argument 'arg' dans le tableau 'args' de
 * la commande 'cmd'. Le tableau se termine toujours par une
 * case avec la valeur '0' (comme argv de main)
 *
 * Si la commande ne contient aucun argument, initialise
 * le tableau 'args' avec une taille 'INIT_NB_ARGS'.
 * Le tableau est créé avec 'malloc', il doit être *free*
 *
 * Si le tableau 'args' est plein après l'insertion de 'arg'
 * il est aggrandi avec 'realloc' de 'EXTENDS_NB_ARGS' de sorte
 * à toujours avoir '0' comme dernière valeur.
 *
 * 'memset' est utilisé pour initialiser toutes les cases du
 * tableau avec la valeur '0', après 'malloc' et 'realloc'
 *
 * @param cmd
 * @param arg
 */
void tesh_add_arg(Command *cmd, char *arg) {
    if(!cmd->args) {
        cmd->args_size = INIT_NB_ARGS;
        cmd->args = (char**)malloc(cmd->args_size * sizeof(char*));
        memset(cmd->args, 0, cmd->args_size * sizeof(char*));
    }

    cmd->args[cmd->args_count] = (char *)malloc(1 + strlen(arg) * sizeof(char));
    strcpy(cmd->args[cmd->args_count], arg);
    cmd->args_count++;

    if(cmd->args_count == cmd->args_size) {
        int     old_args_size;

        old_args_size = cmd->args_size;
        cmd->args_size += EXTENDS_NB_ARGS;

        cmd->args = (char**)realloc(cmd->args, cmd->args_size * sizeof(char*));
        memset(cmd->args + old_args_size, 0, EXTENDS_NB_ARGS * sizeof(char*));
    }
}

void tesh_cmd_redirect(char **r, char *str) {
    if(!*r) {
        // TODO : erreur
    }

    // TODO : check si str pas un mot clé

    *r = (char*)malloc(1 + strlen(str) * sizeof(char));
    strcpy(*r, str);
}

/**
 *  Construit une liste chainée des commandes présentent dans
 * 'line'.
 *
 * Les commandes sont ajoutées dans l'ordre qu'elles sont lues.
 *
 * La valeur retournée est créée avec un 'malloc' elle doit
 * être *free*
 *
 * Si erreur pendant la constuction de la liste chainée, la
 * fonction retourne 'NULL'
 *
 * @param line
 * @return
 */
Program* tesh_build_program(char *line) {
    Program    *program;
    char       *token;
    int         parsing_error;

    parsing_error = 0;
    program = tesh_create_program();

    while(!parsing_error && (token = tesh_read_token(line))) {
        if(!program->root) {
            tesh_add_command(program, tesh_create_cmd());
        }

        if(strcmp(token, "|") == 0) {
            program->last->link = CMD_LINK_PIPE;
            tesh_add_command(program, tesh_create_cmd());
        }
        else if(strcmp(token, ";") == 0) {
            program->last->link = CMD_LINK_SEMICOLON;
            tesh_add_command(program, tesh_create_cmd());
        }
        else if(strcmp(token, "&&") == 0) {
            program->last->link = CMD_LINK_AND;
            tesh_add_command(program, tesh_create_cmd());
        }
        else if(strcmp(token, "||") == 0) {
            program->last->link = CMD_LINK_OR;
            tesh_add_command(program, tesh_create_cmd());
        }
        else if(strcmp(token, "<") == 0) {
            char   *redirect;

            redirect = tesh_read_token(line);
            tesh_cmd_redirect(&program->last->stdin, redirect);
            free(redirect);
        }
        else if(strcmp(token, ">") == 0) {
            char   *redirect;

            redirect = tesh_read_token(line);
            tesh_cmd_redirect(&program->last->stdout, redirect);
            free(redirect);
        }
        else if(strcmp(token, "&>") == 0) {
            char   *redirect;

            redirect = tesh_read_token(line);
            tesh_cmd_redirect(&program->last->stdout, redirect);
            tesh_cmd_redirect(&program->last->stderr, redirect);
            free(redirect);
        }
        else if(strcmp(token, ">>") == 0) {
            char   *redirect;

            redirect = tesh_read_token(line);
            tesh_cmd_redirect(&program->last->stdout, redirect);
            free(redirect);

            program->last->stdout_append = 1;
        }
        else if(strcmp(token, "&") == 0) {
            // TODO : qu'à la fin
            program->last->background = 1;
        }
        else {
            if(program->last->background) {
                parsing_error = 1;
                fprintf(stderr, "&\n");
            }
            else {
                tesh_add_arg(program->last, token);
            }
        }

        if(parsing_error) {
            free_program(&program);
        }

        free(token);
    }

    return program;
}

/**
 * Lit et retourne le prochain token de 'line'.
 *
 * *'line' est modifié en enlevant le token lu*
 *
 * Retourne NULL si :
 *  - le token à une taille de 0
 *  - 'line' est de taille 0
 *
 * Les token sont séparés par :
 *  - si le caractère testé par isspace() (<cctype.h>) retourne une valeur différente de 0 :
 *       + ' ', '\t', '\n', '\v', '\f' et '\r'
 *       + https://linux.die.net/man/3/isspace
 *  - si le caractère fait partie des caractères spéciaux :
 *       + '#' : commentaire
 *       + '&' :
 *          * exécuté une commande en background '&'
 *          * ET logique "&&"
 *          * redirection de stdout et stderr "&>"
 *       + '|' :
 *          * pipe plusieurs commandes
 *          * OU logique "||"
 *       + '<' : redirection de stdin '<'
 *       + '>' :
 *          * redirection de stdout '>'
 *          * redirection de stdout en append ">>"
 *       + ';' : plusieurs commandes en une ligne sans condition d'exécution ';'
 * @param line
 * @return
 */
char* tesh_read_token(char *line) {
    if(line == NULL || strlen(line) == 0) {
        return NULL;
    }

    char   *token,
            quote;

    size_t  i,
            token_begin,
            token_end,
            token_length;

    quote = 0;
    i = 0;

    /*
     * Passe tout les "espaces" inutiles
     * TODO : Tester la range des charactères, ASCII ? UTF ?
     */
    while(isspace(line[i]) || line[i] < 0) {
        i++;
    }

    token_begin = i;

    if(iskeychar(line[i])) {
        switch(line[i]) {
            case '&':
                switch(line[i + 1]) {
                    // Token &> pour rediriger stdout et sterr
                    case '>':
                        i++;
                        break;
                        // Token && (cmd1 && cmd2)
                    case '&':
                        i++;
                        break;
                        // Token & pour les exécutions en arrière plan
                    default:
                        break;
                }
                break;
                // Token > pour rediriger stdout
            case '>':
                // Token >> pour rediriger sur stdout en append
                if(line[i + 1] == '>') {
                    i++;
                }
                break;
                // Token | pour pipe plus commandes (cmd1 | cmd2)
            case '|':
                // Token || (cmd1 || cmd2)
                if(line[i + 1] == '|') {
                    i++;
                }
                break;
                // Autre tokens spéciaux d'un caractère
            default:
                break;
        }

        i++;
    }
    else {
        while (line[i] != 0 && (quote || !isseparator(line[i]) || (i > 0 && isseparator(line[i]) && line[i - 1] == '\\'))) {
            if(isquote(line[i])) {
                if(quote && ((i == 0 && line[i] == quote) || (i > 0 && line[i - 1] != '\\' && line[i] == quote))) {
                    quote = 0;
                }
                else if(!quote) {
                    quote = line[i];
                }
            }

            i++;
        }
    }

    token_end = i;
    token_length = token_end - token_begin;

    if(token_length == 0) {
        return NULL;
    }

    token = (char*)malloc(1 + token_length * sizeof(char));
    strncpy(token, line + token_begin, token_length);
    token[token_length] = 0;

    // Supprime le token lu de 'line'
    memmove(line, line + token_end, strlen(line) - token_end + 1);

    return token;
}

Program* tesh_create_program() {
    Program    *program;

    program = (Program*)malloc(sizeof(Program));
    program->root = NULL;
    program->last = NULL;

    return program;
}

Command* tesh_create_cmd() {
    Command    *tmp_cmd;

    tmp_cmd = (Command*)malloc(sizeof(Command));
    tmp_cmd->stdin = NULL;
    tmp_cmd->stdout = NULL;
    tmp_cmd->stderr = NULL;
    tmp_cmd->next = NULL;
    tmp_cmd->args = NULL;
    tmp_cmd->link = CMD_LINK_NONE;
    tmp_cmd->args_count = 0;
    tmp_cmd->stdout_append = 0;
    tmp_cmd->args_size = 0;
    tmp_cmd->background = 0;

    return tmp_cmd;
}

void free_program(Program **program) {
    if(!*program) {
        return;
    }

    free_cmds(&(*program)->root);
    free(*program);
    *program = NULL;
}

void free_cmd(Command **cmd) {
    if(!*cmd) {
        return;
    }

    if ((*cmd)->args) {
        free_args((*cmd)->args);
    }

    if ((*cmd)->stdin) {
        free((*cmd)->stdin);
    }

    if ((*cmd)->stdout) {
        free((*cmd)->stdout);
    }

    if ((*cmd)->stderr) {
        free((*cmd)->stderr);
    }

    free(*cmd);
    *cmd = NULL;
}

void free_cmds(Command **cmds) {
    Command    *current_cmd,
            *tmp_cmd;

    current_cmd = *cmds;

    while(current_cmd) {
        tmp_cmd = current_cmd->next;
        free_cmd(&current_cmd);
        current_cmd = tmp_cmd;
    }

    *cmds = NULL;
}

void free_args(char **args) {
    int     i;

    for(i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }

    free(args);
}

int execCmds(Program *prg) {
    if(prg->root==NULL)
        return 0;

    switch(prg->root->link){
        case CMD_LINK_PIPE:
            return handlePipe(prg);
        case CMD_LINK_OR:
            return handleCdt(prg,"||",1);
        case CMD_LINK_AND:
            return handleCdt(prg,"&&",-1);
        case CMD_LINK_SEMICOLON:
            return handleCdt(prg,";",0);
    }

    if(prg->root->background)
        return handleBackground(prg->root);
	
	return execCmd(prg->root,NULL);
}

int execCmd(Command *cmd, pid_t* pid) {
    pid_t p;
	int status;
	
    if (!strcmp("cd", cmd->args[0]))
        return cd(cmd->args[1]);
    if (!strcmp("fg", cmd->args[0]))
        return fg(cmd->args[1]);


    if (pid==NULL ? !(p=fork()) : !(*pid=fork()) ) {
        if (cmd->stdin != NULL) {
            int file = open(cmd->stdin, O_RDONLY);
            if(file==-1){
                perror("open");
            }
            dup2(file, 0);
            close(file);
        }
        if (cmd->stdout != NULL) {
            int file = open(cmd->stdout, (cmd->stdout_append ? O_APPEND : O_TRUNC) | O_WRONLY | O_CREAT, 0644);

            if(file==-1){
                perror("open");
            }

            dup2(file, 1);
            close(file);
        }
        execvp(cmd->args[0], cmd->args);
        exit(-1);
    }
    if(pid==NULL)
        waitpid(p,&status,0);
	
    if(pid!=NULL)
        return 0;
    if(WIFEXITED(status)){
        int code = WEXITSTATUS(status);
        if(code==255)
            printf("%s: command not found\n",cmd->args[0]);
		
        return code;
    }
    else
        printf("Command hasn't exited normally.\n");
    return -1;
}

int handlePipe(Program *prg)
{
    pid_t pid;
	Command* cmd=prg->root->next;

    prg->root->link = CMD_LINK_NONE;

    if(!(pid=fork())){
        int fd[2];
        pid_t pid2;
        pipe(fd);
		
		if(fd[0]==-1 || fd[1]==-1){
            perror("pipe");
		}

        if(!(pid2=fork())){
            dup2(fd[1],1);
            close(fd[0]);
            close(fd[1]);
            if(execCmd(prg->root,NULL)){//If error
                exit(-1);
            }
            exit(0);
        }
        close(fd[1]);
        dup2(fd[0],0);
        close(fd[0]);
        waitpid(pid2,NULL,0);
		
		free_cmd(&prg->root);
		prg->root=cmd;
		
        if(execCmds(prg)){//If error
            exit(-1);
        }
        exit(0);
    }
    waitpid(pid,NULL,0);

    return 0;
}

int handleCdt(Program *prg, char* delim, int d) {
    int es1;
	Command* cmd=prg->root->next;

    prg->root->link = CMD_LINK_NONE;

    es1 = execCmd(prg->root,NULL);
	
	free_cmd(&prg->root);
	prg->root=cmd;

    if(!d || (d>0&&es1) || (d<0&&!es1) )
        return execCmds(prg);

    return 0;
}

void * endBackgroundCallback(void *arg) {
    pid_t pid;
    int code,status;
    Command* cmd = arg;

    code = execCmd(cmd,&pid);

    printf("[%d]\n",pid);

    waitpid(pid,&status,0);

    if(WIFEXITED(status)) {
        code = WEXITSTATUS(status);
        if(code==255)
            printf("%s: command not found\n",cmd->args[0]);
    }
    else
        code = -1;

    printf("[%d->%d]",pid,code);
	
	free_cmd(&cmd);

    return NULL;
}

int fg(char* pid) {
    int status = 0;
    if(pid==NULL)
        wait(&status);
    else
        waitpid(atoi(pid),&status,0);
    if(WIFEXITED(status))
        return WEXITSTATUS(status);

    return -1;
}

void copyCommand(Command *cmd, const Command *ref) {
    if(ref->stdin!=NULL){
        cmd->stdin = malloc( (strlen(ref->stdin)+1)*sizeof(char));
        strcpy(cmd->stdin,ref->stdin);
    }
	else
		cmd->stdin = NULL;
    if(ref->stdout!=NULL){
        cmd->stdout = malloc( (strlen(ref->stdout)+1)*sizeof(char));
        strcpy(cmd->stdout,ref->stdout);
    }
	else
		cmd->stdout = NULL;
    if(ref->stderr!=NULL){
        cmd->stderr = malloc( (strlen(ref->stderr)+1)*sizeof(char));
        strcpy(cmd->stderr,ref->stderr);
    }
	else
		cmd->stderr = NULL;
    if(ref->args_size) {
        int i;
        cmd->args = malloc(ref->args_size*sizeof(char*));
        for(i=0;i<ref->args_size && ref->args[i]!=NULL;i++) {
			cmd->args[i] = malloc( (strlen(ref->args[i])+1)*sizeof(char));
			strcpy(cmd->args[i],ref->args[i]);
        }
		for(;i<ref->args_size;i++)
			cmd->args[i] = NULL;
        cmd->args_size = ref->args_size;
    }
    cmd->link = ref->link;
    cmd->args_count = ref->args_count;
    cmd->background = ref->background;
    cmd->stdout_append = ref->stdout_append;
}

int handleBackground(Command* cmd) {
    pthread_t ptr;
	Command* cmd2 = malloc(sizeof(Command));

	copyCommand(cmd2,cmd);

    pthread_create(&ptr,NULL,&endBackgroundCallback,cmd2);
	
    return 0;
}

int cd(char *dir) {
    char *cwd = getcwd(NULL, 0);
    if (!strcmp(dir, ".")) return 0;
    if (!strcmp(dir, ".."))
        *strrchr(cwd, '/') = '\0';
    else {
        if (strchr(dir, '~') != NULL) {
            cd(getenv("HOME"));
            *strchr(dir, '~')='.';
            free(cwd);
            cwd = getcwd(NULL, 0);
        }
        if (dir[0] == '/')
            strcpy(cwd, dir);
        else {
            char *fcwd = malloc((strlen(cwd) + strlen(dir) + 2) * sizeof(char));
            sprintf(fcwd, "%s/", cwd);
            free(cwd);
            strcat(fcwd, dir);
            cwd = fcwd;
        }
    }

    if (!chdir(cwd)) {
        free(cwd);
        return 0;
    }

    return -1;
}