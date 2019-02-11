
# Cancel and restart compression when restarting
sub cancel_compression {
	mcl_send_unbuffered ("compress");
	mcl_send_unbuffered ("compress");
}

done_add(\&cancel_compression);
