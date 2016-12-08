#include "Headers.hpp"
#include "ClientConnection.hpp"
#include "ServerConnection.hpp"
#include "FlakyFakeSocketHandler.hpp"
#include "UnixSocketHandler.hpp"
#include "ProcessHelper.hpp"
#include "CryptoHandler.hpp"

#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <pwd.h>

#include "ETerminal.pb.h"

shared_ptr<ClientConnection> globalClient;

#define FAIL_FATAL(X) if((X) == -1) { printf("Error: (%d), %s\n",errno,strerror(errno)); exit(errno); }

termios terminal_backup;

DEFINE_string(host, "localhost", "host to join");
DEFINE_int32(port, 10022, "port to connect on");
DEFINE_string(passkey, "", "Passkey to encrypt/decrypt packets");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  FLAGS_logbufsecs = 0;
  FLAGS_logbuflevel = google::GLOG_INFO;
  srand(1);

  std::shared_ptr<SocketHandler> clientSocket(new UnixSocketHandler());

  shared_ptr<ClientConnection> client = shared_ptr<ClientConnection>(
    new ClientConnection(clientSocket, FLAGS_host, FLAGS_port, FLAGS_passkey));
  globalClient = client;
  while(true) {
    try {
      client->connect();
    } catch (const runtime_error& err) {
      LOG(ERROR) << "Connecting to server failed: " << err.what() << endl;
      sleep(1);
      continue;
    }
    break;
  }
  cout << "Client created with id: " << client->getClientId() << endl;

  termios terminal_local;
  tcgetattr(0,&terminal_local);
  memcpy(&terminal_backup,&terminal_local,sizeof(struct termios));
  struct winsize win = { 0, 0, 0, 0 };
  ioctl(1, TIOCGWINSZ, &win);
  cfmakeraw(&terminal_local);
  tcsetattr(0,TCSANOW,&terminal_local);
  cout << win.ws_row << " "
       << win.ws_col << " "
       << win.ws_xpixel << " "
       << win.ws_ypixel << endl;


  // Whether the TE should keep running.
  bool run = true;

  // TE sends/receives data to/from the shell one char at a time.
#define BUF_SIZE (1024)
  char b[BUF_SIZE];

  while (run)
  {
    // Data structures needed for select() and
    // non-blocking I/O.
    fd_set rfd;
    fd_set wfd;
    fd_set efd;
    timeval tv;

    FD_ZERO(&rfd);
    FD_ZERO(&wfd);
    FD_ZERO(&efd);
    FD_SET(STDIN_FILENO, &rfd);
    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    select(STDIN_FILENO + 1, &rfd, &wfd, &efd, &tv);

    try {
      // Check for data to send.
      if (FD_ISSET(STDIN_FILENO, &rfd))
      {
        // Read from stdin and write to our client that will then send it to the server.
        int rc = read(STDIN_FILENO, b, BUF_SIZE);
        FAIL_FATAL(rc);
        if (rc > 0) {
          //VLOG(1) << "Sending byte: " << int(b) << " " << char(b) << " " << globalClient->getWriter()->getSequenceNumber();
          string s(b,rc);
          et::TerminalBuffer tb;
          tb.set_buffer(s);
          globalClient->writeProto(tb);
        } else {
          LOG(FATAL) << "Got an error reading from stdin: " << rc;
        }
      }

      while (globalClient->hasData()) {
        et::TerminalBuffer tb =
          globalClient->readProto<et::TerminalBuffer>();
        const string& s = tb.buffer();
        //VLOG(1) << "Got byte: " << int(b) << " " << char(b) << " " << globalClient->getReader()->getSequenceNumber();
        size_t bytesWritten = 0;
        do {
          int rc = write(STDOUT_FILENO, &s[0] + bytesWritten, s.length() - bytesWritten);
          FATAL_FAIL(rc);
          if (rc==0) {
            LOG(ERROR) << "Could not write byte, trying again...";
          }
          bytesWritten += rc;
        } while(bytesWritten != s.length());
      }
    } catch (const runtime_error &re) {
      cout << "Error: " << re.what() << endl;
      run = false;
    }
  }

  tcsetattr(0,TCSANOW,&terminal_backup);
  globalClient.reset();
  client.reset();
  cout << "Client derefernced" << endl;
  return 0;
}
