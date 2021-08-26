#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <boost/asio.hpp>
#include <boost/asio/execution_context.hpp>

using boost::asio::ip::tcp;
using namespace std;

struct SOCKS4_REQUEST
{
  int VN; /* 4, version number */
  int CD; /* 1:Connect or 2:Bind */
  string S_IP;
  unsigned short S_PORT;
  unsigned char D_IP[4];
  string D_IP_str;
  unsigned short D_PORT;
  string DOMAIN_NAME;
  bool use_domain_name;
  int Reply; /* 0:Accept or 1:Reject */
};

struct IPset
{
  string ip[4];
};

vector<IPset> C_firewall;
vector<IPset> B_firewall;
boost::asio::io_context io_context_global;

class session : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
      : socket_(move(socket)),
        socket_dst(io_context_global),
        resolver(io_context_global),
        acceptor(io_context_global)
  {
  }

  void start()
  {
    do_read_request();
  }

private:
  void do_read_request()
  {
    auto self(shared_from_this());
    bzero(request, sizeof(request));
    socket_.async_read_some(boost::asio::buffer(request, max_length),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec)
        {
          socks_req.S_IP = socket_.remote_endpoint().address().to_string();
          socks_req.S_PORT = htons(socket_.remote_endpoint().port());
          parse_socks_request();
          if(socks_req.CD == 1) /* connect mode */
            do_resolve();
          else if (socks_req.CD == 2) /* bind mode */
            do_bind();
        }
      });
  }

  void parse_socks_request()
  {
    socks_req.VN = (int)request[0];
    socks_req.CD = (int)request[1];
    socks_req.D_PORT =((int)request[2]) * 256 + (int)request[3];
    socks_req.D_IP[0] = request[4];
    socks_req.D_IP[1] = request[5];
    socks_req.D_IP[2] = request[6];
    socks_req.D_IP[3] = request[7];
    socks_req.Reply = 0;

    if ((int)request[4] == 0 && (int)request[5] == 0 && (int)request[6] == 0 && (int)request[7] != 0)
    {
      int USERID_length = 0;
      int DOMAIN_NAME_length = 0;
      while((int)request[8 + USERID_length] != 0)
        USERID_length++;
      while((int)request[9 + USERID_length + DOMAIN_NAME_length] != 0){
        socks_req.DOMAIN_NAME += request[9 + USERID_length + DOMAIN_NAME_length];
        DOMAIN_NAME_length++;
      }
      socks_req.D_IP_str = socks_req.DOMAIN_NAME;
      socks_req.use_domain_name = true;
    }
    else{
      socks_req.D_IP_str = to_string((int)request[4]) + "." + to_string((int)request[5]) + "." + to_string((int)request[6]) + "." + to_string((int)request[7]);
      socks_req.use_domain_name = false;
    }
      
  }

  void print_socks_server_info()
  {
    cerr << "<S_IP>: " << socks_req.S_IP << endl;
    cerr << "<S_PORT>: " << socks_req.S_PORT << endl;
    cerr << "<D_IP>: " << socks_req.D_IP_str << endl;
    cerr << "<D_PORT>: " << socks_req.D_PORT << endl;

    if (socks_req.CD == 1)
      cerr << "<Command>: CONNECT" << endl;
    else
      cerr << "<Command>: BIND" << endl;

    if (socks_req.Reply == 0)
      cerr << "<Reply>: Accept" << endl;
    else
      cerr << "<Reply>: Reject" << endl;
  }

  void set_config()
  {
    C_firewall.clear();
    B_firewall.clear();
    ifstream file("socks.conf");
    string line;

    while (getline(file, line))
    {
      stringstream ss(line);
      string tmp;
      vector<string> tok;
      while (getline(ss, tmp, ' '))
      {
        tok.push_back(tmp);
      }
      if (tok.size() < 4)
      {
        IPset permitIp;
        stringstream ss(tok[2]);
        string tok2;
        for (int i = 0; i < 4; i++)
        {
          getline(ss, tok2, '.');
          permitIp.ip[i] = tok2;
        }

        if (tok[1] == "c")
          C_firewall.push_back(permitIp);
        else if (tok[1] == "b")
          B_firewall.push_back(permitIp);
      }
    }
    file.close();
  }

  bool firewall()
  {
    set_config();
    bool re = false;
    vector<IPset>::iterator it1;
    vector<IPset>::iterator it2;
    if (socks_req.CD == 1)
    {
      if (C_firewall.size() < 1)
        return false;
      it1 = C_firewall.begin();
      it2 = C_firewall.end();
    }
    else if (socks_req.CD == 2)
    {
      if (B_firewall.size() < 1)
        return false;
      it1 = B_firewall.begin();
      it2 = B_firewall.end();
    }

    while (it1 != it2)
    {
      int count = 0;
      while (count < 4)
      {
        if ((*it1).ip[count][0] == '*'){
          count++;
        }
        else
        {
          int tmp = (int)socks_req.D_IP[count];
          if (atoi(((*it1).ip[count]).c_str()) != tmp)
            break;
          count++;
        }
      }

      if (count == 4)
      {
        re = true;
        break;
      }
      it1++;
    }

    return re;
  }

  void do_bind(){
    auto self(shared_from_this());
    unsigned short port = 0;
    while (1)
    {
      srand(time(nullptr));
      port = rand() % 10000 + 10000;
      tcp::endpoint _endpoint(tcp::v4(), port);
      boost::system::error_code ec;
      acceptor.open(_endpoint.protocol());
      acceptor.bind(_endpoint, ec);
      if (!ec){
        break;
      }
      else{
        acceptor.close();
      }
    }
    socks_req.D_PORT = port;
    acceptor.listen();
    socks_req.Reply = 0;
    socks4_reply();
  }

  void do_resolve(){
    string p = to_string(socks_req.D_PORT);
    auto self(shared_from_this());
    resolver.async_resolve(socks_req.D_IP_str, p,
        [this,self](const boost::system::error_code& ec,
        tcp::resolver::results_type results)
        {
          if(!ec){
            endpoint = results;
            do_connect();
          }
          else{
            do_reject();
          }
            
        });
  }

  void do_connect()
  {
    auto self(shared_from_this());
    boost::asio::async_connect(socket_dst, endpoint,
      [this,self](boost::system::error_code ec, tcp::endpoint) {
        if (!ec)
        {
          socks_req.D_IP_str = socket_dst.remote_endpoint().address().to_string();
          stringstream dst(socks_req.D_IP_str);
          for (int i = 0; i < 4; i++)
          {
            string s;
            getline(dst, s, '.');
            socks_req.D_IP[i] = (unsigned char)atoi(s.c_str());
          }
          if(firewall()){
            socks_req.Reply = 0;
            socks4_reply();
          }
          else
          {
            do_reject();
          }
        }
        else
        {
          do_reject();
        }
        
      });
  }

  void do_read_from_client(){
    auto self(shared_from_this());
    bzero(client_data, sizeof(client_data));
    socket_.async_read_some(boost::asio::buffer(client_data, max_length),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec)
        {
          do_write_to_server(length);
        }
        else{
          socket_.close();
          socket_dst.close();
        }
      });
  }

  void do_read_from_server()
  {
    auto self(shared_from_this());
    bzero(server_data, sizeof(server_data));
    socket_dst.async_read_some(boost::asio::buffer(server_data, max_length),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec)
        {
          do_write_to_client(length);
        }
        else
        {
          socket_.close();
          socket_dst.close();
        }
      });
  }

  void do_write_to_client(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_,boost::asio::buffer(server_data, length),
      [this,self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec)
        {
          do_read_from_server();
        }
        else
        {
          socket_.close();
          socket_dst.close();
        }
      });
  }

  void do_write_to_server(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_dst,boost::asio::buffer(client_data, length),
      [this,self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec)
        {
          do_read_from_client();
        }
        else
        {
          socket_.close();
          socket_dst.close();
        }
      });
  }

  void do_accept()
  {
    auto self(shared_from_this());
    boost::system::error_code ec;
    acceptor.accept(socket_dst, ec);
    if (!ec){
      acceptor.close();
      do_reply();
    }
    else{
      acceptor.close();
      do_reject();
    }
  }

  void socks4_reply()
  {
    auto self(shared_from_this());
    bzero(reply,sizeof(reply));
    reply[0] = 0x00;
    if(socks_req.Reply == 0)
      reply[1] = 0x5a;
    else
      reply[1] = 0x5b;

    if (socks_req.CD == 1) /* CONNECT mode */
    { 
      reply[2] = 0x00;
      reply[3] = 0x00;
      reply[4] = 0x00;
      reply[5] = 0x00;
      reply[6] = 0x00;
      reply[7] = 0x00;

      do_reply();
    }
    else /* BIND mode */
    { 
      reply[2] = socks_req.D_PORT / 256;
      reply[3] = socks_req.D_PORT % 256;
      reply[4] = 0x00;
      reply[5] = 0x00;
      reply[6] = 0x00;
      reply[7] = 0x00;
      
      do_bind_reply1();
    }
  }

  void do_reply()
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_,boost::asio::buffer(reply, sizeof(unsigned char)*8),
      [this,self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec)
        {
          print_socks_server_info();
          if(socks_req.Reply == 0){
            do_read_from_client();
            do_read_from_server();
          }
        }
      });
  }

  void do_bind_reply1(){
    auto self(shared_from_this());
    boost::asio::async_write(socket_,boost::asio::buffer(reply, sizeof(unsigned char)*8),
      [this,self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec)
        {
          do_accept();
        }
      });
  }

  void do_reject(){
    socks_req.Reply = 1;
    socks4_reply();
    socket_.close();
    socket_dst.close();
  }

  tcp::socket socket_;
  tcp::socket socket_dst;
  tcp::resolver resolver;
  tcp::acceptor acceptor;
  tcp::resolver::results_type endpoint;
  enum {
    max_length = 20480
  };
  unsigned char request[max_length];
  char client_data[max_length];
  char server_data[max_length];
  unsigned char reply[8];
  SOCKS4_REQUEST socks_req;
};

class server
{
public:
  server(short port)
      : acceptor_(io_context_global, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
          if (!ec)
          {
            io_context_global.notify_fork(boost::asio::io_context::fork_prepare);
            pid_t pid;
            pid = fork();
            if (pid == 0)
            {
              io_context_global.notify_fork(boost::asio::io_context::fork_child);
              acceptor_.close();
              make_shared<session>(std::move(socket))->start();
            }
            else if (pid > 0)
            {
              io_context_global.notify_fork(boost::asio::io_context::fork_parent);
              socket.close();
            }
            else // fork error
            {
              socket.close();
            }
          }
          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char *argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }
    server s(atoi(argv[1]));
    io_context_global.run();
  }
  catch (std::exception &e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}