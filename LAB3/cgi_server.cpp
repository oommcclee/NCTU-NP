#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <stdio.h>
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
      [this, self](boost::system::error_code ec, std::size_t length){
        if (!ec)
        {
          if (length > 0)
          {
            recv_data_[length] = '\0';
            string str = recv_data_;
            strcpy(recv_data_, refactor(str).c_str());
            
            cout << "<script>document.getElementById('" << id << "').innerHTML += \"" << recv_data_ << "\";</script>" << endl;
            if((int)str.find('%', 0) >=0)
            {
              ifs.getline(send_data_,max_length); // read from t*.txt
              string tmp = send_data_;
              int send_length = tmp.length();
              tmp = refactor(tmp);
              if (send_length >0) //cmd
              {
                cout << "<script>document.getElementById('" << id << "').innerHTML += \"<b>" << tmp << "<br><b>\";</script>" << endl;
                send_data_[send_length] = '\n';
                do_write((send_length+1)*sizeof('\n'));
              }
            }
            else{
                do_read();
            }
          }
          else if (length == 0) {//exit
            socket_.close();
          } 
            
        }
        else{
          socket_.close();
        }
      });
  }

  void do_write(int send_length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_,boost::asio::buffer(send_data_, send_length),
      [this,self](boost::system::error_code ec, std::size_t /*length*/) {
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
          set_http_env();

          dup2(sock_fd, 0);
          dup2(sock_fd, 1);
          cout << "HTTP/1.1 200 OK\r\n";
          cout.flush();
          if (http.REQUEST_URI == "/panel.cgi")
            do_panel();
          else if (http.REQUEST_URI == "/console.cgi")
            do_console();
          else if (http.REQUEST_URI == "/printenv.cgi")
            do_printenv();
        }
      });
  }

  void do_panel(){
    cout << "Content-type: text/html\n" << endl;
    cout << "<!DOCTYPE html>\
  <html lang=\"en\">\
    <head>\
      <title>NP Project 3 Panel</title>\
      <link\
        rel=\"stylesheet\"\
        href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
        integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
        crossorigin=\"anonymous\"\
      />\
      <link\
        href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
        rel=\"stylesheet\"\
      />\
      <link\
        rel=\"icon\"\
        type=\"image/png\"\
        href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\
      />\
      <style>\
        * {\
          font-family: \'Source Code Pro\', monospace;\
        }\
      </style>\
    </head>\
    <body class=\"bg-secondary pt-5\">" <<endl;

    cout << "<form action=\"console.cgi\" method=\"GET\">\
      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\
        <thead class=\"thead-dark\">\
          <tr>\
            <th scope=\"col\">#</th>\
            <th scope=\"col\">Host</th>\
            <th scope=\"col\">Port</th>\
            <th scope=\"col\">Input File</th>\
          </tr>\
        </thead>\
        <tbody>" << endl;
    
    for(int i = 0;i < 5;i++){
      cout << "<tr>\
            <th scope=\"row\" class=\"align-middle\">Session "<< i + 1 << "</th>\
            <td>\
              <div class=\"input-group\">\
                <select name=\"h" << i << "\" class=\"custom-select\">\
                  <option></option>\
                  <option>nplinux1</option>\
                  <option>nplinux2</option>\
                  <option>nplinux3</option>\
                  <option>nplinux4</option>\
                  <option>nplinux5</option>\
                  <option>nplinux6</option>\
                  <option>nplinux7</option>\
                  <option>nplinux8</option>\
                  <option>nplinux9</option>\
                  <option>nplinux10</option>\
                  <option>nplinux11</option>\
                  <option>nplinux12</option>\
                </select>\
                <div class=\"input-group-append\">\
                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\
                </div>\
              </div>\
            </td>\
            <td>\
              <input name=\"p" << i << "\" type=\"text\" class=\"form-control\" size=\"5\" />\
            </td>\
            <td>\
              <select name=\"f" << i << "\" class=\"custom-select\">\
                <option></option>\
                <option>t1.txt</option>\
                <option>t2.txt</option>\
                <option>t3.txt</option>\
                <option>t4.txt</option>\
                <option>t5.txt</option>\
                <option>t6.txt</option>\
                <option>t7.txt</option>\
                <option>t8.txt</option>\
                <option>t9.txt</option>\
                <option>t10.txt</option>\
              </select>\
            </td>\
          </tr>" << endl;
    }

    cout << "<tr>\
            <td colspan=\"3\"></td>\
            <td>\
              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\
            </td>\
          </tr>\
        </tbody>\
      </table>\
    </form>\
  </body>\
</html>" << endl;
  }

  void do_console(){
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

        threads.emplace_back([&io_context, i] { io_context[i].run();});
      }
    }
    for (auto &thread : threads)
    {
      if(thread.joinable())
        thread.join();
    }
  }

  void do_printenv(){}

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

  void parse_request(){
    char *tok;
    char *tok2;
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
          InfoTable[index].File = p+Split2[1];
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
    cout << "Content-type: text/html\n" << endl;
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
                  <tr>" << endl;
    for (int i = 0; i < 5; i++)
    {
      if (InfoTable[i].isExist)
        cout << "<th scope=\"col\">" << InfoTable[i].host << ":" << InfoTable[i].port << "</th>" << endl;
    }
    cout << "</tr>\
          </thead>\
          <tbody>\
          <tr>" << endl;
    for (int i = 0; i < 5; i++)
    {
      if (InfoTable[i].isExist)
        cout << "<td ><pre id=\"" << InfoTable[i].id << "\" class=\"mb-0\"></pre></td>" << endl;
    }
    cout << "</tr>\
        </tbody>\
      </table>\
    </body>\
  </html>" << endl;
  }

  tcp::socket socket_;
  enum
  {
    max_length = 1024
  };
  char data_[max_length];
  Server_env http;
  bool have_query_string = false;
  int sock_fd = (int)socket_.native_handle();
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
      std::cerr << "Usage: ./cgi_server.exe <port>\n";
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