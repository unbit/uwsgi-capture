use IO::File;
use File::Basename;

my $app = sub {
	my $env = shift;

	if ($env->{PATH_INFO} eq '/eyetoy') {
		uwsgi::websocket_handshake($env->{HTTP_SEC_WEBSOCKET_KEY}, $env->{HTTP_ORIGIN});
		while(1) {
			uwsgi::sharedarea_wait(0, 50);
			uwsgi::websocket_send_binary_from_sharedarea(0, 0)
		}
	}
	else {
		return [200, ['Content-Type' => 'text/html'], new IO::File(dirname(__FILE__).'/eyetoy.html')];
	}
}
