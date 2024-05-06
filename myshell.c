#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

/*
Called before the first invocation of process_arglist()
input- None (Void)
output- 0 on success, exit with 1 on error
*/
int prepare(void)
{
    /*
    After this part, the Shell won't terminate upon SIGIN (we will change it in foreground child)
    inspired by CodeValut: https://www.youtube.com/watch?v=jF-1eFhyz1U&t
    */
    struct sigaction shell_sa; /*A structure used to modify the action taken by a process on receipt of a signal.*/
    shell_sa.sa_handler = SIG_IGN; /*Set up a signal handler to indicate that the corresponding signal should be ignored.*/
    shell_sa.sa_flags = SA_RESTART;  /*Returning from a handler resumes the library function.*/
    if (sigaction(SIGINT, &shell_sa, NULL) == -1)
    {
        /*Try to initialize the signal set to exclude all signals, if doen't work, gets here*/
        fprintf(stderr, "Error: sigaction failed - %s\n", strerror(errno));
        exit(1);
    }
    
    /*
    Zombie handler- The purpose of this particular signal handler is to avoid the creation of zombie processes
    inspired by CodeValut: https://www.youtube.com/watch?v=jF-1eFhyz1U&t
    */
    struct sigaction zombie_sa; /*A structure used to modify the action taken by a process on receipt of a signal.*/
    zombie_sa.sa_handler = SIG_IGN; /*Set up a signal handler to indicate that the corresponding signal should be ignored.*/
    zombie_sa.sa_flags = SA_RESTART;  /*Returning from a handler resumes the library function.*/
    if (sigaction(SIGCHLD, &zombie_sa, NULL) == -1)
    {
        /*Try to initialize the signal set to exclude all signals, if doen't work, gets here*/
        fprintf(stderr, "Error: sigaction failed - %s\n", strerror(errno));
        exit(1);
    }
    return 0;
}

/*
Make the foreground child handel set to SIG_DFL in the cases of regular commands or parts of pipe
input- None (Void)
output- 0 on success, exit with 1 on error/failure
inspired by CodeValut: https://www.youtube.com/watch?v=jF-1eFhyz1U&t
*/
int foreground_handler(void){    
    struct sigaction foreground_sa; /*A structure used to modify the action taken by a process on receipt of a signal.*/
    foreground_sa.sa_handler = SIG_DFL; /*Set up a signal handler to indicate that the corresponding signal should return to default.*/
    foreground_sa.sa_flags = SA_RESTART;  /*Returning from a handler resumes the library function.*/
    if (sigaction(SIGINT, &foreground_sa, NULL) == -1)
    {
        /*Try to initialize the signal set to exclude all signals, if doen't work, gets here*/
        fprintf(stderr, "Error: sigaction failed - %s\n", strerror(errno));
        exit(1);
    }
    return 0;
}

/*
help function for cases; 03: Single piping, case 04: Input redirecting, case 05: Output redirecting- help to find were to divid the array
input- arglist- single line from the parsed command line given by the user, divide_by- the char that divide the arglist to 2 parts
output- int representing the place of the divide_by word in arglist
*/
int place_to_divided(char** arglist, const char* divide_by){
    int count = 0;
    while (strcmp(arglist[count], divide_by) != 0) {
        /*while the word is not the divide_by word*/
		++count;
	}
    return count;
}

/*
find a case: case 01: Executing Commands, case 02: Executing Commands in the background, case 03: Single piping,
case 04: Input redirecting, case 05: Output redirecting
input- arglist- single line from the parsed command line given by the user
output- case number (1-5) 
*/
int find_case(char** arglist){

    for (int i = 0; arglist[i] != NULL; i++)
    {
        if(strchr(arglist[i], '&') != NULL){
            return 2;
        }
        else if(strchr(arglist[i], '|') != NULL){
            return 3;
        }
        else if(strchr(arglist[i], '<') != NULL){
            return 4;
        }
        else if(strchr(arglist[i], '>') != NULL){
            return 5;
        }
    }
    return 1;
}

/*
Called before the shell exits. Didn't use it in my assignment
input- None (Void)
output- 0 on success, any other value on error
*/
int finalize(void)
{
    return 0;
}

/*
excutes the command given by the user from the 5 types of commands (5 cases) given in the assignment
input- count- the size of arglist - 1
       arglist- the parsed command line given by the user, the last entry (arglist[count]) is always NULL
output- 1 if no error occurs, 0 + print error otherwise
*/
int process_arglist(int count, char** arglist)
{
    if (count <= 0)
    {
        printf("Error: No command given\n");
        return 0;
    }
    
    char* current_command = arglist[0];
    int pid;
    int status;
    int case_number = find_case(arglist); /*find the case number*/

    if (case_number == 1){
    /*Case 01: Executing Commands- program and its argument. Shell excutes and waits until it completes before accepting another command*/
        pid = fork(); /*Try to create a son*/
        if (pid == 0) /*Child process*/
        {
            foreground_handler();/*This is a foreground child as mention in the assignment, change signal handler appropriately*/
            execvp(current_command, arglist); /*Replace the current process with a new one*/
            /*If execvp succeed- will never get here*/
            fprintf(stderr, "Error: execvp failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
            exit(1); /*output that there is an error*/
        }
        else if (pid > 0) /*Parent process*/
        {
            if (waitpid(pid, &status, WUNTRACED) == -1) /*Wait for the child to finish + also return if a child has stopped, in addition to returning when a child has exited.*/
            {
                if (errno != ECHILD && errno != EINTR) /*For any error that it no the one mention in the assignmnent (no child)*/
                {
                    fprintf(stderr, "Error: waitpid failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                    return 0; /*output that there is an error*/
                }
            }
        }
        else /*Some error happened during fork()*/
        {
            fprintf(stderr, "fork error: %s\n", strerror(errno)); /*print suitable messege for the error*/
            return 0; /*output that there is an error*/
        }
        return 1; /*case 01 successfully completed!*/
    }

    else if(case_number == 2){
        /*Case 02: Executing Commands in the background- command followed by &. 
        Shell excutes BUT doesn't wait until it completes before accepting another command*/
        pid = fork();
        if(pid == 0) /*Child process*/
        {
            arglist[count - 1] = NULL; /*Remove the & from the command*/
            execvp(current_command, arglist); /*Replace the current process with a new one*/
            /*If execvp succeed- will never get here*/
            fprintf(stderr, "Error: execvp failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
            exit(1); /*output that there is an error*/
        }
        else if (pid < 0)/*Some error happened during fork()*/
        {
            fprintf(stderr, "fork error: %s\n", strerror(errno)); /*print suitable messege for the error*/
            return 0; /*output that there is an error*/
        }
        /*pid > 0 ignored because parent don't have to wait to the child in this case*/
        return 1; /*case 02 successfully completed!*/
    }

    else if(case_number == 3){
    /*Case 03: Single piping- 2 command separated by |. Shell excutes both commands so the output of the first in the input of the second
    inspired by CodeValut: https://www.youtube.com/watch?v=6xbLgZpOBi8*/
        int pipefd[2]; /*pipefd[0] is the read end of the pipe, pipefd[1] is the write end of the pipe*/
        int pid1;
        int pid2;
        int status1;
        int status2;
        int divide_place = place_to_divided(arglist, "|"); /*find the place of the | in the arglist*/
        arglist[divide_place] = NULL; /*Remove the | from the command*/
        
        if (pipe(pipefd) == -1) /*Try to initialize the pipe, if doen't work, gets here*/
        {
            fprintf(stderr, "Error: pipe failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
            return 0; /*output that there is an error*/
        }
        pid1 = fork(); /*Try to create a son*/
        if(pid1 <  0){
            fprintf(stderr, "fork error: %s\n", strerror(errno)); /*print suitable messege for the error*/
            return 0; /*output that there is an error*/
        }
        if (pid1 == 0) /*child1 process*/
        {
            foreground_handler();/*This is a foreground child as mention in the assignment, change signal handler appropriately*/
            close(pipefd[0]); /*Close the read end of the pipe*/
            if(dup2(pipefd[1], STDOUT_FILENO) == -1) /*Duplicate the write end of the pipe to the standard output*/
            {
                /*dup2 faild*/
                fprintf(stderr, "Error: dup2 failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                exit(1); /*output that there is an error*/
            }
            close(pipefd[1]); /*Close the write end of the pipe, we replace this reference with reference make by dup2()*/
            execvp(arglist[0], arglist); /*Replace the current process with a new one*/
            /*If execvp succeed- will never get here*/
            fprintf(stderr, "Error: execvp failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
            exit(1); /*output that there is an error*/
        }
        pid2 = fork(); /*Try to create a son*/
        if(pid2 <  0){
            fprintf(stderr, "fork error: %s\n", strerror(errno)); /*print suitable messege for the error*/
            return 0; /*output that there is an error*/
        }
        if (pid2 == 0) /*child2 process*/
        {
            foreground_handler();/*This is a foreground child as mention in the assignment, change signal handler appropriately*/
            close(pipefd[1]); /*Close the write end of the pipe*/
            if(dup2(pipefd[0], STDIN_FILENO) == -1) /*Duplicate the read end of the pipe to the standard input*/
            {
                /*dup2 faild*/
                fprintf(stderr, "Error: dup2 failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                exit(1); /*output that there is an error*/
            }
            close(pipefd[0]); /*Close the read end of the pipe, we replace this reference with reference make by dup2()*/
            execvp(arglist[divide_place + 1], &arglist[divide_place + 1]); /*Replace the current process with a new one
            using the help function to find out from were the second part begins*/
            /*If execvp succeed- will never get here*/
            fprintf(stderr, "Error: execvp failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
            exit(1); /*output that there is an error*/
        }
        /*Parent process*/
        close(pipefd[0]); /*Close the read end of the pipe*/
        close(pipefd[1]); /*Close the write end of the pipe*/
        if (waitpid(pid1, &status1, WUNTRACED) == -1) /*Wait for the child1 to finish + also return if a child has stopped, in addition to returning when a child has exited.*/
        {
            if (errno != ECHILD && errno != EINTR) /*For any error that it no the one mention in the assignmnent (no child)*/
            {
                fprintf(stderr, "Error: waitpid failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                return 0; /*output that there is an error*/
            }
        }
        if (waitpid(pid2, &status2, WUNTRACED) == -1) /*Wait for the child2 to finish + also return if a child has stopped, in addition to returning when a child has exited.*/
        {
            if (errno != ECHILD && errno != EINTR) /*For any error that it no the one mention in the assignmnent (no child)*/
            {
                fprintf(stderr, "Error: waitpid failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                return 0; /*output that there is an error*/
            }
        }
        return 1; /*case 03 successfully completed!*/
    }

    else if(case_number == 4){
    /*Case 04: Input redirecting- command < input file name. Shell excutes the command and take the input from the file
    inspired by CodeValut: https://www.youtube.com/watch?v=5fnVr-zH-SE that was used in case 05
    */
        int file;
        int divide_place = place_to_divided(arglist, "<"); /*find the place of the < in the arglist*/
        arglist[divide_place] = NULL; /*Remove the < from the command*/
        
        pid = fork(); /*Try to create a son*/
        if (pid == 0) /*Child process*/
        {
            foreground_handler();/*This is a foreground child as mention in the assignment, change signal handler appropriately*/
            file = open(arglist[divide_place + 1], O_RDONLY); /*open the file, read only mode*/
            if(file == -1) /*Try to open the file, if doen't work, gets here*/
            {
                fprintf(stderr, "Error: open failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                exit(1); /*output that there is an error*/
            }
            if (dup2(file, STDIN_FILENO) == -1 ){ /*Duplicate the file to the standard input*/
                /*dup2 faild*/
                fprintf(stderr, "Error: dup2 failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                exit(1); /*output that there is an error*/
            }
            close(file); /*Close the file, we replace this reference with reference make by dup2()*/
            execvp(arglist[0], arglist); /*Replace the current process with a new one*/
            /*If execvp succeed- will never get here*/
            fprintf(stderr, "Error: execvp failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
            exit(1); /*output that there is an error*/
        }
        else if (pid > 0) /*Parent process*/
        {
            if (waitpid(pid, &status, WUNTRACED) == -1) /*Wait for the child to finish + also return if a child has stopped, in addition to returning when a child has exited.*/
            {
                if (errno != ECHILD && errno != EINTR) /*For any error that it no the one mention in the assignmnent (no child)*/
                {
                    fprintf(stderr, "Error: waitpid failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                    return 0; /*output that there is an error*/
                }
            }
        }
        else /*Some error happened during fork()*/
        {
            fprintf(stderr, "fork error: %s\n", strerror(errno)); /*print suitable messege for the error*/
            return 0; /*output that there is an error*/
        }
        return 1; /*case 04 successfully completed!*/

    }
    else if(case_number == 5){
    /*Case 05: Output redirecting- comand >  output file name. Shell excutes the command and put the output in the file (make new file if necessary)
    inspired by CodeValut: https://www.youtube.com/watch?v=5fnVr-zH-SE
    */
        int file;
        int divide_place = place_to_divided(arglist, ">"); /*find the place of the > in the arglist*/
        arglist[divide_place] = NULL; /*Remove the > from the command*/

        pid = fork(); /*Try to create a son*/
        if (pid == 0) /*Child process*/
        {
            foreground_handler();/*This is a foreground child as mention in the assignment, change signal handler appropriately*/
            file = open(arglist[divide_place + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777 ); /*open the file, write only mode, if it doesn't exist, create it + everybody can read/write*/
            if(file == -1) /*Try to open the file, if doen't work, gets here*/
            {
                fprintf(stderr, "Error: open failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                exit(1); /*output that there is an error*/
            }
            if (dup2(file, STDOUT_FILENO) == -1 ){ /*Duplicate the file to the standard output*/
                /*dup2 faild*/
                fprintf(stderr, "Error: dup2 failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                exit(1); /*output that there is an error*/
            }
            close(file); /*Close the file, we replace this reference with reference make by dup2()*/
            execvp(arglist[0], arglist); /*Replace the current process with a new one*/
            /*If execvp succeed- will never get here*/
            fprintf(stderr, "Error: execvp failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
            exit(1); /*output that there is an error*/
        }
        else if (pid > 0) /*Parent process*/
        {
            if (waitpid(pid, &status, WUNTRACED) == -1) /*Wait for the child to finish + also return if a child has stopped, in addition to returning when a child has exited.*/
            {
                if (errno != ECHILD && errno != EINTR) /*For any error that it no the one mention in the assignmnent (no child)*/
                {
                    fprintf(stderr, "Error: waitpid failed - %s\n", strerror(errno)); /*print suitable messege for the error*/
                    return 0; /*output that there is an error*/
                }
            }
        }
        else /*Some error happened during fork()*/
        {
            fprintf(stderr, "fork error: %s\n", strerror(errno)); /*print suitable messege for the error*/
            return 0; /*output that there is an error*/
        }
        return 1; /*case 05 successfully completed!*/
    }

    else {
        fprintf(stderr, "Error: case number not found\n"); /*print suitable messege for the error*/
        return 0; /*output that there is an error*/
    }
 
}
