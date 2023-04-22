all: wReceiver wSender

wReciver: wReciver.c
			gcc -o  wReceiver wReceiver.c 

wSender: wSender.c
			gcc -o  wSender wSender.c 

clean:
	rm wReceiver wSender
