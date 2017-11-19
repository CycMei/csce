GPPOUT = g++ -std=c++11 -Wall
GPP = $(GPPOUT) -c

libboost = -lboost_system -lboost_thread -lpthread

msgo = msg.o
msgofile = chat_message.cpp
msgofilel = chat_message.h $(msgofile)

$(msgo) : $(msgofilel)
	$(GPP) $(msgofile) -o $(msgo)



cliento = client.o
clientofile = chat_client.cpp
clientofilel = $(clientofile) $(msgofile)

$(cliento) : $(clientofilel)
	$(GPP) $(clientofile) -o $(cliento)

clienta = clients
clientfilel = $(msgo) $(cliento)

$(clienta) : $(clientfilel)
	$(GPPOUT) $(clientfilel) -o $(clienta) $(libboost)


serviceo = service.o
serviceofile = chat_server.cpp
serviceofilel = $(msgofile) $(serviceofile)

$(serviceo) : $(serviceofilel)
	$(GPP) $(serviceofile) -o $(serviceo)

servicea = services
servicefilel = $(msgo) $(serviceo)

$(servicea) : $(servicefilel)
	$(GPPOUT) $(servicefilel) -o $(servicea) $(libboost)


all : $(clienta) $(servicea)

clean:
	rm *o
