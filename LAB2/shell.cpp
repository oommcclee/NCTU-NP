#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <errno.h>
#include <signal.h>
#include <iterator>
#include <sys/socket.h>
#include <netinet/in.h>

#define QLEN 5
#define BUFSIZE 4096

using namespace std;

/*--------------server one----------------*/

struct pipeFD
{
    int READ;
    int WRITE;
    int pipe_count;
};

struct client
{
    int fd;
    string name;
    string ip;
    unsigned short port;
    int pipe_count;
};
//global var
vector<client> clientTable;

void type_prompt()
{
    static int first_time = 1;
    if (first_time)
    {
        cout << "****************************************" << endl;
        cout << "**  Welcome to the information server **" << endl;
        cout << "****************************************" << endl;
        first_time = 0;
    }
    cout << "% ";
}
/*remember call by reference(vector call it's addr)*/
void read_command(string *cmd, vector<string> &par)
{
    string line;
    vector<string>::iterator it;
    getline(cin, line);
    if (line.size() == 0)
        *cmd = "empty";
    else
    {
        istringstream iss(line);
        for (string s; iss >> s;)
            par.push_back(s);
        it = par.begin();
        *cmd = *it;
    }
}
/*if file open success -> exist*/
bool exist_CMD(string cmd, vector<string> pathTable)
{
    if (cmd == "exit")
        return true;
    string path = "";
    vector<string>::iterator it = pathTable.begin();
    FILE *fd;
    while (it != pathTable.end())
    {
        path = *it + "/" + cmd;
        fd = fopen(path.c_str(), "r");
        if (fd != NULL)
        {
            fclose(fd);
            return true;
        }
        it++;
    }
    return false;
}

void create_pipe(int pipe_num, int &out_fd, vector<pipeFD> &pipeTable, int &cur_pipe_count)
{
    int pipefd[2];
    struct pipeFD newPipe;
    if (pipe(pipefd) < 0)
    {
        perror("pipe error");
        return;
    }
    out_fd = pipefd[1];
    cur_pipe_count = pipe_num;
    newPipe.pipe_count = pipe_num;
    newPipe.READ = pipefd[1];
    newPipe.WRITE = pipefd[0];
    pipeTable.push_back(newPipe);
}

bool check_same_pipe(int &cur_pipe_count, vector<pipeFD> &pipeTable, int &out_fd)
{
    vector<pipeFD>::iterator it_pipe_same;
    it_pipe_same = pipeTable.begin();
    while (it_pipe_same != pipeTable.end())
    {
        if ((*it_pipe_same).pipe_count == cur_pipe_count)
        {
            out_fd = (*it_pipe_same).READ;
            return true;
        }
        it_pipe_same++;
    }
    return false;
}

char **create_argtable(vector<string> &argTable, string &command)
{
    char **arg = new char *[argTable.size() + 2];
    for (int i = 0; i < argTable.size(); i++)
    {
        arg[i + 1] = new char[argTable[i].size() + 1];
        strcpy(arg[i + 1], argTable[i].c_str());
    }
    arg[0] = new char[command.size() + 1];
    strcpy(arg[0], command.c_str());
    arg[argTable.size() + 1] = NULL;
    return arg;
}

void create_pathTable(string path, vector<string> &pathTable)
{
    stringstream ss(path);
    string s;
    while (getline(ss, s, ':'))
        pathTable.push_back(s);
}

void sub_pipe_count(vector<pipeFD> &pipeTable)
{
    vector<pipeFD>::iterator it = pipeTable.begin();
    while (it != pipeTable.end())
    {
        (*it).pipe_count--;
        it++;
    }
}

void pipe_count_check(vector<pipeFD> &pipeTable, int &in_fd, vector<pipeFD>::iterator &it_pipe_zero)
{

    while (it_pipe_zero != pipeTable.end())
    {
        if ((*it_pipe_zero).pipe_count == 0)
        {
            in_fd = (*it_pipe_zero).WRITE;
            break;
        }
        it_pipe_zero++;
    }
}

void pipe_now_check(vector<pipeFD> &pipeTable, vector<pipeFD>::iterator &it_pipe_zero)
{
    it_pipe_zero = pipeTable.begin();
    while (it_pipe_zero != pipeTable.end())
    {
        if ((*it_pipe_zero).pipe_count == -1)
            (*it_pipe_zero).pipe_count++;
        it_pipe_zero++;
    }
}
void erase_done_pipe(vector<pipeFD> &pipeTable)
{
    vector<pipeFD>::iterator it_zero;
    it_zero = pipeTable.begin();

    while (it_zero != pipeTable.end())
    {
        if ((*it_zero).pipe_count == 0)
        {
            close((*it_zero).READ);
            close((*it_zero).WRITE);
            pipeTable.erase(it_zero);
        }
        else
            it_zero++;
    }
}
void parse(vector<string> &parameters, vector<string>::iterator &it_par, vector<pipeFD> &pipeTable, string &cmd, int &out_fd, int &err_fd, int &cur_pipe_count, vector<string> &argTable, int &use_nohang)
{
    char pipe_Cnum[5];
    int pipe_count = -2, isCmd = 1, count = 0;
    while (it_par != parameters.end())
    {
        string temp = *it_par;
        if (temp[0] == '|')
        {
            use_nohang = 1;
            if (temp.length() == 1)
            {
                pipe_count = -1;
                create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count);
                it_par++;
                break;
            }
            else
            {
                int len = temp.length() - 1;
                for (int i = 1; i < temp.length(); i++)
                {
                    if (temp[i] == '+')
                    {
                        pipe_Cnum[i - 2] += temp[i + 1];
                        i++;
                        len -= 2;
                    }
                    pipe_Cnum[i - 1] = temp[i];
                }
                pipe_Cnum[len] = '\0';
            }
            pipe_count = atoi(pipe_Cnum);
            if (!check_same_pipe(pipe_count, pipeTable, out_fd))
                create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count);
            it_par++;
            break;
        }
        else if (temp[0] == '!')
        {
            use_nohang = 1;
            if (temp.length() == 1)
            {
                pipe_count = -1;
                create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count);
                err_fd = out_fd;
                it_par++;
                break;
            }
            else
            {
                for (int i = 1; i < temp.length(); i++)
                    pipe_Cnum[i - 1] = temp[i];
                pipe_Cnum[temp.length() - 1] = '\0';
            }
            pipe_count = atoi(pipe_Cnum);
            if (!check_same_pipe(pipe_count, pipeTable, out_fd))
                create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count);
            err_fd = out_fd;
            it_par++;
            break;
        }
        else if (temp[0] == '>')
        {
            while (it_par != parameters.end())
            {
                argTable.push_back(*it_par);
                it_par++;
            }
            break;
        }
        else if (isCmd)
        {
            cmd = temp;
            isCmd = 0;
        }
        else
            argTable.push_back(temp);
        it_par++;
    }
}

int shell(int s_fd)
{
    int child_pid;
    vector<string> parameters;
    vector<string> pathTable;
    vector<string> argTable;
    vector<pipeFD> pipeTable;
    vector<string>::iterator it_path;
    string cmd = "", command = "";
    string path = getenv("PATH");
    create_pathTable(path, pathTable);
    while (1)
    {
        command = "";
        parameters.clear();
        type_prompt();
        read_command(&command, parameters);
        if (command == "empty")
            continue;
        vector<string>::iterator it_par;
        it_par = parameters.begin();
        while (it_par != parameters.end() && *it_par != "\0")
        {
            int in_fd = s_fd, out_fd = s_fd, err_fd = s_fd, cur_pipe_count = -2;
            int use_nohang = 0;
            parse(parameters, it_par, pipeTable, command, out_fd, err_fd, cur_pipe_count, argTable, use_nohang);
            /*check if pipe read is done*/
            vector<pipeFD>::iterator it_pipe_zero;
            it_pipe_zero = pipeTable.begin();
            while (it_pipe_zero != pipeTable.end())
            {
                if ((*it_pipe_zero).pipe_count == 0)
                    close((*it_pipe_zero).READ);
                it_pipe_zero++;
            }
            it_pipe_zero = pipeTable.begin();
            pipe_count_check(pipeTable, in_fd, it_pipe_zero);
            if (command == "setenv" || command == "printenv" || command == "exit")
            {
                if (command == "setenv")
                {
                    setenv(argTable[0].c_str(), argTable[1].c_str(), 1);
                    path = getenv("PATH");
                    pathTable.clear();
                    create_pathTable(path, pathTable);
                }
                else if (command == "printenv")
                {
                    if (getenv(argTable[0].c_str()) != NULL)
                        cout << getenv(argTable[0].c_str()) << endl;
                }
                else
                    return 0;
                argTable.clear();
                continue;
            }
            else if (!exist_CMD(command, pathTable))
            {
                cout << "Unknown command: [" << command << "]." << endl;
                argTable.clear();
                continue;
            }

            if (argTable.size() > 1)
            {
                if (argTable[argTable.size() - 2] == ">")
                {
                    string tar_file = argTable[argTable.size() - 1];
                    FILE *redir_file;
                    redir_file = fopen(tar_file.c_str(), "w");
                    out_fd = fileno(redir_file);

                    argTable.pop_back();
                    argTable.pop_back();
                }
            }

            while ((child_pid = fork()) < 0)
                waitpid(-1, NULL, 0);

            if (child_pid == 0)
            {
                char **arg = create_argtable(argTable, command);

                if (in_fd != s_fd)
                    dup2(in_fd, 0);
                if (out_fd != s_fd)
                    dup2(out_fd, 1);
                if (err_fd != s_fd)
                    dup2(err_fd, 2);
                if (in_fd != s_fd)
                    close(in_fd);
                if (out_fd != s_fd)
                    close(out_fd);
                if (err_fd != s_fd)
                    close(err_fd);

                it_path = pathTable.begin();
                while (it_path != pathTable.end())
                {
                    cmd = *it_path + "/" + command;

                    if (execv(cmd.c_str(), arg) == -1)
                        it_path++;
                }
                cout << "exec erro" << endl;
                exit(0);
            }
            else
            {
                int status = 0;
                while (waitpid(-1, &status, WNOHANG) > 0)
                    ;
                if (use_nohang != 1)
                    waitpid(child_pid, &status, 0);
            }
            argTable.clear();
            erase_done_pipe(pipeTable);
            pipe_now_check(pipeTable, it_pipe_zero);
        }
        erase_done_pipe(pipeTable);
        sub_pipe_count(pipeTable); /* code */
    }
}
int main(int argc, const char **argv)
{
    struct sockaddr_in c_addr, s_addr;
    socklen_t c_len;
    fd_set rfds;
    fd_set afds;
    int fd, nfds, port = atoi(argv[1]);
    int pid;

    int msock = socket(AF_INET, SOCK_STREAM, 0);

    bzero((char *)&s_addr, sizeof(s_addr));

    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(port);

    if (::bind(msock, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0)
        perror("can't bind!");

    listen(msock, 5);
    signal(SIGCHLD, SIG_IGN);

    while (1)
    {
        c_len = sizeof(c_addr);
        int ssock = accept(msock, (struct sockaddr *)&c_addr, &c_len);

        if (ssock < 0)
            return 0;
        if (pid = fork() < 0)
            return -1;
        else if (pid == 0)
        {
            setenv("PATH", "bin:.", 1);
            close(msock);
            dup2(ssock, 0);
            dup2(ssock, 1);
            dup2(ssock, 2);

            shell(ssock);

            close(ssock);
        }
        else
            close(ssock);
    }
    return 0;
}