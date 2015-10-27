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

using namespace std;

#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITERS " \t\r\n\a"
#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

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
    cout << "Goodbye!" <<endl;
    exit(0);
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
    else
        return 0; // it's not a builtin-function
    return 1;
}

int shell_exec_segment(command_segment &segment, int in_fd, int out_fd, int mode, int pgid , vector<vector<int> > &fds){
    /*cout << "size : " <<segment.args.size() << " ";
    for(int i=0;i<segment.args.size();++i){
        cout << segment.args[i] << " ";
    }cout <<endl;*/

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
        cout << "in_fd : " << in_fd << "\tout_fd : " << out_fd <<endl;
        if(in_fd != STDIN_FILENO){
            if(dup2(in_fd,STDIN_FILENO) == -1){
                cerr << strerror(errno) <<endl;
                exit(errno);
            }
        }

        if(out_fd != STDOUT_FILENO){
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

        execvp(segment.args[0].c_str(),args);

        exit(errno);
    }
    else{//parent
        int status = 0;
        cout << "Command executed by pid=" << childpid <<endl;
        if(waitpid(childpid,&status,0) == -1){
            cerr << WEXITSTATUS(status) <<endl;
            return 1;
        }

        if(in_fd != STDIN_FILENO)
            close(in_fd);
        if(out_fd != STDOUT_FILENO)
            close(out_fd);
        for(int i=0;i<segment.args.size();++i)
            delete [] args[i];
        delete [] args;
        return 0;
    }

    return 0;
}

int shell_exec_command(command &command_line){
    int rtn_exec = 0;
    vector< vector<int> > fds;
    list<command_segment>::iterator it = command_line.segment.begin();

    for(int i=0;i<command_line.size()-1;++i){
        vector<int> tmp_pipe(2,0);
        int tmp_fd[2];
        if(pipe(tmp_fd) == -1){
            cerr << "wrong ar pipe()" <<endl;
            return -1;
        }
        tmp_pipe[0] = tmp_fd[0];
        tmp_pipe[1] = tmp_fd[1];
        fds.push_back(tmp_pipe);
    }
    for(int i=0;i<command_line.size();++i){
        int in_fd = 0;
        int out_fd = 1;
        if(i != 0)
            in_fd = fds[i-1][0];
        if(i != command_line.size()-1)
            out_fd = fds[i][1];

        rtn_exec = shell_exec_segment(*it,in_fd,out_fd,command_line.mode,0,fds);
        if(rtn_exec != 0){
            cout << rtn_exec <<endl;
            break;
        }
        else{
            it++;
        }
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

void shell_init(){
    return;
}

int main()
{
    shell_init();
    shell_welcome();
    shell_loop();

    return 0;
}

