THREADS = 4

render: test.slo
	render -t:$(THREADS) dispplane.rib

test.slo : test.sl
	shader -I../.. test.sl
