#include <iostream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

string read_line()
{ 
    string line;
    getline(cin, line);
    return line;   
}

vector<string> parsing_string(const string &line)
{
    vector<string> args;
    string arg;
    for (char ch : line) {
        if (ch == ' ') {
            if (!arg.empty()) {
                args.push_back(arg);
                arg.clear();
            }
        } else {
            arg += ch;
        }
    }
    
    if (!arg.empty()) {
        args.push_back(arg);
    }
    return args;
}

int execute_command(const vector<string> &args)
{
  int status;

  if(args.empty())
  return 1;

  // will add some builtin commands for the shell 

  // using fork and exec to execute external commands 
  pid_t pid=fork();
  if(pid==0)       // because fork returns 0 to the child process
  {           
     vector<char *> cargs;
        for (const auto &arg : args) {
            cargs.push_back(const_cast<char *>(arg.c_str()));
        }
        cargs.push_back(nullptr);                          // this is done because of execvp 

        if (execvp(cargs[0], cargs.data()) == -1) {
            perror("exec");
        }
        exit(EXIT_FAILURE);
    } 
    else if (pid < 0) {
        // Error in forking
        perror("fork");
    } else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    
  }
  
  return status;

}

void shell_loop()
{  
  string line;
  vector<string> args;
  int status;
  do
  {
    cout<<"my-sh > ";
    line=read_line();
    args=parsing_string(line);
    status= execute_command(args); 
    // cout<<status<<"\n";
  } while (true);         // we can use `status` but after successful execution of the command, it will go off  
}

int main()
{
    shell_loop();
    return 0;
}