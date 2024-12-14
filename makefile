all: manager feed

manager: manager.c
	gcc -o manager manager.c util.c

feed: feed.c
	gcc -o feed feed.c util.c

clean:
	rm -f manager feed
