#include "Session.h"

int exitValue = 0;

bool cleanupConn = false;

void cleanup()
{
    if (cleanupConn)
        tcsetattr(0, TCSADRAIN, &oldti);
	cleanupConn = 0;
	printf("\n");
}

int main() {
    asio::io_context io;

    std::cerr << "\033[2J";

    auto session = std::make_shared<Session>(io, false);

  	tty.grabwinsize();
	info_to_stderr = false;

	setupterm(NULL, 0, NULL);
	printf("%s", tty.getinfo("enacs", "").c_str());

	struct termios ti;
	tcgetattr(0, &oldti);
	cfmakeraw(&ti);
	tcsetattr(0, TCSADRAIN, &ti);

    cleanupConn = true;
	atexit(cleanup);

    // signal(SIGWINCH, winch);

    session->do_read_stdin();
    session->start("dev.cryosphere.org", "6666"); 
	tty.bad_have = true;

    io.run();
 
 	cleanup();
}