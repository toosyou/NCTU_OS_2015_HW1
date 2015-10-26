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
    for(int i=0;i<segment.args.size();++i){
        cout << segment.args[i] << " ";
    }cout <<endl;

    if(shell_exec_builtin(segment))
        return 0;

    pid_t childpid = fork();

    if(childpid == 0){//child
        if(in_fd != 0 && dup2(in_fd,STDIN_FILENO) == -1){
            exit(errno);
        }

        if(out_fd != 1 && dup2(out_fd,STDOUT_FILENO) == -1){
            exit(errno);
        }

        char **args = new char*[segment.args.size()+1];
        for(int i=0;i<segment.args.size();++i){
            args[i] = new char[ segment.args.size()+2 ];
            strcpy(args[i],segment.args[i].c_str());
        }
        args[ segment.args.size() ] = NULL;

        execvp(segment.args[0].c_str(),args);
        exit(errno);
    }
    else{//parent
        cout << "Command executed by pid=" << childpid <<endl;
        if(mode == FOREGROUND_EXECUTION){
            int state = 0;
            waitpid(childpid,&state,0);
            if(WIFEXITED(state)){
                cout << strerror(WEXITSTATUS(state)) <<endl;
                return -1;
            }
            return 0;
        }
    }

    return 0;
}

int shell_exec_command(command &command_line){
    list<int> fds;
    fds.push_back(0);
    for(int i=0;i<command_line.segment.size()-1;++i){
        int tmp_fd[2];
        pipe(tmp_fd);
        fds.push_back(tmp_fd[0]);
        fds.push_back(tmp_fd[1]);
    }
    fds.push_back(1);

    while(!command_line.segment.empty()){

        list<int>::iterator tmp_it = fds.begin();
        int fd0 = *fds.begin();
        tmp_it++;
        int fd1 = *tmp_it;

        if(shell_exec_segment(*command_line.segment.begin(),fd0,fd1,command_line.mode,0 ) != 0){
            command_line.clear();
            while(!fds.empty()){
                if(fd0 > 1)
                    close(fd0);
                fds.pop_front();
            }
            return 0;
        }
        else{
            command_line.segment.pop_front();
        }
        if(fd0 > 1)
            close(fd0);
        fds.pop_front();
    }
    while(!fds.empty()){
        if((*fds.begin()) > 1)
            close((*fds.begin()));
        fds.pop_front();
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

