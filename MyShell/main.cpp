#include <iostream>
#include <vector>
#include <string>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sstream>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <map>

using namespace std;

#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITERS " \t\r\n\a"
#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

struct pgid_with_size{
    pid_t pgid;
    int size;
    pgid_with_size(){
        pgid = 0;
        size = 0;
    }
};

vector<pgid_with_size> back_pgids;

struct command_segment{
    vector<string> args;
    pid_t pid;
    pid_t pgid;
    command_segment(){
        args.clear();
        pid = 0;
        pgid = 0;
    }
    void clear(void){
        args.clear();
        pid = 0;
        pgid = 0;
    }
};

struct command{
    list<command_segment> segment;
    int mode;
    command(){
        segment.clear();
        mode = FOREGROUND_EXECUTION;
    }
    void clear(void){
        segment.clear();;
        mode = FOREGROUND_EXECUTION;
    }
    int size(void){
        return this->segment.size();
    }
};

int cmd_cd(string path){
    int rtn_cd = chdir(path.c_str());
    if(rtn_cd == -1){
        cout << strerror(errno) <<endl;
        return -1;
    }
    return 0;
}

int cmd_exit(void){
    //clean up
    for(int i=0;i<back_pgids.size();++i){
        kill(-back_pgids[i].pgid,SIGINT);
    }
    //say goodbye!
    cout << "Goodbye!" <<endl;
    exit(0);
}

int cmd_kill(string pid_s){
    kill(atoi(pid_s.c_str()),SIGINT);
    return 0;
}

int cmd_fg(string pgid_s){
    int this_pid = getpid();
    int pgid = atoi(pgid_s.c_str());
    int index_pgid = -1;
    int wait_status;

    //find it in background_pigds
    for(int i=0;i<back_pgids.size();++i){
        if(back_pgids[i].pgid == pgid)
            index_pgid = i;
    }
    if(index_pgid == -1){
        cout << "pid/pgid not found" << endl;
        return -1;
    }

    //make it foreground
    if(tcsetpgrp(STDIN_FILENO,pgid) == -1){
        cerr << "tcsetpgrp error" <<endl;
        cerr << strerror(errno) <<endl;
    }
    //wait for pgid
    for(int i=0;i<back_pgids[index_pgid].size;++i){
        if(waitpid(-pgid,&wait_status,WUNTRACED) == -1 && errno != ECHILD){
            cerr << "waitpid wrong" <<endl;
            cerr << strerror(errno) <<endl;
        }
    }
    if(tcsetpgrp(STDIN_FILENO,this_pid) == -1){
        cerr << "tcsetpgrp error" <<endl;
        cerr << strerror(errno) <<endl;
    }

    back_pgids.erase(back_pgids.begin()+index_pgid);

    return 0;
}

void shell_command_parser(string &input,command &command_line){
    istringstream iss(input);
    string buffer;

    command_segment tmp_seg;
    while(iss >> buffer){
        if( buffer[buffer.size()-1] == '|' ){
            if(buffer.size() != 1){
                string buffer_without_l(buffer.c_str(),buffer.size()-1);
                tmp_seg.args.push_back(buffer_without_l);
            }
            command_line.segment.push_back(tmp_seg);
            tmp_seg.clear();
        }
        else if( buffer[buffer.size()-1] == '&'){
            if(buffer.size() != 1){
                string buffer_without_and(buffer.c_str(),buffer.size()-1);
                tmp_seg.args.push_back(buffer_without_and);
            }
            command_line.segment.push_back(tmp_seg);
            tmp_seg.clear();
            command_line.mode = BACKGROUND_EXECUTION;
        }
        else if(buffer[0] == '|'){
            command_line.segment.push_back(tmp_seg);
            tmp_seg.clear();

            string buffer_without_l(buffer.begin()+1,buffer.end());
            tmp_seg.args.push_back(buffer_without_l);
        }
        else{
            tmp_seg.args.push_back(buffer);
        }
    }
    if(!tmp_seg.args.empty())
        command_line.segment.push_back(tmp_seg);
    return;
}

int shell_exec_builtin(command_segment &segment){
    string &arg0 = segment.args[0];
    if(arg0 == "cd")
        cmd_cd(segment.args[1]);
    if(arg0 == "exit")
        cmd_exit();
    if(arg0 == "kill")
        cmd_kill(segment.args[1]);
    if(arg0 == "fg")
        cmd_fg(segment.args[1]);
    else
        return 0; // it's not a builtin-function
    return 1;
}

int shell_exec_segment(command_segment &segment, int in_fd, int out_fd, int mode, int pgid , vector<vector<int> > &fds){

    if(shell_exec_builtin(segment))
        return 0;

    char** args = new char*[segment.args.size()+1];
    for(int i=0;i<segment.args.size();++i){
        args[i] = new char[segment.args[i].size()+1];
        strcpy(args[i],segment.args[i].c_str());
    }
    args[ segment.args.size() ] = NULL;

    pid_t childpid = fork();
    if(childpid == -1){
        cout << "wrong at fork" <<endl;
        exit(0);
    }

    if(childpid == 0){//child
        //cout << "in_fd : " << in_fd << "\tout_fd : " << out_fd <<endl;

        if(in_fd != STDIN_FILENO && in_fd != -1){
            if(dup2(in_fd,STDIN_FILENO) == -1){
                cerr << strerror(errno) <<endl;
                exit(errno);
            }
        }

        if(out_fd != STDOUT_FILENO && out_fd != -1){
            if(dup2(out_fd,STDOUT_FILENO) == -1){
                cerr << strerror(errno) <<endl;
                exit(errno);
            }
        }

        //close all the fds
        for(int i=0;i<fds.size();++i){
            close(fds[i][0]);
            close(fds[i][1]);
        }

        if(execvp(segment.args[0].c_str(),args) == -1){
            cerr << strerror(errno) <<endl;
        }

        exit(errno);
    }
    else{//parent
        if(pgid == 0){//it's first segment
            if(setpgid(childpid,childpid) == -1){
                cout << "setpgid error" <<endl;
                cout << strerror(errno) <<endl;
            }
            segment.pgid = childpid;
            //cout << "pgid = " << childpid <<endl;
        }
        else{
            if(setpgid(childpid,pgid) == -1){
                cout << "setpgid error" <<endl;
                cout << strerror(errno) <<endl;
            }
            segment.pgid = pgid;
            //cout << "pgid = " << pgid <<endl;
        }
        segment.pid = childpid;

        cout << "Command executed by pid = " << childpid <<endl;

        for(int i=0;i<segment.args.size();++i)
            delete [] args[i];
        delete [] args;
        return 0;
    }

    return 0;
}

int shell_exec_command(command &command_line){
    int wait_status = 0;
    int rtn_exec = 0;
    vector< vector<int> > fds;
    list<command_segment>::iterator it = command_line.segment.begin();

    for(int i=0;i<command_line.size()-1;++i){
        vector<int> tmp_pipe(2,0);
        int tmp_fd[2];
        if(pipe(tmp_fd) == -1){
            cerr << "wrong at pipe()" <<endl;
            return -1;
        }
        tmp_pipe[0] = tmp_fd[0];
        tmp_pipe[1] = tmp_fd[1];
        fds.push_back(tmp_pipe);
    }
    for(int i=0;i<command_line.size();++i){
        int in_fd = STDIN_FILENO;
        int out_fd = STDOUT_FILENO;
        if(i != 0)
            in_fd = fds[i-1][0];
        if(i != command_line.size()-1)
            out_fd = fds[i][1];

        rtn_exec = shell_exec_segment(*it,in_fd,out_fd,command_line.mode,command_line.segment.front().pgid,fds);
        if(rtn_exec != 0){
            cerr << "rtn_exec = " << rtn_exec <<endl;
            break;
        }
        else{
            it++;
        }
    }
    //close all pipe in parent
    for(int i=0;i<fds.size();++i){
        close(fds[i][0]);
        close(fds[i][1]);
    }

    //wait for this group if mode is foreground
    if(command_line.mode == FOREGROUND_EXECUTION){
        pid_t child_pgid = command_line.segment.front().pgid;
        pid_t this_pid = getpid();

        //make it foreground
        if(tcsetpgrp(STDIN_FILENO,child_pgid) == -1){
            cerr << "tcsetpgrp error" <<endl;
            cerr << strerror(errno) <<endl;
        }
        //wait for pgid
        for(int i=0;i<command_line.size();++i){
            if(waitpid(-child_pgid,&wait_status,WUNTRACED) == -1 && errno != ECHILD){
                cerr << "waitpid wrong" <<endl;
                cerr << strerror(errno) <<endl;
            }
        }
        if(tcsetpgrp(STDIN_FILENO,this_pid) == -1){
            cerr << "tcsetpgrp error" <<endl;
            cerr << strerror(errno) <<endl;
        }
    }
    else if(command_line.mode == BACKGROUND_EXECUTION){
        pgid_with_size tmp_pgid_w_size;
        tmp_pgid_w_size.pgid = command_line.segment.front().pgid;
        tmp_pgid_w_size.size = command_line.segment.size();
        back_pgids.push_back(tmp_pgid_w_size);
    }

    command_line.clear();
    fds.clear();
    return 0;
}

void shell_print_promt(){
    char buffer[200];
    char username[200];
    getlogin_r(username,200);
    cout << username << " in " << getcwd(buffer,200) << endl;
    cout << "mysh>";
    return;
}

void shell_loop(){
    string input_command;
    command command_line;

    int status = 1;

    while(status >= 0){
        shell_print_promt();
        getline(cin,input_command);
        if(input_command.size() == 0)
            continue;
        shell_command_parser(input_command,command_line);
        status = shell_exec_command(command_line);
        input_command.clear();
    }
    return;
}

void shell_welcome(){
    cout << "Welcome to mysh by 0316313!" << endl;
    return;
}

void kill_foreground(int signum){
    fflush(stdin);
    shell_print_promt();
    return;
}
void do_nothing(int signum){
    return;
}
void make_shell_forground(int signum){
    int this_pgid = getpgid(0);
    //set parent to foreground
    if(tcsetpgrp(STDIN_FILENO,this_pgid) == -1){
        cerr << "tcsetpgrp error" <<endl;
        cerr << strerror(errno) <<endl;
    }
    return;
}

void deal_with_zombie(int signum){
    int status = 0;
    int zombie_pid = waitpid(-1,&status,0);
    if(zombie_pid == -1){
        cout << "error occures when dealing with zombie" <<endl;
        cout << strerror(errno) <<endl;
        exit(0);
    }
    return;
}

void shell_init(){
    //set this pgid
    setpgid(0,0);
    //set ctrl+C
    signal(SIGINT,&kill_foreground);
    //set ctrl+Z
    signal(SIGTSTP,&make_shell_forground);
    //set SIGCHLD for zombie
    signal(SIGCHLD,&deal_with_zombie);

    signal(SIGTTIN,SIG_IGN);
    signal(SIGTTOU,SIG_IGN);

    return;
}

int main()
{
    shell_init();
    shell_welcome();
    shell_loop();

    return 0;
}

