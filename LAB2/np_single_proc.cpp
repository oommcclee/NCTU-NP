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
#include <arpa/inet.h>
#include <iomanip>
#include <fcntl.h>

#define QLEN 30
#define BUFSIZE 15000
using namespace std;

/*--------------server two: fix bug----------------*/
struct env
{
  string name;
  string value;
};

struct pipeFD
{
  int READ;
  int WRITE;
  int pipe_count;
  int target_client;
  int client_fd;
};

struct client
{
  int fd;
  int uid;
  string name;
  string ip;
  unsigned short port;
  vector<env> envTable;
};
/*------------global var------------*/
vector<client> clientTable;
vector<pipeFD> pipeTable;
int uidTable[QLEN + 1];
int nfds;
int msock;
int client_count = 0;
fd_set afds;
fd_set rfds;

int null_fd;

void type_prompt(int fd)
{
  dup2(fd, 1);
  cout << "**************************************" << endl;
  cout << "* Welcome to the information server. *" << endl;
  cout << "**************************************" << endl;
}

vector<client>::iterator get_clinet(int num, int mode)
{ // 0 for fd, 1 for uid
  vector<client>::iterator it = clientTable.begin();
  if (mode == 0)
  {
    while (it != clientTable.end())
    {
      if ((*it).fd == num)
        return it;
      it++;
    }
  }
  else
  {
    while (it != clientTable.end())
    {
      if ((*it).uid == num)
        return it;
      it++;
    }
  }
  return it;
}

string get_env(string env_name, vector<client>::iterator cur_it)
{
  vector<env>::iterator it = (*cur_it).envTable.begin();
  while (it != (*cur_it).envTable.end())
  {
    if ((*it).name == env_name)
      return (*it).value;
    it++;
  }
  return "";
}

void erase_env(string env_name, vector<client>::iterator cur_it)
{
  vector<env>::iterator it = (*cur_it).envTable.begin();
  while (it != (*cur_it).envTable.end())
  {
    if ((*it).name == env_name)
    {
      (*cur_it).envTable.erase(it);
      break;
    }
    it++;
  }
}

int assign_ID()
{
  for (int i = 1; i < QLEN + 1; i++)
  {
    if (uidTable[i] == 0)
    {
      uidTable[i] = 1;
      return i;
    }
  }
  return 0;
}

void broadcast(int action, string input, vector<client>::iterator cur_client, int target)
{
  for (int fdno = 0; fdno < nfds; fdno++)
  {
    string output = "";
    if (FD_ISSET(fdno, &afds))
    {
      if (fdno != msock)
      {

        if (action == 0)
        { // new register
          output = "* User '" + (*cur_client).name + "' entered from " + (*cur_client).ip + ":" + to_string((*cur_client).port) + ". ***\n";
        }
        else if (action == 1)
        { // client left
          output = "* User '" + (*cur_client).name + "' left. ***\n";
        }
        else if (action == 2)
        { // edit name
          output = "* User from " + (*cur_client).ip + ":" + to_string((*cur_client).port) + " is named '" + (*cur_client).name + "'. ***\n";
        }
        else if (action == 3)
        { // yell
          output = "* " + (*cur_client).name + " yelled **:" + input + "\n";
        }
        else if (action == 4)
        { // pipe to other
          output = "* " + (*cur_client).name + " (#" + to_string((*cur_client).uid) + ") just piped '" + input + "' to " + (*get_clinet(target, 1)).name + " (#" + to_string(target) + ") ***\n";
        }
        else if (action == 5)
        { // get pipe
          output = "* " + (*cur_client).name + " (#" + to_string((*cur_client).uid) + ") just received from " + (*get_clinet(target, 0)).name + " (#" + to_string((*get_clinet(target, 0)).uid) + ") by '" + input + "' ***\n";
        }
        if (write(fdno, output.c_str(), output.length()) == -1)
          perror("send error");
      }
    }
  }
}

void who(vector<client>::iterator cur_it)
{
  cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
  for (int i = 1; i < QLEN + 1; i++)
  {
    if (uidTable[i] == 1)
    {
      vector<client>::iterator it = get_clinet(i, 1);
      cout << (*it).uid << "\t" << (*it).name << "\t" << (*it).ip << ":" << (*it).port;
      if ((*it).fd == (*cur_it).fd)
        cout << "\t"
             << "<-me";
      cout << endl;
    }
  }
}

void name(vector<client>::iterator cur_it, string name)
{
  vector<client>::iterator it = clientTable.begin();
  while (it != clientTable.end())
  {
    if ((*it).name == name)
    {
      cout << "* User '" << name << "' already exists. *" << endl;
      return;
    }
    it++;
  }
  (*cur_it).name = name;
  broadcast(2, "", cur_it, -1);
}

string merge_arg_to_msg(vector<string> argTable, int mode)
{ // 0 for yell  1 for tell
  string tmp = "";
  vector<string>::iterator it = argTable.begin();
  if (mode == 1)
    it++;
  while (it != argTable.end())
  {
    tmp += (" " + (*it));
    it++;
  }
  return tmp;
}

/*remember call by reference(vector call it's addr)*/
void read_command(string line, string *cmd, vector<string> &par)
{
  vector<string>::iterator it;
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

bool uid_existed(int uid)
{
  if (uid > QLEN)
    return false;
  else
  {
    if (uidTable[uid] == 1)
      return true;
  }
  return false;
}

/*if file open success -> exist*/
bool exist_CMD(string cmd, vector<string> pathTable)
{
  if (cmd == "setenv" || cmd == "printenv" || cmd == "exit" || cmd == "who" || cmd == "name" || cmd == "yell" || cmd == "tell")
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

void create_user_pipe(int pipe_num, int &out_fd, vector<pipeFD> &pipeTable, int &cur_pipe_count, int tar, vector<client>::iterator cur_it)
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
  newPipe.target_client = tar;
  newPipe.client_fd = (*cur_it).fd;
  pipeTable.push_back(newPipe);
}

void create_pipe(int pipe_num, int &out_fd, vector<pipeFD> &pipeTable, int &cur_pipe_count, vector<client>::iterator cur_it)
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
  newPipe.target_client = -1;
  newPipe.client_fd = (*cur_it).fd;
  pipeTable.push_back(newPipe);
}

bool check_same_user_pipe(int tar_fd, vector<pipeFD> &pipeTable, int cur_uid)
{
  vector<pipeFD>::iterator it_pipe_same_user;
  it_pipe_same_user = pipeTable.begin();
  while (it_pipe_same_user != pipeTable.end())
  {
    if ((*it_pipe_same_user).target_client == tar_fd && (*it_pipe_same_user).client_fd == (*get_clinet(cur_uid, 1)).fd)
    {
      cout << "* Error: the pipe #" << cur_uid << "->#" << (*get_clinet(tar_fd, 0)).uid << " already exists. **" << endl;
      return true;
    }
    it_pipe_same_user++;
  }
  return false;
}

bool check_same_pipe(int &cur_pipe_count, vector<pipeFD> &pipeTable, int &out_fd, vector<client>::iterator cur_it)
{
  vector<pipeFD>::iterator it_pipe_same;
  it_pipe_same = pipeTable.begin();
  while (it_pipe_same != pipeTable.end())
  {
    if ((*it_pipe_same).pipe_count == cur_pipe_count && (*it_pipe_same).client_fd == (*cur_it).fd)
    {
      out_fd = (*it_pipe_same).READ;
      return true;
    }
    it_pipe_same++;
  }
  return false;
}

int can_find_user_pipe(vector<pipeFD> &pipeTable, int tar_fd, int uid)
{
  vector<pipeFD>::iterator it = pipeTable.begin();
  while (it != pipeTable.end())
  {
    if ((*it).target_client == tar_fd && (*it).client_fd == (*get_clinet(uid, 1)).fd)
      return (*it).WRITE;
    it++;
  }
  return -1;
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

void sub_pipe_count(vector<pipeFD> &pipeTable, vector<client>::iterator cur_it)
{
  vector<pipeFD>::iterator it = pipeTable.begin();
  while (it != pipeTable.end())
  {
    if ((*it).client_fd == (*cur_it).fd)
      (*it).pipe_count--;
    it++;
  }
}

void pipe_count_check(vector<pipeFD> &pipeTable, int &in_fd, vector<pipeFD>::iterator &it_pipe_zero, vector<client>::iterator cur_it)
{
  while (it_pipe_zero != pipeTable.end())
  {
    if ((*it_pipe_zero).pipe_count == 0 && (*it_pipe_zero).client_fd == (*cur_it).fd)
    {
      in_fd = (*it_pipe_zero).WRITE;
      break;
    }
    it_pipe_zero++;
  }
}

void pipe_now_check(vector<pipeFD> &pipeTable, vector<pipeFD>::iterator &it_pipe_zero, vector<client>::iterator cur_it)
{
  it_pipe_zero = pipeTable.begin();
  while (it_pipe_zero != pipeTable.end())
  {
    if ((*it_pipe_zero).pipe_count == -1 && (*it_pipe_zero).client_fd == (*cur_it).fd)
      (*it_pipe_zero).pipe_count++;
    it_pipe_zero++;
  }
}

void erase_exit_user_pipe(vector<pipeFD> &pipeTable, int target_fd)
{
  vector<pipeFD>::iterator it;
  it = pipeTable.begin();

  while (it != pipeTable.end())
  {
    if ((*it).target_client == target_fd)
    {
      close((*it).READ);
      close((*it).WRITE);
      pipeTable.erase(it);
    }
    else
      it++;
  }
}

void erase_user_pipe(vector<pipeFD> &pipeTable, int target_fd)
{
  vector<pipeFD>::iterator it;
  it = pipeTable.begin();

  while (it != pipeTable.end())
  {
    if ((*it).target_client == target_fd && (*it).pipe_count == 0)
    {
      close((*it).READ);
      close((*it).WRITE);
      pipeTable.erase(it);
    }
    else
      it++;
  }
}

void set_user_pipe_to_zero(vector<pipeFD> &pipeTable, int target_fd, int uid)
{
  vector<pipeFD>::iterator it;
  it = pipeTable.begin();

  while (it != pipeTable.end())
  {
    if ((*it).target_client == target_fd && (*it).client_fd == (*get_clinet(uid, 1)).fd)
    {
      (*it).pipe_count = 0;
      //close((*it).READ);
      break;
    }
    else
      it++;
  }
}

void erase_done_pipe(vector<pipeFD> &pipeTable, vector<client>::iterator cur_it)
{
  vector<pipeFD>::iterator it_zero;
  it_zero = pipeTable.begin();

  while (it_zero != pipeTable.end())
  {
    if ((*it_zero).pipe_count == 0 && (*it_zero).client_fd == (*cur_it).fd)
    {
      close((*it_zero).READ);
      close((*it_zero).WRITE);
      pipeTable.erase(it_zero);
    }
    else
      it_zero++;
  }
}

void parse(vector<string> &parameters, vector<string>::iterator &it_par, vector<pipeFD> &pipeTable, string &cmd, int &in_fd, int &out_fd, int &err_fd, int &cur_pipe_count, vector<string> &argTable, int &use_nohang, vector<client>::iterator cur_it, int &target_fd)
{
  char pipe_Cnum[5];
  char user_num[5];
  string msg = merge_arg_to_msg(parameters, 0);
  msg.erase(0, 1);
  int uid;
  int help_me = 0;
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
        create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count, cur_it);
        it_par++;
        break;
      }
      else
      {
        int len = temp.length() - 1;
        for (int i = 1; i < temp.length(); i++)
        {
          pipe_Cnum[i - 1] = temp[i];
        }
        pipe_Cnum[len] = '\0';
      }
      pipe_count = atoi(pipe_Cnum);
      if (!check_same_pipe(pipe_count, pipeTable, out_fd, cur_it))
        create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count, cur_it);
      it_par++;
      break;
    }
    else if (temp[0] == '!')
    {
      use_nohang = 1;
      if (temp.length() == 1)
      {
        pipe_count = -1;
        create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count, cur_it);
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
      if (!check_same_pipe(pipe_count, pipeTable, out_fd, cur_it))
        create_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count, cur_it);
      err_fd = out_fd;
      it_par++;
      break;
    }
    else if (temp[0] == '>')
    {
      if (temp.length() == 1)
      {
        while (it_par != parameters.end())
        {
          argTable.push_back(*it_par);
          it_par++;
        }
        break;
      }
      else if (temp.length() > 1)
      {
        use_nohang = 1;
        if ((it_par + 1) != parameters.end())
        {
          string t = *(it_par + 1);
          if (t[0] == '<')
            parameters.push_back(temp);
          help_me == 1;
          it_par++;
          continue;
        }
        for (int i = 1; i < temp.length(); i++)
          user_num[i - 1] = temp[i];
        user_num[temp.length() - 1] = '\0';
        uid = atoi(user_num);
        if (uid_existed(uid))
        {
          target_fd = (*get_clinet(uid, 1)).fd;
          if (!check_same_user_pipe(target_fd, pipeTable, (*cur_it).uid))
          {
            create_user_pipe(pipe_count, out_fd, pipeTable, cur_pipe_count, target_fd, cur_it);
            broadcast(4, msg, cur_it, uid);
          }
          else
          {
            out_fd = null_fd;
          }
        }
        else
        {
          cout << "* Error: user #" << uid << " does not exist yet. *" << endl;
          out_fd = null_fd;
        }
        it_par++;
        break;
      }
    }
    else if (temp[0] == '<')
    {
      use_nohang = 1;
      for (int i = 1; i < temp.length(); i++)
        user_num[i - 1] = temp[i];
      user_num[temp.length() - 1] = '\0';
      uid = atoi(user_num);
      if (uid_existed(uid))
      {
        int r = can_find_user_pipe(pipeTable, (*cur_it).fd, uid);
        if (r != -1)
        {
          in_fd = r;
          target_fd = (*get_clinet(uid, 1)).fd;
          broadcast(5, msg, cur_it, target_fd);
          set_user_pipe_to_zero(pipeTable, (*cur_it).fd, uid);
        }
        else
        {
          cout << "* Error: the pipe #" << uid << "->#" << (*cur_it).uid << " does not exist yet. **" << endl;
          in_fd = null_fd;
        }
      }
      else
      {
        cout << "* Error: user #" << uid << " does not exist yet. *" << endl;
        in_fd = null_fd;
      }
    }
    else if (isCmd)
    {
      if (temp == "yell" || temp == "tell")
      {
        cmd = temp;
        it_par++;
        while (it_par != parameters.end())
        {
          argTable.push_back(*it_par);
          it_par++;
        }
        break;
      }
      else
        cmd = temp;
      isCmd = 0;
    }
    else
      argTable.push_back(temp);
    it_par++;
  }
}

int shell(int s_fd, string input)
{
  int child_pid;
  vector<string> parameters;
  vector<string> pathTable;
  vector<string> argTable;
  vector<string>::iterator it_path;
  string cmd = "", command = "";
  string path = getenv("PATH");
  create_pathTable(path, pathTable);

  dup2(s_fd, 1);
  dup2(s_fd, 2);

  command = "";
  parameters.clear();
  read_command(input, &command, parameters);
  if (command == "empty")
  {
    cout << "% ";
    fflush(stdout);
    return 0;
  }
  vector<string>::iterator it_par;
  it_par = parameters.begin();
  vector<client>::iterator cur_it = get_clinet(s_fd, 0);
  while (it_par != parameters.end() && *it_par != "\0")
  {
    int in_fd = s_fd, out_fd = s_fd, err_fd = s_fd, cur_pipe_count = -2;
    int use_nohang = 0;
    int target_fd;
    parse(parameters, it_par, pipeTable, command, in_fd, out_fd, err_fd, cur_pipe_count, argTable, use_nohang, cur_it, target_fd);
    vector<pipeFD>::iterator it_pipe_zero;
    it_pipe_zero = pipeTable.begin();
    while (it_pipe_zero != pipeTable.end())
    {
      if ((*it_pipe_zero).pipe_count == 0 && (*it_pipe_zero).client_fd == (*cur_it).fd)
        close((*it_pipe_zero).READ);
      if ((*it_pipe_zero).pipe_count == 0 && (*it_pipe_zero).target_client == (*cur_it).fd)
        close((*it_pipe_zero).READ);
      it_pipe_zero++;
    }
    it_pipe_zero = pipeTable.begin();
    pipe_count_check(pipeTable, in_fd, it_pipe_zero, cur_it);
    if (command == "setenv" || command == "printenv" || command == "exit" || command == "who" || command == "name" || command == "yell" || command == "tell")
    {
      if (command == "setenv")
      {
        erase_env(argTable[0], cur_it);
        env tmp;
        tmp.name = argTable[0];
        tmp.value = argTable[1];
        (*cur_it).envTable.push_back(tmp);
        setenv(argTable[0].c_str(), argTable[1].c_str(), 1);
        path = argTable[1];
        pathTable.clear();
        create_pathTable(path, pathTable);
      }
      else if (command == "printenv")
      {
        if (get_env(argTable[0], cur_it) != "")
          cout << getenv(argTable[0].c_str()) << endl;
      }
      else if (command == "who")
        who(cur_it);
      else if (command == "name")
        name(cur_it, argTable[0]);
      else if (command == "yell")
      {
        string msg = merge_arg_to_msg(argTable, 0);
        broadcast(3, msg, cur_it, -1);
      }
      else if (command == "tell")
      {
        string msg = merge_arg_to_msg(argTable, 1);
        int target_id = stoi(argTable[0]);
        if (uidTable[target_id] == 0)
          cout << "* Error: user #" << target_id << " does not exist yet. *" << endl;
        else
        {
          msg = "* " + (*cur_it).name + " told you **:" + msg + "\n";
          int target_fd = (*get_clinet(target_id, 1)).fd;
          if (write(target_fd, msg.c_str(), msg.size()) < 0)
            perror("can't send");
        }
      }
      else
      {
        erase_exit_user_pipe(pipeTable, (*cur_it).fd);
        return -1;
      }
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
    }
    else
    {
      int status = 0;
      while (waitpid(-1, &status, WNOHANG) > 0)
        ;
      if (use_nohang != 1)
        waitpid(child_pid, &status, 0);
      else
        waitpid(child_pid, &status, WNOHANG);
    }
    argTable.clear();
    erase_done_pipe(pipeTable, cur_it);
    erase_user_pipe(pipeTable, (*cur_it).fd);
    pipe_now_check(pipeTable, it_pipe_zero, cur_it);
  }
  erase_done_pipe(pipeTable, cur_it);
  sub_pipe_count(pipeTable, cur_it);

  cout << "% ";
  fflush(stdout);
  return 0;
}

int main(int argc, const char **argv)
{
  null_fd = open("/dev/null", O_RDWR);
  struct sockaddr_in c_addr, s_addr;
  socklen_t c_len;
  int service = atoi(argv[1]);
  int pid;
  vector<client>::iterator cur_client;

  msock = socket(AF_INET, SOCK_STREAM, 0);

  bzero((char *)&s_addr, sizeof(s_addr));

  s_addr.sin_family = AF_INET;
  s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  s_addr.sin_port = htons(service);

  int flag = 1;
  if (setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, (const char *)&flag, sizeof(flag)) < 0)
  {
    cerr << "reuse addr fail.\n";
    exit(0);
  }

  if (bind(msock, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0)
    perror("can't bind!");

  listen(msock, 5);
  signal(SIGCHLD, SIG_IGN);

  nfds = getdtablesize();
  FD_ZERO(&afds);
  FD_SET(msock, &afds);

  while (1)
  {
    memcpy(&rfds, &afds, sizeof(rfds));

    if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
      perror("select error");
    if (FD_ISSET(msock, &rfds) && client_count < QLEN) //new client
    {
      int ssock;
      c_len = sizeof(c_addr);
      ssock = accept(msock, (struct sockaddr *)&c_addr, &c_len);
      if (ssock < 0)
        perror("accept");
      else
      {
        struct client tmp;
        tmp.fd = ssock;
        tmp.ip = inet_ntoa(c_addr.sin_addr);
        tmp.port = ntohs(c_addr.sin_port);
        tmp.uid = assign_ID();
        tmp.name = "(no name)";
        env env_tmp;
        env_tmp.name = "PATH";
        env_tmp.value = "bin:.";
        tmp.envTable.push_back(env_tmp);
        clientTable.push_back(tmp);

        FD_SET(ssock, &afds);
        type_prompt(ssock);
        broadcast(0, "", --clientTable.end(), -1);
        send(ssock, "% ", 2, 0);
      }
      client_count++;
    }
    for (int fdno = 0; fdno < nfds; fdno++) // handle client
    {
      if (fdno != msock && FD_ISSET(fdno, &rfds))
      {
        int n;
        char buf[BUFSIZE + 1] = {};
        if ((n = recv(fdno, buf, sizeof(buf), 0)) > 0)
        {
          if (n == 1)
          {
            cout << "% ";
            fflush(stdout);
            continue;
          }

          cur_client = get_clinet(fdno, 0);
          string path = get_env("PATH", cur_client);
          setenv("PATH", path.c_str(), 1);

          buf[n - 1] = '\0';
          string input = buf;

          int status = shell(fdno, input);
          if (status == -1)
          {
            broadcast(1, "", cur_client, -1);
            uidTable[(*cur_client).uid] = 0;
            clientTable.erase(cur_client);
            client_count--;

            close(fdno);
            close(1);
            close(2);
            dup2(0, 1);
            dup2(0, 2);

            FD_CLR(fdno, &afds);
          }
        }
        else // close
        {
          broadcast(fdno, "", cur_client, -1);
          uidTable[(*cur_client).uid] = 0;
          clientTable.erase(cur_client);
          client_count--;
          close(fdno);
          FD_CLR(fdno, &afds);
        }
      }
    }
    c_len = sizeof(c_addr);
  }
  return 0;
}