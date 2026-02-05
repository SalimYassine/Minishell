#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

volatile sig_atomic_t fg_pid = 0;

//Handlers de signaux 
void handle_sigint(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\n", 1);
    fflush(stdout);
}

void handle_sigtstp(int sig) {
    (void)sig; 
    write(STDOUT_FILENO, "\n", 1);
    fflush(stdout);
}

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            printf("\n[%d] Terminé (code %d)\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("\n[%d] Tué par signal %d\n", pid, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("\n[%d] Suspendu (signal %d)\n", pid, WSTOPSIG(status));
        } else if (WIFCONTINUED(status)) {
            printf("\n[%d] Repris\n", pid);
        }
    }
    fflush(stdout);
}

// Fonctionnalités TP4 
void list_directory(const char *path) {
    DIR *dir = opendir(path ? path : ".");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(dir);
}

// Fonctionnalités TP5 
void executerPipeline(struct cmdline *commande) {
    int nb_cmds = 0;
    while (commande->seq[nb_cmds]) nb_cmds++;
    
    int (*pipes)[2] = malloc((nb_cmds - 1) * sizeof(int[2]));
    
    // Création des tubes
    for (int i = 0; i < nb_cmds - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < nb_cmds; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { // Processus fils
            // Redirections E/S
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
                close(pipes[i-1][0]);
            }
            if (i < nb_cmds - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
                close(pipes[i][1]);
            }
            
            // Fermer tous les descripteurs inutilisés
            for (int j = 0; j < nb_cmds - 1; j++) {
                if (j != i-1) close(pipes[j][0]);
                if (j != i) close(pipes[j][1]);
            }
            
            // Exécution avec gestion des redirections
            if (commande->in) {
                int fd_in = open(commande->in, O_RDONLY);
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if (commande->out) {
                int fd_out = open(commande->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
            
            execvp(commande->seq[i][0], commande->seq[i]);
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    
    // Fermeture des tubes dans le père
    for (int i = 0; i < nb_cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Attente des fils
    for (int i = 0; i < nb_cmds; i++) {
        wait(NULL);
    }
    
    free(pipes);
}

/* Exécution commandes simples */
void executerCommande(char **cmd, int is_background, const char *in_file, const char *out_file) {
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Gestion des redirections
        if (in_file) {
            int fd_in = open(in_file, O_RDONLY);
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        if (out_file) {
            int fd_out = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        // Configuration des signaux
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        
        sigset_t unblock;
        sigemptyset(&unblock);
        sigaddset(&unblock, SIGINT);
        sigaddset(&unblock, SIGTSTP);
        sigprocmask(SIG_UNBLOCK, &unblock, NULL);

        if (is_background) {
            setpgid(0, 0);
        }

        execvp(cmd[0], cmd);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        if (is_background) {
            printf("[%d] En arrière-plan\n", pid);
        } else {
            fg_pid = pid;
            int status;
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
            fg_pid = 0;
        }
    }
}

//---> Programme principal 
int main(void) {
    bool fini = false;
    struct sigaction sa, sa_int, sa_tstp;

    // Configuration des signaux
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NODEFER;
    sigaction(SIGCHLD, &sa, NULL);

    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    // Masquage initial des signaux
    sigset_t block;
    sigemptyset(&block);
    sigaddset(&block, SIGINT);
    sigaddset(&block, SIGTSTP);
    sigprocmask(SIG_BLOCK, &block, NULL);
    sigprocmask(SIG_UNBLOCK, &block, NULL);

    while (!fini) {
        printf("> ");
        fflush(stdout);
        
        struct cmdline *commande = readcmd();
        
        if (commande == NULL) {
            if (errno == EINTR) continue;
            perror("readcmd");
            exit(EXIT_FAILURE);
        } else if (commande->err) {
            printf("Erreur: %s\n", commande->err);
        } else {
            int nb_cmds = 0;
            while (commande->seq[nb_cmds]) nb_cmds++;

            if (nb_cmds > 1) { // Pipeline détecté
                executerPipeline(commande);
            } else { // Commande simple
                int indexseq = 0;
                char **cmd;
                
                while ((cmd = commande->seq[indexseq])) {
                    if (cmd[0]) {
                        if (strcmp(cmd[0], "exit") == 0) {
                            fini = true;
                        }
                        else if (strcmp(cmd[0], "stop") == 0) {
                            // ... (code existant pour 'stop')
                        }
                        else if (strcmp(cmd[0], "cont") == 0) {
                            // ... (code existant pour 'cont')
                        }
                        else if (strcmp(cmd[0], "cd") == 0) {
                            char *path = cmd[1] ? cmd[1] : getenv("HOME");
                            if (chdir(path) == -1) {
                                perror("chdir");
                            }
                        }
                        else if (strcmp(cmd[0], "dir") == 0) {
                            list_directory(cmd[1]);
                        }
                        else {
                            int is_background = (commande->backgrounded != NULL);
                            executerCommande(cmd, is_background, commande->in, commande->out);
                        }
                    }
                    indexseq++;
                }
            }
        }
    }
    printf("Au revoir !\n");
    return EXIT_SUCCESS;
}