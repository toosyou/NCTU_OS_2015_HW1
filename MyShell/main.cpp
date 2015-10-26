#include <iostream>
#include <vector>
#include <string>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <sstream>

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
            if(buffer.size() != 0){
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

int shell_exec_segment(command_segment &segment, int in_fd, int out_fd, int mode, int pgid){
    //cout << segment.args[0] <<endl;

    if(shell_exec_builtin(segment))
        return 0;



    return 0;
}

int shell_exec_command(command &command_line){
    while(!command_line.segment.empty()){
        int fd[2];
        pipe(fd);

        if(shell_exec_segment(*command_line.segment.begin(),fd[0],fd[1],command_line.mode,0 ) != 0){
            command_line.clear();
            return 0;
        }
        else{
            command_line.segment.pop_front();
        }
    }
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

