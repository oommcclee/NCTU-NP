#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/wait.h>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using namespace std;

struct remoteInfo
{
  string host;
  unsigned short port;
  string File;
  int file_fd;
  int sock_fd;
  string id;
  bool isExist;
} InfoTable[5];

vector<string> split(string str, char delimiter)
{
  vector<string> re;
  stringstream ss(str);
  string tok;
  while (getline(ss, tok, delimiter))
  {
    re.push_back(tok);
  }
  return re;
}

void parse_query_string(string querystring)
{
  vector<string> Split1 = split(querystring, '&');
  vector<string> Split2;

  int index = 0; /* index for host table */
  int it = 0;    /* index for vector */

  while (it < 15)
  {
    for (int i = 0; i < 3; i++)
    {
      Split2.clear();
      Split2 = split(Split1[it], '=');
      /* empty */
      if (Split2.size() < 2)
      {
        InfoTable[index].isExist = false;
        it += (3 - i);
        break;
      }

      if (i == 0)
        InfoTable[index].host = Split2[1];
      else if (i == 1)
        InfoTable[index].port = atoi(Split2[1].c_str());
      else if (i == 2)
      {
        string p = "./test_case/";
        InfoTable[index].File = p + Split2[1];
        if (InfoTable[index].file_fd < 0)
          exit(-1);
        InfoTable[index].id = "s" + to_string(index);
        InfoTable[index].isExist = true;
      }
      it += 1;
    }

    index++;
  }
}

void print_header()
{
  cout << "Content-type: text/html\n"<< endl;
  cout << "<!DOCTYPE html>\
          <html lang=\"en\">\
            <head>\
              <meta charset=\"UTF-8\" />\
              <title>NP Project 3 Sample Console</title>\
              <link\
                rel=\"stylesheet\"\
                href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
                integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
                crossorigin=\" anonymous \"\
              />\
              <link\
                href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
                rel=\"stylesheet\"\
              />\
              <link\
                rel=\"icon\"\
                type=\"image/png\"\
                href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\
              />\
              <style>\
                * {\
                  font-family: \'Source Code Pro\', monospace;\
                  font-size: 1rem !important;\
                }\
                body {\
                  background-color: #212529;\
                }\
                pre {\
                  color: #cccccc;\
                }\
                b {\
                  color: #01b468;\
                }\
              </style>\
            </head>\
            <body>\
              <table class=\"table table-dark table-bordered\">\
                <thead>\
                  <tr>" <<endl;
  for (int i = 0; i < 5; i++)
  {
    if (InfoTable[i].isExist)
      cout << "<th scope=\"col\">" << InfoTable[i].host << ":" << InfoTable[i].port << "</th>" <<endl;
  }
  cout << "</tr>\
          </thead>\
          <tbody>\
          <tr>" <<endl;
  for (int i = 0; i < 5; i++)
  {
    if (InfoTable[i].isExist)
      cout << "<td ><pre id=\"" << InfoTable[i].id << "\" class=\"mb-0\"></pre></td>"<<endl;
  }
  cout << "</tr>\
        </tbody>\
      </table>\
    </body>\
  </html>" <<endl;
}

class client : public enable_shared_from_this<client>
{
public:
  client(boost::asio::io_context &io_context, const tcp::resolver::results_type &endpoints, string ID, string f)
      : io_context_(io_context),
        socket_(io_context),
        id(ID),
        file_name(f)
  {
    ifs.open(file_name.c_str());
    do_connect(endpoints);
  }

  void close()
  {
    boost::asio::post(io_context_, [this]() { socket_.close(); });
  }

private:
  void do_connect(const tcp::resolver::results_type &endpoints)
  {
    boost::asio::async_connect(socket_, endpoints,
      [this](boost::system::error_code ec, tcp::endpoint) {
        if (!ec)
        {
          do_read();
        }
      });
  }

  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(recv_data_, max_length),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec)
        {
          if (length > 0)
          {
            recv_data_[length] = '\0';
            string str = recv_data_;
            strcpy(recv_data_, refactor(str).c_str());

            cout << "<script>document.getElementById('" << id << "').innerHTML += \"" << recv_data_ << "\";</script>" << endl;
            if ((int)str.find('%', 0) >= 0)
            {
              ifs.getline(send_data_, max_length); // read from t*.txt
              string tmp = send_data_;
              int send_length = tmp.length();
              tmp = refactor(tmp);
              if (send_length > 0) //cmd
              {
                cout << "<script>document.getElementById('" << id << "').innerHTML += \"<b>" << tmp << "<br><b>\";</script>" << endl;
                send_data_[send_length] = '\n';
                do_write((send_length + 1) * sizeof('\n'));
              }
            }
            else
            {
              do_read();
            }
          }
          else if (length == 0)
          { //exit
            socket_.close();
          }
        }
        else
        {
          socket_.close();
        }
      });
  }

  void do_write(int send_length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(send_data_, send_length),
      [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if (!ec)
        {
          do_read();
        }
        else
        {
          socket_.close();
        }
      });
  }

  string refactor(string msg)
  {
    string re = "";
    for (int i = 0; i < (int)msg.length(); i++)
    {
      if (msg[i] == '\n')
        re += "<br>";
      else if (msg[i] == '\r')
        re += "";
      else if (msg[i] == '"')
        re += "&quot;";
      else if (msg[i] == '\'')
        re += "\\'";
      else
        re += msg[i];
    }
    return re;
  }

private:
  boost::asio::io_context &io_context_;
  tcp::socket socket_;
  string id;
  string file_name;
  std::ifstream ifs;
  int sock_fd = (int)socket_.native_handle();
  enum
  {
    max_length = 10001
  };
  char recv_data_[max_length];
  char send_data_[max_length];
  int send_length;
};

int main(int argc, char *argv[])
{
  try
  {
    parse_query_string(getenv("QUERY_STRING"));
    print_header();
    vector<shared_ptr<client>> c_ptr;
    vector<thread> threads;
    boost::asio::io_context io_context[5];
    for (int i = 0; i < 5; i++)
    {
      if (InfoTable[i].isExist)
      {

        tcp::resolver resolver(io_context[i]);
        string p = to_string(InfoTable[i].port);
        auto endpoints = resolver.resolve((InfoTable[i].host).c_str(), p.c_str());
        c_ptr.emplace_back(new client(io_context[i], endpoints, InfoTable[i].id, InfoTable[i].File));

        threads.emplace_back([&io_context, i] { io_context[i].run(); });
      }
    }
    for (auto &thread : threads)
    {
      if (thread.joinable())
        thread.join();
    }
  }
  catch (std::exception &e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }
  return 0;
}