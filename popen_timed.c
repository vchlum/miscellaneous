#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct pinfo {
    FILE         *file;
    pid_t         pid;
    struct pinfo *next;
} pinfo;

static pinfo *plist = NULL;

FILE*
popen_timed(const char *command, int timeout,const char *mode)
{
    int fd[2];
    pinfo *cur, *old;

    if (mode[0] != 'r' && mode[0] != 'w') {
        errno = EINVAL;
        return NULL;
    }

    if (mode[1] != 0) {
        errno = EINVAL;
        return NULL;
    }

    if (pipe(fd)) {
        return NULL;
    }

    cur = (pinfo *) malloc(sizeof(pinfo));
    if (! cur) {
        errno = ENOMEM;
        return NULL;
    }

    cur->pid = fork();
    switch (cur->pid) {
        
    case -1:                    /* fork() failed */
        close(fd[0]);
        close(fd[1]);
        free(cur);
        return NULL;
        
    case 0:                     /* child */
        for (old = plist; old; old = old->next) {
            close(fileno(old->file));
        }
        
        if (mode[0] == 'r') {
            dup2(fd[1], STDOUT_FILENO);
        } else {
            dup2(fd[0], STDIN_FILENO);
        }
        close(fd[0]);   /* close other pipe fds */
        close(fd[1]);
        
        execl("/bin/sh", "sh", "-c", command, (char *) NULL);
        _exit(1);

    default:                    /* parent */
        if (mode[0] == 'r') {
            close(fd[1]);
            if (!(cur->file = fdopen(fd[0], mode))) {
                close(fd[0]);
            }
        } else {
            close(fd[0]);
            if (!(cur->file = fdopen(fd[1], mode))) {
                close(fd[1]);
            }
        }
        cur->next = plist;
        plist = cur;
    }

    return cur->file;
}


int main() {
	char cmd[500];
	char buf[5000];
	
	FILE *fp;
	
	strcpy(cmd, "sleep 5; ls -l");

	if ((fp = popen_timed(cmd, 30, "r")) == NULL) {
		return -1;
	}
	
	while (fgets(buf, 5000, fp) != NULL) {
		printf(buf);
	}
	
	if(pclose(fp))  {
		return -2;
	}
}
