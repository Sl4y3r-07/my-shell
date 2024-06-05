#include <iostream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

vector<string> history;
string line;

string input_file;
string output_file;
bool input_redirect=false;
bool output_redirect=false;
bool append_output=false;

string read_line()
{ 
    string line_;
    getline(cin, line_);
    return line_;   
}

vector<string> parsing_string(const string &line)
{
    vector<string> args;
    string arg;
   bool input_redirect=false;
   bool output_redirect=false;
   bool append_output=false;
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == ' ') {
            if (!arg.empty()) {
                if (input_redirect) {
                    input_file = arg;
                    input_redirect = false;
                } else if (output_redirect) {
                    output_file = arg;
                    output_redirect = false;
                } else {
                    args.push_back(arg);
                }
                arg.clear();
            }
        } else if (line[i] == '<') {
            input_redirect = true;
            if (!arg.empty()) {
                args.push_back(arg);
                arg.clear();
            }
        } else if (line[i] == '>') {
            output_redirect = true;
            if (i + 1 < line.length() && line[i + 1] == '>') {
                append_output = true;
                ++i;  
            }
            if (!arg.empty()) {
                args.push_back(arg);
                arg.clear();
            }
        } else {
            arg += line[i];
        }
    }

    if (!arg.empty()) {
        if (input_redirect) {
           input_file = arg;
        } else if (output_redirect) {
            output_file = arg;
        } else {
            args.push_back(arg);
        }
    }
    return args;
}

vector<string> split_pipe(const string &line)
{  vector<string> commands;
   string command="";
    for(int i=0;i<line.length();i++)
    {
      if(line[i]=='|')
      { 
        
        commands.push_back(command);
        command.clear();
        
      }
      else{
        command+=line[i];
      }
 }
 commands.push_back(command);
 return commands;
}





int without_pipe_execute_command(const vector<string> &args)
{
  int status;

  if(args.empty())
  return 1;

  // will add some more builtin commands for the shell 
  if(args[0]=="cd")
  {
    if(args.size()<2)
    {
        cerr << "cd: expected argument\n";
    }
    else{
        if (chdir(args[1].c_str()) != 0) {
                perror("cd");
            }
    }
    return 0;  // if you don't add return 0, command will be executed but it will throw an error-> exec: No such file or directory
  }
  
  if(args[0]=="exit")
  {
    return 0;
  }

  if(args[0]=="history")
  {
    for(int i=0;i<history.size();i++)
    {
      cout<<i+1<<" "<<history[i]<<"\n";
    }
    return 0;
  }

  // using fork and exec to execute external commands 
  pid_t pid=fork();
  if(pid==0)       // because fork returns 0 to the child process
  {           
    // for input redirection
    if(!input_file.empty())
    {
        int fd= open(input_file.c_str(), O_RDONLY);
        if(fd==-1)
        {
            perror("open");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }
            close(fd);
    }
   // for output redirection
   if(!output_file.empty())
   {
    int fd;
    if(append_output==true)
    {
        fd= open(output_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    }
    else{
        fd=open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
     if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }
    close(fd);
   }
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


int with_pipe_execute_command(const vector<string> &commands) // with pipe
{   
    int status;

    // first execute the first command and then redirect the output of the first command to second command
    
    int pipes_count= commands.size()-1;
    int pipefds[pipes_count*2]; // twice of the pipes_count, because two file descriptors are required - one for reading file, other for writing files
     
    for(int i=0;i<pipes_count;i++)
    {
        if(pipe(pipefds+i*2)==-1)   // pipe syscall initializes two consecutive elements of the pipefds array for each pipe it creates
        {
            perror("pipe");
            return 1;
        }
    }


    for(int i=0;i<commands.size();i++)
    {
        pid_t pid= fork();
        if(pid==0)  // child process
        {
          // redirect i/p from the previous pipe
           if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], 0) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            // redirect output to next pipe
            if (i < commands.size() - 1) {
                if (dup2(pipefds[i * 2 + 1], 1) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            
            for (int j = 0; j < 2 * pipes_count; j++) {  // Close all pipe file descriptors
                close(pipefds[j]);
            }

            vector<string> args = parsing_string(commands[i]);  // same normal logic
            
            vector<char *> cargs;
            for (const auto &arg : args) {
                cargs.push_back(const_cast<char *>(arg.c_str()));
            }
            cargs.push_back(nullptr);

            if (execvp(cargs[0], cargs.data()) == -1) {
                perror("exec");
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            perror("fork");   // Error in forking
            return 1;
        }
          
    }

      for (int i = 0; i < 2 * pipes_count; i++) {
        close(pipefds[i]);
    }

    // Wait for all child processes to complete
    for (size_t i = 0; i < commands.size(); i++) {
        do {
            waitpid(-1, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return status;
}

int execute_command(const vector<string> &args)
{
     if(args[0]=="history")
  {
    for(int i=0;i<history.size();i++)
    {
      cout<<i+1<<" "<<history[i]<<"\n";
    }
    return 0;
  }

    
    vector<string> commands = split_pipe(line);

    if (commands.size() > 1) {
        return with_pipe_execute_command(commands);
    } else {
        return without_pipe_execute_command(args);
    }

}

void shell_loop()
{  
  
  vector<string> args;
  
  int status;
  do
  {
    cout<<"my-sh > ";
    line=read_line();
    if(line.empty())
    {
        continue;
    }
    history.push_back(line);  // to save the history
    args=parsing_string(line);
    status= execute_command(args); 
    input_file.clear();
    output_file.clear();
    if(args[0]=="exit")
    {
        break;
    }
  } while (true);         // we can use `status` but after successful execution of the command, it will go off  
}

int main()
{
    shell_loop();
    return 0;
}