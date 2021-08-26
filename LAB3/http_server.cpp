#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

struct Server_env
{
  string REQUEST_METHOD;
  string REQUEST_URI;
  string QUERY_STRING;
  string SERVER_PROTOCOL;
  string HTTP_HOST;
  string SERVER_ADDR;
  string SERVER_PORT;
  string REMOTE_ADDR;
  string REMOTE_PORT;
};

class session
    : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
      : socket_(std::move(socket))
  {
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec)
          {
            parse_request();
         
            int child_pid = fork();
            if (child_pid == 0)
            {
              set_http_env();

              dup2(sock_fd, 0);
              dup2(sock_fd, 1);
              cout << "HTTP/1.1 200 OK\r\n";
              cout.flush();
              path += http.REQUEST_URI;
              if (execl(path.c_str(), http.REQUEST_URI.c_str(), NULL) == -1)
              {
                perror("Execv error!");
                exit(-1);
              }
            }
            else if(child_pid>0){
              close(sock_fd);
              int status = 0;
              while (waitpid(child_pid, &status, 0) > 0)
                ;
            }
            else{
              exit(-1);
            }
          }
        });
  }

  void set_http_env()
  {
    setenv("REQUEST_METHOD", (http.REQUEST_METHOD).c_str(), 1);
    setenv("REQUEST_URI", (http.REQUEST_URI).c_str(), 1);
    setenv("QUERY_STRING", (http.QUERY_STRING).c_str(), 1);
    setenv("SERVER_PROTOCOL", (http.SERVER_PROTOCOL).c_str(), 1);
    setenv("HTTP_HOST", (http.HTTP_HOST).c_str(), 1);
    setenv("SERVER_ADDR", (http.SERVER_ADDR).c_str(), 1);
    setenv("SERVER_PORT", (http.SERVER_PORT).c_str(), 1);
    setenv("REMOTE_ADDR", (http.REMOTE_ADDR).c_str(), 1);
    setenv("REMOTE_PORT", (http.REMOTE_PORT).c_str(), 1);
  }

  void parse_request()
  {
    /*-----check query string------*/
    if (strstr(data_, "?") != NULL)
      have_query_string = true;
    /*------------parse------------*/
    tok = strtok(data_, " ?\n\r");
    http.REQUEST_METHOD = tok;
    tok = strtok(NULL, " ?\n\r");
    http.REQUEST_URI = tok;

    if (have_query_string)
    {
      tok = strtok(NULL, " ?\n\r");
      http.QUERY_STRING = tok;
    }
    tok = strtok(NULL, " ?\n\r");
    http.SERVER_PROTOCOL = tok;
    tok = strtok(NULL, " ?\n\r");
    tok = strtok(NULL, " ?\n\r");
    http.HTTP_HOST = tok;
    tok2 = strtok(tok, ":\n\r");
    http.SERVER_ADDR = tok2;
    tok2 = strtok(NULL, ":\n\r");
    http.SERVER_PORT = tok2;

    http.REMOTE_ADDR = socket_.remote_endpoint().address().to_string();
    http.REMOTE_PORT = to_string(socket_.remote_endpoint().port());
  }

  tcp::socket socket_;
  enum
  {
    max_length = 1024
  };
  char data_[max_length];
  Server_env http;
  char *tok;
  char *tok2;
  bool have_query_string = false;
  int sock_fd = (int)socket_.native_handle();
  string path = "./";
};

class server
{
public:
  server(boost::asio::io_context &io_context, short port)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
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
            std::make_shared<session>(std::move(socket))->start();
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

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception &e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}