use IO::File;
my $app = sub {
	my $env = shift;

	if ($env->{PATH_INFO} eq '/eyetoy') {
		uwsgi::websocket_handshake($env->{HTTP_SEC_WEBSOCKET_KEY}, $env->{HTTP_ORIGIN});
		my $chunk = "\0" x 614400;
		while(1) {
			uwsgi::sharedarea_wait(0, 50);
			#uwsgi::sharedarea_readfast(0, 0, $chunk);
			#my $chunk = uwsgi::sharedarea_read(0, 0);
			#uwsgi::websocket_send_binary($chunk);
			uwsgi::websocket_send_binary_from_sharedarea(0, 0)
		}
	}
	else {
		return [200, ['Content-Type' => 'text/html'], new IO::File('eyetoy.html')];
	}
}
