INCLUDE = ..

all: ptextest.slo ptexenvtest.slo

ptextest.slo : ptextest.sl
	shader -I$(INCLUDE) ptextest.sl

ptexenvtest.slo : ptexenvtest.sl
	shader -I$(INCLUDE) ptexenvtest.sl

